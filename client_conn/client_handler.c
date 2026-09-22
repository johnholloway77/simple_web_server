
#include <magic.h>
#include <poll.h>
#include <sys/socket.h>
#include <sys/types.h>

#include "../client_conn/connections.h"
#include "../client_conn/do_read.h"
#include "../debug/debug.h"
#include "../flags/flags.h"
#include "../logging/logging.h"
#include "../requests/parse_request.h"
#include "../requests/resolve_path.h"
#include "../response/build_response.h"
#include "../logging/logging.h"



extern const uint32_t app_flags;

void handle_client(int i, Client *clients, struct pollfd pfds[], magic_t magic)
{
	Client *c = &clients[i];
	struct pollfd *p = &pfds[i];

	fprintf(stderr,
           "ENTER handle_client: fd=%d state=%d "
           "input_length=%zu capacity=%zu\n",
           c->fd,
           c->state,
           c->input_length,
           c->input_capacity);

	Reading_state reading_state = read_request(c);


	switch (reading_state){

	case READ_EMPTY:
	case READ_INCOMPLETE:
	    return;

	case READ_PEER_CLOSED:
	printf("Read closed!\n");

	case READ_ERROR:
	    c->state = CLOSING;
		printf("Read error!\n");
		return;

	case READ_COMPLETE:
		break;
	}

	if (c->header_slice.start == NULL || c->header_len == 0){
	    fprintf(stderr, "header slice empty! Fix this error check later with a proper error!");
		exit(EXIT_FAILURE);
	}

	parse_header_2(c);

	if (c->headers.request_line.start != NULL) {
    printf("Request line: ");
    fwrite(c->headers.request_line.start, 1,
        c->headers.request_line.length, stdout);
    putchar('\n');
	}

	if (c->headers.host.start != NULL) {
    printf("Host: ");
    fwrite(c->headers.host.start, 1,
        c->headers.host.length, stdout);
    putchar('\n');
	}

	if (c->headers.content_length.start != NULL) {
    printf("Content-Length: ");
    fwrite(c->headers.content_length.start, 1,
        c->headers.content_length.length, stdout);
    putchar('\n');
	}

	if (c->headers.content_type.start != NULL) {
    printf("Content-Type: ");
    fwrite(c->headers.content_type.start, 1,
        c->headers.content_type.length, stdout);
    putchar('\n');
	}

	if (c->headers.user_agent.start != NULL) {
    printf("User-Agent: ");
    fwrite(c->headers.user_agent.start, 1,
        c->headers.user_agent.length, stdout);
    putchar('\n');
	}

	if (c->headers.connection.start != NULL) {
    printf("Connection: ");
    fwrite(c->headers.connection.start, 1,
        c->headers.connection.length, stdout);
    putchar('\n');
	}

	if (c->headers.accept.start != NULL) {
    printf("Accept: ");
    fwrite(c->headers.accept.start, 1,
        c->headers.accept.length, stdout);
    putchar('\n');
	}

	if (c->headers.accept_Language.start != NULL) {
    printf("Accept-Language: ");
    fwrite(c->headers.accept_Language.start, 1,
        c->headers.accept_Language.length, stdout);
    putchar('\n');
	}

	if (c->headers.accept_encoding.start != NULL) {
    printf("Accept-Encoding: ");
    fwrite(c->headers.accept_encoding.start, 1,
        c->headers.accept_encoding.length, stdout);
    putchar('\n');
	}


	if (c->headers.request_fields.method.start != NULL){
	    printf("Request tokens:\n");

		printf("Method: ");
		fwrite(c->headers.request_fields.method.start, 1,c->headers.request_fields.method.length, stdout);
		putchar('\n');

		if (c->headers.request_fields.uri.start != NULL){
		    printf("URI: ");
						fwrite(c->headers.request_fields.uri.start, 1,c->headers.request_fields.uri.length, stdout);
						putchar('\n');
		}

		if (c->headers.request_fields.version.start != NULL){
		    printf("Version: ");
						fwrite(c->headers.request_fields.version.start, 1,c->headers.request_fields.version.length, stdout);
						putchar('\n');
		}
	}

	Request req = {0};
	ResolvedPath rp = {0};
	LogEntry le = {0};

	if ((app_flags & V_FLAG) || (app_flags & L_FLAG)) {
		log_append(&le, "%s ", c->client_addr);
	}

	if (parse_request(c->in_buf, c->header_len, &req, &c->resp_val, &le) ==
		0 &&
	    resolve_path(c, &req, &rp, magic) == 0) {
		// TO DO
		build_okay_response(c, &rp, &req);
	}
	else {
		build_error_response(c, &req);

		DBG("successfully built error response for client:\n\n%s\n",
		    c->out_buf);
	}

	if ((app_flags & V_FLAG) || (app_flags & L_FLAG)) {
		log_append(&le,
		    "%s %d\n",
		    resp_val_to_status_string(c->resp_val),
		    c->body_len);
		DBG("Writing log\n");
		do_logging(&le);
	}

	close_resolve_path_ptr(&rp);

	p->events = POLLOUT;
	c->state = SENDING_HEADER;
}
