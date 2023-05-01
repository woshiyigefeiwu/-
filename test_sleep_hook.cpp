#include <stdio.h>
#include <unistd.h>
#include "coroutine.h"
#include "co_hook.cpp"

void fun2(void *arg)
{
    cout<<"start fun2\n";

    schedule_t *schedule = static_cast<schedule_t *>(arg);

    sleep(2);

    cout<<"end fun2\n";
}

void fun1(void *arg)   
{
    cout<<"start fun1\n";

    schedule_t *schedule = static_cast<schedule_t *>(arg);

    int co2 = co_create(*schedule,fun2,schedule);
    co_resume(*schedule,co2);

    sleep(3);

    cout<<"end fun1\n";
}

int main()
{
    schedule_t schedule;

    int co1 = co_create(schedule,fun1,&schedule);
    co_resume(schedule,co1);

    cout<<"协程被挂起了，这里是main\n";

    event_loop(&schedule);  //事件循环
    return 0;
}

