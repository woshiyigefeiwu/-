#ifndef CO_EVENT_H
#define CO_EVENT_H

/*
    这里是一个 事件驱动 头文件，包含三个结构体：
    
    1. 事件 co_event
        将IO事件（由fd标识），封装成事件

    2. 定时器 co_time
        用于封装定时事件，处理sleep();
    
    3. 事件源 co_event_base
        其实就是个事件监听器，底层用epoll循环检测;

    因为epoll不能检测超时事件，因此这里还用了一个时间堆；
    然后 hook 了 epoll_wait , 调用epoll_wait时先检测时间堆
*/

#include <time.h>
#include <iostream>
#include <map>
#include <queue>
#include <unistd.h>
#include <sys/epoll.h>
using namespace std;

typedef void (*cb_ptr)(void *arg);

#define MAX_EVENT_SIZE 1024     //最大监听的文件描述符

typedef struct co_event //事件体
{
    int fd;             //文件描述符
    cb_ptr handler;     //事件对应的回调函数
    void *arg;          //回调函数的参数（调度器 + 协程号）

    co_event(): handler(nullptr), arg(nullptr) {}
}co_event;

typedef struct co_time  //定时器
{
    time_t timeout;         //超时时间
    cb_ptr handler;     //事件对应的回调函数
    void *arg;          //回调函数的参数（调度器 + 协程号）

    co_time(): handler(nullptr), arg(nullptr) {}
}co_time;

typedef struct co_event_base    //事件源（事件监听器）
{
    class cmp {     //自定义排序，让超时时间小的放前面
    public:
        bool operator()(const co_time *p1, const co_time *p2) {
            return p1->timeout > p2->timeout;
        }
    };

    int epoll_fd;   //epoll实例
    epoll_event events[MAX_EVENT_SIZE]; // 结构体数组，接收检测后的数据

    map<int,co_event*> fd_to_event;     // fd 到 事件体 的映射
    priority_queue<co_time*,vector<co_time*>,cmp> time_heap; //时间堆

    co_event_base()
    {
        epoll_fd = epoll_create(5); //创建一个epoll实例
    }

    ~co_event_base()
    {
        close(epoll_fd);
    }
}co_event_base;

//下面是一些函数
//创建一个事件
co_event* creat_event(int fd,cb_ptr handler,void *arg);  

//创建一个超时事件（定时器）
co_time* creat_time_event(time_t timeout,cb_ptr handler,void *arg);  

//将事件加入事件源
void add_event(co_event *event,co_event_base *base,EPOLL_EVENTS ty);     

#endif