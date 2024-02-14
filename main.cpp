
#include <stdio.h>
#include <random>

// g++ cothread.cpp main.cpp -o test -lpthread -g
// ./test > ./out.txt
#include "cothread.h"

#define COTHREADNUM 4

std::vector<schedule_t> scheduler_attrs(COTHREADNUM);

int getRandomNumber(int min, int max) {
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> dis(min, max);

    return dis(gen);
}

void func2(std::shared_ptr<uthread_t> t, void * arg)
{
    while(1)
    {
        // usleep(getRandomNumber(10, 200)*1000);
        // // printf("0\n");
        // uthread_yield_to_suspend(t);
        usleep(getRandomNumber(10, 200)*1000);
        uthread_yield(t);
    }
}

void func(std::shared_ptr<uthread_t> t, void* arg)
{
    usleep(getRandomNumber(10, 200)*1000);
    uthread_yield_to_suspend(t);
}




int main()
{
    // 需要具有应对SIGINT功能的话必须设置SIGINT的处理函数是头文件规定的函数
    signal(SIGINT, sigINTHandler);
    for(int i = 0; i<COTHREADNUM; ++i)
    {
        createCoThread(i);
    }

    uthread_create(0, func2, 1, NULL);
    uthread_create(1, func2, 1, NULL);
    uthread_create(0, func2, 2, NULL);
    uthread_create(1, func2, 2, NULL);
    uthread_create(0, func2, 3, NULL);
    uthread_create(1, func2, 3, NULL);
    uthread_create(0, func2, 4, NULL);
    uthread_create(1, func2, 4, NULL);
    uthread_create(2, func2, 1, NULL);
    // uthread_create(3, func, 1, NULL);
    usleep(5000*1000);
    uthread_create(3, func2, 1, NULL);
    while(1)
    {
        usleep(getRandomNumber(1, 5)*1000000);
        uthread_create(getRandomNumber(0, COTHREADNUM-1), func, 1, NULL);
        for(auto& s : scheduler_attrs)
        {
            for(auto& j : s.threads)
            {
                if(j.second->state == SUSPEND)
                    j.second->state = RUNNABLE;
            }
        }
    }

    while(1){usleep((unsigned long)1e6);}
    return 0;
}