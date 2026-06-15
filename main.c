#include <sys/select.h>
#include <sys/signal.h>
#include <sys/wait.h>

#include <errno.h>
#include <magic.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "./flags/flags.h"
#include "./sockets/socket.h"

#define SLEEP 5

/* load global flags variables */
extern uint32_t app_flags;

/**
 * @brief Configure SIGCHLD disposition to auto-reap children
 *
 * Sets SIGCHLD to SIG_IGN so no handler function runs on child exit, and
 * sets the SA_NOCLDWAIT flag, which makes the kernel responsible for reaping
 * terminated child processes. Because no handler runs, the parent's main
 * loop is never interrupted by per-child-exit signals, which previously
 * degraded throughput under load.
 *
 * @note Child exit status is discarded (not collected via wait())
 * @note Exits program on failure to install the disposition
 */
void
setup_sigchld_handler()
{
	struct sigaction sa;
	sa.sa_handler = SIG_IGN;
	sigemptyset(&sa.sa_mask);

	sa.sa_flags = SA_NOCLDWAIT;
	if (sigaction(SIGCHLD, &sa, NULL) == -1) {
		perror("sigaction");
		exit(EXIT_FAILURE);
	}
}

/**
 * @brief Main entry point for the FreeBSD web server
 *
 * Initializes the HTTP server with dual-stack IPv4/IPv6 support using a
 * select()-based event loop. The server operates in fork-per-connection mode,
 * creating a new child process for each incoming connection.
 *
 * The server supports optional daemon mode and comprehensive logging. It uses
 * libmagic for MIME type detection and handles SIGCHLD signals to prevent
 * zombie processes.
 *
 * @param[in] argc Number of command line arguments
 * @param[in] argv Array of command line argument strings
 *
 * @retval 0 Normal termination (never reached in practice)
 * @retval EXIT_FAILURE Fatal error during initialization
 *
 * @note The main loop runs indefinitely until the process is terminated
 * @note Requires root privileges for ports below 1024
 * @note Creates both IPv4 and IPv6 sockets regardless of availability
 *
 * @see setFlags(), createSocket_v4(), createSocket_v6(), handleSocket()
 */
int
main(int argc, char *argv[])
{
	int sock_v4;
	int sock_v6;

	if (setFlags(argc, argv) < 0) {
		printf("incorrect flags\nWrite some nice message here\n");
	}

	setup_sigchld_handler();

	if (!(app_flags & D_FLAG)) {
		/*
		 * We are setting nochdir to -1 so that the daemon runs in the
		 * current working directory. Otherwise entering the correct url
		 * for a page is a pain.
		 *
		 * nullfd is set to 1 to direct stdin, stdout and stderr output
		 * do /dev/null
		 */
		daemon(-1, 0);
	}

	sock_v4 = createSocket_v4();
	sock_v6 = createSocket_v6();

	printf("Simple Server %d\n", getpid());

	/* need to create a block for select(2) to check the two sockets and see
	 if they're ready */

	/* initialize magic */
	magic_t magic = magic_open(MAGIC_MIME_TYPE);
	if (magic_load(magic, NULL) != 0) {
		magic_close(magic);

		return (EXIT_FAILURE);
	}

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
			handleSocket(sock_v4, TYPE_SOCK_V4, magic);
		}

		if (FD_ISSET(sock_v6, &ready)) {
			handleSocket(sock_v6, TYPE_SOCK_V6, magic);
		}
	}

	return (0);
}
