#ifndef SERVER_COMMON_H
#define SERVER_COMMON_H

#include <stdint.h>

typedef uint16_t in_port_t;

struct sockaddr;

/*
 * Time interval to check if we have an incoming request on the listening
 * socket. This is in milliseconds.
 */
#define ACCEPT_TIMEOUT 100


/* Represent the status of a given socket. */
typedef enum socket_status_e {
    /* Socket is closed. */
    SOCKET_CLOSED   = 0,
    /* Server accepted connection but is still waiting to perform it's handshake. */
    SOCKET_WAIT     = 1,
    /* Server accepted the connection and is ready. */
    SOCKET_READY    = 2
} socket_status_t;


/* Associate a status with a given socket. */
typedef struct socket_s {
    /* The socket. */
    int socket;
    /* Status of the socket. */
    socket_status_t status;
} socket_t;


/*
 * Extract the IP adress from addr if it's family is AF_INET or AF_INET6. The
 * result of this function is the same as if inet_ntop was called with the
 * correct arguments to extract the IP from addr casted to the appropriate
 * structure.
 *
 * This means that you must free the pointer returned by the function.
 */
const char* extract_ip(const struct sockaddr_storage* addr, in_port_t* port);


/*
 * Same as extract_ip, works with struct sockaddr instead.
 */
const char* extract_ip_classic(const struct sockaddr* addr, in_port_t* port);


/*
 * Send the neighbours to the client on the "other side" of socket.
 */
void send_neighbours_list(int socket, struct sockaddr* neighbours,
                          int nb_neighbours);
#endif /* SERVER_COMMON_H */
