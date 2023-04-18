#ifndef MY_UTHREAD_H
#define MY_UTHREAD_H

#ifdef __APPLE__
#define _XOPEN_SOURCE
#endif 

#include <ucontext.h>
#include <vector>

#define DEFAULT_STACK_SZIE (1024*128)   //每个协程独立栈空间的大小
#define MAX_UTHREAD_SIZE   1024     //最大的协程个数

//协程的状态：不活跃（死了），就绪态，运行态，挂起态
enum ThreadState{FREE,RUNNABLE,RUNNING,SUSPEND};

struct schedule_t;

typedef void (*Fun)(void *arg); //函数指针

typedef struct uthread_t    //协程结构体
{
    ucontext_t ctx;     //当前协程的上下文环境
    Fun func;       //当前协程要执行的函数
    void *arg;      //func的参数
    enum ThreadState state; //表示当前协程的状态
    char stack[DEFAULT_STACK_SZIE]; //每个协程的独立的栈
}uthread_t;

typedef struct schedule_t   //调度器的结构体
{
    ucontext_t main;    //主协程的上下文，方便切回主协程
    int running_thread; //当前正在运行的协程编号
    uthread_t *threads; //协程队列
    int max_index; // 曾经使用到的最大的index + 1

    schedule_t():running_thread(-1), max_index(0) //构造函数初始化调度器
    {
        threads = new uthread_t[MAX_UTHREAD_SIZE];

        for (int i = 0; i < MAX_UTHREAD_SIZE; i++) 
            threads[i].state = FREE;    //一开始都没有协程
    }
    
    ~schedule_t()   //回收协程
    {
        delete [] threads;
    }
}schedule_t;

// 协程执行的入口函数
static void uthread_body(schedule_t *ps);

// 创建一个协程，func为其执行的函数，arg为func的执行函数，返沪编号
int  uthread_create(schedule_t &schedule,Fun func,void *arg);

//  挂起调度器schedule中当前正在执行的协程，切换到主协程。
void uthread_yield(schedule_t &schedule);

// 唤醒运行调度器schedule中编号为id的协程
void uthread_resume(schedule_t &schedule,int id);

// 判断schedule中所有的协程是否都执行完毕，是返回1，否则返回0
int schedule_finished(const schedule_t &schedule);

#endif
