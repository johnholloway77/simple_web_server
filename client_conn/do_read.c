#include <magic.h>
#include <stdio.h>
#include <errno.h>
#include <poll.h>
#include <sys/socket.h>
#include <sys/types.h>

#include "./connections.h"
#include "../requests/parse_request.h"
#include "../requests/resolve_path.h"
#include "../response/build_response.h"

#ifdef __linux__
#include <bsd/string.h>
#else
#include <string.h>
#endif

#define TEMP_BUFFER 2048 /**< Stack scratch buffer for each recv() call */
#define MAX_REQUEST_SIZE                                                       \
	8192 /**< Hard ceiling on inbound request size (bytes) */

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
static void
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

/**
 * @brief Read available bytes from a non-blocking client socket.
 *
 * Calls recv() in a loop until EAGAIN/EWOULDBLOCK signals that no more
 * data is ready, then checks whether the complete HTTP request headers have
 * arrived by searching for the "\r\n\r\n" terminator.
 *
 * State transitions:
 *   - READING  → PROCESSING   when "\r\n\r\n" is found (sets header_len)
 *   - READING  → PROCESSING   when the request exceeds MAX_REQUEST_SIZE
 *                              (sets resp_val = RESP_400 via append())
 *   - READING  → CLOSING      on EOF (recv returns 0) or unrecoverable error
 *
 * If neither a complete header nor an error is detected the function returns
 * with the client still in READING state so the poll loop can wait for more
 * data.
 *
 * @param i        Index of the client in @p clients
 * @param clients  The Client array managed by the poll loop
 */
void
do_read(int i, Client *clients, struct pollfd pfds[], magic_t magic)
{
	Client *c = &clients[i];
	struct pollfd *p = &pfds[i];

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

	const char *end = strnstr(c->in_buf, "\r\n\r\n", c->input_length);

	if (!end) {
		// request not fully received;
		return;
	}

	c->header_len = (end - c->in_buf) + 4;

	Request req = {0};
	ResolvedPath rp = {0};

	if (parse_request(c->in_buf, c->header_len, &req, &c->resp_val) == 0 &&
	    resolve_path(c, &req, &rp, magic) == 0) {
		// TO DO
		build_okay_response(c, &rp, &req);
	}
	else {
		build_error_response(c, &req);

		printf("successfully built error response for client:\n\n%s\n",
		    c->out_buf);
	}

	close_resolve_path_ptr(&rp);

	p->events = POLLOUT;
	c->state = SENDING_HEADER;
}
