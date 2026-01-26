#include <string.h>

/**
 * @brief Validate HTTP request method
 *
 * Checks if the provided HTTP method string is one of the supported
 * methods for this web server. Currently supports GET, HEAD, POST,
 * and DELETE methods as defined in HTTP/1.0 and HTTP/1.1 specifications.
 *
 * @param[in] meth_str HTTP method string to validate
 *
 * @retval 1 Method is valid and supported
 * @retval 0 Method is invalid or unsupported
 *
 * @note Uses case-sensitive string comparison
 * @note HEAD method included for completeness but may not be fully implemented
 * @note POST and DELETE methods supported for CGI scripts
 *
 * @see parseRequest(), checkHttp()
 */
int
checkMethod(const char *meth_str)
{
	if (strcmp(meth_str, "GET") == 0)
		return (1);

	// I don't think I'll end up using this method, but it's in the spec
	if (strcmp(meth_str, "HEAD") == 0)
		return (1);

	if (strcmp(meth_str, "POST") == 0)
		return (1);

	if (strcmp(meth_str, "DELETE") == 0)
		return (1);

	// invalid method
	return (0);
}
