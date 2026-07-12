#pragma once
#include <limits.h>
#include <time.h>
#include <unistd.h>
#include "../client_conn/connections.h"

#ifndef PATH_MAX
#define PATH_MAX 4096
#endif

#define MAX_REQUEST_SIZE                                                       \
	8192 /**< Hard ceiling on inbound request size (bytes) */

/**
 * @brief HTTP methods recognised by the server.
 *
 * HTTP_METHOD_UNKNOWN is returned when the method token does not match any
 * known verb; parse_request() will set resp to RESP_501 in that case.
 */
enum http_method
{
	HTTP_GET,             /**< GET — retrieve a resource */
	HTTP_POST,            /**< POST — currently returns 501 Not Implemented */
	HTTP_HEAD,            /**< HEAD — headers only, no body sent */
	HTTP_METHOD_UNKNOWN,  /**< Unrecognised method token */
	NUM_HTTP_METHOD
};

/**
 * @brief HTTP protocol versions the server can receive.
 *
 * HTTP_VERSION_UNSUPPORTED is set for recognised-but-rejected versions
 * (0.9, 2.0); parse_request() will set resp to RESP_505.
 * HTTP_VERSION_UNKNOWN is set for completely unrecognisable version strings;
 * parse_request() will set resp to RESP_400.
 */
enum http_version
{
	HTTP_1_0,                  /**< HTTP/1.0 — fully supported */
	HTTP_1_1,                  /**< HTTP/1.1 — accepted, served as 1.0 */
	HTTP_VERSION_UNSUPPORTED,  /**< Recognised but not supported (0.9, 2.0) */
	HTTP_VERSION_UNKNOWN,      /**< Unrecognisable version string */
	NUM_HTTP_VERSION
};

/**
 * @brief Parsed representation of an HTTP request line.
 *
 * Populated by parse_request() from the raw bytes in the client's in_buf.
 * The @c path field is always NUL-terminated and at most PATH_MAX-1 bytes.
 */
typedef struct Request
{
	enum http_method method;    /**< HTTP verb */
	enum http_version version;  /**< Protocol version */
	time_t time_received;       /**< Wall-clock time headers were fully received */
	char path[PATH_MAX];        /**< URI path component (no query string) */
} Request;

/**
 * @brief Parse the first line of an HTTP request and validate its fields.
 *
 * Expects @p buf to contain at least the full request headers terminated by
 * "\r\n\r\n".  Only the request line (up to the first "\r\n") is examined.
 *
 * On success, @p out is fully populated and 0 is returned.
 * On any error, @p resp is set to the appropriate error code, the relevant
 * fields of @p out are set to their UNKNOWN sentinel values, and -1 is
 * returned.
 *
 * Error mapping:
 *   - Missing/extra tokens, no "\r\n\r\n", non-'/' path → RESP_400
 *   - Path contains "../"                                → RESP_403
 *   - Path longer than PATH_MAX-1                       → RESP_414
 *   - Unrecognised method (incl. POST for now)          → RESP_501
 *   - Recognised-but-unsupported version (0.9, 2.0)    → RESP_505
 *
 * @param buf   Raw request bytes (need not be NUL-terminated)
 * @param len   Number of valid bytes in @p buf (should include "\r\n\r\n")
 * @param out   Output struct; partially written even on error
 * @param resp  Output error code; only meaningful when -1 is returned
 * @return      0 on success, -1 on parse/validation error
 */
int parse_request(const char *buf,
    size_t len,
    Request *out,
    enum client_response *resp);
