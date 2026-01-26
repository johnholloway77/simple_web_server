#include <sys/socket.h>

#include <netinet/in.h>

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "./socket.h"

/**
 * @brief Accept new connection and fork child process to handle it
 *
 * Accepts an incoming connection on the specified listening socket and
 * creates a child process to handle the connection. Uses a union to
 * support both IPv4 and IPv6 client address structures.
 *
 * The parent process closes the connection file descriptor and continues
 * listening, while the child process handles the HTTP request processing
 * through handleConnection().
 *
 * @param[in] sock Listening socket file descriptor
 * @param[in] sockType Type of socket (TYPE_SOCK_V4 or TYPE_SOCK_V6)
 * @param[in] magic libmagic handle for MIME type detection
 *
 * @note Function returns on accept() failure without terminating program
 * @note Child process calls handleConnection() and then exits
 * @note Parent process continues execution after closing client socket
 * @note Program exits on fork() failure
 *
 * @see handleConnection(), createSocket_v4(), createSocket_v6()
 */
void
handleSocket(int sock, enum sockType sockType, magic_t magic)
{
	int fd;
	pid_t pid;
	socklen_t length;

	// I'm using a union as an excuse to practice with them and learn more.
	union sockaddr_union client;

	length = (sockType == TYPE_SOCK_V4) ? sizeof(client.client_v4)
					    : sizeof(client.client_v6);

	if ((fd = accept(sock,
		 (struct sockaddr *)(sockType == TYPE_SOCK_V4
					 ? (void *)&client.client_v4
					 : (void *)&client.client_v6),
		 &length)) < 0) {
		perror("accept");
		return; // -1;
	}

	if ((pid = fork()) < 0) {
		perror("fork");
		exit(EXIT_FAILURE);
	}
	else if (!pid) {
		handleConnection(fd, &client, sockType, magic);
	}
	else {
		// if parent close fd
		(void)close(fd);
	}
};
