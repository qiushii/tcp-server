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

#define MAX_BUFFLNE     4096
#define MAX_EPOLLSIZE   1024

#define CB_NOSET        0
#define CB_READ         1
#define CB_WRITE        2
#define CB_ACCEPT       3

typedef int (*NCALLBACK)(int fd, int event, void *arg);

struct nitem{
    int fd;
    
    int status;
    int events;
    void *arg;

    NCALLBACK readcb;
    NCALLBACK writecb;
    NCALLBACK acceptcb;

    unsigned char sbuffer[MAX_EPOLLSIZE];
    int slength;

    unsigned char rbuffer[MAX_EPOLLSIZE];
    int rlength;
};

struct itemblock{
    struct itemblock *next;
    struct nitem *items;
};

struct reactor{
    int epfd;
    struct itemblock *head;
    //tail
};

struct reactor *instance = NULL;

int read_callback(int fd, int event, void *arg);

int init_reactor(struct reactor *r)
{
    r->epfd = epoll_create(1);
    if(r->epfd < 0)
    {
        printf("epoll_create error\n");
        close(r->epfd);
        return -1;
    }

    r->head = (struct itemblock*)malloc(sizeof(struct itemblock));
    if(r->head == NULL)
    {
        printf("r->head malloc error\n");
        free(r->head);
        return -1;
    }
    memset(r->head, 0, sizeof(r->head));

    r->head->items = (struct nitem*)malloc(sizeof(struct nitem) * MAX_EPOLLSIZE);
    if(r->head->items == NULL)
    {
        printf("r->head->items malloc error\n");
        free(r->head->items);
        return -1;
    }
    memset(r->head->items, 0, sizeof(r->head->items));

    return 0;
}

struct reactor* getinstance(void)
{
    if(instance == NULL)
    {
        instance = (struct reactor*)malloc(sizeof(struct reactor));
        if(instance == NULL )
        {
            printf("instance malloc error\n");
            return NULL;
        }
        memset(instance, 0, sizeof(struct reactor));
        if(0 > init_reactor(instance))
        {
            free(instance);
            return NULL;
        }
    }
    return instance;
}

int init_server(int argc, char **argv)
{
    if(argc < 1)
    {
        printf("./input port please\n");
        exit(-1);
    }
    int listenfd;
    struct sockaddr_in servaddr;
    char buff[MAX_BUFFLNE];
 
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

    return listenfd;
}

int nreactor_set_event(int fd, NCALLBACK cb, int event, void *arg)
{
    struct reactor* r = getinstance();
    r->head->items[fd].fd       = fd;
    r->head->items[fd].arg      = arg;
    
    struct epoll_event ev = {0};

    if(event == CB_READ)
    {
        r->head->items[fd].readcb = cb;
        ev.events = EPOLLIN;
    }
    if(event == CB_WRITE)
    {
        r->head->items[fd].writecb = cb;
        ev.events = EPOLLOUT;
    }
    if(event == CB_ACCEPT)
    {
        r->head->items[fd].acceptcb = cb;
        ev.events = EPOLLIN;
    }
    ev.data.ptr = &(r->head->items[fd]);
    
    if(r->head->items[fd].events == CB_NOSET)
    {
        if(epoll_ctl(r->epfd, EPOLL_CTL_ADD, fd, &ev) < 0)
        {
            printf("epoll_ctl EPOLL_CTL_ADD error, %d\n",errno);
            return -1;
        }
        r->head->items[fd].events = event;
    }
    else if(r->head->items[fd].events != event)
    {
        if(epoll_ctl(r->epfd, EPOLL_CTL_MOD, fd, &ev) < 0)
        {
            printf("epoll_ctl EPOLL_CTL_MOD error\n");
            return -1;
        }
        r->head->items[fd].events = event;
    }
    return 0;
}

int nreactor_del_event(int fd, NCALLBACK cb, int event, void *arg)
{
    struct reactor *r = getinstance();

    struct epoll_event ev = {0};
    r->head->items[fd].arg = arg;
    ev.data.ptr = &(r->head->items[fd]);
    
    if(epoll_ctl(r->epfd, EPOLL_CTL_DEL, fd, &ev) < 0)
    {
        printf("epoll_ctl EPOLL_CTL_DEL error, %d\n", errno);
        return -1;
    }
    r->head->items[fd].events = CB_NOSET;
    return 0;
}

int write_callback(int fd, int event, void *arg)
{
    struct reactor *r = getinstance();
    unsigned char *stackbuffer = r->head->items[fd].sbuffer;
    int slen = r->head->items[fd].slength;

    int ret = send(fd, stackbuffer, slen, 0);
    if(ret < 0)
    {
        printf("send error\n");
        return -1;
    }
    if(ret < slen)
    {
        nreactor_set_event(fd, write_callback, CB_WRITE, 0);
    }
    else
    {
        nreactor_set_event(fd, read_callback, CB_READ, 0);
    }
    return 0;
}

int read_callback(int fd, int event, void *arg)
{
    struct reactor *r = getinstance();
    unsigned char *stackbuffer = r->head->items[fd].rbuffer;

    int ret = recv(fd, stackbuffer, MAX_BUFFLNE, 0);
    if(ret < 0)
    {
        printf("recv satckbuffer error\n");
        return -1;
    }
    else if(ret == 0)
    {
        printf("sockfd[%d] closed.\n", fd);
        nreactor_del_event(fd, NULL, 0, NULL);
        close(fd);
        return 0;
    }
    else if(ret > 0)
    {
        memcpy(r->head->items[fd].rbuffer, stackbuffer, ret);
        r->head->items[fd].rlength = ret;
        printf("client[%d]:%s\n", fd, stackbuffer);
        nreactor_set_event(fd, write_callback, CB_WRITE, NULL);
        return 0;
    }
}

int accept_callback(int fd, int event, void *arg)
{
    struct reactor *r = getinstance();
    int cfd;
    struct sockaddr_in caddr;
    char *clientip;
    socklen_t len = sizeof(caddr);
    cfd = accept(fd, (struct sockaddr*)&caddr, &len);
    if( cfd < 0 )
    {
        printf("accept error\n");
        close(cfd);
        return -1;
    }
    printf("accept client[%d],ip:%s,port:%d\n",
                                cfd, inet_ntop(AF_INET, &caddr.sin_addr.s_addr, clientip, sizeof(clientip)),
                                ntohs(caddr.sin_port));

    nreactor_set_event(cfd, read_callback, CB_READ, NULL);
    return 0;
}

int reactor_loop(int listenfd)
{
    struct reactor *r = getinstance();
    struct epoll_event ev[MAX_EPOLLSIZE] = {0};
    int Numready;
    while(1)
    {
        Numready = epoll_wait(r->epfd, ev, MAX_EPOLLSIZE, 10);
        if(Numready < 0)
        {
            printf("epoll_wait error, %s, %d\n",strerror(errno), errno);
            return -1;
        }
        
        for(int i=0; i<Numready; i++)
        {
            struct nitem *items = (struct nitem*)ev[i].data.ptr;
            int connfd = items->fd;

            if(connfd == listenfd)
            {
                items->acceptcb(listenfd, 0, NULL);
            }
            else
            {
                if(ev[i].events & POLLIN)
                    items->readcb(connfd, 0, NULL);
                else if(ev[i].events & POLLOUT)
                    items->writecb(connfd, 0, NULL);
            }   
        }
    }
    return 0;
}

int main(int argc, char **argv) 
{
    int listenfd = init_server(argc, argv);

    nreactor_set_event(listenfd, accept_callback, CB_ACCEPT, NULL);

    reactor_loop(listenfd);

    close(listenfd);
    return 0;
}

