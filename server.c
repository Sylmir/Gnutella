#define _GNU_SOURCE

#include <assert.h>
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
#include "list.h"
#include "log.h"
#include "networking.h"
#include "packets_defines.h"
#include "server.h"
#include "server_internal.h"
#include "util.h"


/* As long as this equals 1, the server continues it's loop. */
sig_atomic_t _loop = 1;

/*
 * SIGINT handler.
 */
static void handle_sigint(int sigint);

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
 * Perform the server main loop.
 */
static int loop(server_t* server);


/*
 * Close all sockets on the server. After a call to this function, the server
 * is ready to exit.
 */
static void clear_server(server_t* server);


/*
 * Loop over the whole list of awaiting sockets and answers to their requests
 * if any. If the other extremity of a socket has been closed, we don't answer
 * and we close it as well.
 */
static void handle_awaiting_sockets(server_t* server);


/*
 * Check if the socket has something for us and answer the request if necessary.
 * If there is nothing to read, the function returns 0, otherwise it returns 1.
 */
static int handle_awaiting_socket(server_t* server, int socket);


/*
 * Loop over the neighbours sockets and answers to their requests if any. If the
 * other extremity of a socket has been closed, we set it to -1 and decrease our
 * number of neighbours by one.
 */
static int handle_neighbours(server_t* server);


/*
 * Check if the client has something to say, and handle the requests if any.
 */
static int handle_client(server_t* server);


/*
 * When a new socket is returned by accept_connection, handle it, i.e, if the
 * handshake has not been done, add it to the waiting list with a SOCKET_WAIT
 * status ; if the handshake has been done, add it to the waiting list with a
 * SOCKET_READY status.
 */
static void handle_new_socket(server_t* server, int new_socket);


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


/******************************************************************************/


int run_server(int first_machine, const char *listen_port,
               const char *ip, const char *port) {
    server_t server;
    server.client_socket    = 0;
    server.listening_socket = 0;
    for (int i = 0; i < MAX_NEIGHBOURS; ++i) {
        server.neighbours[i].sock = -1;
        server.neighbours[i].port = 0;
    }
    server.nb_neighbours    = 0;
    server.handshake        = 0;

    char host_name[NI_MAXHOST], port_number[NI_MAXSERV];
    int listening_socket = create_listening_socket(listen_port == NULL ? SERVER_LISTEN_PORT : listen_port,
                                                   MAX_PENDING_REQUESTS,
                                                   host_name, port_number);
    if (listening_socket < 0) {
        applog(LOG_LEVEL_FATAL, "[Serveur] Impossible de créer la socket "
                                "d'écoute. Extinction.\n");
        exit(EXIT_FAILURE);
    } else {
        applog(LOG_LEVEL_INFO, "[Serveur] Listening on %s:%s.\n",
                               host_name, port_number);
    }

    server.listening_socket = listening_socket;

    if (first_machine == 0) {
        int res = join_network(&server, ip, port);

        if (res == -1) {
            applog(LOG_LEVEL_FATAL, "[Serveur] Impossible d'acquérir des voisins. "
                                    "Extinction.\n");
            close(server.listening_socket);
            exit(EXIT_FAILURE);
        }
    }

    server.awaiting_sockets = list_create(compare_ints, add_new_socket);
    signal(SIGINT, handle_sigint);
    loop(&server);

    clear_server(&server);
    return EXIT_SUCCESS;
}


int loop(server_t* server) {
    while (_loop == 1) {
        struct sockaddr client_addr;
        socklen_t client_addr_len = sizeof(client_addr);
        int res = attempt_accept(server->listening_socket, ACCEPT_TIMEOUT,
                                 &client_addr, &client_addr_len);

        handle_accept_result(server, res, &client_addr, client_addr_len);
        handle_awaiting_sockets(server);
        handle_neighbours(server);
        handle_client(server);
    }

    return 0;
}


void handle_accept_result(server_t *server, int result,
                          const struct sockaddr* addr, socklen_t addr_len) {
    if (result == ACCEPT_ERR_TIMEOUT) {
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
        } else {
            handle_new_socket(server, socket);
        }

        free((void*)ip);
    }
}


void handle_new_socket(server_t* server, int new_socket) {
    list_push_back(server->awaiting_sockets, &new_socket);
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


void handle_awaiting_sockets(server_t* server) {
    cell_t* prev = NULL;
    for (cell_t* head = server->awaiting_sockets->head; head != NULL; ) {
        int res = handle_awaiting_socket(server, *(int*)head->data);
        if (res == 0) {
            prev = head;
            head = head->next;
        } else {
            close(*(int*)head->data);
            list_pop_at(server->awaiting_sockets, &prev, &head);
        }
    }
}


int handle_awaiting_socket(server_t* server, int socket) {
    struct pollfd poller;
    int res = poll_fd(&poller, socket, POLLIN, AWAIT_TIMEOUT);
    if (res == 0) {
        return 0;
    }

    opcode_t opcode;
    read_from_fd(socket, &opcode, PKT_ID_SIZE);

    switch (opcode) {
    case CMSG_NEIGHBOURS:
        applog(LOG_LEVEL_INFO, "[Server] Received CMSG_NEIGHBOURS\n");
        compute_and_send_neighbours(server, socket);
        break;

    case CMSG_JOIN:
        applog(LOG_LEVEL_INFO, "[Server] Received CMSG_JOIN\n");
        handle_join_request(server, socket);
        break;


    default:
        break;
    }

    return 1;
}


int handle_neighbours(server_t* server) {
    return 0;
}


int handle_client(server_t* server) {
    return 0;
}


void clear_server(server_t* server) {
    close(server->listening_socket);
    close(server->client_socket);

    for (int i = 0; i < MAX_NEIGHBOURS; ++i) {
        if (server->neighbours[i].sock != -1) {
            close(server->neighbours[i].sock);
        }
        server->neighbours[i].sock = -1;
    }

    for (cell_t* head = server->awaiting_sockets->head; head != NULL; head = head->next) {
        close(*(int*)head);
    }

    list_destroy(&(server->awaiting_sockets));
}


void handle_sigint(int sigint) {
    UNUSED(sigint);
    _loop = 0;
}
