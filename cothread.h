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

#define DEFAULT_STACK_SZIE (1024*128)
#define MAX_UTHREAD_SIZE   1024

unsigned long long getMicroseconds();

enum ThreadState{FREE,RUNNABLE,RUNNING,SUSPEND};

struct schedule_t;

typedef void (*Fun)(void *arg);

typedef struct uthread_t
{
    ucontext_t ctx;
    int id;
    Fun func;
    void *arg;
    enum ThreadState state;
    char stack[DEFAULT_STACK_SZIE];
    unsigned long long usedTime;
    unsigned long long prevTime;
    unsigned long long priority;
}uthread_t;

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

typedef struct schedule_t
{
    ucontext_t main;
    int running_thread;
    std:: unordered_map<int, std::shared_ptr<uthread_t>> threads;
    std:: priority_queue<int, std::vector<int>, std::function<bool(int, int)>>threadPool;
    int max_index; // 曾经使用到的最大的index + 1
    std::shared_ptr<mutexWrapper> mutex;
    pthread_t threadHandle;
    int stopFlag;

    schedule_t();
    
    ~schedule_t() {
        // printf("正在析构...\n");
    }
}schedule_t;

// 创建一个携程调度器（一个独立的线程）
void createCoThread(schedule_t& schedule);

// 寻找此时运行时间最短的线程上处理机
void fairResume(schedule_t &schedule);

/*help the thread running in the schedule*/
static void uthread_body(schedule_t *ps);

/*Create a user's thread
*    @param[in]:
*        schedule_t &schedule 
*        Fun func: user's function
*        void *arg: the arg of user's function
*    @param[out]:
*    @return:
*        return the index of the created thread in schedule
*/
int  uthread_create(schedule_t &schedule,Fun func, unsigned long long priority, void *arg);

/* Hang the currently running thread, switch to main thread */
void uthread_yield(schedule_t &schedule);

/* resume the thread which index equal id*/
void uthread_resume(schedule_t &schedule,int id);

/*test whether all the threads in schedule run over
* @param[in]:
*    const schedule_t & schedule 
* @param[out]:
* @return:
*    return 1 if all threads run over,otherwise return 0
*/
int  schedule_finished(schedule_t &schedule);

#endif