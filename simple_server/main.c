#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/select.h>
#include <sys/wait.h>

#include <unistd.h>

#include "./flags/flags.h"
#include "./sig_handlers/reap.h"
#include "./sockets/socket.h"

#define SLEEP 5

// initialize global flags variable
uint32_t app_flags = 0;
uint32_t port_addr = 8080;

void sigchld_handler(int sig) {
  // Reap all terminated child processes
  while (waitpid(-1, NULL, WNOHANG) > 0)
    ;
}

void setup_sigchld_handler() {
  struct sigaction sa;
  sa.sa_handler = sigchld_handler;
  sigemptyset(&sa.sa_mask);
  sa.sa_flags = SA_RESTART; // Automatically restart interrupted system calls
  if (sigaction(SIGCHLD, &sa, NULL) == -1) {
    perror("sigaction");
    exit(EXIT_FAILURE);
  }
}

int main(int argc, char *argv[]) {

  if (setFlags(argc, argv) < 0) {
    printf("incorrect flags\nWrite some nice message here\n");
  }

  int sock_v4;
  int sock_v6;

  if (signal(SIGCHLD, reap) == SIG_ERR) {
    perror("SIGCHLD reap");
    exit(EXIT_FAILURE);
  }

  setup_sigchld_handler();

  printf("Simple Server %d\n", getpid());

  if(!(app_flags & D_FLAG)){
      /*
       * We are setting nochdir to -1 so that the daemon runs in the current working directory.
       * Otherwise entering the correct url for a page is a pain.
       *
       * nullfd is set to 1 to direct stdin, stdout and stderr output do /dev/null
       */
        daemon(-1, 0);
  }

  sock_v4 = createSocket_v4();
  sock_v6 = createSocket_v6();

    printf("Simple Server %d\n", getpid());

  // need to create a block for select(2) to check the two sockets and see if
  // they're ready

  for (;;) {

    fd_set ready;
    struct timeval to;

    FD_ZERO(&ready);
    FD_SET(sock_v4, &ready);
    FD_SET(sock_v6, &ready);

    to.tv_sec = SLEEP;
    to.tv_usec = 0;

    if (select(sock_v6 + 1, &ready, 0, 0, &to) < 0) {
      if (errno != EINTR) {
        perror("select");
      }
      continue;
    }
    if (FD_ISSET(sock_v4, &ready)) {
      handleSocket(sock_v4, TYPE_SOCK_V4);
    } else if (FD_ISSET(sock_v6, &ready)) {
      handleSocket(sock_v6, TYPE_SOCK_V6);
    }
  }

  return 0;
}
