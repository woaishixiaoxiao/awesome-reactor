# awesome-reactor
create a simple reactor framework for improving skills in network programing。thanks to https://github.com/LeechanX/Easy-Reactor.git
# 类关系图
![image](https://github.com/woaishixiaoxiao/awesome-reactor/blob/master/reator%E7%B1%BB%E5%9B%BE%E5%88%86%E6%9E%90.png)
# 总流程图
![image](https://github.com/woaishixiaoxiao/awesome-reactor/blob/master/easy-reactor%E6%B5%81%E7%A8%8B%E5%9B%BE%E5%88%86%E6%9E%90.png)
# 总体架构解析
1. 支持使用单线程以及多线程的模式。单线程中所有的io以及计算均在一个线程中完成。
2. 使用多线程的模式，每个线程绑定一个event_loop
2.1 主线程监听监听套接字，子线程监听已连接套接字，有客户到来的时候，主线程采用round-robin方法，使得客户均衡分布到各个子线程中去。
2.2 子线程中既有IO读取，也有业务处理。
# day1 
## 编写event_loop类
event_loop类是框架中最重要的类，提供注册/删除事件回调函数的机制。另外还存在一个队列，保存客户端发来的待执行任务。在完成一次epoll处理后，执行客户需要执行的待执行任务。

