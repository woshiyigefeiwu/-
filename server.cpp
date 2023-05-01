#include "coroutine.h"
#include "co_hook.cpp"
#include <stdio.h>
#include <iostream>
#include <unistd.h>
#include <string.h>
#include <sys/socket.h>
#include <arpa/inet.h>
using namespace std;

const int maxn_len = 1024;
#define sever_ip_addr "192.168.107.130"
#define port 9999   //端口号

void fun_read(void *arg)
{
    int cfd = *(int*)arg;
    schedule_t *schedule = pid_to_schedule[pthread_self()];  //调度器

    char buff[1024];
    while(1)
    {
        memset(buff,0,sizeof buff);     //注意这里要清空，不然会读到上一次的数据！！！
        int ret = read(cfd,buff,sizeof(buff));
        printf("接收到的客户端的buff为：%s\n",buff);

        printf("现在发回去！\n");
        write(cfd,buff,strlen(buff));
        printf("发送成功！\n\n");
    }

    close(cfd);
}

void fun_listen(void *arg)
{
    schedule_t *schedule = static_cast<schedule_t *>(arg);  //调度器

    //创建socket
    int lfd = socket(AF_INET,SOCK_STREAM,0);
    if(lfd == -1)
    {
        perror("创建lfd失败！");
        exit(-1);
    }

    //绑定 ip 和 端口
    struct sockaddr_in serveraddr;
    serveraddr.sin_family = AF_INET;            //协议族
    serveraddr.sin_port = htons(port);          //主机字节序转换到网络字节序
    serveraddr.sin_addr.s_addr = INADDR_ANY;    //绑定主机就行
    bind(lfd,(struct sockaddr*)&serveraddr,sizeof serveraddr);

    //监听
    listen(lfd,8);  //第二个参数是accept队列的最大长度+1

    while(1)    //循环接收连接就行
    {
        struct sockaddr_in client_addr;
        socklen_t client_len = sizeof(client_addr);

        int cfd = accept(lfd,(struct sockaddr *) &client_addr, &client_len);

        cout<<"建立连接成功\n";

        //这里就需要创建协程承接这个cfd（读数据并发从数据）
        int co_read = co_create((*schedule),fun_read,&cfd);
        co_resume((*schedule),co_read);
    }

    cout<<"co_listen结束\n";
}

int main()
{
    schedule_t schedule;    //创建一个调度器

    int co_listen = co_create(schedule,fun_listen,&schedule);   //创建一个协程接收连接
    co_resume(schedule,co_listen);

    event_loop(&schedule);  //事件循环
    return 0;
}


