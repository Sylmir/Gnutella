#ifndef UTIL_H
#define UTIL_H

#include <stdlib.h>

#include "common.h"


/*
 * Write the content of buffer up to size bytes in the file designed by fd.
 * If an error occurs during one write, the function returns -1, otherwise
 * it returns 0.
 *
 * Note: if buffer isn't at least size bytes large, the behaviour is undefined.
 *
 * See also: write(2).
 */
ERROR_CODES_USUAL int write_to_fd(int fd, void* buffer, size_t size);


/*
 * Read up to size bytes from the file designed by fd and stores it in buffer.
 * If an error occurs during one read, the function returns -1, otherwise it
 * returns 0.
 *
 * Note: if buffer isn't at least size bytes large, the behaviour is undefined.
 *
 * See also: read(2).
 */
ERROR_CODES_USUAL int read_from_fd(int fd, void* buffer, size_t size);


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
#define CONNECT_OK                  0
/* getaddrinfo failed. */
#define CONNECT_ERROR_NO_ADDRINFO   1
/* getnameinfo failed. */
#define CONNECT_ERROR_NO_NAMEINFO   2
/* No valid socket was found. */
#define CONNECT_ERROR_NO_SOCKET     3


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
int create_listening_socket(const char* port, int max_requests);

/* getaddrinfo failed. */
#define CL_ERROR_NO_ADDRINFO    -1
/* getnameinfo failed. */
#define CL_ERROR_NO_NAMEINFO    -2
/* No socket was created. */
#define CL_ERROR_NO_SOCKET      -3
/* listen failed. */
#define CL_ERROR_NO_LISTEN      -4

#endif /* UTIL_H */
