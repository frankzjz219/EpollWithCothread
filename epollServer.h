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

    /*创建线程
    @param s 传入线程结构体的引用
    @param id 传入线程的id
    */
    void createThread(int id, schedule_t& s);
    /*epoll事件处理函数
        @param ep 传入要处理的epollServer对象的指针
    */
    static void* handleEpoll(void* ep);
    
    /*数据处理函数
        @param u 是指向uthread结构体的指针
    */
    static void socketEcho(std::shared_ptr<uthread_t> u);

    /*协程让出处理机的函数
        @param t 协程结构体
    
    */
    static void uthread_suspend(std::shared_ptr<uthread_t>& t);

    /*创建一个新的协程
        @param schedule 协程结构体
        @param func 协程工作函数
        @param priority 重要性
        @param client_sock 客户端socket端口
    */
    void createUthread(schedule_t &schedule, void(*func)(std::shared_ptr<uthread_t>), unsigned long long priority, int client_sock);
    /* 处理Ctrl C
        @param sig 信号
    */
    static void handleSigINT(EpollServer* t, int sig);
public:
    /*初始化EpollServer
    @param threads 服务器的线程数量
    @param port 服务器所在的端口
    */
    EpollServer(int threads, int port);
    ~EpollServer();
};


#endif