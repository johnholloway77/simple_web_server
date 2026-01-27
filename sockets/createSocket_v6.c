#include <sys/socket.h>

#include <netinet/in.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../flags/flags.h"

#define BACKLOG SOMAXCONN

extern uint32_t app_flags;
extern uint32_t port_addr;

/**
 * @brief Create and bind an IPv6 TCP socket
 *
 * Creates a TCP socket using IPv6, binds it to the configured port address,
 * and starts listening for incoming connections. The socket is configured
 * to accept connections from any IPv6 address (in6addr_any).
 *
 * Uses SOMAXCONN for the listen backlog to allow the maximum number of
 * pending connections supported by the system. Prints port information
 * when debug mode is enabled.
 *
 * @retval Socket file descriptor on success
 * @retval Exits program on failure (via exit(EXIT_FAILURE))
 *
 * @note Uses global variables app_flags and port_addr
 * @note Requires root privileges for ports below 1024
 * @note Prints debug information if D_FLAG is set
 * @note Program exits on any socket operation failure
 * @note IPv6 socket can also accept IPv4 connections (dual-stack)
 *
 * @see createSocket_v4(), handleSocket()
 */
int
createSocket_v6(void)
{
	int sock_v6;
	socklen_t length;
	struct sockaddr_in6 server_v6;

	if ((sock_v6 = socket(PF_INET6, SOCK_STREAM, 0)) < 0) {
		perror("opneing IPv6 sock stream");
		exit(EXIT_FAILURE);
	}

	/* creating socket address information for IPv6 */
	memset(&server_v6, 0, sizeof(server_v6));
	server_v6.sin6_family = PF_INET6;
	server_v6.sin6_addr =
	    in6addr_any; /*  wtf isn't this an all caps macro? */
	server_v6.sin6_port = htons(port_addr);

	if (bind(sock_v6, (struct sockaddr *)&server_v6, sizeof(server_v6)) !=
	    0) {
		perror("binding sock_v6");
		exit(EXIT_FAILURE);
	}

	length = sizeof(server_v6);
	if (getsockname(sock_v6, (struct sockaddr *)&server_v6, &length) != 0) {
		perror("getting sock_v6 name");
		exit(EXIT_FAILURE);
	}

	if (app_flags & D_FLAG) {
		(void)printf("Sock_v6 has port #%d\n",
		    ntohs(server_v6.sin6_port));
	}

	if (listen(sock_v6, BACKLOG) < 0) {
		perror("listen sock_v6");
		exit(EXIT_FAILURE);
	}

	return (sock_v6);
}
