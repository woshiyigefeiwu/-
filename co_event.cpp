#include "co_event.h"

//下面是一些函数
co_event* creat_event(int fd,cb_ptr handler,void *arg)  //创建一个事件
{
    co_event *event = new co_event;

    event->fd = fd;
    event->handler = handler;
    event->arg = arg;

    return event;
}

co_time* creat_time_event(time_t timeout,cb_ptr handler,void *arg)  //创建一个超时事件（定时器）
{
    co_time* time_event = new co_time;

    time_event->timeout = timeout;
    time_event->handler = handler;
    time_event->arg = arg;

    return time_event;
}

void add_event(co_event *event,co_event_base *base,EPOLL_EVENTS ty)     //将事件加入事件源
{
    //做映射
    base->fd_to_event[event->fd] = event;

    //将fd加入epoll中
    epoll_event ev;
    ev.data.fd = event->fd;
    ev.events = ty;     //检测相应的类型

    epoll_ctl(base->epoll_fd, EPOLL_CTL_ADD, event->fd, &ev); //加入epoll
}