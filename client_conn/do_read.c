#include <magic.h>
#include <stdio.h>
#include <errno.h>
#include <poll.h>
#include <sys/socket.h>
#include <sys/types.h>

#include "./connections.h"
#include "../debug/debug.h"
#include "./do_read.h"
//#include "../response/build_response.h"

#ifdef __linux__
#include <bsd/string.h>
#else
#include <string.h>
#endif


#define MAX_REQUEST_SIZE                                                       \
	8192 /**< Hard ceiling on inbound request size (bytes) */

#define TEMP_BUFFER 2048 /**< Stack scratch buffer for each recv() call */



/**
 * @brief Append received bytes to a client's input buffer, growing it as
 * needed.
 *
 * If appending @p n bytes would push the total past MAX_REQUEST_SIZE, the
 * client is transitioned to PROCESSING with resp_val RESP_400 so the event
 * loop can send a 400 Bad Request and close the connection.
 *
 * The buffer is grown geometrically (doubling) via realloc() to keep
 * amortised cost O(1) per byte.  On allocation failure the client is
 * transitioned to CLOSING.
 *
 * @param c     Client whose input buffer should be extended
 * @param data  Pointer to bytes to copy in
 * @param n     Number of bytes to copy
 */
void
append(struct Client *c, const char *data, size_t n)
{
	if (c->input_length + n > MAX_REQUEST_SIZE) {
		c->resp_val = RESP_414;
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


Reading_state read_request(Client *client){

    Client *c = client;

    for (;;) {
		char tmp[TEMP_BUFFER];
		ssize_t n = recv(c->fd, tmp, sizeof(tmp), 0);

		if (n > 0) {
			append(c, tmp, n);

			if (c->state != READING)
				return READ_ERROR;
		}
		else if (0 == n) {
			c->state = CLOSING;
			return READ_PEER_CLOSED;
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
		return READ_EMPTY;
	}


	const char *end = strnstr(c->in_buf, "\r\n\r\n", c->input_length);

	if (!end) {
		// request not fully received;
		return READ_INCOMPLETE;
	}

	c->header_len = (end - c->in_buf) + 4;
	return READ_COMPLETE;
}
