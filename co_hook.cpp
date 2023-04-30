#include <sys/socket.h>
#include <sys/types.h>
#include <assert.h>
#include <unistd.h>
#include <dlfcn.h>
#include <fcntl.h>
#include <string.h>
#include <iostream>
/*
    这个文件我们试一下hook，分为3个部分：

    1. 处理hook函数
    2. 实现各种事件的回调函数
    3. 实现具体的hook函数
*/

#include <event.h>
#include "coroutine.h"
using namespace std;

// #define HOOK_SYS_FUNC(name) name##_fun_ptr_t g_sys_##name##_fun = (name##_fun_ptr_t)dlsym(RTLD_NEXT, #name);
// HOOK_SYS_FUNC(sleep);

//声明 hook 的指针
typedef unsigned int (*sleep_fun_ptr)(unsigned int seconds);
typedef int (*accept_fun_ptr)(int __fd, sockaddr *__restrict__ __addr, socklen_t *__restrict__ __addr_len);
typedef int (*connect_fun_ptr)(int __fd, const sockaddr *__addr, socklen_t __len);
typedef ssize_t (*read_fun_ptr)(int __fd, void *__buf, size_t __nbytes);
typedef ssize_t (*write_fun_ptr)(int __fd, const void *__buf, size_t __n);

//给这个原系统调用指针赋值为动态查询符号
sleep_fun_ptr g_sys_sleep_fun = (sleep_fun_ptr)dlsym(RTLD_NEXT, "sleep");
accept_fun_ptr g_sys_accept_fun = (accept_fun_ptr)dlsym(RTLD_NEXT, "accept");
connect_fun_ptr g_sys_connect_fun = (connect_fun_ptr)dlsym(RTLD_NEXT, "connect");
read_fun_ptr g_sys_read_fun = (read_fun_ptr)dlsym(RTLD_NEXT, "read");
write_fun_ptr g_sys_write_fun = (write_fun_ptr)dlsym(RTLD_NEXT, "write");

//兼容C调用
extern "C"
{
    unsigned int sleep(unsigned int seconds);
    int accept(int __fd, sockaddr *__restrict__ __addr, socklen_t *__restrict__ __addr_len);
    int connect(int __fd, const sockaddr *__addr, socklen_t __len);
    ssize_t read(int __fd, void *__buf, size_t __nbytes);
    ssize_t write(int __fd, const void *__buf, size_t __n);
}

//这里是各种事件的回调函数
void handle_callback(evutil_socket_t fd, short event, void *arg)    //回调函数
{
    schedule_and_cid* sac = static_cast<schedule_and_cid*>(arg);
    schedule_t* schedule = sac->schedule;       //调度器
    struct event_base *base = schedule->base;   //事件监听器
    int cid = sac->cid;                         //协程编号

    //将协程唤醒
    co_resume((*schedule),cid);    //唤醒协程
}

//下面实现 hook 后的具体函数
unsigned int sleep(unsigned int seconds) 
{
    int pid = pthread_self();                       //获取线程号
    schedule_t* schedule = pid_to_schedule[pid];    //获取调度器
    struct event_base *base = schedule->base;       //获取事件监听器

    if(!schedule || schedule->running_thread == -1) //如果是根本没由创建调度器 或者 是主线程，则调用非hook的函数
    {
        return g_sys_sleep_fun(seconds);
    }

    //创建 超时任务 超时事件 将任务加入事件 将事件加入事件监听器
    schedule_and_cid sac(schedule,schedule->running_thread);
    timeval timeout{seconds,0};                                             //设置超时任务(第一个参数是s，第二个参数是ms)
    struct event *timeout_event = evtimer_new(base,handle_callback,&sac);   //设置超时事件
    evtimer_add(timeout_event,&timeout);                                    //将超时任务加入到超时事件中

    //将协程挂起
    co_yield(*schedule);

    return 0;
}

int accept(int __fd, sockaddr *__restrict__ __addr, socklen_t *__restrict__ __addr_len)
{
    int pid = pthread_self();                       //获取线程号
    schedule_t* schedule = pid_to_schedule[pid];    //获取调度器
    struct event_base *base = schedule->base;       //获取事件监听器

    if(!schedule || schedule->running_thread == -1) //如果是根本没由创建调度器 或者 是主线程，则调用非hook的函数
    {
        return g_sys_accept_fun(__fd,__addr,__addr_len);
    }

    //把accept事件注册到事件监听器
    schedule_and_cid sac(schedule,schedule->running_thread);    //封装一下
    struct event *evlisten = event_new(base, __fd, EV_READ|EV_PERSIST, handle_callback, &sac); 
    event_add(evlisten,NULL);

    //将协程挂
    co_yield(*schedule);

    return g_sys_accept_fun(__fd,__addr,__addr_len);
}

int connect(int __fd, const sockaddr *__addr, socklen_t __len)
{
    int pid = pthread_self();                       //获取线程号
    schedule_t* schedule = pid_to_schedule[pid];    //获取调度器
    struct event_base *base = schedule->base;       //获取事件监听器

    if(!schedule || schedule->running_thread == -1) //如果是根本没由创建调度器 或者 是主线程，则调用非hook的函数
    {
        return g_sys_connect_fun(__fd,__addr,__len);
    }

    //把connect事件注册到事件监听器
    schedule_and_cid sac(schedule,schedule->running_thread);    //封装一下
    struct event *evlisten = event_new(base, __fd, EV_READ|EV_PERSIST, handle_callback, &sac); 
    event_add(evlisten,NULL);

    //将协程挂
    co_yield(*schedule);

    return g_sys_connect_fun(__fd,__addr,__len);
}

ssize_t read(int __fd, void *__buf, size_t __nbytes)
{
    int pid = pthread_self();                       //获取线程号
    schedule_t* schedule = pid_to_schedule[pid];    //获取调度器
    struct event_base *base = schedule->base;       //获取事件监听器

    if(!schedule || schedule->running_thread == -1) //如果是根本没由创建调度器 或者 是主线程，则调用非hook的函数
    {
        return g_sys_read_fun(__fd,__buf,__nbytes);
    }

    // cout<<"这里是 hook_read：开始!\n";

    //把读事件注册到事件监听器
    schedule_and_cid sac(schedule,schedule->running_thread);    //封装一下
    struct event *evread = event_new(base, __fd, EV_READ|EV_PERSIST, handle_callback, &sac); 
    event_add(evread,NULL);

    //将协程挂
    co_yield(*schedule);

    return g_sys_read_fun(__fd,__buf,__nbytes);
}

ssize_t write(int __fd, const void *__buf, size_t __n)
{
    int pid = pthread_self();                       //获取线程号
    schedule_t* schedule = pid_to_schedule[pid];    //获取调度器
    struct event_base *base = schedule->base;       //获取事件监听器

    if(!schedule || schedule->running_thread == -1) //如果是根本没由创建调度器 或者 是主线程，则调用非hook的函数
    {
        return g_sys_write_fun(__fd,__buf,__n);
    }

    //把读事件注册到事件监听器
    schedule_and_cid sac(schedule,schedule->running_thread);    //封装一下
    struct event *evread = event_new(base, __fd, EV_WRITE|EV_PERSIST, handle_callback, &sac); 
    event_add(evread,NULL);

    //将协程挂
    co_yield(*schedule);

    return g_sys_write_fun(__fd,__buf,__n);
}