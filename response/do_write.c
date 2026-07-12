#include "../client_conn/connections.h"
#include <stddef.h>
#include <stdlib.h>
#include <errno.h>
#include <stdio.h>
#include <sys/socket.h>
#include "../debug/debug.h"

void
do_write(int i, Client *clients)
{
	DBG("Writing error response to socket\n");

	Client *c = &clients[i];
	if (!c) {
		return;
	}

	if (NULL == c->out_buf || 0 == c->output_length) {
		c->state = CLOSING;
		return;
	}

	DBG("fd: %d\n", c->fd);
	DBG("Output length: %zu\n", c->output_length);
	DBG("Output sent: %zu\n", c->output_sent);

	while (c->output_sent < c->output_length) {
		ssize_t n = send(c->fd,
		    c->out_buf + c->output_sent,
		    c->output_length - c->output_sent,
		    0);
		if (n <= 0) {
			DBG("N < or = to zero!!\n");

			exit(EXIT_FAILURE);
		}
		if (n > 0) {
			c->output_sent += n;
		}
		else {
			if (errno == EAGAIN || errno == EWOULDBLOCK) {
				return;
			}
			if (errno == EINTR) {
				continue;
			}
		}
	}

	DBG("Output sent: %zu\n", c->output_sent);
	DBG("Finished writing to socket, closing client\n");

	shutdown(c->fd, SHUT_WR);
	c->state = CLOSING;

	// exit(EXIT_SUCCESS);
}
