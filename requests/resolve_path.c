#include "./resolve_path.h"

void
close_resolve_path_ptr(ResolvedPath *rp)
{
	if (rp->file_ptr) {
		fclose(rp->file_ptr);
		rp->file_ptr = NULL;
	}
}

int
resolve_path(Client *c, const Request *req, ResolvedPath *rp, magic_t magic)
{
	return -1;
}
