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
        }
        else
        {
            printf("\033[1;32m回收携程线程 %d 完成！\033[0m\n", scheduler_attrs[i].id);
        }
    }
    pthread_cancel(epollThread);
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

    // 网络部分
    // 创建socket
    server_fd = socket(AF_INET, SOCK_STREAM|SOCK_NONBLOCK, 0);
    if(server_fd<0)
    {
        fprintf(stderr, "\033[1;31m***创建server socket失败!***\033[0m\n");
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
        exit(EXIT_FAILURE);
    }

    // 监听端口
    if (listen(server_fd, SOMAXCONN) == -1)
    {
        fprintf(stderr, "\033[1;31m***listen端口失败!***\033[0m\n");
        exit(EXIT_FAILURE);
    }

    // 创建 epoll 实例
    epoll_fd = epoll_create1(0);
    if (epoll_fd == -1) {
        fprintf(stderr, "\033[1;31m***创建epoll失败!***\033[0m\n");
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
        exit(EXIT_FAILURE);
    }
    if(pthread_create(&(epollThread), NULL, handleEpoll, this))
    {
        fprintf(stderr, "\033[1;31m***创建epoll handler 线程失败***\033[0m\n");
        exit(EXIT_FAILURE);
    }
}

// 设置处理用户Ctrl C的析构函数
void EpollServer:: handleSigINT(EpollServer* t, int sig)
{
    for (int i = 0; i < t->scheduler_attrs.size(); ++i)
    {
        t->scheduler_attrs[i].mutex->lock();
        printf("\033[1;31m关闭携程 %d 中...\033[0m\n", t->scheduler_attrs[i].id);
        t->scheduler_attrs[i].stopFlag = 1; // 使得携程停止
        t->scheduler_attrs[i].mutex->unlock();
        if (pthread_join(t->scheduler_attrs[i].threadHandle, NULL))
        {
            fprintf(stderr, "\033[1;31m***回收携程线程 %d 失败!***\033[0m\n", t->scheduler_attrs[i].id);
        }
        else
        {
            printf("\033[1;32m回收携程线程 %d 完成！\033[0m\n", t->scheduler_attrs[i].id);
        }
    }
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
    printf("\033[1;32mhandle epoll 启动！\033[0m\n");
    // 主循环
    while (1) {
        // printf(".");
        n_events = epoll_wait(ep->epoll_fd, events, 10, -1);
        if (n_events == -1) {
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
                ep->createUthread(ep->scheduler_attrs[ep->threadToPut], socketEcho, 1, client_fd);
                ++ep->threadToPut;ep->threadToPut%=ep->threadCnt;
            }
            else
            {
                // if(ep->fdToUthread.find(events[i].data.fd)==ep->fdToUthread.end())continue;
                // printf("\033[1;32m收到socket%d的信息！\033[0m\n", events[i].data.fd);
                // 允许对应的协程上处理机执行
                if(ep->fdToUthread[events[i].data.fd]!=NULL)ep->fdToUthread[events[i].data.fd]->state = RUNNABLE;
            }
        }
    }
}

void EpollServer::createUthread(schedule_t &schedule, void(*func)(std::shared_ptr<uthread_t>), unsigned long long priority, int client_sock)
{
    printf("\033[1;32m 检测到客户端连接，在创建协程！\033[0m\n");
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
    t->schid = schedule.id;

    // 此处需要等到信号触发
    t->state = SUSPEND;
    t->func = func;
    // 保存client的socket地址
    t->sock_fd = client_sock;
    t->priority = priority;
    t->schedPtr = &(schedule);
    t->epoll_fd = epoll_fd;
    t->epServer = this;

    getcontext(&(t->ctx));

    t->ctx.uc_stack.ss_sp = t->stack;
    t->ctx.uc_stack.ss_size = DEFAULT_STACK_SZIE;
    t->ctx.uc_stack.ss_flags = 0;
    t->ctx.uc_link = &(schedule.main);
    t->usedTime = 0;
    t->prevTime = getMicroseconds();

    fdToUthread[client_sock] = t;

    makecontext(&(t->ctx), (void (*)(void))(socketEcho), 1, t);
    schedule.threadPool.push(id);
    schedule.mutex->unlock();
    printf("\033[32m协程 %d 创建coThread完毕！\033[0m\n", client_sock);
}

void EpollServer::socketEcho(std::shared_ptr<uthread_t> u)
{
    char buffer[256] = {0};
    char bufferPrint[256] = {0};
    // 记录用于连接的epoll事件
    struct epoll_event event;
    while(1)
    {
        ssize_t n_read = read(u->sock_fd, buffer, sizeof(buffer));
        printf("读到%d\n", n_read);
        if (n_read == -1) 
        {
            fprintf(stderr, "\033[1;31m***客户端Socket %d 读取失败***\033[0m\n", u->sock_fd);
            exit(EXIT_FAILURE);
        }
        if (n_read == 0)
        {
            printf("\033[32m客户端 %d 连接已断开！\033[0m\n", u->sock_fd);
            close(u->sock_fd);
            // event.events = EPOLLIN;
            event.data.fd = u->sock_fd;
            epoll_ctl(u->epoll_fd, EPOLL_CTL_DEL, u->sock_fd, &event);
            // 删除对应的fd映射
            ((EpollServer*)(u->epServer))->fdToUthread[u->sock_fd] = NULL;
            // printf("\033[32m客户端 %d 的fd到socket的映射已经删除！\033[0m\n", u->sock_fd);
            u->state = FREE;
            u->schedPtr->running_thread = -1;
            return;
        }
        memset(bufferPrint, 0, sizeof(bufferPrint));
        memcpy(bufferPrint, buffer, n_read);
        memset(buffer, 0, sizeof(buffer));
        printf("\033[32msocket %d 收到消息:%s \033[0m\n", u->sock_fd, bufferPrint);
        // printf("1\n");
        write(u->sock_fd, bufferPrint, n_read);
        usleep(2000000);
        // printf("2\n");
        uthread_suspend(u);
        // printf("3\n");
    }
}

void EpollServer::createThread(int id, schedule_t& schedule)
{
    schedule.id = id;
    if (pthread_create(&(schedule.threadHandle), NULL, coThreadScheduler, &schedule))
    {
        fprintf(stderr, "\033[1;31m***Cothread %d creation failed***\033[0m\n", id);
        exit(EXIT_FAILURE);
    }
    printf("\033[1;32m协程 %d 线程创建完毕\033[0m\n", id);
}

void EpollServer::uthread_suspend(std::shared_ptr<uthread_t>& t)
{
    puts("Suspending...\n");
    schedule_t &schedule = *(t->schedPtr);
    printf("yielding scheduler %d\n", t->schid);
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