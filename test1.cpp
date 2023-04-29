#include <iostream>
#include "coroutine.h"
using namespace std;

void fun3(void *arg)
{
    schedule_t *schedule = static_cast<schedule_t *>(arg);

    cout<<"co3: 1"<<"  "<<schedule->running_thread<<"==="<<schedule->coroutine_pool[schedule->running_thread].fa_co->id<<"===\n";
    
    co_yield(*schedule);

    cout<<"co3: 2"<<"  "<<schedule->running_thread<<"==="<<schedule->coroutine_pool[schedule->running_thread].fa_co->id<<"===\n";
}

void fun2(void *arg)
{
    schedule_t *schedule = static_cast<schedule_t *>(arg);

    cout<<"co2: 1"<<"  "<<schedule->running_thread<<"==="<<schedule->coroutine_pool[schedule->running_thread].fa_co->id<<"===\n";

    int co3 = co_create(*schedule,fun3,schedule);
    co_resume(*schedule,co3);

    cout<<"co2: 2"<<"  "<<schedule->running_thread<<"==="<<schedule->coroutine_pool[schedule->running_thread].fa_co->id<<"===\n";

    co_resume(*schedule,co3);

    cout<<"co2: 3"<<"  "<<schedule->running_thread<<"==="<<schedule->coroutine_pool[schedule->running_thread].fa_co->id<<"===\n";

    co_yield(*schedule);

    cout<<"co2: 4"<<"  "<<schedule->running_thread<<"==="<<schedule->coroutine_pool[schedule->running_thread].fa_co->id<<"===\n";

}

void fun1(void *arg)
{
    schedule_t *schedule = static_cast<schedule_t *>(arg);

    cout<<"co1: 1"<<"  "<<schedule->running_thread<<"==="<<schedule->coroutine_pool[0].fa_co->id<<"===\n";

    int co2 = co_create(*schedule,fun2,schedule);
    co_resume(*schedule,co2);

    cout<<"co1: 2"<<"  "<<schedule->running_thread<<"==="<<schedule->coroutine_pool[schedule->running_thread].fa_co->id<<"===\n";

    co_resume(*schedule,co2);

    cout<<"co1: 3"<<"  "<<schedule->running_thread<<"==="<<schedule->coroutine_pool[schedule->running_thread].fa_co->id<<"===\n";
}

void test1()    //测试一下循环嵌套调用协程的时候的正确性
{
    schedule_t schedule;
    
    cout<<schedule.running_thread<<"---\n";
    cout<<schedule.coroutine_pool[0].fa_co->id<<"+++\n";

    int co1 = co_create(schedule,fun1,&schedule);

    co_resume(schedule,co1);
}