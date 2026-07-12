#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include "../client_conn/connections.h"
#include "../requests/resolve_path.h"
#include "../debug/debug.h"
#include "../requests/parse_request.h"

#define MAX_HEADER_BUF 750

static int
check_resolve_args(const Client *c, const ResolvedPath *rp, const Request *req)
{
	if (!c) {
		DBG("Null client pointer\n");
		return -1;
	}
	if (!rp) {
		DBG("NULL ResolvedPath pointer\n");
		return -1;
	}
	if (!req) {
		DBG("Null request pointer\n");
		return -1;
	}
	return 0;
}

static int
build_response_cgi(Client *c, ResolvedPath *rp, Request *req)
{
	if (check_resolve_args(c, rp, req) != 0) {
		return -1;
	}

	printf("build_response_cgi not finished, exiting\n");
	exit(EXIT_FAILURE);
	return -1;
}

static int
build_response_dir(Client *c, ResolvedPath *rp, Request *req)
{
	if (check_resolve_args(c, rp, req) != 0) {
		return -1;
	}

	if (!rp->is_dir_listing) {
		return -1;
	}

	DBG("dir path: %s\n", req->path);

	printf("build_response_dir not finished, exiting\n");
	exit(EXIT_FAILURE);
	return -1;
}

static int
build_response_file(Client *c, ResolvedPath *rp, Request *req)
{
	if (check_resolve_args(c, rp, req) != 0) {
		return -1;
	}

	char header[MAX_HEADER_BUF];

	int header_len = snprintf(header,
	    MAX_HEADER_BUF,
	    "HTTP/1.0 200 OK\r\n"
	    "Content-Type: %s\r\n"
	    "Content-Length: %zu\r\n"
	    "Connection: close\r\n"
	    "\r\n",
	    rp->mime_type,
	    rp->file_size);

	if (HTTP_HEAD == req->method) {
		c->out_buf = malloc(header_len + 1);
		if (!c->out_buf) {
			perror("error build response file: ");
			return -1;
		}
		memcpy(c->out_buf, header, header_len);
		c->output_length = header_len;

		return 0;
	}

	DBG("Header:\n%s\n", header);

	DBG("Client info:\n"
	    "c->file_size = %zu\n",
	    c->file_size);

	c->out_buf = malloc(header_len + c->file_size + 1);
	if (!c->out_buf) {
		perror("error build response file: ");
		return -1;
	}

	memcpy(c->out_buf, header, header_len);

	size_t n = fread(c->out_buf + header_len, 1, c->file_size, c->file_ptr);

	c->output_length = header_len + n;

	return 0;
}

int
build_okay_response(Client *c, ResolvedPath *rp, Request *req)
{
	if (rp->is_cgi_bin) {
		return build_response_cgi(c, rp, req);
	}

	if (rp->is_dir_listing) {
		return build_response_dir(c, rp, req);
	}

	return build_response_file(c, rp, req);
}
