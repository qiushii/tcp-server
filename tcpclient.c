/*************************************************************************
	> File Name: tcpclient.c
	> Author: 
	> Mail: 
	> Created Time: 2021年12月01日 星期三 04时34分27秒
 ************************************************************************/

#include<stdio.h>
#include<unistd.h>
#include<arpa/inet.h>
#include<sys/socket.h>
#include<string.h>
#include<stdlib.h>

int main(int argc, char ** argv)
{
    if(argc < 2)
    {
        printf("please input ip and port\n");
        exit(1);
    }
    int serverfd;
    serverfd = socket(AF_INET, SOCK_STREAM, 0);
    if(serverfd < 0)
    {
        printf("socket error\n");
        exit(1);
    }
    struct sockaddr_in serv_addr;
    bzero(&serv_addr, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(atoi(argv[2]));
    int ret = inet_pton(AF_INET, argv[1], &serv_addr.sin_addr.s_addr);
    if(ret < 0)
    {
        printf("inet_pton error\n");
        exit(1);
    }
    ret = connect(serverfd, (struct sockaddr*)&serv_addr, sizeof(serv_addr));
    if(ret < 0 )
    {
        printf("connect error\n");
        exit(1);
    } 
    char buf[1024];
    while(1)
    {
        fgets(buf, sizeof(buf), stdin);
        write(serverfd, buf, strlen(buf));
    }
    close(serverfd);
    return 1;
}
