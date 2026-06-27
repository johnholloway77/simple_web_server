#pragma once
#include <limits.h>
#include <unistd.h>
#include "../client_conn/connections.h"

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
	HTTP_VERSION_UNKNOWN,
	NUM_HTTP_VERSION
};

typedef struct Request
{
	enum http_method method;
	enum http_version version;
	char path[PATH_MAX];
} Request;

enum http_method method_from_token(const char *tok, size_t len);

enum http_version version_from_token(const char *tok, size_t len);

int path_has_traversal(const char *path, size_t len);

int parse_request(const char *buf,
    size_t len,
    Request *out,
    enum client_response *resp);
