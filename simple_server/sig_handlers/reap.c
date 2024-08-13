/* Simple signal handler to reap children in the event a child process dies.
 * Kill the zombies
 */

#include <stdlib.h>
#include <sys/wait.h>

void reap(){
    wait(NULL);
}
