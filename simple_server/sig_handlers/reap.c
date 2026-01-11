/* Simple signal handler to reap children in the event a child process dies.
 * Kill the zombies
 */

#include <stdlib.h>
#include <sys/wait.h>

// void reap(){
//     wait(NULL);
// }

void reap() {
    while (waitpid(-1, NULL, WNOHANG) > 0);
}
