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


#define DEFAULT_STACK_SZIE (1024)
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
    uthread_t()
    {
        // puts("init uthread...\n");
    }
    uthread_t(const uthread_t&) = delete;
    uthread_t& operator=(const uthread_t&) = delete;
    ~uthread_t()
    {
        // puts("***正在析构uthread...\n");
    }
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
    void* epServer;

    // 便于需要创建协程的时候相应
    bool createFlag;
    int priorityToCreate;
    int client_sock;

    schedule_t();
    schedule_t(const schedule_t&) = delete;
    schedule_t& operator=(const schedule_t&) = delete;
    
    ~schedule_t() {
        // puts("***正在析构...\n");
    };
};


#endif