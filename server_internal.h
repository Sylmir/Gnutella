#ifndef SERVER_INTERNAL_H
#define SERVER_INTERNAL_H


#include "list.h"
#include "server_defines.h"


typedef struct list_s list_t;
struct sockaddr;
struct sockaddr_storage;
typedef uint16_t in_port_t;

/* The structure to represent the server. */
typedef struct server_s {
    /* Socket to wait for new connexions. */
    int listening_socket;
    /* Array of sockets representing the neighbours. */
    int neighbours[MAX_NEIGHBOURS];
    /* Our current number of neighbours. */
    int nb_neighbours;
    /* Socket to communicate with the client. */
    int client_socket;
    /* Indicate if we performed the handshake. */
    int handshake;
    /* Awaiting sockets (i.e, we accepted but we have not yet dealt with them). */
    list_t* awaiting_sockets;
} server_t;


/*
 * Join the P2P network. This is done by first requesting a list of neighbours
 * from the contact point. Then, for each possible neighbour we will ask if we
 * can join. The server will then answer with 'yes' or 'no'.
 *
 * For each neighbour we get, we store a socket inside server->neighbours.
 *
 * The function will return 0 on success, -1 on failure.
 *
 * Note: this function is a helper function used to reduce the amont of if / else
 * int run_server. It basically calls join_network_through with the correct
 * parameters depending on if ip and / or port are NULL.
 */
int join_network(server_t* server, const char* ip, const char* port);


/*
 * Actively join the P2P network (join_network acting as a helper function). The
 * function will return 0 on success, and -1 on failure.
 */
int join_network_through(server_t* server, const char* ip, const char* port);


/*
 * Extract the next neighbour from socket (assuming we are handling SMSG_NEIGHBOURS_REPLY).
 * The IP and port are stored inside ip and port.
 */
void extract_neighbour_from_response(int socket, char **ip, char **port);


/*
 * Extract the IP and port bound to socket and store them inside ip and port.
 */
void extract_neighbour_from_socket(int socket, char** ip, char** port);


/*
 * Helper to avoid redundancy. Send a join request to ip:port and store the
 * communicating socket inside sockets. ip and port are both freed at the
 * end of the function (no, I'm not going to make a version where the free
 * is not called, just malloc your damn buffers).
 */
void join(const char* ip, const char* port, list_t* sockets);


/*
 * Compute a list of neighbours to send through socket.
 */
void compute_and_send_neighbours(server_t* server, int socket);


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


/*******************************************************************************
 * Packets handling.
 */


/*
 * Send a request to host:ip to initiate a communication. The function will
 * return the socket used to communicate with the server.
 */
int send_neighbours_request(const char* ip, const char* port);

/* Max number of attempts to retrieve the neighbours of someone. */
#define GET_NEIGHBOURS_MAX_ATTEMPTS 3
/* Sleep time when we try to get neighbours (seconds). */
#define GET_NEIGHBOURS_SLEEP_TIME 1


/*
 * Send the join request to ip:host. If ip:host can accept the join, we return
 * the socket used to communicate. Otherwise, we return -1. rescue indicates if
 * the rescue flag is to be set to 1 or 0.
 */
int send_join_request(const char* ip, const char* port, uint8_t rescue);


/*
 * Answer to a join request through the socket.
 */
void answer_join_request(int socket);


/*
 * Add a copy-in-heap of socket inside a list (creating function).
 */
LIST_CREATE_FN void* add_new_socket(void* socket);

#endif /* SERVER_INTERNAL_H */
