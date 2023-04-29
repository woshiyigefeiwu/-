#include "coroutine.h"
#include <stdio.h>
#include <iostream>
#include <unistd.h>
#include <event.h>
#include "test2.cpp"
using namespace std;

int main()
{
    test2();    //测试一下在跨协程唤醒的正确性

    return 0;
}


