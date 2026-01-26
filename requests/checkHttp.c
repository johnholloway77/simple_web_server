#include <string.h>

/**
 * @brief Validate HTTP protocol version
 *
 * Verifies that the HTTP protocol version string from the request line
 * is a supported version. Currently supports HTTP/1.0 and HTTP/1.1 as
 * defined in RFC 1945 and RFC 2616 respectively.
 *
 * @param[in] http_str HTTP protocol version string (e.g., "HTTP/1.1")
 *
 * @retval 1 HTTP version is valid and supported
 * @retval 0 HTTP version is invalid or unsupported
 *
 * @note Uses case-sensitive string comparison
 * @note Server implements HTTP/1.0 features with some HTTP/1.1 compatibility
 * @note Rejects unsupported versions like HTTP/0.9, HTTP/2.0, etc.
 *
 * @see parseRequest(), checkMethod()
 */
int
checkHttp(const char *http_str)
{
	if (strcmp(http_str, "HTTP/1.0") == 0)
		return (1);

	if (strcmp(http_str, "HTTP/1.1") == 0)
		return (1);

	return 0;
}
