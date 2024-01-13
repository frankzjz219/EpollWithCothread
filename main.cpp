
#include <stdio.h>
#include <random>

// g++ cothread.cpp main.cpp -o test -lpthread -g
#include "cothread.h"
extern int schedule_t::cntScheduler;
#define COTHREADNUM 4

std::vector<schedule_t> scheduler_attrs(COTHREADNUM);

int getRandomNumber(int min, int max) {
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> dis(min, max);

    return dis(gen);
}

void func2(void * arg)
{
    while(1)
    {
        usleep(getRandomNumber(10, 200)*1000);
        // printf("0\n");
        uthread_yield(*(schedule_t *)arg);
    }
}

void func(void* arg)
{
    usleep(getRandomNumber(10, 200)*1000);
}




int main()
{
    // 需要具有应对SIGINT功能的话必须设置SIGINT的处理函数是头文件规定的函数
    signal(SIGINT, sigINTHandler);
    for(int i = 0; i<COTHREADNUM; ++i)
    {
        createCoThread(scheduler_attrs[i]);
    }

    uthread_create(scheduler_attrs[0], func2, 1, &(scheduler_attrs[0]));
    uthread_create(scheduler_attrs[1], func2, 1, &(scheduler_attrs[1]));
    uthread_create(scheduler_attrs[0], func2, 2, &(scheduler_attrs[0]));
    uthread_create(scheduler_attrs[1], func2, 2, &(scheduler_attrs[1]));
    uthread_create(scheduler_attrs[0], func2, 3, &(scheduler_attrs[0]));
    uthread_create(scheduler_attrs[1], func2, 3, &(scheduler_attrs[1]));
    uthread_create(scheduler_attrs[0], func2, 4, &(scheduler_attrs[0]));
    uthread_create(scheduler_attrs[1], func2, 4, &(scheduler_attrs[1]));
    uthread_create(scheduler_attrs[2], func2, 1, &(scheduler_attrs[2]));
    uthread_create(scheduler_attrs[3], func, 1, &(scheduler_attrs[2]));

    while(1){usleep((unsigned long)1e6);}
    return 0;
}