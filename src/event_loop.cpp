#include "event_loop.h"
#include "timer_queue.h"
#include "print_error.h"
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <string.h>
#include <errno.h>
#include <time.h>

/*和时间相关的之后回再来看*/
void timerqueue_cb(event_loop* loop, int fd, void *args)
{
    std::vector<timer_event> fired_evs;
    loop->_timer_que->get_timo(fired_evs);
    for (std::vector<timer_event>::iterator it = fired_evs.begin();
        it != fired_evs.end(); ++it)
    {
        it->cb(loop, it->cb_data);
    }
}

event_loop::event_loop()
{
	_epfd = ::epoll_create1(0);//加上了:: 表示使用全局的函数而不是类内部的函数。epoll_create1和epoll_create无区别，自行google；
    exit_if(_epfd == -1, "when epoll_create1()");
    _timer_que = new timer_queue();
    exit_if(_timer_que == NULL, "when new timer_queue");
    //register timer event to event loop
    add_ioev(_timer_que->notifier(), timerqueue_cb, EPOLLIN, _timer_que);
}

void process() {
	int nfds, n;
	int fd;
	while(1) {
		nfds = ::epoll_wait(epfd, _fried_eves, MAXEVENTS, 10);//后面还有run_task，所以这里不要是-1。
		for(n = 0; n < nfds; n++) {
			fd = _fried_eves[n].data.fd;
			ioev_it ite = _ioev_evs.find(fd);
			if(ite != _ioev_evs.end()) {
				if(_fried_eves[n].events & (EPOLLIN | EPOLLOUT)) {
					if(_fried_eves[n].events & EPOLL_IN && ite->second.read_cb) {
						void * args = ite->second.rcb_args;
						ite->second.read_cb(this, fd, args)
					}
					if(_fried_eves[n].events & EPOLL_OUT && ite->second.write_cb) {
						void *args = ite->second.wcb_args;
						ite->second.write_cb(this, fd, args);
					}
				}else if(_fried_eves[n].events & (EPOLLHUP | EPOLLERR)) {
					if(_fried_eves[n].events & EPOLL_IN && ite->second.read_cb) {
						void * args = ite->second.rcb_args;
						ite->second.read_cb(this, fd, args)
					}
					if(_fried_eves[n].events & EPOLL_OUT && ite->second.write_cb) {
						void *args = ite->second.wcb_args;
						ite->second.write_cb(this, fd, args);
					}
					error_log("fd %d get error, delete it from epoll", _fired_evs[i].data.fd);
                    del_ioev(_fired_evs[i].data.fd);
				}
			}
		}
		run_task();
	}
}
/*该函数有两个功能 一个是添加  一个是修改 修改只能从EPOLL_OUT(EPOLL_ADD)=>EPOLL_OUT|EPOLL_OUT
mask传来的参数只能是EPOLLOUT或者EPOLLIN 但是不能是两个的并集。修改的方式可以增加一个回调函数的参数
但是通过每次增加单个的IN或者OUT也可以达到目的*/
void event_loop::add_ioev(int fd, io_callback *proc, int mask, void *args=NULL) {//这里没有传来io_event结构体挺好的，否则会有两个问题，一个是函数体中要提取，另外一个如果传递值类型，那就会有效率的问题，如果传递指针 就有内存泄露的问题。
	int opt;
	int final_mask;
	struct epoll_event ev;
	io_event ioev;
	ioev_it ite = _ioev_evs.find(fd);
	if(ite == _ioev_evs.end()) {
		final_mask = mask;
		opt = EPOLL_CTL_ADD;
	}else {
		final_mask = (mask | _ioev_it->mask);
		opt = EPOLL_CTL_MOD;
	}
	ioev.mask = final_mask;
	if(mask & EPOLLOUT) {   //因为mask只能是IN 或者 OUT 所以这里用等号最好
		ioev.write_cb = proc;
		rcb_args = args;
	}else if(mask & EPOLLIN) {
		ioev.read_cb = proc;
		wcb_args = args;
	}
	_listening.insert(fd);
	_ioev_evs[fd] = ioev;
	ev.data.fd = fd;
	ev.events = final_mask;
	epoll_ctl(_epfd, opt, fd, &ev);//最好还是对这些标准库进行包裹函数封装
	error_if(ret == -1, "epoll_ctl");
}

void event_loop::del_ioev(int fd, int mask) {
	int opt;
	ioev_it ite = _ioev_evs.find(fd);
	int &o_mask = ite->second.mask;
	o_mask = (ite->second.mask &= (~mask))
	if(ite != _ioev_evs.end()) {
		if(o_mask) {
			struct epoll_event event;
			event.events = o_mask;
			event.data.fd = fd;
			ret = ::epoll_ctl(_epfd, EPOLL_CTL_MOD, fd, &event);
			error_if(ret == -1, "epoll_ctl EPOLL_CTL_MOD");
		}else {
			_io_evs.erase(it);
			ret = ::epoll_ctl(_epfd, EPOLL_CTL_DEL, fd, NULL);
			error_if(ret == -1, "epoll_ctl EPOLL_CTL_DEL");
			listening.erase(fd);//从监听集合中删除;
		}
	}
}

void event_loop::del_ioev(int fd) {
	ioev_it ite = _ioev_evs.find(fd);
	if(ite != _ioev_evs.end()) {
		_io_evs.erase(it);
		ret = ::epoll_ctl(_epfd, EPOLL_CTL_DEL, fd, NULL);
		error_if(ret == -1, "epoll_ctl EPOLL_CTL_DEL");
		listening.erase(fd);//从监听集合中删除;
	}
}

/*下面几个和时间相关的后面在实现*/

//这个是ts是绝对时间。
int event_loop::run_at(timer_callback cb, void *args, uint64_t ts) {
	timer_event te(cb, args, ts);
	return _timer_que->add_timer(te);
}

//下面这两个都是在多长时间以后，一个是一次性的，一个是有间隔的。
int event_loop::run_after(time_callback cb, void *args, int sec, int millis=0) {
	struct timespec tpc;
	clock_gettime(CLOCK_REALTIME, &tpc);
	uint64_t ts = tpc.tv_sec * 1000 + tpc.tv_nsec / 1000000;
	ts += sec * 1000 + millis; //先统一转换为ms
	timer_event te(cb, args, ts);
	return _timer_que->add_timer(te);
}
int event_loop::run_every(time_callback cbm void *args, int sec, int millis=0) {
	uint32_t interval = sec * 1000 + millis;
	struct timespec tpc;
	clock_gettime(CLOCK_REALTIME, &tpc);
    uint64_t ts = tpc.tv_sec * 1000 + tpc.tv_nsec / 1000000UL + interval;
    timer_event te(cb, args, ts, interval);
    return _timer_que->add_timer(te);
}

void event_loop::del_timer(int timer_id) {
}

void event::loop::add_task(pendingFunc func, void *args) {
	std::pair<pendingFunc, void *>item(func, args);
	_pendingFactors.push_back(item);
}

void event_loop::run_task() {
	std::vector<std::pair<pendingFunc, void *> >::iterator ite;//回调函数以及其参数，用此pair结构体正好。
	for(ite = _pendingFactors.begin(); ite != _pendingFactors.end(); ite++) {
		pendingFunc func = it->first;
        void* args = it->second;
        func(this, args);//这里的第一个参数是event_loop对象。
	}
	_pendingFactors.clear();
}
