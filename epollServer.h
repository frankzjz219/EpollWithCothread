#ifndef EPOLLSERVER_H
#define EPOLLSERVER_H

#include "cothread.h"
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/epoll.h>
#include <unistd.h>
#include <cstdlib>
#include <cerrno>
#include <iostream>
#include <string.h>

typedef struct TimerInfo
{
    unsigned long long expireTime;
    uthread_t* cb;
    TimerInfo(unsigned long long e, uthread_t* u):expireTime(e), cb(u)
    {
        printf("Creating timer %d\n", cb->sock_fd);
    };
    ~TimerInfo()
    {
        printf("Destroying timer %d\n", cb->sock_fd);
    }
} TimerInfo;

class EpollServer
{
    // 线程的集合
    std::vector<schedule_t> scheduler_attrs;
    // 将client的socket fd映射到协程
    std::unordered_map<int, std::shared_ptr<uthread_t>> fdToUthread;
    // 客户端计数
    int clientEnt;
    // 总线程数
    int threadCnt;
    // 服务器socketfd
    int server_fd;
    // epoll的fd
    int epoll_fd;
    // server的端口号
    int server_port;
    // 下一个要添加协程的线程号
    int threadToPut;
    // epoll监听线程的线程
    pthread_t epollThread;
    // 存放定时器的优先级队列
    std::priority_queue<std::shared_ptr<TimerInfo>, std::vector<std::shared_ptr<TimerInfo>>, std::function<bool(std::shared_ptr<TimerInfo>&, std::shared_ptr<TimerInfo>&)>> timers;
    // 定时器控制线程
    pthread_t timerThread;
    // 保护定时器用的锁
    std::shared_ptr<mutexWrapper> timerMutex;

    /*创建线程
    @param s 传入线程结构体的引用
    @param id 传入线程的id
    */
    void createThread(int id, schedule_t& s);
    /*epoll事件处理函数
        @param ep 传入要处理的epollServer对象的指针
    */
    static void* handleEpoll(void* ep);

    /*判断一个线程此时是否没有在执行的内容
        @param schedule 传入协程结构体
    */
    static int schedule_finished(schedule_t &schedule);

    /*选出一个上处理机的线程
        @param schedule 传入协程结构体
    */
    static void fairResume(schedule_t &schedule);

    /*切换到工作协程
        @param schedule 传入协程结构体
        @param id 要恢复的协程id
    */
    static void uthread_resume(schedule_t &schedule, int id);
    /*数据处理函数
        @param u 是指向uthread结构体的指针
    */
    static void socketEcho(uthread_t* u);

    /*协程让出处理机的函数
        @param t 协程结构体指针
    
    */
    static void uthread_suspend(uthread_t* t);

    /*创建一个新的协程，执行过程已经获取锁
        @param schedule 协程结构体
        @param func 协程工作函数
        @param priority 重要性
        @param client_sock 客户端socket端口
    */
    static void createUthread(EpollServer* ep, schedule_t &schedule, unsigned long long priority, int client_sock);
    /* 处理Ctrl C
        @param sig 信号
    */
    static void handleSigINT(EpollServer* t, int sig);
    /* 协程调度器工作函数
        @param schedule 协程信息结构体
    */
    static void *coThreadScheduler(void *schedule);

    /* 定时器控制函数
        @param ep 类自身指针
    */
    static void *timerManager(void *ep);

    /*
        @param u 当前协程指针
        @param timeToDelay 需要延迟的时间 微秒
    */
    static void addTimer(uthread_t* u, unsigned long long timeToDelay);

public:
    /*初始化EpollServer
    @param threads 服务器的线程数量
    @param port 服务器所在的端口
    */
    EpollServer(int threads, int port);
    ~EpollServer();
};


#endif