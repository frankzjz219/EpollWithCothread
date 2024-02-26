#include "epollServer.h"

EpollServer::~EpollServer()
{
    for (int i = 0; i < scheduler_attrs.size(); ++i)
    {
        scheduler_attrs[i].mutex->lock();
        printf("\033[1;31m关闭携程 %d 中...\033[0m\n", scheduler_attrs[i].id);
        scheduler_attrs[i].stopFlag = 1; // 使得携程停止
        scheduler_attrs[i].mutex->unlock();
        if (pthread_join(scheduler_attrs[i].threadHandle, NULL))
        {
            fprintf(stderr, "\033[1;31m***回收携程线程 %d 失败!***\033[0m\n", scheduler_attrs[i].id);
            std::cout<<strerror(errno)<<std::endl;
        }
        else
        {
            printf("\033[1;32m回收携程线程 %d 完成！\033[0m\n", scheduler_attrs[i].id);
        }
    }
    pthread_cancel(epollThread);
    pthread_cancel(timerThread);
    // 关闭epoll fd
    close(epoll_fd);
    // 关闭服务器socket fd
    close(server_fd);
    printf("\033[1;32m回收epoll handler完成！\033[0m\n");
}

EpollServer::EpollServer(int threads, int port):threadCnt(threads), clientEnt(0), server_port(port),
    threadToPut(0), scheduler_attrs(std::vector<schedule_t>(threads))
{
    struct sockaddr_in server_addr;
    // 创建线程
    for(int i = 0; i<threadCnt; ++i)
    {
        createThread(i, scheduler_attrs[i]);
    }
    pthread_create(&(timerThread), NULL, timerManager, this);
    printf("\033[1;32m定时器线程创建完成！\033[0m\n");
    // 网络部分
    // 创建socket
    server_fd = socket(AF_INET, SOCK_STREAM|SOCK_NONBLOCK, 0);
    if(server_fd<0)
    {
        fprintf(stderr, "\033[1;31m***创建server socket失败!***\033[0m\n");
        std::cout<<strerror(errno)<<std::endl;
        exit(EXIT_FAILURE);
    }
    printf("\033[1;32m创建端口完成！\033[0m\n");
    // 设置服务器地址
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(server_port);

    // 绑定地址
    if (bind(server_fd, (struct sockaddr *)&server_addr, sizeof(server_addr)) == -1) {
        fprintf(stderr, "\033[1;31m***bind端口失败!***\033[0m\n");
        std::cout<<strerror(errno)<<std::endl;
        exit(EXIT_FAILURE);
    }

    // 监听端口
    if (listen(server_fd, SOMAXCONN) == -1)
    {
        fprintf(stderr, "\033[1;31m***listen端口失败!***\033[0m\n");
        std::cout<<strerror(errno)<<std::endl;
        exit(EXIT_FAILURE);
    }

    // 创建 epoll 实例
    epoll_fd = epoll_create1(0);
    if (epoll_fd == -1) {
        fprintf(stderr, "\033[1;31m***创建epoll失败!***\033[0m\n");
        std::cout<<strerror(errno)<<std::endl;
        exit(EXIT_FAILURE);
    }
    printf("\033[1;32m创建epoll完成！\033[0m\n");

    // 记录用于连接的epoll事件
    struct epoll_event event;
    event.events = EPOLLIN;
    event.data.fd = server_fd;

    // 将服务器 socket 添加到 epoll 实例中
    if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, server_fd, &event) == -1) {
        fprintf(stderr, "\033[1;31m***添加服务器epoll事件失败***\033[0m\n");
        std::cout<<strerror(errno)<<std::endl;
        exit(EXIT_FAILURE);
    }
    if(pthread_create(&(epollThread), NULL, handleEpoll, this))
    {
        fprintf(stderr, "\033[1;31m***创建epoll handler 线程失败***\033[0m\n");
        std::cout<<strerror(errno)<<std::endl;
        exit(EXIT_FAILURE);
    }
}

// 设置处理用户Ctrl C的析构函数
void EpollServer:: handleSigINT(EpollServer* t, int sig)
{
    for (int i = 0; i < t->scheduler_attrs.size(); ++i)
    {
        // 获取锁
        t->scheduler_attrs[i].mutex->lock();
        printf("\033[1;31m关闭携程 %d 中...\033[0m\n", t->scheduler_attrs[i].id);
        t->scheduler_attrs[i].stopFlag = 1; // 使得携程停止
        t->scheduler_attrs[i].mutex->unlock();
        if (pthread_join(t->scheduler_attrs[i].threadHandle, NULL))
        {
            fprintf(stderr, "\033[1;31m***回收携程线程 %d 失败!***\033[0m\n", t->scheduler_attrs[i].id);
            std::cout<<strerror(errno)<<std::endl;
        }
        else
        {
            printf("\033[1;32m回收携程线程 %d 完成！\033[0m\n", t->scheduler_attrs[i].id);
        }
    }
}

void *EpollServer:: coThreadScheduler(void *schedule)
{
    int cnt = 0;
    schedule_t &s = *((schedule_t *)schedule);
    while (!s.stopFlag)
    {
        if(s.createFlag)
        {
            createUthread((EpollServer*)s.epServer, s, 1, s.client_sock);
        }
        if (!schedule_finished(s))
        {
            fairResume(s);
            if (cnt % 50 == 0)
            {
                printf("\033[1;4;34mScheduler %d:\033[0m\n", s.id);
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
            ++cnt;
            if (cnt % (int)5e3 == 0) 
                printf("\033[1;4;34mcoThread %d no active job!\033[0m\n", s.id);
            usleep((unsigned long)1e3);
            
        }
    }
    printf("\033[34mscheduler %d quiting...\033[0m\n", s.id);
    return NULL;
}

void EpollServer::fairResume(schedule_t& schedule)
{
    printf("fair resuming sched: %d\n", schedule.id);
    schedule.mutex->lock();
    int id = schedule.threadPool.top();

    // 将当前的任务弹出
    schedule.threadPool.pop();

    // 持续将当前不适合执行的id加入队列
    std::queue<int> notRunnableQ;
    while(schedule.threadPool.size()&&schedule.threads[id]->state!=RUNNABLE)
    {
        notRunnableQ.push(id);
        id = schedule.threadPool.top();
        schedule.threadPool.pop();
    }

    schedule.mutex->unlock();
    // printf("poping %d\n", id);
    // printf("resuming %d\n", id);
    if(schedule.threads[id]->state == RUNNABLE)
        uthread_resume(schedule, id);

    // 将前面状态不适合执行的放回队列中
    int temp;
    while(notRunnableQ.size())
    {
        temp = notRunnableQ.front();
        notRunnableQ.pop();
        schedule.threadPool.push(temp);
    }
}

void EpollServer:: uthread_resume(schedule_t &schedule, int id)
{
    schedule.mutex->lock();
    printf("Resuming cothread %d, sock %d\n", id, schedule.threads[id]->sock_fd);
    // printf("cnt: %ld\n", schedule.threads[id].use_count());
    if (id < 0 || id >= schedule.max_index)
    {
        schedule.mutex->unlock();
        return;
    }

    std::shared_ptr<uthread_t> t = schedule.threads[id];

    if (t->state == RUNNABLE)
    {
        t->state = RUNNING;
        schedule.running_thread = id;
        t->prevTime = getMicroseconds();
        schedule.mutex->unlock();
        swapcontext(&(schedule.main), &(t->ctx));
    }
    else
    {
        schedule.mutex->unlock();
    }
}

int EpollServer:: schedule_finished(schedule_t &schedule)
{
    schedule.mutex->lock();
    if (schedule.running_thread != -1)
    {
        schedule.mutex->unlock();
        return 0;
    }
    else
    {
        for(auto i : schedule.threads)
        {
            if(i.second->state == RUNNABLE)
            {
                schedule.mutex->unlock();
                return 0;
            }
        }
    }
    schedule.mutex->unlock();
    // printf("无可用协程\n");
    return 1;
}

void* EpollServer::timerManager(void* e)
{
    EpollServer* ep = (EpollServer*)e;
    std::priority_queue<std::shared_ptr<TimerInfo>>& ti = ep->timers;
    while(ti.size() && ti.top()->expireTime>=getMicroseconds())
    {
        ti.top()->cb->state = RUNNABLE;
        ti.pop();
    }
    return NULL;
}

void EpollServer:: addTimer(uthread_t* u, unsigned long long timeToDelay)
{
    std::shared_ptr<TimerInfo> newTi = std::make_shared<TimerInfo>(getMicroseconds()+timeToDelay, u);
    EpollServer* ep = (EpollServer*)(u->epServer);
    ep->timers.push(newTi);
}


void* EpollServer::handleEpoll(void* e)
{
    EpollServer* ep = (EpollServer*)e;
    int n_events;
    struct epoll_event events[10];
    int client_fd;
    struct sockaddr_in client_addr;
    socklen_t client_addr_len;

    // 记录用于连接的epoll事件
    struct epoll_event event;
    printf("\033[1;32mepoll handler启动！\033[0m\n");
    // 主循环
    while (1) {
        // printf(".");
        n_events = epoll_wait(ep->epoll_fd, events, 10, -1);
        if (n_events == -1) 
        {
            fprintf(stderr, "\033[1;31m***epoll wait失败***\033[0m\n");
            std::cout<<strerror(errno)<<std::endl;
            exit(EXIT_FAILURE);
        }
        // 逐个处理epoll事件
        for (int i = 0; i < n_events; ++i) 
        {
            if (events[i].data.fd == ep->server_fd)
            {
                client_addr_len = sizeof(client_addr);
                client_fd = accept(ep->server_fd, (struct sockaddr *)&client_addr, &client_addr_len);
                printf("接收到链接sock: %d\n", client_fd);
                if(client_fd<0)
                {
                    fprintf(stderr, "\033[1;31m***接受客户端连接失败***\033[0m\n");
                    std::cout<<strerror(errno)<<std::endl;
                    exit(EXIT_FAILURE);
                }
                // 将新连接的客户端 socket 添加到 epoll 实例中
                event.events = EPOLLIN;
                event.data.fd = client_fd;
                if (epoll_ctl(ep->epoll_fd, EPOLL_CTL_ADD, client_fd, &event) == -1) {
                    fprintf(stderr, "\033[1;31m***添加客户端epoll事件失败***\033[0m\n");
                    std::cout<<strerror(errno)<<std::endl;
                    exit(EXIT_FAILURE);
                }
                // ep->createUthread(ep->scheduler_attrs[ep->threadToPut], 1, client_fd);
                // ++ep->threadToPut;ep->threadToPut%=ep->threadCnt;
                ep->scheduler_attrs[ep->threadToPut].createFlag = true;
                ep->scheduler_attrs[ep->threadToPut].client_sock = client_fd;
                ep->scheduler_attrs[ep->threadToPut].priorityToCreate = 1;
                ++ep->threadToPut;
                ep->threadToPut %= ep->threadCnt;
            }
            else
            {
                // if(ep->fdToUthread.find(events[i].data.fd)==ep->fdToUthread.end())continue;
                // printf("\033[1;32m收到socket%d的信息！\033[0m\n", events[i].data.fd);
                // 允许对应的协程上处理机执行
                // if(ep->fdToUthread[events[i].data.fd]!=NULL)
                auto it= ep->fdToUthread.find(events[i].data.fd);
                if(it != ep->fdToUthread.end() && it->second!=NULL)
                    it->second->state = RUNNABLE;
            }
        }
    }
}

void EpollServer::createUthread(EpollServer* ep, schedule_t &schedule, unsigned long long priority, int client_sock)
{
    // printf("\033[1;32m 检测到客户端连接，在创建协程！\033[0m\n");
    // 防止重复创建
    schedule.createFlag = false;
    int id = schedule.max_index++;
    schedule.mutex->lock();
    
    printf("Creating cothread id: %d\n, sock id: %d", id, client_sock);
    schedule.threads[id] = std::make_shared<uthread_t>();
    std::shared_ptr<uthread_t> t = schedule.threads[id];
    t->id = id; // 建立thread指针向携程id的索引
    t->schid = schedule.id;

    // 此处需要等到信号触发
    t->state = SUSPEND;
    // t->func = func;
    // 保存client的socket地址
    t->sock_fd = client_sock;
    t->priority = priority;
    t->schedPtr = &(schedule);
    t->epoll_fd = ep->epoll_fd;
    t->epServer = ep;

    getcontext(&(t->ctx));

    t->ctx.uc_stack.ss_sp = t->stack;
    t->ctx.uc_stack.ss_size = sizeof(t->stack);
    t->ctx.uc_stack.ss_flags = 0;
    t->ctx.uc_link = &(schedule.main);
    t->usedTime = 0;
    t->prevTime = getMicroseconds();

    ep->fdToUthread[client_sock] = t;

    makecontext(&(t->ctx), (void (*)(void))(socketEcho), 1, t.get());
    schedule.threadPool.push(id);
    schedule.mutex->unlock();
    printf("\033[32mSocket fd: %d 创建coThread完毕！\033[0m\n", t->sock_fd);
}

void EpollServer::socketEcho(uthread_t* u)
{
    char buffer[32] = {0};
    char bufferPrint[32] = {0};
    // 记录用于连接的epoll事件
    struct epoll_event event;
    while(1)
    {
        // printf("to read from socket: %d\n", u->sock_fd);
        ssize_t n_read = read(u->sock_fd, buffer, sizeof(buffer));
        // printf("读到%ld\n", n_read);
        if (n_read == -1) 
        {
            fprintf(stderr, "\033[1;31m***客户端Socket %d 读取失败***\033[0m\n", u->sock_fd);
            std::cout<<strerror(errno)<<std::endl;
            exit(EXIT_FAILURE);
        }
        if (n_read == 0)
        {
            printf("\033[31m客户端 %d 连接已断开！\033[0m\n", u->sock_fd);
            close(u->sock_fd);
            // event.events = EPOLLIN;
            event.data.fd = u->sock_fd;
            // 删除epoll监听的事件
            epoll_ctl(u->epoll_fd, EPOLL_CTL_DEL, u->sock_fd, &event);
            // 删除sockfd到uthread的映射
            auto it = ((EpollServer*)(u->epServer))->fdToUthread.find(u->sock_fd);
            ((EpollServer*)(u->epServer))->fdToUthread.erase(it);
            u->state = FREE;
            u->schedPtr->running_thread = -1;
            // 删除schedule_t 中uthread id 到uthread的映射
            auto it_ = u->schedPtr->threads.find(u->id);
            u->schedPtr->threads.erase(it_);
            // printf("\033[32m客户端 %d 的fd到socket的映射已经删除！\033[0m\n", u->sock_fd);
            return;
        }
        memset(bufferPrint, 0, sizeof(bufferPrint));
        memcpy(bufferPrint, buffer, n_read);
        memset(buffer, 0, sizeof(buffer));
        printf("\033[32msocket %d 收到消息:%s \033[0m\n", u->sock_fd, bufferPrint);
        // printf("1\n");
        write(u->sock_fd, bufferPrint, n_read);
        // usleep(1000000);
        addTimer(u, 1000000);
        // printf("2\n");
        uthread_suspend(u);
        // printf("3\n");
    }
}

void EpollServer::createThread(int id, schedule_t& schedule)
{
    schedule.mutex->lock();
    schedule.id = id;
    schedule.epServer = this;
    if (pthread_create(&(schedule.threadHandle), NULL, coThreadScheduler, &schedule))
    {
        fprintf(stderr, "\033[1;31m***Cothread %d creation failed***\033[0m\n", id);
        std::cout<<strerror(errno)<<std::endl;
        exit(EXIT_FAILURE);
    }
    printf("\033[1;32m协程 %d 线程创建完毕\033[0m\n", id);
    schedule.mutex->unlock();
}

void EpollServer::uthread_suspend(uthread_t* t)
{
    printf("Suspending: %d, sock %d\n", t->id, t->sock_fd);
    schedule_t &schedule = *(t->schedPtr);
    // printf("yielding scheduler %d\n", t->schid);
    schedule.mutex->lock();

    t->state = RUNNABLE;
    schedule.running_thread = -1;
    t->usedTime += getMicroseconds() - t->prevTime;
    schedule.threadPool.push(t->id); // 重新加入回队列
    // printf("pushing %d\n", t->id);

    schedule.mutex->unlock();
    t->state = SUSPEND;

    swapcontext(&(t->ctx), &(schedule.main));
}