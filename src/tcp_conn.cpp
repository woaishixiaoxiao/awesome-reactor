#include"tcp_conn.h"
#include "msg_head.h"
#include "tcp_server.h"
#include "print_error.h"
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <pthread.h>
#include <netinet/tcp.h>

static void tcp_rcb(event_loop* loop, int fd, void *args) {
	tcp_conn* conn = (tcp_conn*)args;
	conn->handle_read();
}

static void tcp_wcb(event_loop* loop, int fd, void *args) {
	tcp_conn* conn = (tcp_conn*)args;
	conn->handle_write();
}

void tcp_conn::init(int connfd, event_loop *loop) {
	_connfd = connfd;
	_loop = loop; 
	int flag = ::fcntl(_connfd, F_GETFL, 0);
	::fcntl(_connfd, F_SETFL, O_NONBLOCK | flag);
	
	int opend = 1;
	int ret = ::setsockopt(_connfd, IPPROTO_TCP, TCP_NODELAY, &opend, sizeof(opend));
	error_if(ret < 0, "setsockopt TCP_NODELAY");
	 //调用用户设置的连接建立后回调函数,主要是用于初始化作为连接内变量的：parameter参数
	if(tcp_server::connBuildCb) {
		tcp_server::connBuildCb(this);
	}
	_loop->add_ioev(_connfd, tcp_rcb, EPOLLIN, this);
	tcp_server::inc_conn();
}

//当读的时候，先从TCP接收缓冲中读出来数据，放到用户缓冲。这里是生成者
//然后在while循环中消费数据，每次消费完之后，要跳转用户缓冲，使得数据往前移动。
//处理业务。 这里引入了用户的缓存，但是没有出问题，为什么书上说引入用户缓存会有问题，是书上对一次读取的全部内容没有处理完全吗。
//对上面的疑问进行解答。这里引入的是用户的缓冲区，UNP中非阻塞那一张也引入了用户缓冲去，而且应该是必须引入的。书上说的是最好不要使用标准IO库，比如fget fput这些，因为这些函数会引入标准IO的缓冲区，而这个缓冲区对我们不可见，也就是不能操作。
void tcp_conn::handle_read() { 
	int ret = ibuf.read_data(_connfd);
    if (ret == -1)
    {
        //read data error
        error_log("read data from socket");
        clean_conn();
        return ;
    }
    else if (ret == 0)
    {
        //The peer is closed, return -2
        info_log("connection closed by peer"); //客户端是将所有的数据都发送完之后，才会发送结束的标识符。而发送结束符之前，服务端肯定已经将之前的请求处理完了。
        clean_conn();
        return ;
    }
	commu_head head;
	//每次都处理一个完整的数据包，不完整不处理
	while(_ibuf.length() >= COMMU_HEAD_LENGTH) {
		::memcpy(&head, _ibuf->data(), COMMU_HEAD_LENGTH);//这里存在跨平台问题，大端小端，结构体的大小。
		/***
		head结构体定义的时候，应该用uint32_t而不是int。
		head.cmdid = ntohs(*(uint32_t*)(_ibuf_data()));
		head.length = ntohs(*(uint32_t*)(_ibuf_data() + 4));
		相应的发送数据的时候也要将head结构体变成网络字节流
		***/
		if(head.length > MSG_LENGTH_LIMIT || head.length < 0) {
			//data format is messed up
            error_log("data format error in data head, close connection");
            clean_conn();
            break;
		}
		if(_ibuf.length() < COMMU_HEAD_LENGTH + head.length) {
			break;  //半个包不处理。
		}
		if (!tcp_server::dispatcher.exist(head.cmdid))
        {
            //data format is messed up
            error_log("this message has no corresponding callback, close connection");
            clean_conn();
            break;
        }
		_ibuf.pop(COMMU_HEAD_LENGTH);
		tcp_server::dispatcher.cb(_ibuf.data(), head.length, head.cmdid, this);
		_ibuf.pop(head.length);
		//pop这个函数一旦发现_ibuf中length为0，那么就会将底层的io_buffer回收。
		//而_ibuf.length以及adjust函数中会用到io_buffer。
		//举了上述的例子是为了说明，编写函数的时候，一定要判断参数。每个函数要将自己的功能写全，不能依赖其他的函数行为
		//这样即使组合起来用也没有问题。
	}
	_ibuf.adjust();
}

//当发送数据的时候，要消费用户发送缓冲中的数据，写完之后要进行用户缓冲跳转。这也是为什么在write_fd中要跳转用户缓冲，而read_fd不用
//无论是读缓冲还是写缓冲，只要buffer.length为0，就要将缓冲释放会内存池中。
void tcp_conn::handle_write() {
	while(_obuf.length()) {
		int ret = _obuf.write_fd(_connfd);
        if (ret == -1)
        {
            error_log("write TCP buffer error, close connection");
            clean_conn();
            return ;
        }
        if (ret == 0)
        {
            //不是错误，仅返回为0表示此时不可继续写
            break;
        }
	}
	if(!_obuf.length()) {  //这里删除掉监听写事件。
		_loop->del_ioev(_connfd, EPOLLOUT);
	}
}

//发送数据，此时需要监听描述符的写事件了。
int tcp_conn::send_data(const char *data, int datalen, int cmdid) {
	assert(data);
	int ret;
	bool need_listen = false;
	if(_buf.length()) {
		need_listen = true; //当_buf.length为0的时候，会取消掉监听。其他的时候不会取消掉
	}
	commu_head head;
	head.cmdid = cmdid;
	head.length = datalen;
	/**
	要将结构体变成网络字节序
	uint32_t *data = (uint32_t*)&head.cmdid;
	ret = _obuf->send(data, 4);
	if (ret != 0) {
		return -1;
	}
	data = (uint32_t*)&head.length;
	ret = _obuf->send(data, 4);
	if (ret != 0) {
		return -1;
	}
	**/
	ret = _obuf->send(&head, COMMU_HEAD_LENGTH);
	if (ret != 0) {
		return -1;
	}
	ret = _obuf->send_data(data, datalen);//io_buff类就么有返回<0的情况，在该类的send_data函数中，只要出问题就会直接暴漏结束。
	if (ret != 0)
    {
        //只好取消写入的消息头
        obuf.pop(COMMU_HEAD_LENGTH);
        return -1;
    }
	if(need_listen) {
		_loop->add_ioev(_connfd, tcp_wcb, EPOLLOUT, this);
	}
}

void tcp_conn::clean_conn() {
	//调用用户设置的连接释放后回调函数,主要是用于销毁作为连接内变量的：parameter参数
    if (tcp_server::connCloseCb)
        tcp_server::connCloseCb(this);
	tcp_server::dec_conn();
	_loop.del_ioev(_connfd);
	_loop = NULL;
	_ibuf->clear();
	_obuf->clear();
	int fd = _connfd;
	_connfd = -1;
	::close(fd);
}
