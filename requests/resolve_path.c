#include <stdlib.h>
#include <stdio.h>
#include <sys/stat.h>
#include <limits.h>

#ifdef __linux__
#include <bsd/string.h>
#else
#include <string.h>
#endif

#include "./resolve_path.h"
#include "../debug/debug.h"
#include "../flags/flags.h"

#define BASE_URL "."
#define BASE_LEN strlen(BASE_URL)

#include "../debug/debug.h"

extern char *cgi_addr;
extern uint32_t app_flags;

const char *
get_mime_type_by_ext(const char *filename, magic_t magic, int file_des)
{
	const char *ext = strrchr(filename, '.');
	if (!ext) {
		return magic_descriptor(magic, file_des);
	}

	// Wowzers a lookup table!
	if (strcasecmp(ext, ".htm") == 0)
		return "text/html";
	if (strcasecmp(ext, ".txt") == 0)
		return "text/plain; charset=utf-8";
	if (strcasecmp(ext, ".csv") == 0)
		return "text/csv; charset=utf-8";
	if (strcasecmp(ext, ".md") == 0)
		return "text/markdown; charset=utf-8";
	if (strcasecmp(ext, ".xml") == 0)
		return "application/xml";
	if (strcasecmp(ext, ".json") == 0)
		return "application/json";
	if (strcasecmp(ext, ".map") == 0)
		return "application/json"; // sourcemaps
	if (strcasecmp(ext, ".wasm") == 0)
		return "application/wasm";

	// CSS/JS variants
	if (strcasecmp(ext, ".mjs") == 0)
		return "text/javascript";
	if (strcasecmp(ext, ".cjs") == 0)
		return "text/javascript";

	// Images
	if (strcasecmp(ext, ".jpeg") == 0)
		return "image/jpeg";
	if (strcasecmp(ext, ".svg") == 0)
		return "image/svg+xml";
	if (strcasecmp(ext, ".webp") == 0)
		return "image/webp";
	if (strcasecmp(ext, ".ico") == 0)
		return "image/x-icon";

	// Fonts
	if (strcasecmp(ext, ".woff") == 0)
		return "font/woff";
	if (strcasecmp(ext, ".woff2") == 0)
		return "font/woff2";
	if (strcasecmp(ext, ".ttf") == 0)
		return "font/ttf";
	if (strcasecmp(ext, ".otf") == 0)
		return "font/otf";

	// Audio / video (common)
	if (strcasecmp(ext, ".mp3") == 0)
		return "audio/mpeg";
	if (strcasecmp(ext, ".wav") == 0)
		return "audio/wav";
	if (strcasecmp(ext, ".mp4") == 0)
		return "video/mp4";
	if (strcasecmp(ext, ".webm") == 0)
		return "video/webm";

	// Documents
	if (strcasecmp(ext, ".pdf") == 0)
		return "application/pdf";

	// Archives / binaries
	if (strcasecmp(ext, ".zip") == 0)
		return "application/zip";
	if (strcasecmp(ext, ".gz") == 0)
		return "application/gzip";
	if (strcasecmp(ext, ".tgz") == 0)
		return "application/gzip"; // tar+gzip
	if (strcasecmp(ext, ".tar") == 0)
		return "application/x-tar";

	return magic_descriptor(magic, file_des);
}

int
path_includes_cgi(const char *path)
{
	return strnstr(path, "./cgi-bin/", 10) ? 1 : 0;
}

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
	int path_length = strlen(req->path);

	char path_buffer[PATH_MAX];
	struct stat st = {0};

	DBG("---- New Test run -----\n\treq->path: %s length %ld\n",
	    req->path,
	    strlen(req->path));
	DBG("base_url: %s\t base_len: %ld\n", BASE_URL, BASE_LEN);

	snprintf(path_buffer, PATH_MAX, "%s%s", BASE_URL, req->path);

	if (path_includes_cgi(path_buffer)) {
		if (app_flags & C_FLAG) {
			// Will handle cgi-bin
			rp->is_cgi_bin = 1;
		}
		else {
			rp->file_ptr = NULL;
			c->resp_val = RESP_501;
			return -1;
		}
	}

	DBG("Entering resolve_path ! \n");
	DBG("path_buffer: %s\n", path_buffer);

	if (('/' == req->path[path_length - 1]) ||
	    '.' == req->path[path_length - 1]) {
		if (rp->is_cgi_bin) {
			// no cgi file provided.
			DBG("CGI file not listed");
			c->resp_val = RESP_501;
			return -1;
		}

		DBG("searching for index.html\n");
		rp->file_ptr = fopen("./index.html", "r");
	}
	else {
		DBG("In else branch\n");
		rp->file_ptr = fopen(path_buffer, "r");
	}

	if (rp->file_ptr) {
		DBG("file opened!\n");

		if (-1 == fstat(fileno(rp->file_ptr), &st)) {
			perror("resolve_path fstat");
			exit(EXIT_FAILURE);
		};

		if (S_ISDIR(st.st_mode)) {
			rp->is_dir_listing = 1;
			return 0;
		}

		rp->mime_type = get_mime_type_by_ext(path_buffer,
		    magic,
		    fileno(rp->file_ptr));
		rp->file_size = st.st_size;

		return 0;
	}
	else {
		DBG("open failed\n");
		c->resp_val = RESP_404;
		return -1;
	}

	fclose(rp->file_ptr);
	return -1;
}
