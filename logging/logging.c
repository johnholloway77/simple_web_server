#include <stdio.h>

#include "../client_conn/connections.h"
#include "../debug/debug.h"
#include "../flags/flags.h"
#include "../requests/resolve_path.h"
#include "../requests/parse_request.h"

extern const uint32_t app_flags;
extern FILE *log_ptr;

int
do_logging(Client *c, Request *req, ResolvedPath *rp)
{
	FILE *tmp;

	if (app_flags & L_FLAG) {
		if (!log_ptr) {
			return -1;
		}
		tmp = log_ptr;
	}
	else {
		tmp = stdout;
	}

	fprintf(tmp,
	    "Log: %s %s\nend of log\n",
	    c->client_addr,
	    req->first_line);

	return 0;
}
