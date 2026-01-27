#include <sys/socket.h>

#include <arpa/inet.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include "../flags/flags.h"
#include "../requests/requests.h"
#include "./socket.h"

#define BUFFER_SIZE 1024

extern uint32_t app_flags;
extern char *log_addr;

/**
 * @brief Process HTTP request and send response
 *
 * Handles a complete HTTP request-response cycle for a single client
 * connection. Reads the HTTP request, parses it, generates an appropriate
 * response, and sends both headers and content back to the client.
 *
 * Features:
 * - Client IP address logging (IPv4 and IPv6 support)
 * - UTC timestamp generation for log entries
 * - HTTP request parsing and validation
 * - File content streaming in chunks
 * - Request/response logging to stdout (debug mode) or file
 * - Proper connection cleanup and process termination
 *
 * @param[in] fd Client connection socket file descriptor
 * @param[in] client Client address information (IPv4 or IPv6)
 * @param[in] sockType Socket type (TYPE_SOCK_V4 or TYPE_SOCK_V6)
 * @param[in] magic libmagic handle for MIME type detection
 *
 * @note This function runs in a child process and calls exit()
 * @note Logs format: "IP timestamp "request" status bytes_sent"
 * @note Closes connection and exits on read errors
 * @note Uses memset_s() for secure memory clearing
 *
 * @see handleSocket(), parseRequest()
 */
void
handleConnection(int fd,
    union sockaddr_union *client,
    enum sockType sockType,
    magic_t magic)
{
	const char *rip;
	char claddr[INET6_ADDRSTRLEN];
	int bytes_sent = 0;
	int rval;
	int resp_status;
	time_t current_time;
	struct tm *utc_time;
	char timestamp[21];

	if ((app_flags & D_FLAG) || (app_flags & L_FLAG)) {
		current_time = time(NULL);
		utc_time = gmtime(&current_time);

		strftime(timestamp,
		    sizeof(timestamp),
		    "%Y-%m-%dT%H:%M:%SZ",
		    utc_time);
	}

	memset_s(claddr, INET6_ADDRSTRLEN, 0, INET6_ADDRSTRLEN);

	if (sockType == TYPE_SOCK_V4) {
		if ((rip = inet_ntop(PF_INET,
			 &client->client_v4.sin_addr,
			 claddr,
			 INET_ADDRSTRLEN)) == NULL) {
			rip = "Unknown";
		}
	}
	else if (sockType == TYPE_SOCK_V6) {
		if ((rip = inet_ntop(PF_INET6,
			 &client->client_v6.sin6_addr,
			 claddr,
			 INET6_ADDRSTRLEN)) == NULL) {
			rip = "Unknown";
		}
	}

	char buf[BUFSIZ];
	memset_s(&buf, BUFSIZ, 0, BUFSIZ);

	rval = read(fd, buf, sizeof(buf) - 1);
	if (rval < 0) {
		perror("read");
		close(fd);
		exit(EXIT_FAILURE);
	}

	if (rval > 0) {
		/* ensure string is null terminated */
		buf[rval] = '\0';

		/* gets the first line of the request */
		char *req_token = strtok(buf, "\r\n");

		FILE *file_ptr = NULL;
		char *response =
		    parseRequest(req_token, &file_ptr, &resp_status, magic);

		bytes_sent += send(fd, response, strlen(response), 0);

		if (file_ptr) {
			char buffer[BUFFER_SIZE];
			size_t bytes_read;
			while ((bytes_read = fread(buffer,
				    sizeof(char),
				    BUFFER_SIZE,
				    file_ptr)) > 0) {
				if (send(fd, buffer, bytes_read, 0) < 0) {
					break;
				}

				bytes_sent += bytes_read;
			}

			fclose(file_ptr);
		}
		if (app_flags & D_FLAG) {
			fprintf(stdout,
			    "%s %s \"%s\" %d %d\n",
			    rip,
			    timestamp,
			    req_token,
			    resp_status,
			    bytes_sent);
		}

		if (app_flags & L_FLAG) {
			FILE *log_ptr = fopen(log_addr, "a");
			if (log_ptr == NULL) {
				perror("Unable to create logfile: ");
				exit(EXIT_FAILURE);
			}
			fprintf(log_ptr,
			    "%s %s \"%s\" %d %d\n",
			    rip,
			    timestamp,
			    req_token,
			    resp_status,
			    bytes_sent);

			fclose(log_ptr);
		}

		free(response);
		(void)close(fd);
	}
	else {
		if (app_flags & D_FLAG) {
			fprintf(stdout,
			    "%s %s ERROR: Unable to read http request %d\n",
			    rip,
			    timestamp,
			    bytes_sent);
		}
	}

	exit(EXIT_SUCCESS);
}
