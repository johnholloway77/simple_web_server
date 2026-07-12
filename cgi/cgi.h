#ifndef CGI_H
#define CGI_H

/**
 * @brief Execute a CGI script and return its output as a heap-allocated string.
 *
 * Forks a child process, sets up pipe communication, and exec()s the named
 * script from the configured cgi-bin directory.  Query-string parameters are
 * decoded and passed to the child as environment variables.  The child
 * communicates its HTTP status back to the parent through a dedicated pipe
 * (RESPONSE_PIPE environment variable).
 *
 * Requires C_FLAG to be set in app_flags and cgi_addr to be initialised by
 * setFlags().  Returns a static error-response string (not heap-allocated) on
 * configuration errors; returns a heap-allocated string on success or on
 * allocation/exec failures — the caller must free() the return value only when
 * it differs from the static RESPONSE_* constants.
 *
 * @param file        CGI script filename, optionally followed by "?key=value"
 *                    pairs (the string is modified in place by strtok)
 * @param cgi_argc    Number of extra command-line arguments for the script
 * @param cgi_args    Array of extra argument strings passed to execvp()
 * @param resp_status Output: HTTP status code set by the child via the
 *                    response pipe, or an error code on failure
 * @return            Heap-allocated NUL-terminated string containing the
 *                    script's stdout output, or a static error-response
 *                    string on early failure
 */
char *cgiExe(char *file, int cgi_argc, char *cgi_args[], int *resp_status);

#endif // CGI_H
