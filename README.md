# 使用epoll和协程处理信息的socket通信库
- 底层是`ucontext`和`epoll`
- 提供一个`EpollServer`类
## 结构
- 创建一个服务器socket
- 使用epoll检测每个事件
- 检测到新的客户端连接的时候，顺次循环将新遇到的请求分配给下一个线程处理以实现负载的均衡，给对应的线程`schedule_t`置一个flag，同时将对应的`socket_fd`等信息通过这个结构体传入，提示这个线程需要创建一个新的协程处理客户端链接的问题
- 新的协程**自己在线程内**创建协程（最好在同一个线程内创建和使用协程，否则会导致容易产生无原因而且不易解决的`segmentation fault`）
- 创建之后将协程状态设置为`SUSPEND`
- 等待epoll接收到信号之后，通过一个哈希映射`unordered_map`将对应的协程状态设置为`RUNNABLE`提示该协程的调度器可以上处理机运行
- 然后对应的**协程调度器**将协程调度上处理机，协程处理完之后会继续将自己设置为`SUSPEND`状态并且下处理机，等待`epoll_wait`收到信号之后如上所述将自己的状态设置为`RUNNABLE`, 然后本线程的协程调度器激活自己
- ![Alt text](/imgs/epollServer.drawio.png)

## 使用
```cpp
EpollServer ep(<线程数>, <端口号>);
```
- 头文件是`epollServer.h`
- 测试文件是`client.cpp`
- 编译指令`g++ cothread.cpp main2.cpp epollServer.cpp -o test1 -lpthread -g`

## 输出展示
- ![Alt text](/imgs/image.png)
- ![Alt text](/imgs/image2.png)
- ![Alt text](/imgs/image3.png)