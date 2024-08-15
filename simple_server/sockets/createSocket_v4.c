#include <netinet/in.h>
#include <sys/socket.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define DEFAULT_PORT 8080
#define BACKLOG 5

int createSocket_v4(void)
{
    int sock_v4;
    socklen_t length;

    struct sockaddr_in server_v4;

    //create both IPv4 and IPv6 sockets...

    if((sock_v4 = socket(PF_INET, SOCK_STREAM, 0)) < 0){
        perror("opening IPv4 sock stream");
        exit(EXIT_FAILURE);
    }



    //creating socket address information for IPv4
    memset(&server_v4, 0, sizeof(server_v4));

    server_v4.sin_family = PF_INET;
    server_v4.sin_addr.s_addr = INADDR_ANY;
    server_v4.sin_port = htons(DEFAULT_PORT);

   // server_v4.sin_port = DEFAULT_PORT;
    if(bind(sock_v4, (struct sockaddr *)&server_v4, sizeof(server_v4)) != 0){
        perror("getting socket_v4 name");
        exit(EXIT_FAILURE);
    }




    length = sizeof(server_v4);
    if(getsockname(sock_v4, (struct sockaddr *)&server_v4, &length) != 0){
        perror("getting sock_v4 name");
        exit(EXIT_FAILURE);
    }

    (void)printf("Sock_v4 has port #%d\n", ntohs(server_v4.sin_port));

    if(listen(sock_v4, BACKLOG) < 0){
        perror("listen sock_v4");
        exit(EXIT_FAILURE);
    }




    return sock_v4; //Should revise this so that it only sends one socket back...
}
