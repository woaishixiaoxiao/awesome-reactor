#ifndef __MSG_HEAD_H__
#define __MSG_HEAD_H__

class event_loop;//前向声明，后面只能用引用的格式

struct commu_head {
	int cmdid;
	int length;
};
//采取多线程模式的时候，主线程接受客户的连接或是执行任务的请求，并将此任务用下面的该结构体表示。
//每个子进程均会绑定一个event_loop，
//子线程中会监听一个fd，该fd用于和主进程通信。主进程中收到新的请求的时候，就会向此向此fd写东西，从而使其可读。子进程中注册的回调函数会判断此消息类型，如果是客户连接，则加入到loop监听中去，如果是任务，则加入到pendingFac中去。
//子进程初始化的时候，需要将上述fd加入到监听队列中去，并绑定对应的回调函数。
//子线程需要维护一个队列，存该结构体，主进程往该队列中写内容。
struct queue_msg {
	enum MSG_TYPE {
		NEW_TASK.
		STOP_THD,
		NEW_TASK
	};
	MSG_TYPE cmd_type;
	union {
		int connfd;
		/*结构体应该声明后使用，下面这个是否存在问题？*/
		struct {
			void (*task)(event_loop*, void *);
			void *args;
		};
	};
};

#define COMMU_HEAD_LENGTH 8
#define MSG_LENGTH_LIMIT ( 65535 - COMMU_HEAD_LENGTH )

#endif