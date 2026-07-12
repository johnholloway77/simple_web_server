#pragma once
#include <limits.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>
#include <stdio.h>
#include <magic.h>

#include "../client_conn/connections.h"
#include "./parse_request.h"

/**
 * @brief Result of resolving a request URI to a filesystem resource.
 *
 * Populated by resolve_path().  Callers must call close_resolve_path_ptr()
 * when done to release the open file handle, even if the file was later
 * transferred to Client.file_ptr (both pointers may refer to the same FILE).
 *
 * Exactly one of the following describes the resolved resource:
 *   - file_ptr non-NULL and is_dir_listing == 0 and is_cgi_bin == 0:
 *       a regular file ready to be sent
 *   - is_dir_listing == 1: directory with no index file; caller builds listing
 *   - is_cgi_bin == 1: URI maps to the cgi-bin; caller invokes CGI execution
 */
typedef struct ResolvedPath
{
	char query_string[PATH_MAX]; /**< Query string portion of the URI
	                              *   (everything after '?'), or "" if none */
	FILE *file_ptr;              /**< Open read handle for the resolved file,
	                              *   or NULL for directory listings / CGI */
	off_t file_size;             /**< Size of the file in bytes (0 for dirs) */
	const char *mime_type;       /**< MIME type string from extension table or
	                              *   libmagic; NULL for dirs and CGI */
	time_t last_mod;             /**< st_mtime of the resolved filesystem entry */
	int is_dir_listing;          /**< 1 when the URI resolves to a directory
	                              *   that has no index.htm / index.html */
	int is_cgi_bin;              /**< 1 when the URI path is under cgi-bin */
} ResolvedPath;

/**
 * @brief Close and NULL the file pointer held by a ResolvedPath.
 *
 * Safe to call when rp->file_ptr is already NULL (no-op).  Does not free
 * the ResolvedPath struct itself.
 *
 * @param rp  ResolvedPath whose file_ptr should be closed
 */
void close_resolve_path_ptr(ResolvedPath *rp);

/**
 * @brief Map a parsed request URI to a filesystem resource.
 *
 * Prepends BASE_URL (".") to req->path to build a local path, strips the
 * query string into rp->query_string, then stat()/fopen()s the result.
 *
 * Resolution rules:
 *   - If the path is under "./cgi-bin/" and C_FLAG is set, sets
 *     rp->is_cgi_bin = 1 and returns 0 (no file opened).
 *   - If the path is under "./cgi-bin/" and C_FLAG is NOT set, sets
 *     c->resp_val = RESP_501 and returns -1.
 *   - If the path resolves to a directory, looks for index.htm then
 *     index.html.  If found, opens that file and populates rp normally.
 *     If not found, sets rp->is_dir_listing = 1, leaves rp->file_ptr NULL,
 *     and returns 0.
 *   - If the path resolves to a regular file, opens it, detects the MIME
 *     type (extension table falling back to libmagic), and populates rp and
 *     c->file_ptr / c->file_size.
 *   - On EACCES, sets c->resp_val = RESP_403 and returns -1.
 *   - On any other open failure, sets c->resp_val = RESP_404 and returns -1.
 *
 * @param c      Client whose resp_val and file_ptr may be updated
 * @param req    Parsed request containing the URI path
 * @param rp     Output struct to populate (must be zero-initialised by caller)
 * @param magic  Libmagic handle for MIME detection fallback
 * @return       0 on success, -1 on error (c->resp_val set to error code)
 */
int resolve_path(Client *c, const Request *req, ResolvedPath *rp, magic_t magic);
