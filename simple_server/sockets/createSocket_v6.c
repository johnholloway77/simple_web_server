#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>

#include "../flags/flags.h"

#define BACKLOG 5

extern uint32_t app_flags;
extern uint32_t port_addr;

int createSocket_v6(void) {

  int sock_v6;
  socklen_t length;
  struct sockaddr_in6 server_v6;

  if ((sock_v6 = socket(PF_INET6, SOCK_STREAM, 0)) < 0) {
    perror("opneing IPv6 sock stream");
    exit(EXIT_FAILURE);
  }

  // creating socket address information for IPv6
  memset(&server_v6, 0, sizeof(server_v6));
  server_v6.sin6_family = PF_INET6;
  server_v6.sin6_addr = in6addr_any; // wtf isn't this an all caps macro?
  server_v6.sin6_port = htons(port_addr);

  if (bind(sock_v6, (struct sockaddr *)&server_v6, sizeof(server_v6)) != 0) {
    perror("binding sock_v6");
    exit(EXIT_FAILURE);
  }

  length = sizeof(server_v6);
  if (getsockname(sock_v6, (struct sockaddr *)&server_v6, &length) != 0) {
    perror("getting sock_v6 name");
    exit(EXIT_FAILURE);
  }

  if (app_flags & D_FLAG) {
    (void)printf("Sock_v6 has port #%d\n", ntohs(server_v6.sin6_port));
  }

  if (listen(sock_v6, BACKLOG) < 0) {
    perror("listen sock_v6");
    exit(EXIT_FAILURE);
  }

  return sock_v6;
}
