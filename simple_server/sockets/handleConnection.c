#include <stdio.h>
#include <arpa/inet.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

#include "./socket.h"


void handleConnection(int fd, union sockaddr_union *client, enum sockType sockType)
{
    const char *rip;
    int rval;
    char claddr[INET6_ADDRSTRLEN];

    memset(claddr, 0, INET6_ADDRSTRLEN);

    if(sockType == TYPE_SOCK_V4){

        if((rip = inet_ntop(PF_INET, &client->client_v4.sin_addr, claddr, INET_ADDRSTRLEN)) == NULL){
            perror("inet_net");
            rip = "Unknown";
        }

    } else if(sockType == TYPE_SOCK_V6){
        if((rip = inet_ntop(PF_INET6, &client->client_v6.sin6_addr, claddr, INET6_ADDRSTRLEN)) == NULL){
            perror("inet_net");
            rip = "Unknown";
        }

    }

    do{
        char buf[BUFSIZ];
        memset(&buf, 0, BUFSIZ);

        if((rval = read(fd, buf, BUFSIZ)) < 0){
            perror("reading stream message");
            //return -1;
        } else if (rval == 0){
            printf("Ending connection from %s\n", rip);
        } else{
            printf("Client %s sent:\n\n%s", rip, buf);
        }
    } while( rval != 0);

    (void)close(fd);

    //if we exit this process, it should kill the child process
    exit(EXIT_SUCCESS);
}
