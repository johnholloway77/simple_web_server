#include <netinet/in.h>
#include <poll.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/_sockaddr_storage.h>
#include <sys/socket.h>
#include "./connections.h"
#include <stdlib.h>

#include "../debug/debug.h"

/**
 * @brief Remove a client connection and compact the poll/client arrays.
 *
 * See connections.h for full documentation.
 */
void
close_conn(int i, int *fd_count, struct pollfd pfds[], Client clients[])
{
	DBG("Closing connection with fd %d\n", clients[i].fd);

	if (0 == i) {
		fprintf(stderr, "Close_connect listener!");
		return;
	}

	close(clients[i].fd);
	if (clients[i].file_ptr) {
		fclose(clients[i].file_ptr);
	}
	free(clients[i].in_buf);
	free(clients[i].out_buf);

	// Don't swap if i is last index, avoid self-memcpy
	if (i != *fd_count - 1) {
		pfds[i] = pfds[*fd_count - 1];
		clients[i] = clients[*fd_count - 1];
	}
	(*fd_count)--;
}

/**
 * @brief Add a newly accepted socket to the poll and client arrays.
 *
 * See connections.h for full documentation.
 */
int
add_to_lists(struct pollfd **pfds,
    Client **clients,
    int newfd,
    int *fd_count,
    int *fd_size)
{
	DBG("Adding newfd %d to list\n", newfd);

	// Increase size of array if we do not have enough room
	if (*fd_count == *fd_size) {
		*fd_size *= 2;

		struct pollfd *pfd_tmp = realloc(*pfds,
		    sizeof(**pfds) * (*fd_size));
		if (NULL == pfd_tmp) {
			fprintf(stderr, "pollfd realloc");
			exit(EXIT_FAILURE);
		}
		*pfds = pfd_tmp;

		Client *clients_tmp = realloc(*clients,
		    sizeof(Client) * (*fd_size));
		if (NULL == clients_tmp) {
			fprintf(stderr, "Clients realloc");
			exit(EXIT_FAILURE);
		}
		*clients = clients_tmp;
	}

	int index = *fd_count;

	(*pfds)[index].fd = newfd;
	(*pfds)[index].events = POLLIN; // check ready to use
	(*pfds)[index].revents = 0;

	(*clients)[index].fd = newfd;
	(*clients)[index].state = READING;
	(*clients)[index].in_buf = NULL;
	(*clients)[index].input_capacity = 0;
	(*clients)[index].input_length = 0;
	(*clients)[index].out_buf = NULL;
	(*clients)[index].output_capacity = 0;
	(*clients)[index].output_length = 0;

	(*clients)[index].file_ptr = NULL;
	(*clients)[index].resp_val = NUM_CLIENT_RESP; // Use for unknown/not set

	(*fd_count)++;

	return index; // this is the client/pfds we just added to the lists!
}
