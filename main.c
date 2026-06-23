#include <poll.h>
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
#include "client_conn/connections.h"
#include "requests/request2.h"

#define INITIAL_SIZE 32
#define N_LISTENERS 2

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
	setup_sigchld_handler();

	if (setFlags(argc, argv) < 0) {
		printf("incorrect flags\nWrite some nice message here\n");
	}
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

	/* initialize magic */
	magic_t magic = magic_open(MAGIC_MIME_TYPE);
	if (magic_load(magic, NULL) != 0) {
		magic_close(magic);

		return (EXIT_FAILURE);
	}

	printf("Simple Server %d\n", getpid());

	int listener_v4 = get_listener_v4();
	int listener_v6 = get_listener_v6();

	int fd_size = INITIAL_SIZE;
	int fd_count = 0;

	struct pollfd *pfds = malloc(sizeof(*pfds) * fd_size);
	Client *clients = malloc(sizeof(Client) * fd_size);
	if (!pfds || !clients) {
		perror("malloc");
		exit(EXIT_FAILURE);
	}

	pfds[0].fd = listener_v4;
	pfds[0].events = POLLIN;
	pfds[0].revents = 0;
	pfds[1].fd = listener_v6;
	pfds[1].events = POLLIN;
	pfds[1].revents = 0;

	// Numbers 0 and 1 are reserved for listeners
	// Therefore, clients[0] and clients[1] are dummies
	fd_count = 2;

	// Let's get this baby spinnin!
	for (;;) {
		int poll_count = poll(pfds, fd_count, -1);
		if (-1 == poll_count) {
			if (errno == EINTR) {
				continue;
			}
			perror("poll");
			exit(EXIT_FAILURE);
		}

		for (int i = 0; i < fd_count; i++) {
			if (i < N_LISTENERS) {
				if (pfds[i].revents & POLLIN) {
					int listener = 0 == i ? listener_v4
							      : listener_v6;

					accept_new_conn(listener,
					    &pfds,
					    &clients,
					    &fd_count,
					    &fd_size);
				}
				continue; // We don't want to treat a listener
					  // as a client!
			}

			if (CLOSING == clients[i].state) {
				close_conn(i, &fd_count, pfds, clients);
				i--;
				continue;
			}

			short revents = pfds[i].revents;
			if (revents == 0) {
				continue;
			}

			// Check if error and close if so
			if (revents & (POLLERR | POLLHUP | POLLNVAL)) {
				close_conn(i, &fd_count, pfds, clients);
				i--; // close_con will replace current value of
				     // i, decrement to get the new value
				continue;
			}

			if ((revents & POLLIN) &&
			    (READING == clients[i].state)) {
				// handle read for new request
				do_read(i, clients);
			}

			if (revents & POLLOUT &&
			    ((SENDING_HEADER == clients[i].state) ||
				(SENDING_BODY == clients[i].state))) {
				// handle writing

				// To-do!
			}
		}
	}

	return (0);
}
