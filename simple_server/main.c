#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <sys/select.h>

#include "./sig_handlers/reap.h"
#include "./sockets/createSocket_v4.h"
#include "./sockets/createSocket_v6.h"

int main(int argc, char* argv[])
{

    int sock_v4;
    int sock_v6;

    if(signal(SIGCHLD, reap) == SIG_ERR){
        perror("SIGCHLD reap");
        exit(EXIT_FAILURE);
    }

    sock_v4 = createSocekt_v4();
    sock_v6 = createSocekt_v6();

    //need to create a block for select(2) to check the two sockets and see if they're ready

    return 0;
}
