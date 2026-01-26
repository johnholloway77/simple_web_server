#include <sys/socket.h>

#include <netinet/in.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../flags/flags.h"

// #define BACKLOG 5
#define BACKLOG SOMAXCONN

extern uint32_t app_flags;
extern uint32_t port_addr;

/**
 * @brief Create and bind an IPv4 TCP socket
 *
 * Creates a TCP socket using IPv4, binds it to the configured port address,
 * and starts listening for incoming connections. The socket is configured
 * to accept connections from any IPv4 address (INADDR_ANY).
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
 *
 * @see createSocket_v6(), handleSocket()
 */
int
createSocket_v4(void)
{
	int sock_v4;
	socklen_t length;

	struct sockaddr_in server_v4;

	// create both IPv4 and IPv6 sockets...

	if ((sock_v4 = socket(PF_INET, SOCK_STREAM, 0)) < 0) {
		perror("opening IPv4 sock stream");
		exit(EXIT_FAILURE);
	}

	// creating socket address information for IPv4
	memset(&server_v4, 0, sizeof(server_v4));

	server_v4.sin_family = PF_INET;
	server_v4.sin_addr.s_addr = INADDR_ANY;
	server_v4.sin_port = htons(port_addr);

	if (bind(sock_v4, (struct sockaddr *)&server_v4, sizeof(server_v4)) !=
	    0) {
		perror("getting socket_v4 name");
		exit(EXIT_FAILURE);
	}

	length = sizeof(server_v4);
	if (getsockname(sock_v4, (struct sockaddr *)&server_v4, &length) != 0) {
		perror("getting sock_v4 name");
		exit(EXIT_FAILURE);
	}
	if (app_flags & D_FLAG) {
		(void)printf("Sock_v4 has port #%d\n",
		    ntohs(server_v4.sin_port));
	}

	if (listen(sock_v4, BACKLOG) < 0) {
		if (app_flags & D_FLAG) {
			perror("listen sock_v4");
		}
		exit(EXIT_FAILURE);
	}

	return (sock_v4); // Should revise this so that it only sends one socket
			  // back...
}
