#include <stdio.h>
#include <stdlib.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
#include "./socket.h"

void handleSocket(int sock, enum sockType sockType){

    int fd;
    pid_t pid;
    socklen_t length;

    //I'm using a union as an excuse to practice with them and learn more.
    union sockaddr_union client;

    length = (sockType == TYPE_SOCK_V4) ? sizeof(client.client_v4) : sizeof(client.client_v6);

    if((fd = accept(sock, (struct sockaddr *)(sockType == TYPE_SOCK_V4 ? (void *)&client.client_v4 : (void *)&client.client_v6), &length)) < 0){
        perror("accept");
        return;// -1;
    }

    if((pid = fork()) < 0){
        perror("fork");
        exit(EXIT_FAILURE);
    } else if(!pid){
        handleConnection(fd, &client, sockType);

    } else{
        //if parent close fd
        (void)close(fd);
    }

    //return 0;

};
