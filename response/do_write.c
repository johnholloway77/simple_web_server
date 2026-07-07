#include "../client_conn/connections.h"
#include <errno.h>
#include <stdio.h>
#include <sys/socket.h>

void
do_write(int i, Client *clients)
{
	Client *c = &clients[i];

	if (!c) {
		return;
	}

	c->state = SENDING_HEADER;

	if (NULL == c->out_buf || 0 == c->output_length) {
		c->state = CLOSING;
		return;
	}

	while (c->output_sent < c->output_length) {
		ssize_t n = send(c->fd,
		    c->out_buf + c->output_sent,
		    c->output_length - c->output_sent,
		    0);
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

	c->state = CLOSING;
}
