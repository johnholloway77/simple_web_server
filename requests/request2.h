#pragma once

#include "../client_conn/connections.h"

void do_read(int i, int *fd_count, struct pollfd *pfds, Client *clients);
