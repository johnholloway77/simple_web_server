
#include <stdio.h>
#include <stdarg.h>
#include <stdint.h>

#include "../client_conn/connections.h"
#include "../logging/logging.h"
#include "../debug/debug.h"
#include "../flags/flags.h"

extern const uint32_t app_flags;
extern FILE *log_ptr;

/**
 * @brief Map a client_response enum value to its numeric HTTP status code
 *        as a string literal, for use in log lines and response headers.
 *
 * @param[in] resp The response enum value to convert
 *
 * @return A static string literal (e.g. "200", "404"). Returns "000" for
 *         any value outside the known range (including NUM_CLIENT_RESP),
 *         so a caller printing this into a log line gets an obviously
 *         invalid placeholder rather than undefined/garbage text.
 */
const char *
resp_val_to_status_string(enum client_response resp)
{
	switch (resp) {
	case RESP_200:
		return "200";
	case RESP_400:
		return "400";
	case RESP_403:
		return "403";
	case RESP_404:
		return "404";
	case RESP_414:
		return "414";
	case RESP_500:
		return "500";
	case RESP_501:
		return "501";
	case RESP_505:
		return "505";
	case NUM_CLIENT_RESP:
	default:
		return "000";
	}
}

void
log_append(LogEntry *le, const char *fmt, ...)
{
	va_list args;
	va_start(args, fmt);
	int n = vsnprintf(le->buf + le->offset,
	    LOG_BUF_SIZE - le->offset,
	    fmt,
	    args);
	va_end(args);

	le->offset += (size_t)n;
}

int
do_logging(LogEntry *le)
{
	// FILE *tmp;

	// if (app_flags & L_FLAG) {
	// 	if (!log_ptr) {
	// 		return -1;
	// 	}
	// 	tmp = log_ptr;
	// }
	// else {
	// 	tmp = stdout;
	// }

	fprintf(log_ptr, "%s", le->buf);

	return 0;
}
