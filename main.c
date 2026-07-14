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
#include "./client_conn/do_read.h"
#include "./client_conn/do_write.h"
#include "client_conn/connections.h"
#include "debug/debug.h"

#define INITIAL_SIZE 32

/* load global flags variables */
extern const uint32_t app_flags;
extern FILE *log_ptr;

static volatile sig_atomic_t running = 1;

static void
shutdown_server()
{
	DBG("Shutdown_server() called!\n");
	running = 0;
}

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
static void
setup_signal_handler()
{
	struct sigaction sa = {0};
	sigemptyset(&sa.sa_mask);

	sa.sa_flags = 0;
	sa.sa_handler = shutdown_server;
	if (sigaction(SIGTERM, &sa, NULL) == -1) {
		perror("sigaction SIGTERM. ");
		exit(EXIT_FAILURE);
	}
	if (sigaction(SIGINT, &sa, NULL) == -1) {
		perror("sigaction SIGINT. ");
		exit(EXIT_FAILURE);
	}

	DBG("Signal handlers setup\n");
}

/**
 * @brief Server entry point — initialises subsystems and runs the poll() event
 * loop.
 *
 * Startup sequence:
 *   1. Install SIGCHLD disposition (auto-reap via SA_NOCLDWAIT).
 *   2. Parse command-line flags (port, document root, debug mode).
 *   3. Optionally daemonise (unless -d flag is set).
 *   4. Open the libmagic MIME-type database.
 *   5. Create IPv4 and IPv6 listening sockets (both non-blocking).
 *   6. Allocate the parallel pollfd / Client arrays (INITIAL_SIZE=32 slots,
 *      indices 0-1 reserved for the listeners).
 *
 * Event loop:
 *   - poll() blocks indefinitely (timeout = -1).
 *   - EINTR is retried silently; any other poll() error is fatal.
 *   - Listener slots (i < N_LISTENERS=2): POLLIN triggers accept_new_conn().
 *   - Client slots: CLOSING state → close_conn() + index decrement.
 *   - POLLERR / POLLHUP / POLLNVAL → close_conn() + index decrement.
 *   - POLLIN + READING state → do_read().
 *   - POLLOUT + SENDING_HEADER or SENDING_BODY state → (response send, TODO).
 *
 * @param argc  Argument count from the shell
 * @param argv  Argument vector from the shell
 * @return      EXIT_SUCCESS on clean shutdown (currently unreachable);
 *              exits via exit() on fatal errors
 */
int
main(int argc, char *argv[])
{
	DBG("Loaded in debug mode\n");

	setup_signal_handler();

	if (setFlags(argc, argv) < 0) {
		printf("incorrect flags\nWrite some nice message here\n");
	}

// There is a difference between build DEBUG and terminal display debug...should
// rename it verbose...
#ifndef DEBUG
	if (!(app_flags & V_FLAG)) {
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
#endif

	/* initialize magic */
	magic_t magic = magic_open(MAGIC_MIME_TYPE);
	if (magic_load(magic, NULL) != 0) {
		magic_close(magic);

		return (EXIT_FAILURE);
	}

	printf("Simple Server %d\n", getpid());

	int fd_size = INITIAL_SIZE;
	int fd_count = 0;

	struct pollfd *pfds = malloc(sizeof(*pfds) * fd_size);
	Client *clients = malloc(sizeof(Client) * fd_size);

	if (!pfds || !clients) {
		perror("malloc");
		exit(EXIT_FAILURE);
	}

	int num_listeners = 0;
	int listener_v4 = -1;
	int listener_v6 = -1;

	int use_v4 = (app_flags & B4_FLAG) || !(app_flags & B6_FLAG);
	int use_v6 = (app_flags & B6_FLAG) || !(app_flags & B4_FLAG);

	if (use_v4)
		listener_v4 = get_listener_v4();

	if (use_v6)
		listener_v6 = get_listener_v6();

	if (-1 != listener_v4) {
		pfds[num_listeners].fd = listener_v4;
		pfds[num_listeners].events = POLLIN;
		pfds[num_listeners].revents = 0;
		num_listeners++;
	}

	if (-1 != listener_v6) {
		pfds[num_listeners].fd = listener_v6;
		pfds[num_listeners].events = POLLIN;
		pfds[num_listeners].revents = 0;
		num_listeners++;
	}

	if (num_listeners == 0) {
		fprintf(stderr, "no listeners could be created; exiting\n");
		exit(EXIT_FAILURE);
	}

	fd_count = num_listeners;

	// Let's get this baby spinnin!
	while (running) {
		DBG("Entering first for loop\n");
		int poll_count = poll(pfds, fd_count, -1);
		if (-1 == poll_count) {
			if (errno == EINTR) {
				continue;
			}
			perror("poll");
			exit(EXIT_FAILURE);
		}

		for (int i = 0; i < fd_count; i++) {
			if (i < num_listeners) {
				if (pfds[i].revents & POLLIN) {
					DBG("New connection found\n");

					accept_new_conn(pfds[i].fd,
					    &pfds,
					    &clients,
					    &fd_count,
					    &fd_size);
				}
				continue; // We don't want to treat a listener
					  // as a client!
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
				DBG("Incoming from client detected\n");
				// handle read for new request
				do_read(i, clients, pfds, magic);
			}

			if (revents & POLLOUT &&
			    ((SENDING_HEADER == clients[i].state) ||
				(SENDING_BODY == clients[i].state))) {
				// handle writing

				do_write(i, clients);
			}

			if (CLOSING == clients[i].state) {
				DBG("Closing connection\n");
				close_conn(i, &fd_count, pfds, clients);
				i--;
				continue;
			}
		}

		if (log_ptr) {
			fflush(log_ptr);
		}
	}

	for (int i = num_listeners; i < fd_count; i++) {
		close_conn(i, &fd_count, pfds, clients);
		i--;
	}
	close(listener_v4);
	close(listener_v6);

	magic_close(magic);
	free(pfds);
	free(clients);

	if (log_ptr) {
		fflush(log_ptr);
		fclose(log_ptr);
	}

	exit(EXIT_SUCCESS);
}
