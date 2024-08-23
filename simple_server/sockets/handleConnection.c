#include <arpa/inet.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

#include <time.h>

#include "../flags/flags.h"
#include "../requests/requests.h"
#include "./socket.h"

#define BUFFER_SIZE 1024

extern uint32_t app_flags;

void handleConnection(int fd, union sockaddr_union *client,
                      enum sockType sockType) {
  const char *rip;
  char claddr[INET6_ADDRSTRLEN];
  int bytes_sent = 0;
  int rval;
  int resp_status;
  time_t current_time;
  struct tm *utc_time;

  current_time = time(NULL);
  utc_time = gmtime(&current_time);
  char timestamp[21];
  strftime(timestamp, sizeof(timestamp), "%Y-%m-%dT%H:%M:%SZ", utc_time);

  memset_s(claddr, INET6_ADDRSTRLEN, 0, INET6_ADDRSTRLEN);

  if (sockType == TYPE_SOCK_V4) {

    if ((rip = inet_ntop(PF_INET, &client->client_v4.sin_addr, claddr,
                         INET_ADDRSTRLEN)) == NULL) {
      // perror("inet_net");
      rip = "Unknown";
    }

  } else if (sockType == TYPE_SOCK_V6) {
    if ((rip = inet_ntop(PF_INET6, &client->client_v6.sin6_addr, claddr,
                         INET6_ADDRSTRLEN)) == NULL) {
      // perror("inet_net");
      rip = "Unknown";
    }
  }

  char buf[BUFSIZ];
  memset_s(&buf, BUFSIZ, 0, BUFSIZ);

  rval = read(fd, buf, BUFSIZ);
  // perror("reading stream message");

  if (rval > 0) {
    // gets the first line of the request
    char *req_token = strtok(buf, "\r\n");

    FILE *file_ptr = NULL;
    char *response = parseRequest(req_token, &file_ptr, &resp_status);

    bytes_sent += send(fd, response, strlen(response), 0);

    if (file_ptr) {
      char buffer[BUFFER_SIZE];
      size_t bytes_read;
      while ((bytes_read = fread(buffer, sizeof(char), BUFFER_SIZE, file_ptr)) >
             0) {
        if (send(fd, buffer, bytes_read, 0) < 0) { /*
               perror("error sending body");*/
          break;
        }

        bytes_sent += bytes_read;
      }

      fclose(file_ptr);
    }
    if (app_flags & D_FLAG){
        fprintf(stdout, "%s %s \"%s\" %d %d\n", rip, timestamp, req_token,
                resp_status, bytes_sent);
    }


    free(response);
    (void)close(fd);
  } else {
    if (app_flags & D_FLAG) {
      fprintf(stdout, "%s %s ERROR: Unable to read http request %d\n", rip,
              timestamp, bytes_sent);
    }
  }

  exit(EXIT_SUCCESS);
}
