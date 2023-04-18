#ifndef MY_UTHREAD_CPP
#define MY_UTHREAD_CPP

#include "uthread.h"
#include <stdio.h>
#include <iostream>
using namespace std;

// 从schedule中唤醒id号协程
void uthread_resume(schedule_t &schedule , int id)
{
    if(id < 0 || id >= schedule.max_index)  // 出错
    { 
        cout<<"要唤醒的协程号不合法！！！"<<endl;
        return;
    }

    // 当前协程
    uthread_t *t = &(schedule.threads[id]);
 
    switch(t->state)
    {
        // 如果当前是就绪态，也就是第一次，那么给这个协程设置上下文信息
        case RUNNABLE:  

            getcontext(&(t->ctx));  // 获取当前上下文到 t->ctx 中
            t->ctx.uc_stack.ss_sp = t->stack;   //指定栈空间
            t->ctx.uc_stack.ss_size = DEFAULT_STACK_SZIE;   // 指定栈空间大小
            t->ctx.uc_stack.ss_flags = 0;
            t->ctx.uc_link = &(schedule.main);  // 设置当前协程的后继上下文
            t->state = RUNNING;     // 将当前协程状态设置为运行态
 
            schedule.running_thread = id;   // 设置当前正在运行的协程id

            /*
                函数：void makecontext(ucontext_t *ucp, void (*func)(), int argc, ...)
                功能：
                    修改上下文信息，参数ucp就是我们要修改的上下文信息结构体；
                    func是上下文的入口函数；
                    argc是入口函数的参数个数，后面的…是具体的入口函数参数，该参数必须为整形值
            */  
            makecontext(&(t->ctx),(void (*)(void))(uthread_body),1,&schedule);
            
            /*
                函数：int swapcontext(ucontext_t *oucp, ucontext_t *ucp)
                功能：
                    将当前cpu中的上下文信息保存带oucp结构体变量中，然后将ucp中的结构体的上下文信息恢复到cpu中
                    这里可以理解为调用了两个函数，第一次是调用了getcontext(oucp)然后再调用setcontext(ucp)
            
                就是把当前上下文环境保存到oucp，然后把ucp的上下文环境放到cpu上执行。
            */
            swapcontext(&(schedule.main), &(t->ctx));

            break;

        // 如果是悬挂状态，则直接切换就行
        case SUSPEND:

            schedule.running_thread = id;
            t->state = RUNNING;
            swapcontext(&(schedule.main),&(t->ctx));
 
            break;

        default: ;
    }
}

// 协程切出，也就是挂起
void uthread_yield(schedule_t &schedule)
{
    if(schedule.running_thread != -1 )
    {
        uthread_t *t = &(schedule.threads[schedule.running_thread]);
        t->state = SUSPEND;
        schedule.running_thread = -1;

        // 保存上下文到 t->ctx，切换到主协程
        swapcontext(&(t->ctx),&(schedule.main));
    }
}

// 协程执行的入口函数
void uthread_body(schedule_t *ps)
{
    int id = ps->running_thread;

    if(id != -1)
    {
        uthread_t *t = &(ps->threads[id]);

        t->func(t->arg);    //执行函数，中间可能会被挂起

        t->state = FREE;
        ps->running_thread = -1;
    }
}

// 创建一个协程
int uthread_create(schedule_t &schedule,Fun func,void *arg)
{
    int id = 0;
    for(id = 0; id < schedule.max_index; ++id )
    {
        if(schedule.threads[id].state == FREE)  //找到一个位置
        {
            break;
        }
    }
    
    if (id == schedule.max_index) {
        schedule.max_index++;   // 最大id++
    }

    uthread_t *t = &(schedule.threads[id]); // 当前协程

    t->state = RUNNABLE;    // 创建完当前状态为就绪态
    t->func = func;     // 绑定函数
    t->arg = arg;   // 函数的参数

    return id;  // 返回协程的编号
}

// 判断schedule中是否有协程没运行完
int schedule_finished(const schedule_t &schedule)
{
    if (schedule.running_thread != -1)  // 当前没有协程
    {
        return 0;
    }
    else
    {
        for(int i = 0; i < schedule.max_index; ++i)
        {
            if(schedule.threads[i].state != FREE)
            {
                return 0;
            }
        }
    }

    return 1;
}

#endif
