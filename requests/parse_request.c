#include "./parse_request.h"
#include <string.h>
#include <stdio.h>
#include <sys/syslimits.h>

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

#define DETERMINED_501                                                         \
	*resp = RESP_501;                                                      \
	return -1;

#define DETERMINED_505                                                         \
	*resp = RESP_505;                                                      \
	return -1;

enum http_method
method_from_token(const char *tok, size_t len)
{
	if (0 == len) {
		return HTTP_METHOD_UNKNOWN;
	}

	if (strncmp(tok, "GET", len) == 0) {
		return HTTP_GET;
	}

	if (strncmp(tok, "POST", len) == 0) {
		return HTTP_POST;
	}

	if (strncmp(tok, "PUT", len) == 0) {
		return HTTP_METHOD_UNKNOWN;
	}

	if (strncmp(tok, "DELETE", len) == 0) {
		return HTTP_METHOD_UNKNOWN;
	}

	return HTTP_METHOD_UNKNOWN; // use for junk/incorrect
};

enum http_version
version_from_token(const char *tok, size_t len)
{
	if (0 == len) {
		return HTTP_VERSION_UNKNOWN;
	}

	if (strncmp(tok, "HTTP/1.0", len) == 0) {
		return HTTP_1_0;
	}

	if (strncmp(tok, "HTTP/1.1", len) == 0) {
		return HTTP_1_1;
	}

	if (strncmp(tok, "HTTP/0.9", len) == 0) {
		return HTTP_VERSION_UNSUPPORTED;
	}

	if (strncmp(tok, "HTTP/2.0", len) == 0) {
		return HTTP_VERSION_UNSUPPORTED;
	}

	return HTTP_VERSION_UNKNOWN; // use for junk/incorrect
}

int
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
	if (0 == len || !buf || !strnstr(buf, "\r\n\r\n", len)) {
		DETERMINED_400
	}

	// printf("request:\n%s\n", buf);

	char fmt[64] = {0};
	char method[METHOD_MAX] = {0};
	char version[VERSION_MAX] = {0};
	char extra[10] = {0};

	snprintf(fmt,
	    sizeof(fmt),
	    "%%%ds %%%ds %%%ds %%%ds",
	    METHOD_MAX - 1,
	    PATH_MAX - 1,
	    VERSION_MAX - 1,
	    9);
	int parsed = sscanf(buf, fmt, method, (*out).path, version, extra);

	if (3 == parsed) {
		// printf("Method %s\nPath: %s\nVersion %s\n",
		//     method,
		//     (*out).path,
		//     version);

		(*out).method = method_from_token(method, METHOD_MAX);
		(*out).version = version_from_token(version, VERSION_MAX);
	}
	else {
		DETERMINED_400
	}

	// Check paths for validity
	if ((*out).path[0] != '/') {
		DETERMINED_400
	}

	if ((*out).version == HTTP_VERSION_UNKNOWN) {
		DETERMINED_400
	}

	if (path_has_traversal((*out).path, PATH_MAX)) {
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
