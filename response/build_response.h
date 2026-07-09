#include <magic.h>
#include "../client_conn/connections.h"
#include "../requests/resolve_path.h"

int build_okay_response(Client *c, ResolvedPath *rp);

int build_error_response(Client *c);
