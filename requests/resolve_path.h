#pragma once
#include <limits.h>
#include <sys/stat.h>
#include <unistd.h>
#include <stdio.h>
#include <magic.h>

#include "../client_conn/connections.h"
#include "./parse_request.h"

typedef struct ResolvedPath
{
	char query_string[PATH_MAX]; /* Query string for get requests (can later
					be used for post) */
	FILE *file_ptr;
	off_t file_size;
	const char *mime_type; /* from get_mime_type_by_ext or magic */
	int is_dir_listing; /* 1 if no index found, needs listing */
	int is_cgi_bin; /* 1 if true */
} ResolvedPath;

int
resolve_path(Client *c, const Request *req, ResolvedPath *rp, magic_t magic);
