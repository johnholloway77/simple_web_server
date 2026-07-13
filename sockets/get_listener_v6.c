#include <arpa/inet.h>
#include <sys/socket.h>

#include <netinet/in.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>

#include "../flags/flags.h"

#define BACKLOG SOMAXCONN

extern const uint32_t app_flags;
extern const uint32_t port_addr;
extern const char *bind_addr6;

int
get_listener_v6(void)
{
	int listener_v6;

	int yes = 1;
	int v6only = 1;
	socklen_t length;
	struct sockaddr_in6 server_v6;

	if ((listener_v6 = socket(AF_INET6, SOCK_STREAM, 0)) < 0) {
		perror("opening IPv6 sock stream");
		exit(EXIT_FAILURE);
	}

	(void)setsockopt(listener_v6,
	    SOL_SOCKET,
	    SO_REUSEADDR,
	    &yes,
	    sizeof(yes));
	(void)setsockopt(listener_v6,
	    IPPROTO_IPV6,
	    IPV6_V6ONLY,
	    &v6only,
	    sizeof(v6only));

	int listener_flags = fcntl(listener_v6, F_GETFL, 0);
	if (listener_flags < 0 ||
	    fcntl(listener_v6, F_SETFL, listener_flags | O_NONBLOCK) < 0) {
		perror("fcntl O_NONBLOCK listener_v6");
		exit(EXIT_FAILURE);
	}

	/* creating socket address information for IPv6 */
	memset(&server_v6, 0, sizeof(server_v6));
	server_v6.sin6_family = AF_INET6;

	if (app_flags & B6_FLAG) {
		if (inet_pton(AF_INET6, bind_addr6, &server_v6.sin6_addr) !=
		    1) {
			fprintf(stderr, "invalid ipv6 bind address\n");
			return -1;
		}
	}
	else {
		server_v6.sin6_addr =
		    in6addr_any; /*  wtf isn't this an all caps macro? */
	}

	server_v6.sin6_port = htons(port_addr);

	if (bind(listener_v6,
		(struct sockaddr *)&server_v6,
		sizeof(server_v6)) != 0) {
		perror("bind listener_v6");
		fprintf(stderr,
		    "%s not assigned to local interface\n",
		    bind_addr6);
		exit(EXIT_FAILURE);
	}

	length = sizeof(server_v6);
	if (getsockname(listener_v6, (struct sockaddr *)&server_v6, &length) !=
	    0) {
		perror("getting listener_v6 name");
		exit(EXIT_FAILURE);
	}

	if (app_flags & V_FLAG) {
		(void)fprintf(stdout,
		    "listener_v6 has port #%d\n",
		    ntohs(server_v6.sin6_port));
	}

	if (listen(listener_v6, BACKLOG) < 0) {
		perror("listen listener_v6");
		exit(EXIT_FAILURE);
	}

	return (listener_v6);
}
