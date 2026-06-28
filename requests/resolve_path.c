#include "./resolve_path.h"
#include <stdio.h>
#include "../debug/debug.h"

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
	printf("\033[31;1;4m%s not built. Returning -1\033[0m\n", __func__);

	return -1;
}
