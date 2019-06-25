#ifdef __EVENT_LOOP_H
#define __EVENT_LOOP_H

#include <sys/epoll.h>
#include <ext/hash_map>
#include <ext/hash_set>
#include <vector>
#include "event_base.h"
#include "timer_queue.h"

#define MAXEVENTS 10
class event_loop {
	friend void timerqueue_cb(event_loop *loop, int fd, void *args);//友元函数的声明位置在哪里都可以。另外友元函数的参数里面一定会包含对应的类
	private:
		int _epfd;
		epoll_event _fried_eves[MAXEVENTS];
		__gnu_cxx::hash_map<int, io_event>_ioev_evs;
		typedef __gnu_cxx::hash_map<int, io_event>::iterator ioev_it;
		timer_queue* _timer_que;
		std::vector<std::pair<pendingFunc, void *> >_pendingFactors;//回调函数以及其参数，用此pair结构体正好。
		__gnu_cxx::hash_set<int> _listening;
	public:
		event_loop();
		void process_evs();
		/*既然名字都叫做add del了为什么还要传递mask*/
		void add_ioev(int fd, io_callback* proc, int mask, void *args=NULL);
		void del_ioev(int fd, int mask);
		void del_ioev(int fd);
		void nlistenings(__gnu_cxx::hash_set<int>& conns)) {conns = _listening; }//这个函数返回listens的
		
		//下面均是和时间相关的
		int run_at(timer_callback cb, void *args, uint64_t ts);
		int run_after(time_callback cb, void *args, int sec, int millis=0);
		int run_every(time_callback cbm void *args, int sec, int millis=0);
		void del_timer(int timer_id);
		
		//下面的两个是每次循环中完成发生的事件后，调用该函数，完成客户端注册的函数，RPC机制？？？
		void add_task(pendingFunc func, void *args);//进行注册  pendingFunc该函数的第一个参数是event_loop
		void run_task();
};