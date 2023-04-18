#include "uthread.h"
#include <stdio.h>
#include <iostream>
#include <unistd.h>
using namespace std;

void func1(void * arg)  //协程1函数
{
    for(int i=1;i<=5;i++)
    {
        cout<<"协程 1 正在运行...\n";
        sleep(1);
        uthread_yield(*(schedule_t *)arg);  //将此协程挂起
    }
}

void func2(void *arg)   //协程2函数
{
    for(int i=1;i<=5;i++)
    {
        cout<<"协程 2 正在运行...\n";
        sleep(1);
        uthread_yield(*(schedule_t *)arg);  //将此协程挂起
    }
}

void schedule_test()
{
    schedule_t s;
    
    int id1 = uthread_create(s,func1,&s);   //创建协程1
    int id2 = uthread_create(s,func2,&s);   //创建协程2
    
    // puts("start\n");

    while(!schedule_finished(s))    //判断调度器中是否有活跃协程
    {
        uthread_resume(s,id1);      //唤醒协程id1
        cout<<"主协程...\n";
        uthread_resume(s,id2);      //唤醒协程id2
        cout<<"主协程...\n";
    }
    puts("main over");
}

int main()
{
    puts("start test：-----------------\n");
    schedule_test();    //开始测试

    return 0;
}
