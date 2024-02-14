#include "epollServer.h"

bool flag = true;

void sigIntHandler(int sig)
{
    flag = false;
}

int main()
{
    signal(SIGINT, sigIntHandler);
    EpollServer ep(1, 18080);
    while(flag){usleep(1e3);}
    return 0;
}