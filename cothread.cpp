#include "cothread.h"

schedule_t::schedule_t() : running_thread(-1), max_index(0), mutex(std::make_shared<mutexWrapper>()), stopFlag(0)
{
    auto cmp = [this](int a, int b)
    {
        return (threads[a]->usedTime) / (threads[a]->priority) > (threads[b]->usedTime) / (threads[b]->priority); // 小根堆
    };
    threadPool = std::priority_queue<int, std::vector<int>, std::function<bool(int, int)>>(cmp);
}

unsigned long long getMicroseconds()
{
    struct timeval currentTime;
    gettimeofday(&currentTime, NULL);
    return currentTime.tv_sec * (int)1e6 + currentTime.tv_usec;
}

void uthread_resume(schedule_t &schedule, int id)
{
    schedule.mutex->lock();
    // printf("Resuming\n");
    if (id < 0 || id >= schedule.max_index)
    {
        schedule.mutex->unlock();
        return;
    }

    std::shared_ptr<uthread_t> t = schedule.threads[id];

    if (t->state == RUNNABLE)
    {
        schedule.running_thread = id;
        t->prevTime = getMicroseconds();
        schedule.mutex->unlock();
        swapcontext(&(schedule.main), &(t->ctx));
        // setcontext(&(t->ctx));
    }
    else
    {
        schedule.mutex->unlock();
    }
}

// 找到当前运行时间最短的携程上处理机
void fairResume(schedule_t &schedule)
{
    schedule.mutex->lock();
    int id = schedule.threadPool.top();
    schedule.threadPool.pop();
    schedule.mutex->unlock();
    // printf("poping %d\n", id);
    printf("resuming %d\n", id);
    uthread_resume(schedule, id);
}

// 协程让出控制流的函数
void uthread_yield(std::shared_ptr<uthread_t> t)
{
    
    schedule_t &schedule = scheduler_attrs[t->schid];
    printf("yielding scheduler %d\n", t->schid);
    schedule.mutex->lock();

    // printf("running: %d\n", schedule.id);
    t->state = RUNNABLE;
    schedule.running_thread = -1;
    t->usedTime += getMicroseconds() - t->prevTime;
    schedule.threadPool.push(t->id); // 重新加入回队列
    // printf("pushing %d\n", t->id);

    schedule.mutex->unlock();

    swapcontext(&(t->ctx), &(schedule.main));

}

// 协程包装函数
void uthread_body(int i, int id)
{
    schedule_t &s = scheduler_attrs[i];
    std::shared_ptr<uthread_t> t = s.threads[id]; // 找到当前在执行的函数的信息

    t->func(t, t->arg); // 这里执行了任务函数（arg是协程任务函数的参数）

    t->state = FREE; // 执行结束之后设置为FREE提示结束

    s.running_thread = -1; // 设置为当前没有在执行函数
}

// 添加一个协程
int uthread_create(int i, Fun func, unsigned long long priority, void *arg)
{
    schedule_t &schedule = scheduler_attrs[i];
    int id = 0;
    schedule.mutex->lock();
    // 寻找一个当前可用的协程ID
    for (id = 0; id < schedule.max_index; ++id)
    {
        if (schedule.threads[id]->state == FREE)
        {
            break;
        }
    }

    if (id == schedule.max_index)
    {
        schedule.max_index++;
    }
    schedule.threads[id] = std::make_shared<uthread_t>();
    std::shared_ptr<uthread_t> t = schedule.threads[id];
    t->id = id; // 建立thread指针向携程id的索引
    t->schid = i;

    t->state = RUNNABLE;
    t->func = func;
    t->arg = arg;
    t->priority = priority;

    getcontext(&(t->ctx));

    t->ctx.uc_stack.ss_sp = t->stack;
    t->ctx.uc_stack.ss_size = DEFAULT_STACK_SZIE;
    t->ctx.uc_stack.ss_flags = 0;
    t->ctx.uc_link = &(schedule.main);
    t->usedTime = 0;
    t->prevTime = getMicroseconds();

    makecontext(&(t->ctx), (void (*)(void))(uthread_body), 2, i, id);
    schedule.threadPool.push(id);
    schedule.mutex->unlock();
    printf("创建coThread完毕！\n");
    // 执行携程任务函数
    // swapcontext(&(schedule.main), &(t->ctx));
    // setcontext(&(t->ctx));

    return id;
}

// 判断当前有无在运行的协程
int schedule_finished(schedule_t &schedule)
{
    schedule.mutex->lock();
    if (schedule.running_thread != -1)
    {
        schedule.mutex->unlock();
        return 0;
    }
    else
    {
        for (int i = 0; i < schedule.max_index; ++i)
        {
            if (schedule.threads[i]->state != FREE)
            {
                schedule.mutex->unlock();
                return 0;
            }
        }
    }
    schedule.mutex->unlock();
    return 1;
}

// 每个线程的调度器
void *coThreadScheduler(void *schedule)
{
    int cnt = 0;
    schedule_t &s = *(schedule_t *)schedule;
    while (!s.stopFlag)
    {

        if (!schedule_finished(s))
        {
            fairResume(s);
            if (cnt % 30 == 0)
            {
                printf("Scheduler %d:\n", s.id);
                for (auto i = s.threads.begin(); i != s.threads.end(); ++i)
                {
                    printf("thread %d has been running for %llu\n", i->second->id, i->second->usedTime);
                }
                printf("\n");
            }
            ++cnt;
        }
        else
        {
            printf("coThread %d no active job!\n", s.id);
            usleep((unsigned long)1e6);
        }
    }
    puts("scheduler over");
    return NULL;
}

void createCoThread()
{
    int id = ++schedule_t::cntScheduler;
    schedule_t &schedule = scheduler_attrs[id - 1];
    schedule.id = id;
    printf("Creating coThread scheduler %d ...\n", schedule.id);
    if (pthread_create(&(schedule.threadHandle), NULL, coThreadScheduler, &schedule))
    {
        perror("Cothread creation failed");
        exit(EXIT_FAILURE);
    }
}

void sigINTHandler(int i)
{
    for (int i = 0; i < scheduler_attrs.size(); ++i)
    {
        scheduler_attrs[i].mutex->lock();
        printf("关闭携程 %d 中...\n", scheduler_attrs[i].id);
        scheduler_attrs[i].stopFlag = 1; // 使得携程停止
        scheduler_attrs[i].mutex->unlock();
        if (pthread_join(scheduler_attrs[i].threadHandle, NULL))
        {
            printf("***回收携程线程 %d 失败!***\n", scheduler_attrs[i].id);
        }
        else
        {
            printf("回收携程线程 %d 完成！\n", scheduler_attrs[i].id);
        }
    }
    exit(0);
}