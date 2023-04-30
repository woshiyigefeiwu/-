#ifndef COROUTIME_H     //条件编译一下，避免多个.cpp文件包含相同.h文件时报重定义错误
#define COROUTIME_H

#include <ucontext.h>
#include <vector>
#include <sys/epoll.h>
#include <iostream>
#include <map>
#include <event.h>
using std::map;

#define DEFAULT_STACK_SZIE (1024*128)   //每个协程独立栈空间的大小
#define MAX_UTHREAD_SIZE   1024     //最大的协程个数
#define maxn_fd_size 1024

struct schedule_t;  //声明

//这里是一个全局的pid -> 调度器的映射关系
static map<int,schedule_t*> pid_to_schedule;    //线程对应的调度器

//协程的状态：不活跃（死了），就绪态，运行态，挂起态
enum CoroutineState{FREE,RUNNABLE,RUNNING,SUSPEND};

typedef void (*Fun)(void *arg); //函数指针

typedef struct coroutine    //协程结构体
{
    ucontext_t ctx;     //当前协程的上下文环境
    Fun func;           //当前协程要执行的函数
    void *arg;          //func的参数
    enum CoroutineState state;      //表示当前协程的状态
    char stack[DEFAULT_STACK_SZIE]; //每个协程的独立的栈

    int id; //当前协程的编号
    coroutine *fa_co;   //调用当前协程的父协程体

}coroutine;

typedef struct schedule_t   //调度器的结构体
{
    ucontext_t main;            //主协程的上下文，方便切回主协程
    int running_thread;         //当前正在运行的协程编号
    coroutine *coroutine_pool;  //协程队列(协程池)
    int max_index;              // 曾经使用到的最大的index + 1

    //封装一个事件监听器
    struct event_base *base;

    //下面是构造函数 和 析构函数
    schedule_t():running_thread(-1), max_index(0) //构造函数初始化调度器
    {
        coroutine_pool = new coroutine[MAX_UTHREAD_SIZE];

        for (int i = 0; i < MAX_UTHREAD_SIZE; i++)  //初始化协程池
        {
            coroutine_pool[i].state = FREE;         //一开始都没有协程
            coroutine_pool[i].id = i;
            coroutine_pool[i].fa_co = new coroutine;    //注意这里不能设置为空指针，因为下面要用
            coroutine_pool[i].fa_co->id = -1;           //表示主协程的环境
        }

        //初始化pid_to_schedule的映射关系
        pid_to_schedule[pthread_self()] = this; //当前线程id -> 调度器地址

        //创建一个事件监听器
        base = event_base_new();
    }
    
    ~schedule_t()   
    {
        delete [] coroutine_pool;   //回收协程
        event_base_free(base);      //销毁监听器
    }
}schedule_t;

/*
    由于回调函数中，我们需要把协程唤醒；
    所以我们给回调函数传参的时候需要传调度器和协程编号进去；
    因此这里封装一个结构体.
*/
typedef struct schedule_and_cid
{
    schedule_t *schedule;   //调度器
    int cid;                //协程编号

    schedule_and_cid(): schedule(nullptr), cid(-1) {}
    schedule_and_cid(schedule_t *sc,int id): schedule(sc), cid(id) {}
}schedule_and_cid;

// 协程执行的入口函数
static void co_body(schedule_t *ps);

// 创建一个协程，func为其执行的函数，arg为func的执行函数，返沪编号
int co_create(schedule_t &schedule,Fun func,void *arg);

//  挂起调度器schedule中当前正在执行的协程，切换到主协程。
void co_yield(schedule_t &schedule);

// 唤醒运行调度器schedule中编号为id的协程
void co_resume(schedule_t &schedule,int id);

// 判断schedule中所有的协程是否都执行完毕，是返回1，否则返回0
int schedule_finished(const schedule_t &schedule);

//启动事件循环
void event_loop(struct schedule_t *ptr_schedule_t);  
#endif
