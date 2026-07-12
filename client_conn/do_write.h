#pragma once

#include "../client_conn/connections.h"

/**
 * @brief Send the staged response buffer to the client socket.
 *
 * Loops calling send() until all bytes in c->out_buf have been written
 * (c->output_sent == c->output_length).  On completion, issues
 * shutdown(SHUT_WR) and transitions the client to CLOSING so the poll
 * loop can reclaim the slot.
 *
 * If send() returns <= 0 for any reason the process exits immediately
 * (EXIT_FAILURE).  The EAGAIN/EINTR branches in the loop body are
 * currently unreachable dead code — partial-write and non-blocking send
 * handling has not yet been implemented.
 *
 * If c->out_buf is NULL or c->output_length is 0 on entry, the client
 * is transitioned directly to CLOSING without sending anything.
 *
 * Called only when poll() reports POLLOUT and the client is in
 * SENDING_HEADER or SENDING_BODY state.
 *
 * @param i        Index of the client in @p clients
 * @param clients  The Client array managed by the poll loop
 */
void do_write(int i, Client *clients);
