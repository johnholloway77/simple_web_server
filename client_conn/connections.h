#pragma once

#include <poll.h>
#include <stdio.h>
#include <netinet/in.h>
#include <stdlib.h>
#include <stddef.h>
#include <time.h>

/**
 * @brief Lifecycle state of a client connection in the poll() event loop.
 *
 * Transitions follow a linear path through the request/response cycle:
 *   READING -> PROCESSING -> SENDING_HEADER -> SENDING_BODY -> CLOSING
 *
 * Any state may transition directly to CLOSING on error or EOF.
 */
enum client_state
{
	READING,        /**< Accumulating inbound HTTP request bytes */
	PROCESSING,     /**< Full headers received; building the response */
	SENDING_HEADER, /**< Writing HTTP response headers to the socket */
	SENDING_BODY,   /**< Writing response body (file data or CGI output) */
	CLOSING,        /**< Connection is done; pending removal from poll set */
	NUM_CLIENT_STATE,
};

/**
 * @brief HTTP response codes that the server can generate.
 *
 * NUM_CLIENT_RESP serves as a sentinel / "not yet set" value and is stored
 * in Client.resp_val immediately after a connection is accepted.
 */
enum client_response
{
	RESP_200, /**< 200 OK */
	RESP_400, /**< 400 Bad Request */
	RESP_403, /**< 403 Forbidden */
	RESP_404, /**< 404 Not Found */
	RESP_500, /**< 500 Internal Server Error */
	RESP_501, /**< 501 Not Implemented (CGI disabled) */

	NUM_CLIENT_RESP, /**< Sentinel — response not yet determined */
};

/**
 * @brief Per-connection state tracked across poll() iterations.
 *
 * One Client entry exists for every open file descriptor in the poll set
 * (indices 0 and 1 are dummy entries reserved for the listener sockets).
 * The parallel pollfd array is kept in sync: pfds[i] always corresponds
 * to clients[i].
 *
 * Buffer ownership:
 *   - @c in_buf  is heap-allocated by append() on first write and freed by
 *     close_conn().
 *   - @c out_buf is heap-allocated when the response is assembled and freed
 *     by close_conn().
 *   - @c file_ptr, if non-NULL, is fclose()'d by close_conn().
 */
typedef struct Client
{
	char *in_buf;   /**< Heap buffer accumulating the inbound request */
	char *out_buf;  /**< Heap buffer holding the outbound response headers */
	FILE *file_ptr; /**< Open file being streamed as the response body, or NULL */

	size_t input_length;    /**< Bytes written into in_buf so far */
	size_t input_capacity;  /**< Allocated size of in_buf */
	size_t output_length;   /**< Bytes written into out_buf so far */
	size_t output_capacity; /**< Allocated size of out_buf */
	size_t header_len;      /**< Byte length of the HTTP request headers
	                         *   (including the trailing \r\n\r\n), set by
	                         *   do_read() once the end-of-headers marker
	                         *   is found */

	int fd;                      /**< Socket file descriptor */
	enum client_state state;     /**< Current lifecycle state */
	enum client_response resp_val; /**< Response code to send; NUM_CLIENT_RESP
	                                *   until determined by request processing */

	char timestamp[24];                /**< RFC-formatted timestamp string */
	char client_addr[INET6_ADDRSTRLEN]; /**< Dotted-decimal / colon-hex peer
	                                     *   address string */
} Client;

/**
 * @brief Add a newly accepted socket to the poll and client arrays.
 *
 * If the arrays are full they are doubled via realloc(). The new entry is
 * appended at index @c *fd_count and that index is returned so the caller
 * can immediately populate address fields.
 *
 * The new Client is zero-initialised with:
 *   - state  = READING
 *   - resp_val = NUM_CLIENT_RESP (not yet determined)
 *   - all buffer pointers NULL, lengths/capacities 0
 *
 * Exits the process with EXIT_FAILURE if realloc() fails.
 *
 * @param pfds     Pointer to the poll array pointer (may be updated by realloc)
 * @param clients  Pointer to the Client array pointer (may be updated by realloc)
 * @param newfd    File descriptor of the accepted socket
 * @param fd_count Pointer to the current number of active entries; incremented
 *                 on success
 * @param fd_size  Pointer to the current allocated capacity; doubled on growth
 * @return         Index of the new entry in @p pfds / @p clients
 */
int add_to_lists(struct pollfd **pfds,
    Client **clients,
    int newfd,
    int *fd_count,
    int *fd_size);

/**
 * @brief Remove a client connection and compact the poll/client arrays.
 *
 * Closes the socket, fclose()'s any open file_ptr, and frees in_buf and
 * out_buf. The slot is filled by swapping in the last active entry so that
 * the arrays remain dense.  The caller must decrement its loop index after
 * this call to re-examine the entry now occupying slot @p i.
 *
 * Refuses to close slot 0 (listener guard) and logs a message to stderr.
 *
 * @param i        Index of the connection to remove
 * @param fd_count Pointer to the active entry count; decremented on removal
 * @param pfds     The poll array (not a pointer-to-pointer; direct array)
 * @param clients  The Client array (not a pointer-to-pointer; direct array)
 */
void close_conn(int i, int *fd_count, struct pollfd pfds[], Client clients[]);

/**
 * @brief Drain all pending connections from a listening socket.
 *
 * Loops calling accept() until EAGAIN/EWOULDBLOCK signals that no more
 * connections are queued (the listener is non-blocking).  For each accepted
 * socket the function:
 *   1. Sets the socket to O_NONBLOCK.
 *   2. Registers it via add_to_lists().
 *   3. Resolves the peer address (IPv4 or IPv6) into clients[idx].client_addr.
 *
 * On accept() or fcntl() failure the offending descriptor is closed and the
 * loop continues rather than aborting.
 *
 * @param listener_fd  The non-blocking listening socket to accept from
 * @param pfds         Pointer to the poll array pointer (forwarded to
 *                     add_to_lists, may be reallocated)
 * @param clients      Pointer to the Client array pointer (forwarded to
 *                     add_to_lists, may be reallocated)
 * @param fd_count     Pointer to the active entry count (forwarded to
 *                     add_to_lists)
 * @param fd_size      Pointer to the allocated capacity (forwarded to
 *                     add_to_lists)
 */
void accept_new_conn(int listener_fd,
    struct pollfd **pfds,
    Client **clients,
    int *fd_count,
    int *fd_size);
