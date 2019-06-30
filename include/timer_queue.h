#ifndef __TIMER_QUEUE_H__
#define __TIMER_QUEUE_H__

#include <stdint.h>
#include <vector>
#include <ext/hash_map>
#include "event_base.h"


/***
1、这个类里面含有_timerfd以及数据结构为timer_event的最小堆。timer_event中包含了时间发生的时候希望执行的函数。
2、该类被event_loop类所包含。event_loop类中提供接口注册的时间事件。
3、用户可以一次注册多个timer_event，timer_queue会用最小堆维护。
4、每次都是以堆顶的最小时间作为事件项。如果该事件发生了，就执行用户注册的动作。然后进行最小堆的调整。这里注意的是如果本次发生的事件存在时间间隔，那么该时间事件还要加入到堆中，并进行调整。
5、时间间隔并没有写到数据结构中去，因为第4点知道了要进行调整的。
***/

class timer_queue {
	public:
		timer_queue();
		~timer_queue();
		
		int add_timer(timer_event& te);
		void del_timer(int timer_id);
		
		int notifier() const { return _timerfd; }
		int size() const  { return _count; }
		void get_timo(std::vector<timer_event>& fired_evs);
	private:
		void reset_timo();
		
		void heap_add(timer_event& te);
		void heap_del(int pos);
		void heap_pop();
		void heap_hold(int pos);
		
		std::vector<timer_event> _event_lst;
		typedef std::vector<timer_event>::iterator vit;
		
		__gnu_cxx::hash_map<int, int> _position;
		typedef __gnu_cxx::hash_map<int, int>::iterator mit;
		
		int _count;
		int _next_timer_id;
		int _timerfd;
		uint64_t _pioneer;
};
#endif 