#ifndef SOCKET_H
#define SOCKET_H

/**
 * @brief Create and bind a non-blocking IPv4 listening socket.
 *
 * @return File descriptor of the listening socket; exits on failure.
 */
int get_listener_v4(void);

/**
 * @brief Create and bind a non-blocking IPv6 listening socket.
 *
 * @return File descriptor of the listening socket; exits on failure.
 */
int get_listener_v6(void);

#endif // SOCKET_H
