#include <errno.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <dirent.h>
#include <limits.h>
#include <time.h>
#include <unistd.h>
#include <sys/wait.h>

#include "./version_info.h"
#include "../client_conn/connections.h"
#include "../debug/debug.h"
#include "../flags/flags.h"
#include "../requests/resolve_path.h"
#include "../requests/parse_request.h"
#include "../response/build_response.h"

#define KEY_BUF 64
#define VALUE_BUF 256
#define MAX_HEADER_BUF 750

#define START_HTML_STRING "<html><body><h1>Directory:</h1><ul>"
#define END_HTML_STRING "</ul></body></html>"
#define HTML_STRING_LEN strlen(START_HTML_STRING) + strlen(END_HTML_STRING)

#if defined(__FreeBSD__) || defined(__APPLE__) || defined(__linux__)
#define HAVE_DIRENT_D_TYPE 1
#else
#define HAVE_DIRENT_D_TYPE 0
#endif

#define DIR_ICON                                                                                                                                   \
	"<svg xmlns=\"http://www.w3.org/2000/svg\" width=\"16\" height=\"16\" fill=\"currentColor\" class=\"bi bi-folder\" viewBox=\"0 0 16 16\">" \
	"<path d=\"M.54 3.87.5 3a2 2 0 0 1 2-2h3.672a2 2 0 0 1 1.414.586l.828.828A2 2 0 0 0 9.828 3h3.982a2 2 0 0 1 1.992 2.181l-.637 7A2 2 0 0 1 13.174 14H2.826a2 2 0 0 1-1.991-1.819l-.637-7a2 2 0 0 1 .342-1.31zM2.19 4a1 1 0 0 0-.996 1.09l.637 7a1 1 0 0 0 .995.91h10.348a1 1 0 0 0 .995-.91l.637-7A1 1 0 0 0 13.81 4zm4.69-1.707A1 1 0 0 0 6.172 2H2.5a1 1 0 0 0-1 .981l.006.139q.323-.119.684-.12h5.396z\"/></svg>"

#define FILE_ICON                                                                                                                                                      \
	"<svg xmlns=\"http://www.w3.org/2000/svg\" width=\"16\" height=\"16\" fill=\"currentColor\" class=\"bi bi-file-earmark\" viewBox=\"0 0 16 16\">"               \
	"<path d=\"M14 4.5V14a2 2 0 0 1-2 2H4a2 2 0 0 1-2-2V2a2 2 0 0 1 2-2h5.5zm-3 0A1.5 1.5 0 0 1 9.5 3V1H4a1 1 0 0 0-1 1v12a1 1 0 0 0 1 1h8a1 1 0 0 0 1-1V4.5z\"/>" \
	"</svg>"

#define MAX_ICON_LEN strlen(DIR_ICON)

extern const uint32_t app_flags;

static int
check_resolve_args(const Client *c, const ResolvedPath *rp, const Request *req)
{
	if (!c) {
		DBG("Null client pointer\n");
		return -1;
	}
	if (!rp) {
		DBG("NULL ResolvedPath pointer\n");
		return -1;
	}
	if (!req) {
		DBG("Null request pointer\n");
		return -1;
	}
	return 0;
}

static const char *
method_to_string(enum http_method m)
{
	switch (m) {
	case HTTP_GET:
		return "GET";
	case HTTP_POST:
		return "POST";
	case HTTP_HEAD:
		return "HEAD";
	default:
		return "UNKNOWN";
	}
}

static const char *
version_to_string(enum http_version v)
{
	switch (v) {
	case HTTP_1_0:
		return "HTTP/1.0";
	case HTTP_1_1:
		return "HTTP/1.1";
	case HTTP_VERSION_UNSUPPORTED:
		return "HTTP_UNSUPPORTED";
	default:
		return "HTTP_UNKNOWN";
	}
}

static int
build_response_cgi(Client *c, ResolvedPath *rp, Request *req)
{
	if (check_resolve_args(c, rp, req) != 0) {
		c->resp_val = RESP_500;
		return -1;
	}

	if (!(app_flags & C_FLAG)) {
		c->resp_val = RESP_501;
		return -1;
	}

	int stdout_pipe[2];
	if (pipe(stdout_pipe) == -1) {
		c->resp_val = RESP_500;
		return -1;
	}

	size_t query_len = strlen(rp->query_string);
	if (query_len > PATH_MAX) {
		c->resp_val = RESP_414;
		return -1;
	}

	pid_t pid = fork();

	if (pid == -1) {
		close(stdout_pipe[0]);
		close(stdout_pipe[1]);
		c->resp_val = RESP_500;
		return -1;
	}

	// check child process first
	if (pid == 0) {
		close(stdout_pipe[0]);
		dup2(stdout_pipe[1], STDOUT_FILENO);

		char query_buf[PATH_MAX + 1];
		char local_path_buf[PATH_MAX + 1];

		memcpy(query_buf, rp->query_string, query_len);
		query_buf[query_len] = '\0';

		local_path_buf[0] = '.';
		uint32_t i = 1;
		while (req->path[i - 1] && (req->path[i - 1] != '?')) {
			local_path_buf[i] = req->path[i - 1];
			i++;
		}
		local_path_buf[i] = '\0';

		setenv("QUERY_STRING", rp->query_string, 1);
		setenv("REQUEST_METHOD", method_to_string(req->method), 1);
		setenv("SCRIPT_NAME", local_path_buf, 1);
		setenv("SERVER_PROTOCOL", version_to_string(req->version), 1);
		setenv("GATEWAY_INTERFACE", "CGI/1.1", 1);
		setenv("REMOTE_ADDR", c->client_addr, 1);
		setenv("SERVER_SOFTWARE", SERVER_VERSION, 1);

		char *argv[] = {local_path_buf, NULL};
		execvp(local_path_buf, argv);

		perror("exeicv failure!");
		_exit(EXIT_FAILURE);
	}

	// in parent
	close(stdout_pipe[1]);

	// got to read from child! But this is better
	size_t capacity = 8192;
	size_t cgi_resp_len = 0;

	char *cgi_response = (char *)malloc(capacity);
	if (!cgi_response) {
		close(stdout_pipe[0]);
		c->resp_val = RESP_500;
		return -1;
	}

	for (;;) {
		if (cgi_resp_len == capacity) {
			capacity *= 2;
			char *tmp = realloc(cgi_response, capacity);
			if (!tmp) {
				DBG("CGI realloc fail!\n");
				free(cgi_response);
				close(stdout_pipe[0]);
				c->resp_val = RESP_500;
				return -1;
			}
			cgi_response = tmp;
		}

		ssize_t n = read(stdout_pipe[0],
		    cgi_response + cgi_resp_len,
		    capacity - cgi_resp_len);
		if (n > 0) {
			cgi_resp_len += (size_t)n;
			continue;
		}
		if (n == 0) {
			break; // End of file!
		}
		if (errno == EINTR) {
			continue;
		}

		// if reached error occurred
		free(cgi_response);
		close(stdout_pipe[0]);
		c->resp_val = RESP_500;
		return -1;
	}

	// clean up!
	close(stdout_pipe[0]);

	int status;
	if (waitpid(pid, &status, 0) == -1) {
		free(cgi_response);
		c->resp_val = RESP_500;
		return -1;
	}

	if (!WIFEXITED(status) || WEXITSTATUS(status) != 0) {
		DBG("CGI script exited abnormally: status=%d\n", status);
		free(cgi_response);
		c->resp_val = RESP_500;
		return -1;
	}

	char header[MAX_HEADER_BUF];
	int header_len = snprintf(header,
	    MAX_HEADER_BUF,
	    "HTTP/1.0 200 OK\r\n"
	    "Date: %s\r\n"
	    "Server: %s\r\n"
	    "Content-Type: text/html\r\n"
	    "Content-Length: %zu\r\n"
	    "Connection: close\r\n"
	    "\r\n",
	    req->time_received,
	    SERVER_VERSION,
	    cgi_resp_len);

	c->out_buf = malloc(header_len + cgi_resp_len);
	if (!c->out_buf) {
		free(cgi_response);
		c->resp_val = RESP_500;
		return -1;
	}

	memcpy(c->out_buf, header, header_len);
	memcpy(c->out_buf + header_len, cgi_response, cgi_resp_len);
	c->output_length = header_len + cgi_resp_len;
	c->body_len = cgi_resp_len;
	c->header_len = header_len;

	return 0;
}

static int
build_response_dir(Client *c, ResolvedPath *rp, Request *req)
{
	if (check_resolve_args(c, rp, req) != 0) {
		c->resp_val = RESP_500;
		return -1;
	}

	if (IS_DIR != rp->path_type) {
		c->resp_val = RESP_500;
		return -1;
	}

	char *dir_path = req->path + 1;
	unsigned long dir_path_len = strlen(dir_path);

	DBG("dir path: %s\n", dir_path);

	DIR *dir = NULL;
	struct dirent *dirp;
	char *response_body;
	int response_body_on_heap =
	    0; // to be used when dynamically allocating response

	char header[MAX_HEADER_BUF];
	int header_len;

	dir = opendir(dir_path);
	if (!dir) {
		DBG("Dir null!\n");
		closedir(dir);
		c->resp_val = RESP_500;
		return -1;
	}

	uint32_t count = 0;
	while ((dirp = readdir(dir)) != NULL) {
		if (dirp->d_name[0] != '.') {
			count++;
		}
	}

	if (count) {
		// To do!
		rewinddir(dir);

		size_t buf_capacity = HTML_STRING_LEN + dir_path_len +
				      (PATH_MAX + 64) * count;

#ifdef HAVE_DIRENT_D_TYPE
		buf_capacity += MAX_ICON_LEN;
#endif

		response_body = (char *)malloc(buf_capacity);

		if (!response_body) {
			DBG("Error allocating dynamic response body\n");
			closedir(dir);
			c->resp_val = RESP_500;
			return -1;
		}

		response_body_on_heap = 1;

		unsigned long offset = 0;

		offset += snprintf(response_body,
		    buf_capacity - offset,
		    START_HTML_STRING);

		while ((dirp = readdir(dir)) != NULL) {
			if (dirp->d_name[0] == '.') {
				continue;
			}

#if HAVE_DIRENT_D_TYPE
			const char *icon = (dirp->d_type == DT_DIR) ? DIR_ICON
								    : FILE_ICON;
#else
			const char *icon = "";
#endif

			offset += snprintf(response_body + offset,
			    buf_capacity - offset,
			    "<li>%s <a href=\"/%s/%s\">%s</a></li>",
			    icon,
			    dir_path,
			    dirp->d_name,
			    dirp->d_name);
		}

		offset += snprintf(response_body + offset,
		    buf_capacity - offset,
		    END_HTML_STRING);
	}
	else {
		response_body =
		    "<html><body><h2>Empty directory</h2></body></html>";
	}

	c->body_len = strlen(response_body);

	struct tm *utc_time = gmtime(&rp->last_mod);
	char mod_time_buf[32];
	strftime(mod_time_buf,
	    sizeof(mod_time_buf),
	    "%Y-%m-%dT%H:%M:%SZ",
	    utc_time);

	header_len = snprintf(header,
	    MAX_HEADER_BUF,
	    "HTTP/1.0 200 OK\r\n"
	    "Date: %s\r\n"
	    "Server: %s\r\n"
	    "Last-Modified: %s\r\n"
	    "Content-Type: text/HTML\r\n"
	    "Content-Length: %zu\r\n"
	    "Connection: close\r\n"
	    "\r\n",
	    req->time_received,
	    SERVER_VERSION,
	    mod_time_buf,
	    c->body_len);

	DBG("# of files in dir: %u\n", count);

	if (HTTP_HEAD == req->method) {
		c->out_buf = malloc(header_len + 1);
		if (!c->out_buf) {
			perror("error build response file: ");
			c->resp_val = RESP_500;
			return -1;
		}
		memcpy(c->out_buf, header, header_len);
		c->output_length = header_len;

		if (response_body_on_heap) {
			free(response_body);
		}

		closedir(dir);
		return 0;
	}

	c->out_buf = malloc(header_len + c->body_len + 1);
	if (!c->out_buf) {
		perror("error build response file: ");
		closedir(dir);
		c->resp_val = RESP_500;
		return -1;
	}

	memcpy(c->out_buf, header, header_len);
	memcpy(c->out_buf + header_len, response_body, c->body_len);

	c->output_length = header_len + c->body_len;

	// free dynamic response buffer if created
	if (response_body_on_heap) {
		free(response_body);
	}

	closedir(dir);
	return 0;
}

static int
build_response_file(Client *c, ResolvedPath *rp, Request *req)
{
	if (check_resolve_args(c, rp, req) != 0) {
		c->resp_val = RESP_500;
		return -1;
	}

	char header[MAX_HEADER_BUF];
	struct tm *utc_time = gmtime(&rp->last_mod);
	char mod_time_buf[32];
	strftime(mod_time_buf,
	    sizeof(mod_time_buf),
	    "%Y-%m-%dT%H:%M:%SZ",
	    utc_time);

	int header_len = snprintf(header,
	    MAX_HEADER_BUF,
	    "HTTP/1.0 200 OK\r\n"
	    "Date: %s\r\n"
	    "Server: %s\r\n"
	    "Last-Modified: %s\r\n"
	    "Content-Type: %s\r\n"
	    "Content-Length: %zu\r\n"
	    "Connection: close\r\n"
	    "\r\n",
	    req->time_received,
	    SERVER_VERSION,
	    mod_time_buf,
	    rp->mime_type,
	    rp->file_size);

	if (HTTP_HEAD == req->method) {
		c->out_buf = malloc(header_len + 1);
		if (!c->out_buf) {
			perror("error build response file: ");

			c->resp_val = RESP_500;
			return -1;
		}
		memcpy(c->out_buf, header, header_len);
		c->output_length = header_len;
		c->body_len = c->file_size;

		return 0;
	}

	DBG("Header:\n%s\n", header);

	DBG("Client info:\n"
	    "c->file_size = %zu\n",
	    c->file_size);

	c->out_buf = malloc(header_len + c->file_size + 1);
	if (!c->out_buf) {
		perror("error build response file: ");
		c->resp_val = RESP_500;
		return -1;
	}

	memcpy(c->out_buf, header, header_len);

	size_t n = fread(c->out_buf + header_len, 1, c->file_size, c->file_ptr);

	c->output_length = header_len + n;
	c->body_len = c->file_size;

	return 0;
}

int
build_okay_response(Client *c, ResolvedPath *rp, Request *req)
{
	c->resp_val = RESP_200;

	int status = 0;

	if (IS_CGI == rp->path_type) {
		status = build_response_cgi(c, rp, req);
	}
	else if (IS_DIR == rp->path_type) {
		status = build_response_dir(c, rp, req);
	}
	else if (IS_FILE == rp->path_type) {
		status = build_response_file(c, rp, req);
	}

	if (status < 0) {
		// shit something went wrong!
		DBG("shit something went wrong!\n");
		if (c->resp_val == RESP_200) {
			c->resp_val = RESP_500;
		}
		c->output_length = 0;
		c->header_len = 0;
		c->body_len = 0;
		c->file_size = 0;
		if (c->out_buf) {
			free(c->out_buf);
		}
		return build_error_response(c, req);
	}

	return status;
}
