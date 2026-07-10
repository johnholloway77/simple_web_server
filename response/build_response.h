#include <magic.h>
#include "../client_conn/connections.h"
#include "../requests/resolve_path.h"
#include "../requests/parse_request.h"

int build_okay_response(Client *c, ResolvedPath *rp, Request *req);

int build_error_response(Client *c, Request *req);
