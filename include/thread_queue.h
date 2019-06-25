#ifndef __THREAD_QUEUE_H__
#define __THREAD_QUEUE_H__

#include <queue>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>
#include <algorithm>
#include <sys/eventfd.h>
#include "event_loop.h"

//每一个线程都会绑定一个该队列，该队列一是存放消息队列，二是作为主进程和子进程通信的载体。第二点需要借助thread_pool
template<typename T>
class thread_queue {
	private:
		int _evfd;
		event_loop* _loop;
		std::queue<T> _queue;
		pthread_mutext_t _mutex;
	public:
		thread_queue() _loop(NULL) {
			::pthread_mutext_init(&_mutex)
			_evfd = ::eventfd(0,EFD_NONBLOCK);
			 if (_evfd == -1)
			{
				perror("eventfd(0, EFD_NONBLOCK)");
				::exit(1);
			}
		}
		~thread_queue() {		
			::thread_mutex_destroy(&mutex);
			close(_evfd);
			//_loop应该不由这里释放， 到时候外面看下 由哪个释放。
		}
		void send_msg(const T &item) {
			thread_mutex_lock(_mutex);
			_queue.push_back(item);
			thread_mutex_unlock(_mutex);
			unsigned long long number = 1;
			int ret = ::write(_evfd, &number, sizeof(unsigned long long));
			if (ret == -1) perror("eventfd write");
		}
		enent_loop* get_loop() { return _loop; }
		void recv_msg(std::queue<T>& tmp_queue) {
			unsigned long long number;
			::read(_evfd, &number, sizeof(unsigned long long));
			pthread_mutex_lock(&mutex);
			std::swap(_queue, tmp_queue);
			pthread_mutex_unlock(&mutex);
		}
		void set_loop(event_loop *loop, io_callback *proc, void *args=NULL) {
			_loop = loop;
			_loop->add_ioev(_evfd, proc, EPOLLIN, args);
		}
};
#endif