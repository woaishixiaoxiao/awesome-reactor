#ifndef __TCP_SERVER_H__
#define __TCP_SERVER_H__

#include "event_loop.h"
#include "thread_pool.h"
#include "net_commu.h"
#include "tcp_conn.h"
#include "msg_dispatcher.h"
#include <netinet/in.h>

class tcp_server {
	private:
		int _sockfd;
		int _reservfd;
		event_loop* _loop;
		thread_pool* _thd_pool;
		struct sockaddr_in _connaddr;
		socklen_t _addrlen;
		bool _keepalive;
		static int _conns_size;
		static int _max_cons;
		static int _curr_conns;
		static pthread_mutex_t _mutex; //这个是对_curr_conns上锁的。
	public:
		static msg_dispatcher _dispatcher;
		static tcp_conn* *_conns; //类的共有变量是全局的，意味着子进程中也可以使用。
		typedef void (*conn_callback)(net_commu* com);
		static conn_callback connBuildCb;//用户设置连接建立后的回调函数
		static conn_callback connCloseCb;//用户设置连接关闭后的回调函数
		
		static void onConnBuild(conn_callback cb) { connBuildCb = cb; }
		static void onConnClose(conn_callback cb) { connBuildCb = cb; }
	public:
		tcp_server(event_loop* loop, const char* ip, uint16_t port);
		~tcp_server();
		void keep_alive() { _keepalive = true; }
		void do_accept();
		//这个函数注册的是处理业务逻辑的函数，而之前在loop中注册的是套接字可读可写的函数。两者的关系是读事件的回调函数读出了数据，将其组成数据包，然后调用dispatcher进行处理。
		void add_msg_cb(int cmdid, msg_callback* msg_cb, void* usr_data = NULL) { dispatcher.add_msg_cb(cmdid, msg_cb, usr_data); }
		static void inc_conn();
		static void get_conn_num(int &cnt);
		static void dec_conn();
		event_loop* loop(){ return _loop; }
		thread_pool* threadPool() { return _thd_pool; }
};
#endif