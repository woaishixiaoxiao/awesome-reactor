#ifndef __THREAD_POOL_H__
#define __THREAD_POOL_H__

#include "msg_head.h"
#include "thread_queue.h"
#include <pthread.h>

#define MAX_THREAD_NUM 30
class thread_pool {
	private:
		int _curr_index;
		int _thread_cnt;
		thread_queue<queue_msg>** _pool;//这里如果是单个指针会如何
		pthread_t* _tids;
	public:
		thread_pool(int thread_cnt);
		thread_queue<queue_msg>* thread_pool::get_next_thread();
		void thread_pool::run_task(int thd_index, pendingFunc task, void *args=NULL);
		void thread_pool::run_task(pendingFunc task, void *args);
};

#endif