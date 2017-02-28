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
                          uint8_t nb_neighbours);
#endif /* SERVER_COMMON_H */
