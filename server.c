#define _GNU_SOURCE

#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <string.h>

#include <arpa/inet.h>
#include <fcntl.h>
#include <netdb.h>
#include <poll.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <unistd.h>

#include "common.h"
#include "core_server.h"
#include "log.h"
#include "networking.h"
#include "packets_defines.h"
#include "server.h"
#include "server_common.h"
#include "util.h"


/* The structure to represent the server. */
typedef struct server_s {
    /* Socket to wait for new connexions. */
    int listening_socket;
    /* Array of sockets representing the neighbours. */
    socket_t neighbours[MAX_NEIGHBOURS];
    /* Socket to communicate with the client. */
    int client_socket;
    /* Indicate if we performed the handshake. */
    int handshake;
    /* IP of the contact point. */
    const char* contact_point_ip;
    /* Port of the contact point. */
    const char* contact_point_port;
} server_t;


/* Maximum number of pending requests on the listening socket. */
#define MAX_PENDING_REQUESTS 50


/*
 * Performs the handshake. The client is exepcted to send CMSG_INT_HANDSHAKE,
 * and, if that is the case, the server will answer with SMSG_INT_HANDSHAKE.
 * Both the server and the client assume that what they send was received by
 * the other.
 *
 * The function will return one of the HANDSHAKE_-family error codes to
 * indicate what happened. On success, the server->client_socket will become
 * the socket used to communicate with the client.
 */
static int handshake(server_t *server, int client_socket);

/* Handshake performed gracefully. */
#define HANDSHAKE_OK                -1
/* Opcode sent by client was no the good one. */
#define HANDSHAKE_BAD_OPCODE        -2
/* Handshake was already performed. */
#define HANDSHAKE_ALREADY_SHAKED    -3
/* Exclusion with ACCEPT- family functions. */
#define HANDSHAKE_MAX               -4


static void handle_handshake_result(server_t* server, int result);

/*
 * Join the P2P network. This is done by first requesting a list of neighbours
 * from the contact point. Then, for each possible neighbour we will ask if we
 * can join. Depending on the reply (almost saturated, saturated, available) we
 * will then roll a chance to decide if we join or not.
 *
 * For each neighbour we get, we store a socket inside server->neighbours with a
 * SOCKET_READY state.
 *
 * The function will return 0 on success.
 */
static int join_network(server_t* server);


/*
 * Send the connection reply.
 */
static void send_conect_reply(const server_t* server, int socket);


/*
 * Perform the server main loop.
 */
static int loop(server_t* server);


/*
 * Close all sockets on the server. After a call to this function, the server
 * is ready to exit.
 */
static void clear_server(server_t* server);


/*
 * Loop over the socket that we are waiting or ready. If a socket is waiting,
 * notify the other endpoint that we are ready if the handshake has been done.
 * If a socket is ready, read from it if there is something to read.
 */
static int handle_sockets(server_t* server);


/*
 * When a new socket is returned by accept_connection, handle it, i.e, if the
 * handshake has not been done, add it to the waiting list with a SOCKET_WAIT
 * status ; if the handshake has been done, add it to the waiting list with a
 * SOCKET_READY status.
 */
static int handle_new_socket(server_t* server, int new_socket);


/*
 * After we accept a new socket, look at the result returned by attempt_accept.
 *
 * If the local client connected and handshaked gracefully or if a remote client
 * connected gracefully, the function does nothing. If the local client handshaked
 * twice, the function issues a warning. If the local client send a bad paquet
 * during the handshake, the function kills the client, clears the server and
 * exit. Finally, if accept() failed earlier, the function displays the content
 * of errno. In all other cases, the function does nothing.
 */
static void handle_accept_result(server_t* server, int result,
                                 const struct sockaddr* addr, socklen_t addr_len);


/*******************************************************************************
 * Packets handling.
 */


/*
 * Send a request to host:ip to initiate a communication. The fu
 */
static int get_neighbours(const char* ip, const char* host);

/* Max number of attempts to retrieve the neighbours of someone. */
#define GET_NEIGHBOURS_MAX_ATTEMPTS 3
/* Sleep time when we try to get neighbours (seconds). */
#define GET_NEIGHBOURS_SLEEP_TIME 1


/*
 * Send the join request to ip:host. If ip:host can accept the join, we return
 * the socket used to communicate. Otherwise, we return -1.
 */
static void send_join_request(const char* ip, const char* host);


/*
 * Answer to a join request through the socket.
 */
static void answer_join_request(int socket);


/******************************************************************************/


int run_server(int first_machine, const char *ip, const char *port) {
    server_t server;
    server.client_socket    = 0;
    server.listening_socket = 0;
    for (int i = 0; i < MAX_NEIGHBOURS; ++i) {
        server.neighbours[i].socket = 0;
        server.neighbours[i].status = SOCKET_CLOSED;
    }
    server.handshake        = 0;

    char host_name[NI_MAXHOST], port_number[NI_MAXSERV];
    int listening_socket = create_listening_socket(SERVER_LISTEN_PORT,
                                                   MAX_PENDING_REQUESTS,
                                                   host_name, port_number);
    if (listening_socket < 0) {
        return listening_socket;
    } else {
        applog(LOG_LEVEL_INFO, "[Serveur] Listening on %s:%s.\n",
                               host_name, port_number);
    }

    server.listening_socket = listening_socket;
    if (ip != NULL) {
        server.contact_point_ip = ip;
        server.contact_point_port = port;
    }

    if (first_machine == 0) {
        join_network(&server);
    }

    loop(&server);

    clear_server(&server);
    return EXIT_SUCCESS;
}


int loop(server_t* server) {
    while (1) {
        struct sockaddr client_addr;
        socklen_t client_addr_len = sizeof(client_addr);
        int res = attempt_accept(server->listening_socket, ACCEPT_TIMEOUT,
                                 &client_addr, &client_addr_len);

        handle_accept_result(server, res, &client_addr, client_addr_len);

        handle_sockets(server);
    }

    return 0;
}


int handshake(server_t* server, int client_socket) {
    if (server->handshake == 1) {
        return HANDSHAKE_ALREADY_SHAKED;
    }

    server->client_socket = client_socket;

    opcode_t client_data;
    read_from_fd(server->client_socket, &client_data, PKT_ID_SIZE);

    if (client_data != CMSG_INT_HANDSHAKE) {
        return HANDSHAKE_BAD_OPCODE;
    }

    opcode_t data = SMSG_INT_HANDSHAKE;
    write_to_fd(server->client_socket, &data, PKT_ID_SIZE);

    server->handshake = 1;

    return HANDSHAKE_OK;
}


int join_network(server_t* server) {
    return 0;
}


int get_neighbours(const char* ip, const char* host) {
    return 0;
}


void send_join_request(const char* ip, const char* host) {

}


void answer_join_request(int socket) {

}


void handle_accept_result(server_t *server, int result,
                          const struct sockaddr* addr, socklen_t addr_len) {
    if (result == ACCEPT_ERR_TIMEOUT) {
        // applog(LOG_LEVEL_WARNING, "[Serveur] Accept timed out.\n");
        return;
    }

    if (result == -1) {
        applog(LOG_LEVEL_ERROR, "Erreur durant l'acceptation : %s.\n",
               strerror(errno));
        return;
    }

    int socket = result;
    in_port_t port;
    const char* ip = extract_ip_classic(addr, &port);
    if (ip == NULL) {
        applog(LOG_LEVEL_WARNING, "[Serveur] inet_ntop failed. Erreur : %s.\n",
               strerror(errno));

        if (errno == EAFNOSUPPORT) {
            close(socket);
            return;
        }
    } else {
        /* Check if we can handshake. */
        applog(LOG_LEVEL_INFO, "[Serveur] Connexion acceptée depuis %s:%d.\n",
               ip, port);

        if (strcmp(ip, "127.0.0.1") == 0 ||
            strcmp(ip, "0000:0000:0000:0000:0000:0000:0000:0001") == 0) {
            int handshake_result = handshake(server, socket);
            handle_handshake_result(server, handshake_result);
            return;
        }

        free((void*)ip);
    }

    send_conect_reply(server, socket);
}


void handle_handshake_result(server_t* server, int result) {
    switch (result) {
    case HANDSHAKE_OK:
        applog(LOG_LEVEL_INFO, "[Serveur] Client OK (Handshake).\n");
        break;

    case HANDSHAKE_BAD_OPCODE:
        applog(LOG_LEVEL_FATAL, "[Serveur] Mauvais opcode dans le handshake. "
                               "Arrêt.\n");
        clear_server(server);
        kill(getppid(), SIGINT);
        exit(EXIT_FAILURE);
        break;

    case HANDSHAKE_ALREADY_SHAKED:
        applog(LOG_LEVEL_WARNING, "[Serveur] Multiple handshake.\n");
        break;
    }
}


void send_conect_reply(const server_t* server, int socket) {
    opcode_t id = SMSG_CONNECT_REPLY;
    connect_reply_t reply;
    if (server->handshake == 1) {
        reply = REPLY_READY;
    } else {
        reply = REPLY_NOT_READY;
    }

    write_to_fd(socket, &id, PKT_ID_SIZE);
    write_to_fd(socket, &reply, 1);
}


int handle_new_socket(server_t* server, int new_socket) {
    return 0;
}


int handle_sockets(server_t* server) {
    return 0;
}


void clear_server(server_t* server) {
    close(server->listening_socket);
    close(server->client_socket);

    for (int i = 0; i < MAX_NEIGHBOURS; ++i) {
        if (server->neighbours[i].status != SOCKET_CLOSED) {
            close(server->neighbours[i].socket);
            server->neighbours[i].status = SOCKET_CLOSED;
        }
    }
}
