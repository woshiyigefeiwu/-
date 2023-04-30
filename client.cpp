#include <stdio.h>
#include <arpa/inet.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <iostream>
#include <string>
#include "coroutine.h"
#include "co_hook.cpp"
using namespace std;

const int maxn_len = 1024;

#define sever_ip_addr "192.168.107.130"
#define port 9999   //端口号

void fun_connect(void *arg)
{
    schedule_t *schedule = static_cast<schedule_t *>(arg);  //调度器

    //创建socket
    int fd = socket(AF_INET,SOCK_STREAM,0);
    if(fd == -1)    //创建失败
    {
        perror("创建失败！");
        exit(-1);
    }

    //连接服务器
    struct sockaddr_in serveradd;
    inet_pton(AF_INET,sever_ip_addr,&serveradd.sin_addr.s_addr);    //点分十进制 转换成 整数字节序
    serveradd.sin_family = AF_INET;     //协议族
    serveradd.sin_port = htons(port);   //主机字节序 转换为 网络字节序
    int ret = connect(fd,(struct sockaddr*)&serveradd,sizeof(serveradd));

    if(ret == -1)
    {
        perror("连接失败！");
        exit(-1);
    }

    //通信
    char recvbuff[maxn_len];
    while(1)
    {
        //从键盘输入，给服务端发送数据
        printf("请输入发送数据：");
        string str;
        getline(cin,str);   //为的是读数据的时候包含空格
        char *date = (char*)str.c_str();
        write(fd,date,strlen(date));    //注意这里的写法：千万不能写成sizeof(date)，这样写的话是计算这个指针的大小，始终是8！

        printf("发送成功，发送的数据为：%s\n",date);

        //接受数据
        memset(recvbuff,0,sizeof recvbuff);
        int len = read(fd,recvbuff,sizeof recvbuff);

        if(len == -1)
        {
            perror("接收数据错误！");
            exit(-1);
        }
        else if(len == 0)
        {
            printf("服务器断开连接！");
            break;
        }
        else if(len > 0)
        {
            printf("我接受到了回射服务器的返回的数据 : %s\n\n", recvbuff);
        }
    }

    close(fd);
}

int main()
{
    schedule_t schedule;

    int co_connect = co_create(schedule,fun_connect,&schedule); //创建一个协程来connect并发送数据
    co_resume(schedule,co_connect);

    event_loop(&schedule);  //事件循环
    return 0;
}
