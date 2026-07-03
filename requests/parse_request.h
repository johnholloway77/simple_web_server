#pragma once
#include <limits.h>
#include <unistd.h>
#include "../client_conn/connections.h"

#ifndef PATH_MAX
#define PATH_MAX 4096
#endif

#define MAX_REQUEST_SIZE                                                       \
	8192 /**< Hard ceiling on inbound request size (bytes) */

enum http_method
{
	HTTP_GET,
	HTTP_POST,
	HTTP_METHOD_UNKNOWN,
	NUM_HTTP_METHOD
};
enum http_version
{
	HTTP_1_0,
	HTTP_1_1,
	HTTP_VERSION_UNSUPPORTED,
	HTTP_VERSION_UNKNOWN,
	NUM_HTTP_VERSION
};

typedef struct Request
{
	enum http_method method;
	enum http_version version;
	char path[PATH_MAX];
} Request;

int parse_request(const char *buf,
    size_t len,
    Request *out,
    enum client_response *resp);
