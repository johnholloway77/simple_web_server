#include <sys/socket.h>

#include <netinet/in.h>
#include <arpa/inet.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>

#include "../flags/flags.h"

#define BACKLOG SOMAXCONN

extern const uint32_t app_flags;
extern const uint32_t port_addr;
extern const char *bind_addr4;

int
get_listener_v4(void)
{
	int listener_v4;
	int yes = 1;
	socklen_t length;

	struct sockaddr_in server_v4;

	if ((listener_v4 = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
		perror("opening IPv4 sock stream");
		exit(EXIT_FAILURE);
	}

	(void)setsockopt(listener_v4,
	    SOL_SOCKET,
	    SO_REUSEADDR,
	    &yes,
	    sizeof(yes));

	int listener_flags = fcntl(listener_v4, F_GETFL, 0);
	if (listener_flags < 0 ||
	    fcntl(listener_v4, F_SETFL, listener_flags | O_NONBLOCK) < 0) {
		perror("fcntl O_NONBLOCK listener_v4");
		exit(EXIT_FAILURE);
	}

	/* creating socket address information for IPv4 */
	memset(&server_v4, 0, sizeof(server_v4));

	server_v4.sin_family = AF_INET;

	if (app_flags & B4_FLAG) {
		if (inet_pton(AF_INET, bind_addr4, &server_v4.sin_addr) != 1) {
			fprintf(stderr, "invalid ipv4 address\n");
			return -1;
		}
	}
	else {
		server_v4.sin_addr.s_addr = INADDR_ANY;
	}

	server_v4.sin_port = htons(port_addr);

	if (bind(listener_v4,
		(struct sockaddr *)&server_v4,
		sizeof(server_v4)) != 0) {
		perror("bind listener_v4");
		fprintf(stderr,
		    "%s not assigned to local interface\n",
		    bind_addr4);
		exit(EXIT_FAILURE);
	}

	length = sizeof(server_v4);
	if (getsockname(listener_v4, (struct sockaddr *)&server_v4, &length) !=
	    0) {
		perror("getting listener_v4 name");
		exit(EXIT_FAILURE);
	}
	if (app_flags & V_FLAG) {
		(void)printf("listener_v4 has port #%d\n",
		    ntohs(server_v4.sin_port));
	}

	if (listen(listener_v4, BACKLOG) < 0) {
		if (app_flags & V_FLAG) {
			perror("listen listener_v4");
		}
		exit(EXIT_FAILURE);
	}

	/* Should revise this so that it only sends one socket back... */
	return (listener_v4);
}
