#include "./connections.h"
#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/socket.h>
#include <unistd.h>

#include "../debug/debug.h"

/**
 * @brief Drain all pending connections from a listening socket.
 *
 * See connections.h for full documentation.
 */
void
accept_new_conn(int listener_fd,
    struct pollfd **pfds,
    Client **clients,
    int *fd_count,
    int *fd_size)
{
	for (;;) {
		DBG("Accepting new connection\n");
		struct sockaddr_storage remote_addr;
		socklen_t addr_len = sizeof(remote_addr);

		int newfd = accept(listener_fd,
		    (struct sockaddr *)&remote_addr,
		    &addr_len);

		if (newfd < 0) {
			if (EAGAIN == errno || EWOULDBLOCK == errno) {
				break;
			}

			if (EINTR == errno) {
				continue;
			}
			perror("newfd accept");
			break;
		}

		int flags = fcntl(newfd, F_GETFL, 0);
		if (flags < 0 ||
		    fcntl(newfd, F_SETFL, flags | O_NONBLOCK) < 0) {
			perror("fcntl newfd O_NONBLOCK");
			close(newfd);
			continue;
		}

		int idx = add_to_lists(pfds, clients, newfd, fd_count, fd_size);
		if (idx < 0) {
			/* This will never fire as add to list doesn't have
			 * error handling */
			close(newfd); // couldn't add
			continue;
		}

		const char *return_ip;

		if (remote_addr.ss_family == AF_INET) {
			struct sockaddr_in *s =
			    (struct sockaddr_in *)&remote_addr;
			return_ip = inet_ntop(AF_INET,
			    &s->sin_addr,
			    (*clients)[idx].client_addr,
			    INET6_ADDRSTRLEN);
		}
		else {
			struct sockaddr_in6 *s =
			    (struct sockaddr_in6 *)&remote_addr;
			return_ip = inet_ntop(AF_INET6,
			    &s->sin6_addr,
			    (*clients)[idx].client_addr,
			    INET6_ADDRSTRLEN);
		}

		if (!return_ip) {
			strlcpy((*clients)[idx].client_addr,
			    "Unknown IP",
			    INET6_ADDRSTRLEN);
		}
	}
}
