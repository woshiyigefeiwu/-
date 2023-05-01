#include <iostream>
#include "coroutine.h"
using namespace std;

int co2;

void fun2(void *arg)
{
    schedule_t *schedule = static_cast<schedule_t *>(arg);
    for(int i=1;i<=2;i++)
    {
        cout<<i<<'\n';
        co_yield(*schedule);
    }
}

void fun1(void *arg)
{
    schedule_t *schedule = static_cast<schedule_t *>(arg);

    co2 = co_create(*schedule,fun2,schedule);
    co_resume(*schedule,co2);
}

void test2()    //测试一下在跨协程唤醒的正确性
{
    schedule_t schedule;

    int co1 = co_create(schedule,fun1,&schedule);
    co_resume(schedule,co1);

    co_resume(schedule,co2);    //在其他的协程唤醒co2
}

int main()
{
    test2();

    return 0;
}
