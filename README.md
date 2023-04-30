# Mini_Coroutine_library
**基于 Linux ucontext 函数族实现的 简易的，非对称的 协程库**
**（v2分支）**

------------------------------------------------------------------------------------------------

对于协程，不仅仅是一个挂起唤醒的函数，**更重要的是得封装异步，这样才能保证当一个协程阻塞的时候不会阻塞整个线程，从而体现出协程的真正作用。**

下面 接着 main 分支的 readme 讲

------------------------------------------------------------------------------------------------

**首先我对项目的构想是这样的：**

一个进程 <=> 多个线程

一个线程 <=> 多个协程

那么这里多线程的话就可以搞一个线程池（这里我没封装线程池hhh）

如何一个线程中用一个调度器来控制所有协程，所以这里需要有一个 (线程号 <=> 调度器) 的映射关系：这里我用的是一个全局的 map<int,schedule_t*>

（ps：map是非线程安全的，多线程在操作map的时候可能会有问题，需要加锁（不过这里我也还没搞hhh，有时间搞搞））

------------------------------------------------------------------------------------------------

**那么如何通过封装异步 使得调用同步函数达到协程异步的效果呢？（这个是核心）**

更加具体的：比如我一个协程调用read，没有数据可以读时我们怎么切换到其他协程上面去？当有数据可以读的时候我们怎么切换回来呢？

分为两部分：

（1）我们如何在系统调用中增加自己想要的逻辑代码？

（2）如何切回来？

&emsp;&emsp;&emsp;&emsp;  

（1）这里我们可以用hook技术（简单来说，hook是一种截取信息、更改程序执行流向、添加新功能的技术）

&emsp;&emsp;&emsp;&emsp;  第一步：声明一个想要hook的函数指针，比如 hook read 函数：typedef ssize_t (*read_fun_ptr)(int __fd, void *__buf, size_t __nbytes);

&emsp;&emsp;&emsp;&emsp;  第二部：动态获取到这个系统调用编译后的入口函数符号，并赋值给上面的指针，read_fun_ptr g_sys_read_fun = (read_fun_ptr)dlsym(RTLD_NEXT, "read");

&emsp;&emsp;&emsp;&emsp;  第三步：重写对应的系统调用就行了，ssize_t read(int __fd, void *__buf, size_t __nbytes) { ... }

&emsp;&emsp;&emsp;&emsp;  这里我 hook 了：accpet, connect, read, write, sleep（实际上都大同小异，因为用了库...）


（2）在这之前我参考了微信后台在13年开源的 libco 协程库，里面用到了事件驱动，事件循环。

&emsp;&emsp;&emsp;&emsp;  然后嘞，我就在网上找，发现了一个好东西：libevent，是一个事件通知库，里面实现了事件驱动，主线程事件循环一直检测就行了

&emsp;&emsp;&emsp;&emsp; （所以说还得先装一下这个 libevent 库...我是废物555，找个时间自己实现一下事件驱动，看了libco，底层是用epoll kqueue实现的，具体还不太懂...）

&emsp;&emsp;&emsp;&emsp;  

**那么大致的思想就是：**

主线程有一个事件监听器，这里我把事件监听器封装到调度器里面了，创建一个调度器就直接有了；

当协程进行同步的系统调用时，在hook后的系统调用中，会创建一个事件（需要设置回调函数），加入到事件监听器中，然后切出协程；

然后当事件监听器检测到有事件触发时，就会调用回调函数（这里我设置回调函数的作用只是切回协程而已），切回对应的协程，然后协程就能进行相应的操作了。

比如 read 的时候，没数据时就切出，不会阻塞住整个线程，当有数据到达的时候会触发事件，从而将协程唤醒，读数据。

------------------------------------------------------------------------------------------------

然后在代码里面我用这个协程库写了一个回射服务器...

**然后就没了...一个很简单的项目，没有什么东西...主要是事件驱动部分是核心...但是我直接用来libevent库...有时间学习一下然后造个轮子补充！**

当然当然，epoll本质上还是同步的，真正实现异步只能用异步IO（这个暂时还不会...）

------------------------------------------------------------------------------------------------

下面是参考资料，非常非常感谢：

[Tencent - libco](https://github.com/Tencent/libco)

[微信开源C++Libco介绍与应用（一）](https://zhuanlan.zhihu.com/p/51078499)

[微信开源C++Libco介绍与应用（二）](https://zhuanlan.zhihu.com/p/51081816)

[当谈论协程时，我们在谈论什么](https://mp.weixin.qq.com/s/IO4ynnKEfy2Rt-Me7EIeqg)

[从无栈协程到 C++异步框架](https://mp.weixin.qq.com/s/QVXE7QbxEchl8ue4SoijiQ)

[tinyrpc](https://github.com/Gooddbird/tinyrpc#4-%E5%BF%AB%E9%80%9F%E4%B8%8A%E6%89%8B)

[C++实现的协程异步 RPC 框架 TinyRPC（一）-- 协程封装](https://zhuanlan.zhihu.com/p/466349082)

[C++实现的协程异步 RPC 框架 TinyRPC（二）-- 协程Hook](https://zhuanlan.zhihu.com/p/474353906)

[协程篇（三）-- 协程Hook](https://zhuanlan.zhihu.com/p/466995546)

[Libevent深入浅出 - Aceld(刘丹冰)](https://aceld.gitbooks.io/libevent/content/)






