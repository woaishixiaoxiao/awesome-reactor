#include <stdio.h>
#include <fcntl.h>
#include <errno.h>
#include <unistd.h>
#include <signal.h>
#include <strings.h>
#include <pthread.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/socket.h>

#include "msg_head.h"
#include "tcp_conn.h"
#include "tcp_server.h"
#include "print_error.h"
#include "config_reader.h"

void accepter_cb(event_loop* loop, int fd, void *args) {
	tcp_server* server = (tcp_server*)args;
	server->do_accept();
}
tcp_conn** tcp_server::conns = NULL;
int tcp_server::_conns_size = 0;
int tcp_server::_max_conns = 0;
int tcp_server::_curr_conns = 0;
pthread_mutex_t tcp_server::_mutex = PTHREAD_MUTEX_INITIALIZER;
msg_dispatcher tcp_server::dispatcher;

tcp_server::conn_callback tcp_server::connBuildCb = NULL;//用户设置连接建立后的回调函数
tcp_server::conn_callback tcp_server::connCloseCb = NULL;//用户设置连接释放后的回调函数

tcp_server::tcp_server(event_loop *loop, const char *ip, uint16_t port): _keepalive(false) {
	::bzero(&_connaddr, sizeof(_connaddr));
	if(::signal(SIGHUP, SIG_IGN) == SIG_ERR) {
		error_log("signal ignore SiGHUP");
	}
	if(::signal(SIGPIPE, SIG_IGN) === SIG_ERR) {
		error_log("signal ignore SIGPIPE");
	}
	_sockfd = ::socket(AF_INET, SOCK_STREAM | SOCK_NONBLOCK | SOCK_CLOEXEC, IPPROTO_TCP);
	exit_if(_sockfd == -1, "socket()");

	_reservfd = ::open("/tmp/reactor_accepter", O_CREAT | O_RDONLY | O_CLOEXEC, 0666);//这是为了之后可以处理accept返回EMFILE的情况，在accept函数中有说明
	error_if(_reservfd == -1, "open()");
	
	struct sockaddr_in servaddr;
    ::bzero(&servaddr, sizeof (servaddr));
    servaddr.sin_family = AF_INET;
    int ret = ::inet_aton(ip, &servaddr.sin_addr);
    exit_if(ret == 0, "ip format %s", ip);
    servaddr.sin_port = htons(port);

    int opend = 1;
    ret = ::setsockopt(_sockfd, SOL_SOCKET, SO_REUSEADDR, &opend, sizeof(opend));
    error_if(ret < 0, "setsockopt SO_REUSEADDR");

    ret = ::bind(_sockfd, (const struct sockaddr*)&servaddr, sizeof servaddr);
    exit_if(ret == -1, "bind()");
	
	linfo_log("server on %s:%u is running...", ip, port);
	
	_loop = loop;
	
	_addrlen = sizeof(struct sockaddr_in);
	
	int thread_cnt = config_reader::ins()->GetNumber("reactor", "threadNum", 0);
	_thd_pool = NULL;
	if(thread_cnt) {
		_thd_pool = new thread_pool(thread_cnt);
		 exit_if(_thd_pool == NULL, "new thread_pool");
	}
	
	_max_conns = config_reader::ins()->GetNumber("reactor", "maxConns", 10000);
	int next_fd = ::dup(1);
	_conns_size = _max_conns + next_fd;
	::close(next_fd);
	
	_conns = new tcp_conn*[_max_conns];
	exit_if(conns == NULL, "new conns[%d]", _max_conns);
	for(int i = 0; i < _max_conns; ++i) {//这里不应该是_conns。
		conns[i] = NULL;
	}
	_loop->add_ioev(_sockfd, acceptor, EPOLLIN, this);
}

tcp_server::~tcp_server() {
	_loop->del_ioev(_sockfd);
	::close(_sockfd);
	::close(_reservfd);
}
//注册的三个和fd有关的回调函数，都是while循环。并且均是在系统调用返回EAGIN之后跳出。通过分析函数可知，1、这可以处理一些错误 2、在处理可读事件，能处理完所有的完整数据包
void tcp_server::do_accept() {
	int connfd;
	bool conn_full = false;
	while(true) {
		connfd = ::accept(_sockfd, (struct sockaddr*)&_connaddr, &_addrlen);
		if(connfd == -1) {
			if(errno == EINTR) {
				continue;
			}else if (errno == EMFILE){
				//三次握手已经完成，客户的描述符已经加入到连接队列中，等待服务器用accept提取出来。
				//但是当服务器进程的文件描述符用完了，accept便会返回错误并置ERROR = EMFILE
				//这时候将保留的resever_fd关闭。并再次accept就能将此次连接从已连接队列中提取出来了
				//但是需要将此connfd关闭掉，因为fd不够用了。之后的连接也希望用这种方式来进行关闭
				conn_full = true;
				::close(_reser)
			}else if (errno == EAGAIN) {
				break;
			}else {
				exit_log("accept()");
			}
		}else if (conn_full) {
			::close(connfd);
			_reservfd = ::open("/tmp/reactor_accepter", O_CREAT | O_RDONLY | O_CLOEXEC, 0666);
			error_if(_reservfd == -1, "open()");
		}else {
			int curr_conns;
			get_conn_num(curr_conns);
			if(curr_conns >= _max_conns) {
				error_log("connection exceeds the maximum connection count %d", _max_conns);
				::close(connfd);
			}else {
				assert(connfd < _conns_size);
				if(_keep_alive) {
					int opend = 1;
					int ret = ::setsockopt(connfd, SOL_SOCKET, SO_KEEPALIVE, &opend, sizeof(opend));
                    error_if(ret < 0, "setsockopt SO_KEEPALIVE");
				}
				//提取出来描述符后，看是根据多线程方式还是单线程的方式
				if(_thd_pool) {
					//每个线程绑定了一个thread_queue作为线程函数的参数。
					thread_queue<queue_msg>* cq = _thd_pool->get_next_thread();//这里不用加锁，可以看下这个函数，没有线程改变_pool这个变量。
					queue_msg msg;
					msg.cmd_type = queue_msg::NEW_CONN;
					msg.connfd = connfd;
					cq->send_msg(msg);//这个函数中就是需要加锁了，因为cq这个队列的内容这里会写入，还会有子线程提取出来
				}else {
					tcp_conn* conn = conns[connfd];
					if(conn) {
						conn->init(confd, _loop);
					}else {
						conn = new tcp_conn(connfd, _loop);
						exit_if(conn == NULL, "new tcp_conn");
                        conns[connfd] = conn;
					}
				}
			}
		}
	}
}

thread_queue<queue_msg>* thread_pool::get_next_thread() {
	if(_curr_index == _thread_cnt) {
		_curr_index = 0;
	}
	return _pool[_curr_index++];
}

void tcp_server::inc_conn() {
	::pthread_mutex_lock(&_mutex);
	++_curr_conns;
	::pthread_mutex_unlock(&_mutex);
}

void tcp_server::inc_conn() {
	::pthread_mutex_lock(&_mutex);
	--_curr_conns;
	::pthread_mutex_unlock(&_mutex);
}

void tcp_server::get_conn_num(int& cnt)
{
    ::pthread_mutex_lock(&_mutex);
    cnt = _curr_conns;
    ::pthread_mutex_unlock(&_mutex);
}
