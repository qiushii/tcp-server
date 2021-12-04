#include <errno.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>
#include <pthread.h> 
#include <arpa/inet.h> 
#include <sys/select.h>
#include <sys/time.h>
#include <poll.h>
#include <sys/epoll.h>

#define MAXLNE  4096
#define POLLSIZE 1024

void* tfun(void* arg)
{
    int connfd = *(int*)arg;
    char buff[MAXLNE];
    while(1)
    {
        int n = recv(connfd, buff, MAXLNE, 0);
        if (n > 0) 
        {
            buff[n] = '\0';
            printf("recv msg from client: %s\n", buff);
	    } 
        /*else if (n == 0) 
        {
            close(connfd);
            break;
        }*/
    }
    pthread_exit((void*)1);
}

int main(int argc, char **argv) 
{
    int listenfd, connfd, n;
    struct sockaddr_in servaddr;
    char buff[MAXLNE];
 
    if ((listenfd = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
        printf("create socket error: %s(errno: %d)\n", strerror(errno), errno);
        return 0;
    }
 
    memset(&servaddr, 0, sizeof(servaddr));
    servaddr.sin_family = AF_INET;
    servaddr.sin_addr.s_addr = htonl(INADDR_ANY);
    servaddr.sin_port = htons(atoi(argv[1]));
 
    if (bind(listenfd, (struct sockaddr *)&servaddr, sizeof(servaddr)) == -1) {
        printf("bind socket error: %s(errno: %d)\n", strerror(errno), errno);
        return 0;
    }
 
    if (listen(listenfd, 10) == -1) {
        printf("listen socket error: %s(errno: %d)\n", strerror(errno), errno);
        return 0;
    }
    printf("listen...\n");

#if 0   //单线程
    struct sockaddr_in client;
    menset(&client, 0, sizeof(client));
    socklen_t len = sizeof(client);
    if ((connfd = accept(listenfd, (struct sockaddr *)&client, &len)) == -1) {
        printf("accept socket error: %s(errno: %d)\n", strerror(errno), errno);
        return 0;
    }

    printf("========waiting for client's request========\n");
    while (1) {

        n = recv(connfd, buff, MAXLNE, 0);
        if (n > 0) {
            buff[n] = '\0';
            printf("recv msg from client: %s\n", buff);

	    send(connfd, buff, n, 0);
        } else if (n == 0) {
            close(connfd);
        }
        
        //close(connfd);
    }

#elif 0     //多线程
    pthread_t tid;
    struct sockaddr_in clientsock;
    socklen_t len = sizeof(clientsock);
    char clie_IP[BUFSIZ];
    int i=0;
    int thread_connfd[128];
    while(1)
    {
        if((thread_connfd[i] = accept(listenfd, (struct sockaddr*)&clientsock, &len)) != -1)
        {
            printf("------client ip:%s, port:%d---:", inet_ntop(AF_INET, &clientsock.sin_addr.s_addr,
                                                                clie_IP, sizeof(clie_IP)), ntohs(clientsock.sin_port));
            pthread_create(&tid, NULL, (void*)tfun, (void*)&thread_connfd[i]);
            pthread_detach(tid);
            i++;
        }
    }

#elif 0     //select实现
    fd_set rset, rfds;
    FD_ZERO(&rset);
    FD_SET(listenfd, &rset);

    char clie_IP[BUFSIZ];
    int Numready;
    int maxfd = listenfd;
    while(1)
    {
        rfds = rset;
        Numready = select(maxfd+1, &rfds, NULL, NULL, NULL);
        struct sockaddr_in sockclient;
        socklen_t len = sizeof(sockclient);
        if(FD_ISSET(listenfd, &rfds))
        {
            if((connfd = accept(listenfd,(struct sockaddr*)&sockclient, &len)) == -1)
            {
                printf("accept error\n");
                exit(1);
            }
            printf("------connfd:%d,client ip:%s, port:%d---:\n", connfd,inet_ntop(AF_INET, &sockclient.sin_addr.s_addr,
                                                                    clie_IP, sizeof(clie_IP)), ntohs(sockclient.sin_port));
            FD_SET(connfd, &rset);
            if(connfd > maxfd)maxfd = connfd;
            if(--Numready == 0)continue;
        }
        for(int i = listenfd; i<=maxfd; i++)
        {
            if(FD_ISSET(i, &rfds))
            {
                n = recv(i, buff, sizeof(buff), 0);
                if(n>0)
                {
                    buff[n] = '\0';
                printf("msg from client[%d]%s\n", i,buff);
                }
                else if(n == 0)
                {
                    FD_CLR(i, &rfds);
                    printf("%d disconnected\n", i);
                    close(i);
                }
                if(--Numready == 0)break;
            }   
        }
    }
#elif 0   //poll实现
    struct pollfd pollarrfd[POLLSIZE] = {0};
    int maxfd = listenfd;
    char clie_IP[BUFSIZ];
    pollarrfd[0].fd = listenfd;
    pollarrfd[0].events = POLLIN;
    struct sockaddr_in sockclient;
    socklen_t len = sizeof(sockclient);
    while(1)
    {
        int Numready = poll(pollarrfd, maxfd+1, -1);
        if(Numready < 0)
        {
            printf("poll error\n");
            exit(1);
        }
        if(pollarrfd[0].revents & POLLIN)
        {
            connfd = accept(listenfd, (struct sockaddr*)&sockclient, &len);
            if(connfd < 0)
            {
                printf("accept error\n");
                exit(1);
            }
            printf("accept\n");
            printf("------connfd:%d,client ip:%s, port:%d---:\n", connfd, inet_ntop(AF_INET, &sockclient.sin_addr.s_addr,
                                                                        clie_IP, sizeof(clie_IP)), ntohs(sockclient.sin_port));
            pollarrfd[connfd].fd = connfd;
            pollarrfd[connfd].events = POLLIN;
            if(connfd > maxfd)maxfd = connfd;
            if(--Numready == 0)continue;
        }
        for(int i = 1; i<=maxfd; i++)
        {
            if(pollarrfd[i].revents & POLLIN)
            {
                n = recv(i, buff, sizeof(buff), 0);
                if(n<0)
                {
                    printf("recv error\n");
                    exit(1);
                }
                if(n>0)
                {
                    buff[n] = '\0';
                    printf("msg from client[%d]:%s\n", i, buff);
                }
                if(n==0)
                {
                    close(i);
                    printf("socket[%d] close\n", i);
                }
            }
        }
    }
    
#else   //epoll实现
    int Numready;
    char clie_IP[BUFSIZ];
    struct sockaddr_in sockclient;
    socklen_t len = sizeof(sockclient);
    int epollfd = epoll_create(1);
    if(epollfd < 0 )
    {
        printf("epoll_create error\n");
        exit(1);
    }
    struct epoll_event event[POLLSIZE];
    struct epoll_event ev;

    ev.events = EPOLLIN;
    ev.data.fd = listenfd;

    int ret = epoll_ctl(epollfd, EPOLL_CTL_ADD, listenfd, &ev);
    if(ret < 0)
    {
        printf("epoll_ctl error\n");
        exit(1);
    }
    while(1)
    {
        Numready = epoll_wait(epollfd, event, POLLSIZE, -1);
        if(Numready < 0)
        {
            printf("epoll_wait error\n");
            exit(1);
        }
        for(int i=0; i<Numready; i++)
        {
            if(event[i].data.fd == listenfd)
            {
                connfd = accept(listenfd, (struct sockaddr*)&sockclient, &len);
                if(connfd < 0)
                {
                    printf("accept error\n");
                    exit(1);
                }
                printf("------connfd:%d,client ip:%s, port:%d---:\n", connfd, inet_ntop(AF_INET, &sockclient.sin_addr.s_addr,
                                                                        clie_IP, sizeof(clie_IP)), ntohs(sockclient.sin_port));
                
                event[i].data.fd = connfd;
                event[i].events = EPOLLIN;
                ret = epoll_ctl(epollfd, EPOLL_CTL_ADD, connfd, event);
                if(ret < 0)
                {
                    printf("epoll_ctl_add error\n");
                    exit(1);
                }
                continue;
            }
            else if(event[i].events & EPOLLIN)
            {
                int clientfd = event[i].data.fd;
                n = recv(clientfd, buff, sizeof(buff), 0);
                if(n < 0 )
                {
                    printf("recv error\n");
                    exit(1);
                }
                if(n>0)
                {
                    buff[n] = '\0';
                    printf("msg from client[%d]:%s\n", clientfd, buff);
                }
                if(n==0)
                {
                    close(clientfd);
                    printf("client[%d] close\n", clientfd);
                }
            }  
        }
    }

#endif
 
    close(listenfd);
    return 0;
}




