#ifndef CO_HOOK_CPP
#define CO_HOOK_CPP

#include <sys/socket.h>
#include <sys/types.h>
#include <assert.h>
#include <unistd.h>
#include <dlfcn.h>
#include <fcntl.h>
#include <string.h>
#include <signal.h>
#include <iostream>
#include "coroutine.h"
using namespace std;

#define MAX_TIMER_CHECK 5   //一次性检测超时事件的最大数量

//声明 hook 的指针
typedef unsigned int (*sleep_fun_ptr)(unsigned int seconds);
typedef int (*accept_fun_ptr)(int __fd, sockaddr *__restrict__ __addr, socklen_t *__restrict__ __addr_len);
typedef int (*connect_fun_ptr)(int __fd, const sockaddr *__addr, socklen_t __len);
typedef ssize_t (*read_fun_ptr)(int __fd, void *__buf, size_t __nbytes);
typedef ssize_t (*write_fun_ptr)(int __fd, const void *__buf, size_t __n);
typedef int (*epoll_wait_fun_ptr)(int __epfd, struct epoll_event *__events, int __maxevents, int __timeout);

//给这个原系统调用指针赋值为动态查询符号
sleep_fun_ptr g_sys_sleep_fun = (sleep_fun_ptr)dlsym(RTLD_NEXT, "sleep");
accept_fun_ptr g_sys_accept_fun = (accept_fun_ptr)dlsym(RTLD_NEXT, "accept");
connect_fun_ptr g_sys_connect_fun = (connect_fun_ptr)dlsym(RTLD_NEXT, "connect");
read_fun_ptr g_sys_read_fun = (read_fun_ptr)dlsym(RTLD_NEXT, "read");
write_fun_ptr g_sys_write_fun = (write_fun_ptr)dlsym(RTLD_NEXT, "write");
epoll_wait_fun_ptr g_sys_epoll_wait_fun = (epoll_wait_fun_ptr)dlsym(RTLD_NEXT, "epoll_wait");

//兼容C调用
extern "C"
{
    unsigned int sleep(unsigned int seconds);
    int accept(int __fd, sockaddr *__restrict__ __addr, socklen_t *__restrict__ __addr_len);
    int connect(int __fd, const sockaddr *__addr, socklen_t __len);
    ssize_t read(int __fd, void *__buf, size_t __nbytes);
    ssize_t write(int __fd, const void *__buf, size_t __n);
    int epoll_wait(int __epfd, struct epoll_event *__events, int __maxevents, int __timeout);
}

// vvvvvv 下面是对超时事件的处理
static int pipefd[2];       //这里有一对管道，用于处理超时事件
static int is_pipefd = 0;   //判断管道是否被创建了

// 向管道写数据的信号捕捉回调函数
void sig_to_pipe(int sig)
{
    int save_errno = errno;
    int msg = sig;
    send( pipefd[1], ( char* )&msg, 1, 0 );     //这个不hook
    errno = save_errno;
}

// 信号处理，添加信号捕捉
void addsig(int sig, void(handler)(int)){       
    struct sigaction sigact;                    // sig 指定信号， void handler(int) 为处理函数
    memset(&sigact, '\0', sizeof(sigact));      // bezero 清空
    sigact.sa_flags = 0;                        // 调用sa_handler
    // sigact.sa_flags |= SA_RESTART;           // 指定收到某个信号时是否可以自动恢复函数执行，不需要中断后自己判断EINTR错误信号
    sigact.sa_handler = handler;                // 指定回调函数
    sigfillset(&sigact.sa_mask);                // 将临时阻塞信号集中的所有的标志位置为1，即都阻塞
    sigaction(sig, &sigact, NULL);              // 设置信号捕捉sig信号值
}
// ^^^^^^ 到这里结束

//回调函数，只有一个任务，就是将协程唤醒
void handle_callback(void *arg)    
{
    schedule_and_cid* sac = static_cast<schedule_and_cid*>(arg);
    schedule_t* schedule = sac->schedule;           //调度器
    struct co_event_base *base = schedule->base;    //事件监听器
    int cid = sac->cid;                             //协程编号

    co_resume((*schedule),cid);    //唤醒协程
}

//下面实现 hook 后的具体函数
unsigned int sleep(unsigned int seconds)
{
    int pid = pthread_self();   //获取线程号
    schedule_t* schedule = pid_to_schedule[pid];        //获取调度器

    if(!schedule || schedule->running_thread == -1)     //如果是根本没由创建调度器 或者 是主线程，则调用非hook的函数
    {
        return g_sys_sleep_fun(seconds);
    }

    struct co_event_base *base = schedule->base;        //注意这里逻辑，要先判断调度器是否为空，再获取事件监听器

    //创建 超时任务 超时事件 将任务加入事件 将事件加入事件监听器
    schedule_and_cid sac(schedule,schedule->running_thread);
    time_t ti = time(NULL) + seconds;
    co_time *timeout = creat_time_event(ti, handle_callback, &sac);   //创建一个超时事件
    base->time_heap.push(timeout);      //将超时任务加入定时器

    //这里需要特别注意！！！
    if(!is_pipefd)  //如果管道还没被创建，那么得先创建管道，并创建事件加入到epoll！
    {
        is_pipefd = 1;
        int ret = socketpair(PF_UNIX, SOCK_STREAM, 0, pipefd);
        assert( ret != -1 );

        epoll_event event;
        event.data.fd = pipefd[0];
        event.events = EPOLLIN;
        epoll_ctl(base->epoll_fd, EPOLL_CTL_ADD, pipefd[0], &event); //加入epoll
    }

    //如果时间堆为空 或者 当前超时事件 <= 时间堆的最小超时时间，则我们需要重新设置闹钟
    if(base->time_heap.size() == 0 ||  ti <= base->time_heap.top()->timeout) alarm(seconds);

    //将协程挂起
    co_yield(*schedule);

    return 0;
}

int accept(int __fd, sockaddr *__restrict__ __addr, socklen_t *__restrict__ __addr_len)
{
    int pid = pthread_self();                       //获取线程号
    schedule_t* schedule = pid_to_schedule[pid];    //获取调度器

    if(!schedule || schedule->running_thread == -1) //如果是根本没由创建调度器 或者 是主线程，则调用非hook的函数
    {
        return g_sys_accept_fun(__fd,__addr,__addr_len);
    }

    struct co_event_base *base = schedule->base;    //注意这里逻辑，要先判断调度器是否为空，获取事件监听器

    //把accept事件注册到事件监听器
    schedule_and_cid sac(schedule,schedule->running_thread);    //封装一下
    struct co_event *event = creat_event(__fd,handle_callback,&sac);
    add_event(event,base,EPOLLIN);  //检测EPOLLIN

    //将协程挂
    co_yield(*schedule);

    return g_sys_accept_fun(__fd,__addr,__addr_len);
}

int connect(int __fd, const sockaddr *__addr, socklen_t __len)
{
    int pid = pthread_self();                       //获取线程号
    schedule_t* schedule = pid_to_schedule[pid];    //获取调度器

    if(!schedule || schedule->running_thread == -1) //如果是根本没由创建调度器 或者 是主线程，则调用非hook的函数
    {
        return g_sys_connect_fun(__fd,__addr,__len);
    }

    struct co_event_base *base = schedule->base;    //注意这里逻辑，要先判断调度器是否为空，获取事件监听器

    //把connect事件注册到事件监听器
    schedule_and_cid sac(schedule,schedule->running_thread);    //封装一下
    struct co_event *event = creat_event(__fd,handle_callback,&sac);
    add_event(event,base,EPOLLIN);  //检测EPOLLIN

    //将协程挂
    co_yield(*schedule);

    return g_sys_connect_fun(__fd,__addr,__len);
}

ssize_t read(int __fd, void *__buf, size_t __nbytes)
{
    int pid = pthread_self();                       //获取线程号
    schedule_t* schedule = pid_to_schedule[pid];    //获取调度器

    if(!schedule || schedule->running_thread == -1) //如果是根本没由创建调度器 或者 是主线程，则调用非hook的函数
    {
        return g_sys_read_fun(__fd,__buf,__nbytes);
    }

    struct co_event_base *base = schedule->base;    //注意这里逻辑，要先判断调度器是否为空，获取事件监听器

    //把读事件注册到事件监听器
    schedule_and_cid sac(schedule,schedule->running_thread);    //封装一下
    struct co_event *event = creat_event(__fd,handle_callback,&sac);
    add_event(event,base,EPOLLIN);  //检测EPOLLIN

    //将协程挂
    co_yield(*schedule);

    return g_sys_read_fun(__fd,__buf,__nbytes);
}

ssize_t write(int __fd, const void *__buf, size_t __n)
{
    int pid = pthread_self();                       //获取线程号
    schedule_t* schedule = pid_to_schedule[pid];    //获取调度器

    if(!schedule || schedule->running_thread == -1) //如果是根本没由创建调度器 或者 是主线程，则调用非hook的函数
    {
        return g_sys_write_fun(__fd,__buf,__n);
    }

    struct co_event_base *base = schedule->base;    //注意这里逻辑，要先判断调度器是否为空，获取事件监听器

    //把读事件注册到事件监听器
    schedule_and_cid sac(schedule,schedule->running_thread);    //封装一下
    struct co_event *event = creat_event(__fd,handle_callback,&sac);
    add_event(event,base,EPOLLOUT);  //检测EPOLLOUT

    //将协程挂
    co_yield(*schedule);

    return g_sys_write_fun(__fd,__buf,__n);
}

void event_loop(struct schedule_t *ptr_schedule_t)      //启动事件循环
{
    struct co_event_base *base = ptr_schedule_t->base;  //事件源

    if(!is_pipefd)  //如果管道还没被创建，那么得先创建管道，并将管道加入epoll检测
    {
        is_pipefd = 1;
        int ret = socketpair(PF_UNIX, SOCK_STREAM, 0, pipefd);
        assert( ret != -1 );

        epoll_event event;          //这里也是一样的
        event.data.fd = pipefd[0];
        event.events = EPOLLIN;
        epoll_ctl(base->epoll_fd, EPOLL_CTL_ADD, pipefd[0], &event); //加入epoll
    }

    addsig(SIGALRM, sig_to_pipe);   // 设置定时器信号

    while(1)    //底层循环调用epoll_wait检测事件（事件循环）
    {   
        int num = epoll_wait(base->epoll_fd, base->events, MAX_EVENT_SIZE, -1);     // 阻塞，返回事件数量

        // 循环遍历事件数组
        for(int i = 0; i < num; ++i)
        {
            int cfd = base->events[i].data.fd;

            if(cfd == pipefd[0] && (base->events[i].events & EPOLLIN))    // 有定时事件发生
            {
                int idx = 0;
                while(base->time_heap.size() && time(NULL) >= base->time_heap.top()->timeout)  //超时了，注意这里要=，只能精确到秒...
                {
                    if(idx > MAX_TIMER_CHECK) break;  //处理超过限定值则不再处理，避免出现不处理IO事件的情况
                    
                    co_time *timeout = base->time_heap.top();   //超时事件
                    base->time_heap.pop();  //弹出

                    timeout->handler(timeout->arg);     //调用回调函数
                }

                //因为默认是LT模式，所以触发后我们要把数据读出来，不然会一直触发...调了好久...
                char signals[1024];
                int ret = recv(pipefd[0], signals, sizeof(signals), 0);

                //这里处理完之后，如果时间堆里面还有超时任务，那么我们需要重新设置闹钟，这样后面的超时任务才能触发！
                if(base->time_heap.size())
                {
                    alarm(base->time_heap.top()->timeout - time(NULL));     //时间堆顶的时间 - 当前时间 = 最快又能触发时间（注意别减错了...）
                }
            }
            else
            {
                //删除文件描述符
                epoll_ctl(base->epoll_fd, EPOLL_CTL_DEL, cfd, NULL); 

                //执行cfd对应事件的回调函数
                co_event *event = base->fd_to_event[cfd];
                event->handler(event->arg);

                /*
                    这里的逻辑需要特别注意...
                    要先删除文件描述符，在执行相应事件的回调函数！！！
                    因为你先执行回调的时候，切回协程，那么协程里面可能又会有系统调用，
                    此时又会将fd重新加入epoll，然后直到回调流程全部结束（或者阻塞了）才会yield回来，
                    此时再删除文件描述符，那么此时epoll里面就没有这个fd了！所以会一直卡在这里...

                    这个bug找了好几个小时呜呜呜，找到半夜一点...
                */
            }
        }
    }
}



#endif