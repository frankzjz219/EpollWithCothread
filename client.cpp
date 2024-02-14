#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <string.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <stdlib.h>
#include <cerrno>

#define SERVER_PORT 18080
#define SERVER_IP "0.0.0.0"
int sockFd = 0;
void SIGINTHandler(int sig)
{
    close(sockFd);
    printf("\nClient stopping, already closed socket %d. \n", sockFd);
    exit(EXIT_SUCCESS);
}

int main()
{
    
    int ret;
    struct sockaddr_in sock_server_addr;
    char sendBuf[100] = "Hello\0";
    char readBuf[100] = {0};
    int send_len;

    sockFd = socket(AF_INET, SOCK_STREAM, 0);
    if(sockFd <0)
    {
        printf("Open socket Error!\n");
        exit(EXIT_FAILURE);
    }
    sock_server_addr.sin_family = AF_INET;
    sock_server_addr.sin_port = htons(SERVER_PORT);

    inet_aton(SERVER_IP, &sock_server_addr.sin_addr);
    memset(sock_server_addr.sin_zero, 0, 8);
    printf("Client started, trying to connect %s:%4d. \n", SERVER_IP, SERVER_PORT);
    ret = connect(sockFd, (struct sockaddr*)&sock_server_addr, sizeof(struct  sockaddr));
    printf("Client Connected!\n");
    if(ret<0)
    {
        printf("connect Error!\n");
        printf(strerror(errno));
        exit(EXIT_FAILURE);
    }
    while(1)
    {
        // scanf("%s", (char *)&sendBuf);
        write(sockFd, sendBuf, strlen(sendBuf)+1);
        int n = 0;
        n = read(sockFd, readBuf, 1);
        puts("Server Repeat: ");
        // printf(readBuf);
        while (readBuf[0]!=0&&n>0)
        {
            // puts(":");
            printf("%c", readBuf[0]);
            n = read(sockFd, readBuf, 1);
        }
        printf("\n");
        
        // memset(sendBuf, sizeof(sendBuf), 0);
        memset(readBuf, sizeof(readBuf), 0);
        sleep(1);
    }
}