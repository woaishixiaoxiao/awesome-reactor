#include <unistd.h>
#include <strings.h>
#include <pthread.h>
#include <sys/timerfd.h>
#include "timer_queue.h"
#include "print_error.h"

timer_queue::timer_queue(): _count(0), _next_timer_id(0), _pioneer(-1) {
	_timerfd = ::timerfd_create(CLOCK_REALTIME, TFD_NONBLOCK | TFD_CLOEXEC);
	exit_if(_timerfd == -1, "timerfd_create()");
}

timer::queue::timer_queue() {
	::close(_timerfd);
}

int timer_queue::add_tiemr(timer_event& te) {
	te.timer_id = _next_tiemr_id++;
	heap_add(te);
	if(_event_lst[0].ts < _pioneer) {  
		_pioneer = event_lst[0].ts;
		_rest_timo();
	}
	return te.timer_id;
}

void timer_queue::del_timer(int timer_id) {
	mit it = _position.find(timer_id);
	if(it == _position.end()) {
		error_log("no such a timerid %d", timer_id);
        return ;
	}
	int pos = it->second;
	heap_del(pos);
	
	if(_count == 0) {
		_pioneer = -1;
		reset_timo();
	}else if(_event_lst[0].ts < _pioneer) {
		_pioneer = _event_lst[0].ts;
		reset_timo();
	}
}

//时间事件对应的回调函数是timerqueue_cb，在回调函数中，会调用下述函数，将timer_event提取出来并执行相应的函数
void timer_queue::get_timo(std::vector<timer_event>& fired_evs) {
	std::vector<timer_event> _reuse_lst;
	//一个timerfd描述符 每次只能有一个事件发生。这里之所以使用循环是因为用户注册了时间相同的事件，这里可以回收到
	while(_count != 0 && _pioneer == _event_lst[0].ts) {
		timer_event te = _event_lst[0];
		fired_evs.push_back(te);
		if(te.interval) {
			te.ts += te.interval;
			_reuse_lst.push_back(te);
		}
		heap_pop();
	}
	for(vit it = _reuse_lst.begin(); it != _reuse_lst.end(); ++it) {
		add_timer(*it);
	}
	if(_count == 0) {
		_pioneer = -1;
	}else {
		_pioneer = _event_lst[0].ts;
	}
	reset_timo();
}
void timer_queue::rest_timo() {
	struct itimerspec old_ts, new_ts;
	::bzero(&new_ts, sizeof(new_ts));
	::bzero(&old_ts, sizeof(old_ts));
	if(_pioneer != (uint64_t)-1) {
		new_ts.it_value.tv_sec = _pioneer / 1000;
		new_ts.it_value.tv.nsec = (_pioneer %1000) * 1000000;
	}
	 //when _pioneer = -1, new_ts = 0 will disarms the timer
    ::timerfd_settime(_timerfd, TFD_TIMER_ABSTIME, &new_ts, &old_ts);
}

//当前的数组是最小堆，所以只需要维护一下即可。
void timer_queue::heap_add(timer_event& te) {
	_event_lst.push_back(te);
	_position[te.timer_id] = _count;
	
	int curr_pos = _count++;
	
	int prt_pos = (curr_pos -1) / 2;
	while(prt_pos >= 0&& _event_lst[curr_pos].ts < _event_lst[prt_pos].ts) {
		timer_event tmp = _event_lst[curr_pos];    //涉及到结构体的拷贝的时候 一定要多考虑一下 是浅拷贝还是深拷贝
		_event_lst[curr_pos] = _event_lst[prt_pos];
		_event_lst[prt_pos] = tmp; //换完后向下不用维护了，因为原先的父节点肯定比当前节点所在的子树小，但是向上需要维护。
		_position[_event_lst[curr_pos].timer_id] = curr_pos;
		_position[tmp.timer_id] = prt_pos;
		
		curr_pos = prt_pos;
		prt_pos = (curr_pos - 1) / 2; 
	}
}

void timer_queue::heap_del(int pos) {
	timer_event to_del = _event_lst[pos];
	//update position
	_postion.erase(to_del.timer_id);
	
	timer_event tmp = _event_lst[_count - 1];
	_event_lst[pos] = tmp;
	_position[tmp.timer_id] = pos;
	
	_count--;
	_event_lst.pop_back();
	
	heap_hold(pos);
}

void timer_queue::heap_pop() {
	if(_count <= 0) return;
	//update position
	_position.erase(_event_lst[0].timer_id);
	if(_count > 1) {
		timer_event tmp = _event_lst[_count - 1];
		_event_lst[0] = tmp;
		_position[tmp.timer_id] = 0;
		
		_event_lst.pop_back();
		_count--;
		heap_hold(0);
	}else if(_count == 1) {
		_event_lst.clear();
		_count = 0;
	}
}

//下面这个函数从上往下调整 可以采用尾递归的方式，或者不采用伪递归
void timer_queue::heap_hold(int pos) {
	int left = 2 * pos + 1, right = 2 * pos + 2;
	int min_pos = pos;
	if(left < _count && _event_lst[min_pos].ts > _event_lst[left].ts) {
		min = left;
	}
	if(right < _count && _event_lst[min_pos].ts > _event_lst[right].ts) {
		min = right;
	}
	if(min_pos != pos) {
		timer_event tmp = _event_lst[min_pos];
		_event_lst[min_pos] = _event_lst[pos];
		_event_lst[pos] = tmp;
		
		_position[_event_lst[min_pos].timer_id] = min_pos;
		_position[tmp.tmier_id] = pos;
		
		heap_hold(min_pos);
	}
}