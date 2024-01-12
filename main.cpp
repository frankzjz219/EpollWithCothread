#include "cothread.h"
#include <stdio.h>
#include <random>
#include <csignal>

#define COTHREADNUM 2

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



schedule_t s[COTHREADNUM];


void sigINTHandler(int i)
{
    for(auto&i : s)
    {
        printf("关闭携程中...\n");
        i.stopFlag = 1;//使得携程停止
        pthread_join(i.threadHandle, NULL);
    }
    exit(0);
}


int main()
{
    signal(SIGINT, sigINTHandler);
    for(int i = 0; i<COTHREADNUM; ++i)
    {
        createCoThread(s[i]);
    }

    uthread_create(s[0], func2, 1, &(s[0]));
    uthread_create(s[1], func2, 1, &(s[1]));
    uthread_create(s[0], func2, 2, &(s[0]));
    uthread_create(s[1], func2, 2, &(s[1]));
    uthread_create(s[0], func2, 3, &(s[0]));
    uthread_create(s[1], func2, 3, &(s[1]));
    uthread_create(s[0], func2, 4, &(s[0]));
    uthread_create(s[1], func2, 4, &(s[1]));

    while(1){usleep((unsigned long)1e6);}
    return 0;
}