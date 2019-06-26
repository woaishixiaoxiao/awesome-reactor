#include"thread_pool.h"
#include"thread_queue.h"

void msg_comming_cb(event_loop *loop, int fd, void *args) {
	thread_queue<queue_msg>* queue = (thread_queue<queue_msg>*)args;
	std::queue<queue_msg> msgs;
	queue->recv_msg(msgs);
	while(!msgs.empty()) {
		queue_msg msg_one = msgs.front();
		msgs.pop();
		if(msg_one.cmd_type == queue_msg::NEW_CONN) {//当有客户连接来的时候，
			tcp_conn* conn = tcp_server::conns[msg.connfd];
			if(tcp_conn) {
				conn->init(msg.connfd, loop);
			}else {
				tcp_server::conns[msg.connfd] = new tcp_conn(mgs.connfd, loop);
				exit_if(tcp_server::conns[msg.connfd] == NULL, "new tcp_conn");
			}
		}else if(msg_one.cmd_type == queue_msg::NEW_TASK) {
			loop->add_task(msg.task, msg.args);//主进程可以通过thread_pool中的run_task向子进程中添加任务。
		}else if(msg_one.cmd_type === queue_msg::STOP_THD) {//主进程希望停止子进程
			
		}
	}
}

void do_main(void *args) {
	thread_queue<queue_msg>* tmp = (thread_queue<queue_msg>*)args;
	event_loop *loop = new event_loop;
	tmp->set_loop(loop, msg_comming, args);
	loop->process();
}

thread_pool::thread_pool(int thread_cnt) _thread_cnt(thread_cnt), _curr_index(0){ 
	exit_if(thread_cnt <= 0 || thread_cnt > MAX_THREAD_NUM, "error thread_cnt %d", thread_cnt);
	_pool = new thread_queue<queue_msg>*[_thread_cnt];
	tids  = new pthread_t[_thread_cnt];
	for(int i = 0; i < thread_cnt; i++) {
		_pool[i] = new thread_queue<queue_msg>();
		::pthread_create(&tids[i], NULL, do_main, (void *)_pool[i]);
	}	
}

//每一个线程可以由一个thread_queue<queue_msg>*来代表。因为主线程可以根据此队列和子进程通信。
thread_queue<queue_msg>* thread_pool::get_next_thread() {
	/**
	代码写的又丑又有问题
	thread_queue<queue_msg>* tmp = _pool[_curr_index];
	if(++_curr_index >= MAX_THREAD_NUM) {
		_curr_index = 0;
	}
	return tmp;
	**/
	if(_curr_index == _thread_cnt) {
		_curr_index = 0;
	}
	return _pool[_curr_index++];
}


//这个函数是向某一个进程增加task任务
void thread_pool::run_task(int thd_index, pendingFunc task, void *args=NULL) {
	queue_msg msg;
	msg.cmd_type = NEW::TASK;
	msg.task = task;
	msg.args = args;
	thread_queue<queue_msg>* cq = _pool[thd_index];
	cq->send_msg(msg);
}

//该函数是对每个子线程的pendingFunc添加任务。
void thread_pool::run_task(pendingFunc task, void *args) {
	for(int i = 0; i < _thread_cnt; ++i) {
		run_task(i, task, args);
	}
}