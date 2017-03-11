#ifndef SERVER_INTERNAL_H
#define SERVER_INTERNAL_H

#include <stdint.h>
#include <stdlib.h>

#include "list.h"
#include "request.h"
#include "server_defines.h"


typedef struct list_s list_t;
struct sockaddr;
struct sockaddr_storage;
typedef uint16_t in_port_t;


typedef int socket_t;


/*
 * Helper structure, storing a socket, and the port to contact the machine
 * at the other extremity (used when sending neighbours).
 */
typedef struct socket_contact_s {
    socket_t sock;
    /* Port to connect to the machine where the other extremity of the socket
     * is present. */
    char* port;
} socket_contact_t;


/* The structure to represent the server. */
typedef struct server_s {
    /* Socket to wait for new connexions. */
    int listening_socket;
    /* Array of sockets representing the neighbours. */
    socket_contact_t neighbours[MAX_NEIGHBOURS];
    /* Our current number of neighbours. */
    int nb_neighbours;
    /* Socket to communicate with the client. */
    int client_socket;
    /* Indicate if we performed the handshake. */
    int handshake;
    /* Awaiting sockets (i.e, we accepted but we have not yet dealt with them). */
    list_t* awaiting_sockets;
    /* Pending requests. */
    list_t* pending_requests;
    /* Counter to indicate how many packets we send (identify them). */
    uint32_t packet_counter;
    /* Our own IP. */
    char* self_ip;
} server_t;


/*******************************************************************************
 * Join the network
 */

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
 *
 * The function will call itself recursively up to nb_attempts time (counting
 * from the first call). This prevents us from looping indefinitely searching
 * neighbours, but also increases our chances of finding at least MIN_NEIGHBOURS
 * neighbours.
 *
 * If we end up with at least MIN_NEIGHBOURS at one point, the recursion stops.
 */
int join_network_through(server_t* server, const char* ip, const char* port,
                         int nb_attempts);


/*
 * Extract the next neighbour from socket (assuming we are handling SMSG_NEIGHBOURS_REPLY).
 * The IP and port are stored inside ip and port.
 */
void extract_neighbour_from_response(socket_t s, char **ip, char **port);


/*
 * Helper to avoid redundancy. Send a join request to ip:port and store the
 * communicating socket inside sockets. ip and port are both freed at the
 * end of the function (no, I'm not going to make a version where the free
 * is not called, just malloc your damn buffers).
 */
void join(server_t* server, const char* ip, const char* port, list_t* sockets,
          int force);


/*
 * For each socket in targets, handle the SMSG_JOIN.
 */
void handle_join_responses(server_t* server, list_t* targets);


/*
 * Read SMSG_JOIN. Basically, this function just indicates if the servent
 * accepted or refused the request, it won't add the the socket inside our
 * neighbours.
 *
 * Return 0 if the servent refused the request, return 1 if the servent accepted
 * the request.
 */
int handle_join_response(socket_t s);


/*
 * Compute a list of neighbours to send through socket.
 */
void compute_and_send_neighbours(server_t* server, socket_t s);


/*
 * Add the socket if we can handle more neighbours.
 */
void add_neighbour(server_t* server, socket_t s, const char* contact_port);


/*
 * Handle CMSG_JOIN, i.e determine if we are going to accept this request, and
 * if we accept (assuming we can) we extract the contact port of the other
 * machine.
 *
 * The function return 0 or 1 to indicate if refused the request or not (so we
 * can remove the socket from the awaiting queue without closing it).
 */
int handle_join_request(server_t* server, socket_t sock);


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
int send_join_request(server_t *server, const char* ip, const char* port, uint8_t rescue);


/*
 * Answer to a join request through the socket. join indicate if we accepted
 * the request.
 */
void answer_join_request(server_t *server, socket_t s, uint8_t join);



/*
 * Send the neighbours to the client on the "other side" of socket.
 */
void send_neighbours_list(socket_t s, char** ips, char** ports,
                          uint8_t nb_neighbours);


/*
 * Broadcast the packet to all the neighbours we have.
 */
void broadcast_packet(server_t* server, void* packet, size_t size);


/*
 * Read the informations about the request on the socket and create a request to
 * deal with it later.
 */
void handle_remote_search_request(server_t* server, int sock);


/*******************************************************************************
 * Handling user packets
 */


/*
 * Read the informations about the request on the socket and store a request
 * inside the server.
 */
void handle_local_search_request(server_t* server);


/*******************************************************************************
 * Handling requests
 */


void answer_local_search_request(server_t* server, request_t* request);
void answer_remote_search_request(server_t* server, request_t* request);
void answer_local_download_request(server_t* server, request_t* request);
void answer_remote_download_request(server_t* server, request_t* request);


/*******************************************************************************
 * Utilities
 */


/*
 * Add a copy-in-heap of socket inside a list (creating function).
 */
LIST_CREATE_FN void* add_new_socket(void* s);


#endif /* SERVER_INTERNAL_H */
