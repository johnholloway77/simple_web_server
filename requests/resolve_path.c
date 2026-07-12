#include <errno.h>
#include <stdlib.h>
#include <stdio.h>
#include <sys/stat.h>
#include <limits.h>
#include <limits.h>
#include <sys/syslimits.h>

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
#define DIR_INDEX "index.htm"
#define DIR_INDEX2 "index.html"

#define HANDLE_DIR_INDEX                                                       \
	rp->file_ptr = index;                                                  \
	fstat(fileno(rp->file_ptr), &st);                                      \
	rp->file_size = st.st_size;                                            \
	rp->mime_type = "text/html";                                           \
	c->file_ptr = rp->file_ptr;                                            \
	c->file_size = st.st_size;                                             \
	if (-1 == fstat(fileno(index), &st)) {                                 \
		perror("dir Index resolve_path fstat");                        \
		exit(EXIT_FAILURE);                                            \
	};                                                                     \
	rp->last_mod = st.st_mtime;

#include "../debug/debug.h"

extern char *cgi_addr;
extern uint32_t app_flags;

enum trailing_char
{
	TRAILING_SLASH,
	TRAILING_CHAR,
	TRAILING_COUNT
};

static FILE *
get_file_path_slash(const char *path, const char *dir_index)
{
	char buffer[PATH_MAX];
	snprintf(buffer, PATH_MAX, "%s%s", path, dir_index);
	DBG("get_file_path_slash buffer: %s\n", buffer);
	return fopen(buffer, "r");
}
static FILE *
get_file_path_char(const char *path, const char *dir_index)
{
	char buffer[PATH_MAX];
	snprintf(buffer, PATH_MAX, "%s/%s", path, dir_index);

	DBG("get_file_path_char buffer: %s\n", buffer);
	return fopen(buffer, "r");
}

static FILE *(*get_file_path[TRAILING_COUNT])(const char *,
    const char *) = {[TRAILING_SLASH] = get_file_path_slash,
    [TRAILING_CHAR] = get_file_path_char};

static const char *
get_mime_type_by_ext(const char *filename, magic_t magic, int file_des)
{
	const char *ext = strrchr(filename, '.');
	if (!ext) {
		return magic_descriptor(magic, file_des);
	}

	// Wowzers a lookup table!
	if (strcasecmp(ext, ".html") == 0)
		return "text/html";
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
	if (strcasecmp(ext, ".jpg") == 0)
		return "image/jpeg";
	if (strcasecmp(ext, ".gif") == 0)
		return "image/gif";
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

static int
path_includes_cgi(const char *path)
{
	return strnstr(path, "./cgi-bin/", 10) ? 1 : 0;
}

static enum trailing_char
get_last_char(const char *path)
{
	int len = strlen(path);

	return ('/' == path[len - 1]) ? TRAILING_SLASH : TRAILING_CHAR;
}

int

resolve_path(Client *c, const Request *req, ResolvedPath *rp, magic_t magic)
{
	char path_buffer[PATH_MAX] = {0};
	char *local_path = NULL;
	char *query_buffer = NULL;
	const char *request_delim = "?";

	snprintf(path_buffer, PATH_MAX, "%s%s", BASE_URL, req->path);

	local_path = strtok(path_buffer, request_delim);
	query_buffer = strtok(NULL, request_delim);

	if (query_buffer) {
		strlcpy(rp->query_string, query_buffer, PATH_MAX);
	}

	struct stat st = {0};

	DBG("---- New Test run -----\n\treq->path: %s length %zu\n",
	    req->path,
	    strlen(req->path));

	if (path_includes_cgi(local_path)) {
		if (app_flags & C_FLAG) {
			// Will handle cgi-bin
			rp->is_cgi_bin = 1;
			// printf("\tCGI-bin path: %s\n", path_buffer);
		}
		else {
			rp->file_ptr = NULL;
			c->resp_val = RESP_501;
			return -1;
		}
	}

	DBG("local_path: %s\n", local_path);

	enum trailing_char trailing_char = get_last_char(local_path);

	rp->file_ptr = fopen(local_path, "r");

	if (rp->file_ptr) {
		DBG("file opened!\n");

		if (-1 == fstat(fileno(rp->file_ptr), &st)) {
			perror("resolve_path fstat");
			exit(EXIT_FAILURE);
		};

		if (S_ISDIR(st.st_mode)) {
			FILE *index = get_file_path[trailing_char](local_path,
			    DIR_INDEX);

			if (index) {
				DBG("Index.htm found!\n");
				HANDLE_DIR_INDEX
				return 0;
			}

			index = get_file_path[trailing_char](local_path,
			    DIR_INDEX2);
			if (index) {
				DBG("Index.html found!\n");
				HANDLE_DIR_INDEX
				return 0;
			}

			rp->last_mod = st.st_mtim.tv_sec;

			rp->is_dir_listing = 1;
			rp->file_ptr = NULL;
			return 0;
		}

		rp->last_mod = st.st_mtime;

		rp->mime_type = get_mime_type_by_ext(local_path,
		    magic,
		    fileno(rp->file_ptr));
		rp->file_size = st.st_size;

		c->file_ptr = rp->file_ptr;
		c->file_size = st.st_size;

		return 0;
	}
	else {
		DBG("open failed\n");
		if (EACCES == errno) {
			c->resp_val = RESP_403;
		}
		else {
			c->resp_val = RESP_404;
		}
		return -1;
	}
}
