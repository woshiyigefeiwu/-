#include <stdio.h>
#include <iostream>
#include <unistd.h>
#include <event.h>
#include "coroutine.h"
#include "co_hook.cpp"
using namespace std;

void fun_sleep2(void *arg)
{
    schedule_t *schedule = static_cast<schedule_t *>(arg);

    sleep(2);   //试一下hook后的sleep

    /*
        在这里可以发现，当co2调用sleep时；
        阻塞，然后通过回调函数唤醒协程时，这个协程由主协程接管了；
        也就是说co2的父协程是主协程；
        所以说没问题！
    */
}

void fun_sleep(void *arg)   //测试hook后的sleep
{
    schedule_t *schedule = static_cast<schedule_t *>(arg);

    cout<<schedule->running_thread<<"==="<<schedule->coroutine_pool[schedule->running_thread].fa_co->id<<"===\n";

    int co_sleep = co_create(*schedule,fun_sleep2,schedule);
    co_resume(*schedule,co_sleep);

    cout<<"fun_sleep 结束！\n";
}

int main()
{
    schedule_t schedule;

    cout<<schedule.running_thread<<"==="<<schedule.coroutine_pool[0].fa_co->id<<"===\n";

    int co_sleep = co_create(schedule,fun_sleep,&schedule);
    co_resume(schedule,co_sleep);

    event_loop(&schedule);  //事件循环
    return 0;
}

