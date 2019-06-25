#ifndef __TCP_CONN_H__
#define __TCP_CONN_H__

#include"net_commu.h"
#include"io_buffer.h"
//这个类要提供注册的回调函数。
//每个已经连接的客户都应该对应这一个类
//该类中包含用户的读写缓冲，读写缓冲是从内存池中分配出来的。
class tcp_conn {
	private:
		int _connfd;
		event_loop *_loop;
		input_buffer ibuf;
		output_buffer obuf;
	public:
		tcp_conn(int connfd, event_loop *loop) { init(connfd, loop); }
		void init(int connfd, event_loop *loop);
		void handle_read();
		void handle_write();
		virtual int send_data(const char *data, int datlen, int cmdid);
		virtual int get_fd() { return _connfd; }
		void clean_conn();
};
#endif