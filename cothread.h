#ifndef COTHREAD_H
#define COTHREAD_H

#include <ucontext.h>
#include <vector>
#include <unordered_map>
#include <queue>
#include <functional>
#include <unistd.h>
#include <sys/time.h>
#include <stdio.h>
#include <memory>
#include <pthread.h>
#include <stdlib.h>
#include <csignal>
#include <string>
#include <queue>


#define DEFAULT_STACK_SZIE (1024*128)
#define MAX_UTHREAD_SIZE   1024

// unsigned long long getMicroseconds();

enum ThreadState{FREE,RUNNABLE,RUNNING,SUSPEND};

struct schedule_t;

unsigned long long getMicroseconds();

typedef struct uthread_t
{
    ucontext_t ctx;
    int id;
    int schid;
    schedule_t* schedPtr;
    void* epServer;
    int epoll_fd;
    void (*func)(std::shared_ptr<uthread_t> t);
    void *arg;
    int sock_fd;
    enum ThreadState state;
    char stack[DEFAULT_STACK_SZIE];
    unsigned long long usedTime;
    unsigned long long prevTime;
    unsigned long long priority;
}uthread_t;

typedef void (*Fun)(std::shared_ptr<uthread_t> t, void *arg);

class mutexWrapper
{
    pthread_mutex_t mutex;
public:
    mutexWrapper():mutex(PTHREAD_MUTEX_INITIALIZER){}
    ~mutexWrapper(){pthread_mutex_destroy(&mutex);}
    void lock()
    {
        pthread_mutex_lock(&mutex);
    }
    void unlock()
    {
        pthread_mutex_unlock(&mutex);
    }
};

class schedule_t
{
public:
    ucontext_t main;
    int running_thread;
    std:: unordered_map<int, std::shared_ptr<uthread_t>> threads;
    std:: priority_queue<int, std::vector<int>, std::function<bool(int, int)>>threadPool;
    int max_index; // 曾经使用到的最大的index + 1
    std::shared_ptr<mutexWrapper> mutex;
    pthread_t threadHandle;
    int stopFlag;
    // static int cntScheduler;
    int id;

    schedule_t();
    
    ~schedule_t() {
        // printf("正在析构...\n");
    };
};

/*创建一个携程调度器（一个独立的线程）
    @param id 这个协程所在线程的编号
*/ 
void createCoThread(int id);

/* 寻找此时运行时间最短的线程上处理机
    @param id 需要执行调度的调度器线程id
*/
void fairResume(schedule_t &schedule);

/*help the thread running in the schedule*/
// static void uthread_body(int, int);

/*创建一个协程
    @param id 希望创建协程任务的协程线程ID
    @param func 协程工作函数
    @param priority 协程优先级
    @param arg 指向协程工作函数参数的指针
*/
int uthread_create(int id,Fun func, unsigned long long priority, void *arg);

/* Hang the currently running thread, switch to main thread 
    @param t 当前协程的协程信息结构体指针
*/
// void uthread_yield(std::shared_ptr<uthread_t> t);


/*  将线程的状态设置为suspend提示在等待资源，并且返回主协程
    @param t 当前协程的协程信息结构体指针

*/
// void uthread_yield_to_suspend(std::shared_ptr<uthread_t> t);

/* resume the thread which index equal id
    @param schedule 传入的协程线程结构体
    @param id 需要恢复的对应协程号
*/
// void uthread_resume(schedule_t &schedule,int id);

/*test whether all the threads in schedule run over·
    @param schedule 需要判断的协程调度器线程结构体
*/
// int schedule_finished(schedule_t &schedule);

// 这里打开的线程数组全局变量
// extern std::vector<schedule_t> scheduler_attrs;

// 处理关闭多线程的函数
// void sigINTHandler(int i);

/* 协程的调度器函数
    @param schedule 引入协程信息结构体
*/
void *coThreadScheduler(void *schedule);
#endif