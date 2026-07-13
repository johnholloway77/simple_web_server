#pragma once

#include <stddef.h>

#include "../client_conn/connections.h"
#include "../requests/max_request.h"

#define LOG_BUF_SIZE (MAX_REQUEST_SIZE + 128)

const char *resp_val_to_status_string(enum client_response resp);

typedef struct LogEntry
{
	char buf[LOG_BUF_SIZE];
	size_t offset;
} LogEntry;

void log_append(LogEntry *le, const char *fmt, ...);

int do_logging(LogEntry *le);
