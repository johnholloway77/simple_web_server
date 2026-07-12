#pragma once

#include "../client_conn/connections.h"
#include <magic.h>
#include <poll.h>

/**
 * @brief Read available bytes from a client socket, detect end-of-headers,
 * and build the response.
 *
 * Drains the non-blocking socket into the client's in_buf via append(),
 * scanning after each batch for the HTTP header terminator ("\r\n\r\n").
 * Once the full headers have arrived, calls parse_request() and
 * resolve_path(), then build_okay_response() or build_error_response() as
 * appropriate.  Finally transitions the poll event mask to POLLOUT and the
 * client state to SENDING_HEADER.
 *
 * State transitions initiated by this function:
 *   READING → PROCESSING   (internally, while building the response)
 *   READING → SENDING_HEADER  (on successful response build)
 *   READING → CLOSING      (on EOF or unrecoverable recv() error)
 *
 * @param i        Index of the client in @p clients / @p pfds
 * @param clients  The Client array managed by the poll loop
 * @param pfds     The pollfd array; events field updated to POLLOUT on success
 * @param magic    Libmagic handle forwarded to resolve_path() for MIME
 * detection
 */
void do_read(int i, Client *clients, struct pollfd pfds[], magic_t magic);
