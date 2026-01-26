/* Simple signal handler to reap children in the event a child process dies.
 * Kill the zombies
 */

#include <stdlib.h>
#include <sys/wait.h>

/**
 * @brief Signal handler to reap terminated child processes
 *
 * Prevents zombie processes by reaping all terminated children when
 * a SIGCHLD signal is received. Uses waitpid() with WNOHANG option
 * to avoid blocking and handles multiple simultaneous child terminations.
 *
 * This is a legacy signal handler that is no longer used in the current
 * implementation. The main.c file now contains an integrated signal
 * handler (sigchld_handler) that provides the same functionality.
 *
 * @note This function is async-signal-safe
 * @note Uses while loop to handle multiple child deaths
 * @note Does not examine exit status of children
 * @note Legacy function - main.c has integrated handler
 *
 * @deprecated Use sigchld_handler() in main.c instead
 * @see sigchld_handler() in main.c
 */
void reap()
{
    while (waitpid(-1, NULL, WNOHANG) > 0)
        ;
}
