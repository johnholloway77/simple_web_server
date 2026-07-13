#pragma once

#include "../requests/parse_request.h"
#include "../requests/resolve_path.h"

int do_logging(Client *c, Request *req, ResolvedPath *rp);
