#include "cothread.h"
#include <stdio.h>
#include <random>
#include <csignal>

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

void func3(void * arg)
{
    while(1)
    {
        usleep(getRandomNumber(10, 200)*1000);
        // printf("1\n");
        uthread_yield(*(schedule_t *)arg);
    }
    
    
}


schedule_t s;

void sigINTHandler(int i)
{
    for(auto i = s.threads.begin(); i!=s.threads.end(); ++i)
    {
        printf("thread %d has been running for %llu\n", i->second->id, i->second->usedTime);
    }
    exit(0);
}

void schedule_test()
{
    
    int id1 = uthread_create(s,func2, 1,&s);
    int id2 = uthread_create(s,func2, 2,&s);
    int id3 = uthread_create(s,func2, 3,&s);
    int id4 = uthread_create(s,func2, 4,&s);
    int cnt = 0;
    while(!schedule_finished(s)){
        // uthread_resume(s,id1);
        // uthread_resume(s,id2);
        fairResume(s);
        if(cnt%10 == 0)
        {
            for(auto i = s.threads.begin(); i!=s.threads.end(); ++i)
            {
                printf("thread %d has been running for %llu\n", i->second->id, i->second->usedTime);
            }
            printf("\n");
            
        }
        ++cnt;
    }
    puts("main over");

}
int main()
{
    signal(SIGINT, sigINTHandler);
    // s = schedule_t();
    schedule_test();

    return 0;
}