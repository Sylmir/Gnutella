#ifndef NETWORKING_H
#define NETWORKING_H

struct sockaddr;
struct pollfd;

#include <sys/types.h>

typedef __socklen_t socklen_t;

#include "common.h"

/*
 * Attemp to connect to a server on IP adress ip, on port port, and store the
 * communicating socket in sock.
 *
 * The function will return one of the CONNECT_-family error codes to indicate
 * what happened. If the function does not return CONNECT_OK, *sock is not
 * modified.
 */
int connect_to(const char* ip, const char* port, int* sock);

/* Everything OK. */
#define CONNECT_OK                  -1
/* getaddrinfo failed. */
#define CONNECT_ERROR_NO_ADDRINFO   -2
/* getnameinfo failed. */
#define CONNECT_ERROR_NO_NAMEINFO   -3
/* No valid socket was found. */
#define CONNECT_ERROR_NO_SOCKET     -4


/*
 * Performs up to nb_attempt attemps to connect on ip:port. If one attemps
 * succeeds (as in "connect_to returns CONNECT_OK", the communicating socket
 * is stored in sock. sleep_time indicate the time to wait (in seconds) before
 * attempting again.
 *
 * If one attemp succeeds, the function returns 0 ; otherwise, if all attempts
 * fail, the function returns -1.
 */
ERROR_CODES_USUAL int attempt_connect_to(const char* ip,
                                         const char* port,
                                         int* sock, int nb_attempt,
                                         int sleep_time);


/*
 * Create a socket that will listen on port, ready to handle a maximum of
 * max_requests requests. On success, the function will return the socket,
 * on failure it will return one of the CL_-family error code (< 0).
 */
int create_listening_socket(const char* port, int max_requests,
                            char* host_name, char* port_number);

/* getaddrinfo failed. */
#define CL_ERROR_NO_ADDRINFO    -1
/* getnameinfo failed. */
#define CL_ERROR_NO_NAMEINFO    -2
/* No socket was created. */
#define CL_ERROR_NO_SOCKET      -3
/* listen failed. */
#define CL_ERROR_NO_LISTEN      -4


/*
 * Attempt to accept() an incoming connection on the listening_socket. This is
 * done by first creating a pollfd structure that will watch the socket for
 * a POLLIN event, waiting up to timeout milliseconds before stopping.
 *
 * If there was something to read, the function subsequently calls accept()
 * and return it's result, storing the informations inside addr and addr_len.
 * If there was nothing to read, the function return ACCEPT_ERR_TIMEOUT.
 */
int attempt_accept(int listening_socket, int timeout,
                   struct sockaddr* addr, socklen_t* addr_len);


/* Timeout reached. */
#define ACCEPT_ERR_TIMEOUT -2

#endif /* NETWORKING_H */
