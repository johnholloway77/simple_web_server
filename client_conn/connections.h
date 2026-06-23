#pragma once

#include <poll.h>
#include <stdio.h>
#include <netinet/in.h>
#include <stdlib.h>
#include <stddef.h>
#include <time.h>

enum client_state
{
	READING,
	PROCESSING,
	SENDING_HEADER,
	SENDING_BODY,
	CLOSING,
	NUM_CLIENT_STATE,
};

enum client_response
{
	RESP_200, // Okay
	RESP_400, // Bad Request
	RESP_403, // Forbidden
	RESP_404, // Not found
	RESP_500, // Internal Error
	RESP_501, // CGI Not Enabled

	NUM_CLIENT_RESP,
};

typedef struct Client
{
	char *in_buf;
	char *out_buf;
	FILE *file_ptr;

	size_t input_length;
	size_t input_capacity;
	size_t output_length;
	size_t output_capacity;
	size_t header_len;

	int fd;
	enum client_state state;
	enum client_response resp_val;

	char timestamp[24];
	char client_addr[INET6_ADDRSTRLEN];
} Client;

int add_to_lists(struct pollfd **pfds,
    Client **clients,
    int newfd,
    int *fd_count,
    int *fd_size);

void close_conn(int i, int *fd_count, struct pollfd pfds[], Client clients[]);

void accept_new_conn(int listener_fd,
    struct pollfd **pfds,
    Client **clients,
    int *fd_count,
    int *fd_size);
