#include "../client_conn/connections.h"
#include "../debug/debug.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define HTML_RESPONSE "text/html"
#define TEXT_RESPONSE "text/plain"
#define MAX_RESPONSE_BUF 750

typedef struct Response
{
	const char *status_line;
	const char *mime_type;
	const char *body;
} Response;

static const Response error_responses[] = {[RESP_400] = {"400 Bad Request",
					       TEXT_RESPONSE,
					       ""},
    [RESP_403] = {"403 Forbidden", TEXT_RESPONSE, ""},
    [RESP_404] = {"404 Not Found",
	HTML_RESPONSE,
	"<html><body><h1>404 Not Found</h1>"
	"<h3>Sorry, the file you are looking for doesn't exist</h3>"
	"<br><br><p>Or does it?!?!</p><br><br><br><br>"
	"<p>No... it doesn't</p></body></html>"},
    [RESP_414] = {"414 URI Too Long", TEXT_RESPONSE, ""},
    [RESP_500] = {"500 Internal Server Error", TEXT_RESPONSE, ""},
    [RESP_501] = {"501 Not Implemented",
	HTML_RESPONSE,
	"<html><body><h1>501 CGI Not Enabled on Server</h1>"
	"<h3>CGI application requires the server to be run with the CGI flag</h3>"
	"<p>Please restart the simple_server application with the -c flag "
	"and the location of the cgi-bin directory</p>"
	"<p><b>Eg:<br/></b> ./simple_server -c ./cgi-bin</p></body></html>"},
    [RESP_505] = {"505 HTTP Version Not Supported", TEXT_RESPONSE, ""}};

int
build_error_response(Client *c)
{
	if (NULL == c) {
		perror("build error - Null client");
		return -1;
	}

	if (c->resp_val < RESP_400 || c->resp_val >= NUM_CLIENT_RESP) {
		DBG("Build Error Response called for incorrect response num");
		return -1;
	}

	Response error_resp = error_responses[c->resp_val];

	c->out_buf = (char *)malloc(MAX_RESPONSE_BUF);
	if (!c->out_buf) {
		perror("error response malloc");
		return -1;
	}

	int resp_len = snprintf(c->out_buf,
	    MAX_RESPONSE_BUF,
	    "HTTP/1.0 %s\r\n"
	    "Content-Type: %s\r\n"
	    "Content-Length: %zu\r\n"
	    "Connection: close\r\n"
	    "\r\n"
	    "%s",
	    error_resp.status_line,
	    error_resp.mime_type,
	    strlen(error_resp.body),
	    error_resp.body);

	if (resp_len < 0 || (size_t)resp_len >= MAX_RESPONSE_BUF) {
		DBG("error response truncated");
		free(c->out_buf);
		c->out_buf = NULL;
		return -1;
	}

	c->output_length = resp_len;

	return 0;
}
