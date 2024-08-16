#include <stdio.h>
#include <arpa/inet.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

#include "./socket.h"
#include "../requests/requests.h"

#define BUFFER_SIZE 1024

void handleConnection(int fd, union sockaddr_union *client, enum sockType sockType)
{
    const char *rip;
    int rval;
    char claddr[INET6_ADDRSTRLEN];
    int bytes_sent = 0;

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

        char *req_token = strtok(buf, "\r\n");

        if(req_token){
            printf("request: %s\n", req_token);
        }

        FILE *file_ptr = NULL;
        char *response = parseRequest(req_token, &file_ptr);
        bytes_sent += send(fd, response, strlen(response), 0);

        if(file_ptr){
            char buffer[BUFFER_SIZE];
            size_t bytes_read;
            while((bytes_read = fread(buffer, sizeof(char), BUFFER_SIZE, file_ptr)) > 0){
                if(send(fd, buffer, bytes_read, 0) < 0){
                    perror("error sending body");
                    break;
                }

                bytes_sent += bytes_read;
            }

            fclose(file_ptr);
            (void)close(fd);
            exit(EXIT_SUCCESS);
        }


    (void)close(fd);
    exit(EXIT_SUCCESS);
}
