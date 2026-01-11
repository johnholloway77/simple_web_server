#ifndef SOCKET_H
#define SOCKET_H

#include <netinet/in.h>

enum sockType { TYPE_SOCK_V4, TYPE_SOCK_V6 };

union sockaddr_union {
  struct sockaddr_in client_v4;
  struct sockaddr_in6 client_v6;
};

int createSocket_v4(void);

int createSocket_v6(void);

void handleSocket(int sock, enum sockType sockType);

void handleConnection(int fd, union sockaddr_union *client,
                      enum sockType sockType);

#endif // SOCKET_H
