#include <errno.h>
#include <poll.h>
#include <sys/socket.h>
#include <string.h>
#include <sys/types.h>
#include "../client_conn/connections.h"

#define TEMP_BUFFER 2048
#define MAX_REQUEST_SIZE 8192

/*
 * Grow a client's in buffer
 */
void
append(struct Client *c, const char *data, size_t n)
{
	if (c->input_length + n > MAX_REQUEST_SIZE) {
		c->resp_val = RESP_400;
		c->state = PROCESSING;
		return;
	}

	if (c->input_length + n > c->input_capacity) {
		size_t new_cap = c->input_capacity ? c->input_capacity * 2
						   : 1024;
		while (new_cap < c->input_length + n) {
			new_cap *= 2;
		}

		char *p = realloc(c->in_buf, new_cap);

		if (!p) {
			c->state = CLOSING;
			return;
		}
		c->in_buf = p;
		c->input_capacity = new_cap;
	}

	memcpy(c->in_buf + c->input_length, data, n);
	c->input_length += n;
}

void
do_read(int i, int *fd_count, struct pollfd *pfds, Client *clients)
{
	Client *c = &clients[i];

	for (;;) {
		char tmp[TEMP_BUFFER];
		ssize_t n = recv(c->fd, tmp, sizeof(tmp), 0);
		if (n > 0) {
			append(c, tmp, n);
			if (c->state != READING)
				return;
		}
		else if (0 == n) {
			c->state = CLOSING;
			return;
		}
		else {
			if (EAGAIN == errno || EWOULDBLOCK == errno) {
				break;
			}
			if (EINTR == errno) {
				continue;
			}
			c->state = CLOSING;
			break;
		}
	}

	if (NULL == c->in_buf || 0 == c->input_length) {
		return;
	}

	char *end = strnstr(c->in_buf, "\r\n\r\n", c->input_length);

	if (!end) {
		// request not fully received;
		return;
	}

	c->header_len = (end - c->in_buf) + 4;
	//	parse_request(c); //To be written
	c->state = PROCESSING;
}
