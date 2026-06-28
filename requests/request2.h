#pragma once

#include "../client_conn/connections.h"
#include <magic.h>

/**
 * @brief Read available bytes from a client socket and detect end-of-headers.
 *
 * Drains the non-blocking socket into the client's in_buf via append(),
 * then scans for the HTTP header terminator ("\r\n\r\n").  On success the
 * client transitions to PROCESSING; the raw request bytes remain in in_buf
 * and header_len records where the headers end.
 *
 * @param i        Index of the client in the @p clients array
 * @param clients  The Client array managed by the poll loop
 */
void do_read(int i, Client *clients, magic_t magic);
