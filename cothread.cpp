#include "cothread.h"

unsigned long long getMicroseconds() {
    struct timeval currentTime;
    gettimeofday(&currentTime, NULL);
    return currentTime.tv_sec * (int)1e6 + currentTime.tv_usec;
}

void uthread_resume(schedule_t &schedule , int id)
{
    // printf("Resuming\n");
    if(id < 0 || id >= schedule.max_index){
        return;
    }

    uthread_t *t = schedule.threads[id];

    if (t->state == RUNNABLE) {
        schedule.running_thread = id;
        t->prevTime = getMicroseconds();
        swapcontext(&(schedule.main),&(t->ctx));
        // setcontext(&(t->ctx));
    }
}

// 找到当前运行时间最短的携程上处理机
void fairResume(schedule_t &schedule)
{
    int id = schedule.threadPool.top();
    schedule.threadPool.pop();
    // printf("poping %d\n", id);
    uthread_resume(schedule, id);
}

void uthread_yield(schedule_t &schedule)
{
    // printf("yielding\n");
    if(schedule.running_thread != -1 ){
        uthread_t *t = schedule.threads[schedule.running_thread];
        t->state = RUNNABLE;
        schedule.running_thread = -1;
        t->usedTime += getMicroseconds() - t->prevTime;
        schedule.threadPool.push(t->id); // 重新加入回队列
        // printf("pushing %d\n", t->id);
        swapcontext(&(t->ctx),&(schedule.main));
    }
}

void uthread_body(schedule_t *ps)
{
    int id = ps->running_thread;

    if(id != -1){
        uthread_t *t = ps->threads[id]; // 找到当前在执行的函数的信息

        t->func(t->arg); // 这里执行了任务函数

        t->state = FREE; // 执行结束之后设置为FREE提示结束
        
        ps->running_thread = -1; // 设置为当前没有在执行函数
    }
}

int uthread_create(schedule_t &schedule,Fun func, unsigned long long priority, void *arg)
{
    int id = 0;
    
    for(id = 0; id < schedule.max_index; ++id ){
        if(schedule.threads[id]->state == FREE){
            break;
        }
    }
    
    if (id == schedule.max_index) {
        schedule.max_index++;
    }
    schedule.threads[id] = new uthread_t;
    uthread_t *t = schedule.threads[id];
    t->id = id; // 建立thread指针向携程id的索引

    t->state = RUNNABLE;
    t->func = func;
    t->arg = arg;
    t->priority = priority;

    getcontext(&(t->ctx));
    
    t->ctx.uc_stack.ss_sp = t->stack;
    t->ctx.uc_stack.ss_size = DEFAULT_STACK_SZIE;
    t->ctx.uc_stack.ss_flags = 0;
    t->ctx.uc_link = &(schedule.main);
    schedule.running_thread = id;
    t->usedTime = 0;
    t->prevTime = getMicroseconds();
    
    makecontext(&(t->ctx),(void (*)(void))(uthread_body),1,&schedule);
    swapcontext(&(schedule.main), &(t->ctx));
    // setcontext(&(t->ctx));
    
    return id;
}

int schedule_finished(schedule_t &schedule)
{
    if (schedule.running_thread != -1){
        return 0;
    }else{
        for(int i = 0; i < schedule.max_index; ++i){
            if(schedule.threads[i]->state != FREE){
                return 0;
            }
        }
    }

    return 1;
}
