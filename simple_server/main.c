#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <sys/select.h>

#include "./sig_handlers/reap.h"
#include "./sockets/socket.h"

#define SLEEP 5


int main(int argc, char* argv[])
{

    int sock_v4;
    int sock_v6;

    if(signal(SIGCHLD, reap) == SIG_ERR){
        perror("SIGCHLD reap");
        exit(EXIT_FAILURE);
    }

    sock_v4 = createSocket_v4();
    sock_v6 = createSocket_v6();

    //need to create a block for select(2) to check the two sockets and see if they're ready

    for (;;){

        fd_set ready;
        struct timeval to;

        FD_ZERO(&ready);
        FD_SET(sock_v4, &ready);
        FD_SET(sock_v6, &ready);

        to.tv_sec = SLEEP;
        to.tv_usec = 0;

        if(select(sock_v6 + 1, &ready, 0, 0, &to) < 0){
            if(errno != EINTR){
                perror("select");
            }
            continue;
        }
        if(FD_ISSET(sock_v4, &ready)){
            handleSocket(sock_v4, TYPE_SOCK_V4);
        } else if(FD_ISSET(sock_v6, &ready)){
            handleSocket(sock_v6, TYPE_SOCK_V6);
        } else {
            (void)printf("Waiting for either IPv4 or IPv6 connections...\n");
        }

    }


    return 0;
}
