#ifndef __EVENT_BASE_H__
#define __EVENT_BASE_H__

#include<stdint.h>
#include<stdio.h>
class event_loop;

typedef void io_callback(event_loop* loop, int fd, void *args);//IO事件回调函数
typedef void timer_callback(event_loop* loop, void* usr_data);//Timer事件回调函数

//让当前loop在一次poll循环后执行指定任务
typedef void (*pendingFunc)(event_loop*, void *);

struct io_event {
	io_event(): read_cb(NULL), write_cb(NULL), rcb_args(NULL), wcb_args(NULL) {}
	int mask;
	io_callback* read_cb;
	io_callback* write_cb;
	void* rcb_args;
	void* wcb_args;
};

struct timer_event {
	timer_event(timer_callback* time_cb, void* data, uint64_t arg_ts, uint32_t arg_interval = 0):
	cb(time_cb), cb_data(data), ts(arg_ts), interval(arg_interval)
	{
	}
	timer_callback* cb;
	void* cb_data;
	uint64_t ts;
	uint32_t interval;
	int timer_id;
};
#endif