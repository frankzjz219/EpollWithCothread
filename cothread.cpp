#include "cothread.h"

schedule_t::schedule_t() : running_thread(-1), max_index(0), mutex(std::make_shared<mutexWrapper>()), stopFlag(0),
    createFlag(false)
{
    // puts("init schedule...");
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

