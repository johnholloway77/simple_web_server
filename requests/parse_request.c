#include "./parse_request.h"
#include <stddef.h>
#include <stdio.h>
#include <limits.h>

#ifdef __linux__
#include <bsd/string.h>
#else
#include <string.h>
#endif

#include "../debug/debug.h"

#define METHOD_MAX 10
#define VERSION_MAX 10

#define DETERMINED_400                                                         \
	*resp = RESP_400;                                                      \
	(*out).method = HTTP_METHOD_UNKNOWN;                                   \
	(*out).version = HTTP_VERSION_UNKNOWN;                                 \
	return -1;

#define DETERMINED_403                                                         \
	*resp = RESP_403;                                                      \
	return -1;

#define DETERMINED_414                                                         \
	*resp = RESP_414;                                                      \
	return -1;

#define DETERMINED_501                                                         \
	*resp = RESP_501;                                                      \
	return -1;

#define DETERMINED_505                                                         \
	*resp = RESP_505;                                                      \
	return -1;

static enum http_method
method_from_token(const char *tok, size_t len)
{
	if (0 == len) {
		return HTTP_METHOD_UNKNOWN;
	}

	if (len == strlen("GET") && strncmp(tok, "GET", len) == 0) {
		return HTTP_GET;
	}
	if (len == strlen("POST") && strncmp(tok, "POST", len) == 0) {
		return HTTP_POST;
	}
	if (len == strlen("HEAD") && strncmp(tok, "HEAD", len) == 0) {
		return HTTP_HEAD;
	}

	return HTTP_METHOD_UNKNOWN; // use for junk/incorrect
};

static enum http_version
version_from_token(const char *tok, size_t len)
{
	if (0 == len) {
		return HTTP_VERSION_UNKNOWN;
	}

	if (len == strlen("HTTP/1.0") && strncmp(tok, "HTTP/1.0", len) == 0)
		return HTTP_1_0;
	if (len == strlen("HTTP/1.1") && strncmp(tok, "HTTP/1.1", len) == 0)
		return HTTP_1_1;
	if (len == strlen("HTTP/0.9") && strncmp(tok, "HTTP/0.9", len) == 0)
		return HTTP_VERSION_UNSUPPORTED;
	if (len == strlen("HTTP/2.0") && strncmp(tok, "HTTP/2.0", len) == 0)
		return HTTP_VERSION_UNSUPPORTED;

	return HTTP_VERSION_UNKNOWN; // use for junk/incorrect
}

static int
path_has_traversal(const char *path, size_t len)
{
	return strnstr(path, "../", len) ? 1 : 0;
}

int
parse_request(const char *buf,
    size_t len,
    Request *out,
    enum client_response *resp)
{
	DBG("Entering parse_request()\n");

	if (0 == len || !buf || !strnstr(buf, "\r\n\r\n", len)) {
		DBG("Determined 400\n");
		DETERMINED_400
	}

	time_t current = time(NULL);
	struct tm *utc_time = gmtime(&current);
	strftime(out->time_received,
	    TIME_RECEIVED_LENGTH,
	    "%Y-%m-%dT%H:%M:%SZ",
	    utc_time);

	const char *first_line = strnstr(buf, "\r\n", len);
	// out->first_line = strnstr(buf, "\r\n", len);
	size_t line_length = first_line ? (size_t)(first_line - buf) : len;

	char line_buf[MAX_REQUEST_SIZE + 1];
	memcpy(line_buf, buf, line_length);
	line_buf[line_length] = '\0';

	DBG("request:\n%s\n", line_buf);

	const char *method = NULL;
	const char *path = NULL;
	const char *version = NULL;
	const char *extra = NULL;

	const char *deliminator = " ";

	method = strtok(line_buf, deliminator);
	path = strtok(NULL, deliminator);
	version = strtok(NULL, deliminator);
	extra = strtok(NULL, deliminator);

	if (method && path && version && !extra) {
		DBG("Method %s\tPath: %s\tVersion %s\n", method, path, version);

		(*out).method = method_from_token(method, strlen(method));
		(*out).version = version_from_token(version, strlen(version));

		size_t path_len = strlcpy((*out).path, path, PATH_MAX);
		if (path_len >= PATH_MAX) {
			DETERMINED_414
		}
	}

	else {
#ifdef DEBUG
		DBG("Method %s\tPath: %s\tVersion %s\textra? %s\n",
		    method ? method : "missing",
		    path ? path : "missing",
		    version ? version : "missing",
		    extra ? extra : "no provided");
#endif

		DETERMINED_400
	}

	// Check paths for validity
	if ((*out).path[0] != '/') {
		DETERMINED_400
	}

	if ((*out).version == HTTP_VERSION_UNKNOWN) {
		DETERMINED_400
	}

	if (path_has_traversal((*out).path, strlen((*out).path))) {
		DETERMINED_403
	}

	if ((*out).method == HTTP_METHOD_UNKNOWN) {
		DETERMINED_501
	}

	if ((*out).version == HTTP_VERSION_UNSUPPORTED) {
		DETERMINED_505
	}

	// Implement this in the future johnnyboy
	if ((*out).method == HTTP_POST) {
		DETERMINED_501
	}
	return 0;
}
