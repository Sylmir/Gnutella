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
#include "server_common.h"
#include "util.h"


/* As long as this equals 1, the server continues it's loop. */
sig_atomic_t _loop = 1;

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
static int join_network(server_t* server, const char* ip, const char* port);


/*
 * Actively join the P2P network (join_network acting as a helper function). The
 * function will return 0 on success, and -1 on failure.
 */
static int join_network_through(server_t* server, const char* ip, const char* port);


/*
 * Extract the next neighbour from socket (assuming we are handling SMSG_NEIGHBOURS_REPLY).
 * The IP and port are stored inside ip and port.
 */
static void extract_neighbour_from_response(int socket, char **ip, char **port);


/*
 * Extract the IP and port bound to socket and store them inside ip and port.
 */
static void extract_neighbour_from_socket(int socket, char** ip, char** port);

/* Port on which the server will listen and to which clients will talk. */
/** @todo Move to a .ini file we can parse. */
#define CONTACT_PORT "10000"

/* IP which will be used as the contact point. */
/** @todo Move to a .ini file we can parse. */
/** @todo Add support for IPv4 / IPv6 ? */
#define CONTACT_POINT "134.214.88.230"


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
 * Timeout (milliseconds) when we check if there is something to read on an
 * awaiting socket.
 */
#define AWAIT_TIMEOUT 10


/*
 * Helper to avoid redundancy. Send a join request to ip:port and store the
 * communicating socket inside sockets. ip and port are both freed at the
 * end of the function (no, I'm not going to make a version where the free
 * is not called, just malloc your damn buffers).
 */
static void join(const char* ip, const char* port, list_t* sockets);


/*
 * Compute a list of neighbours to send through socket.
 */
static void compute_and_send_neighbours(server_t* server, int socket);


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


/*
 * Add a copy-in-heap of socket inside a list (creating function).
 */
LIST_CREATE_FN static void* add_new_socket(void* socket);


/*******************************************************************************
 * Packets handling.
 */


/*
 * Send a request to host:ip to initiate a communication. The function will
 * return the socket used to communicate with the server.
 */
static int send_neighbours_request(const char* ip, const char* port);

/* Max number of attempts to retrieve the neighbours of someone. */
#define GET_NEIGHBOURS_MAX_ATTEMPTS 3
/* Sleep time when we try to get neighbours (seconds). */
#define GET_NEIGHBOURS_SLEEP_TIME 1


/*
 * Send the join request to ip:host. If ip:host can accept the join, we return
 * the socket used to communicate. Otherwise, we return -1. rescue indicates if
 * the rescue flag is to be set to 1 or 0.
 */
static int send_join_request(const char* ip, const char* port, uint8_t rescue);


/*
 * Answer to a join request through the socket.
 */
static void answer_join_request(int socket);


/******************************************************************************/


int run_server(int first_machine, const char *listen_port,
               const char *ip, const char *port) {
    server_t server;
    server.client_socket    = 0;
    server.listening_socket = 0;
    memset(server.neighbours, -1, MAX_NEIGHBOURS * sizeof(int));
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


int join_network(server_t* server, const char* ip, const char* port) {
    int res;

    if (ip == NULL) {
        if (port == NULL) {
            res = join_network_through(server, CONTACT_POINT, CONTACT_PORT);
        } else {
            res = join_network_through(server, CONTACT_POINT, port);
        }
    } else {
        if (port == NULL) {
            res = join_network_through(server, ip, CONTACT_PORT);
        } else {
            res = join_network_through(server, ip, port);
        }
    }

    return res;
}


int join_network_through(server_t* server, const char* ip, const char* port) {
    int socket = send_neighbours_request(ip, port);
    if (socket == -1) {
        return -1;
    }

    opcode_t opcode;
    read_from_fd(socket, &opcode, PKT_ID_SIZE);

    if (opcode != SMSG_NEIGHBOURS) {
        close(socket);
        return -1;
    }

    /* Store the sockets we send CMSG_JOIN_REQUEST through. */
    list_t* awaiting = list_create(compare_ints, add_new_socket);

    uint8_t nb_neighbours;
    read_from_fd(socket, &nb_neighbours, sizeof(uint8_t));


    char *current_ip, *current_port;
    for (int i = 0; i < nb_neighbours; i++) {
        extract_neighbour_from_response(socket, &current_ip, &current_port);
        join(current_ip, current_port, awaiting);
    }

    if (nb_neighbours < MAX_NEIGHBOURS) {
        extract_neighbour_from_socket(socket, &current_ip, &current_port);
        join(current_ip, current_port, awaiting);
    }

    cell_t* prev = NULL;
    for (cell_t* cell = awaiting->head; cell != NULL; ) {
        int socket = *(int*)cell->data;
        struct pollfd poller;
        int res = poll_fd(&poller, socket, POLLIN, AWAIT_TIMEOUT);
        if (res == 1) {
            list_pop_at(&prev, &cell);
        }
        prev = cell;
    }

    list_destroy(&awaiting);

    return 0;
}


void extract_neighbour_from_response(int socket, char** ip, char** port) {
    ip_version_id_t version;
    read_from_fd(socket, &version, IP_VERSION_ID_SIZE);

    uint8_t ip_len;
    read_from_fd(socket, &ip_len, sizeof(uint8_t));

    *ip = malloc(ip_len);
    read_from_fd(socket, *ip, ip_len);

    in_port_t iport;
    read_from_fd(socket, &iport, sizeof(in_port_t));

    *port = (char*)int_to_cstring(iport);
}


void extract_neighbour_from_socket(int socket, char** ip, char** port) {
    struct sockaddr addr;
    socklen_t len = sizeof(addr);

    getpeername(socket, &addr, &len);

    in_port_t iport;
    *ip = (char*)extract_ip_classic(&addr, &iport);
    *port = (char*)int_to_cstring(iport);
}

int send_neighbours_request(const char* ip, const char* port) {
    int socket = -1;
    int res = connect_to(ip, port, &socket);
    if (res == -1) {
        return -1;
    }

    opcode_t opcode = CMSG_NEIGHBOURS;
    write_to_fd(socket, &opcode, PKT_ID_SIZE);

    return socket;
}


int send_join_request(const char* ip, const char* port, uint8_t rescue) {
    int socket = -1;
    int res = connect_to(ip, port, &socket);
    if (res == -1) {
        return -1;
    }

    void* data = malloc(PKT_ID_SIZE + sizeof(uint8_t));
    char* ptr = data;
    *(opcode_t*)ptr = CMSG_JOIN;
    ptr += PKT_ID_SIZE;
    *(uint8_t*)ptr = rescue;
    ptr += sizeof(uint8_t);

    write_to_fd(socket, data, (intptr_t)ptr - (intptr_t)data);

    return socket;
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
            list_pop_at(&prev, &head);
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
        compute_and_send_neighbours(server, socket);
        break;

    case CMSG_JOIN:
        answer_join_request(socket);
        break;


    default:
        break;
    }

    return 1;
}


void compute_and_send_neighbours(server_t* server, int socket) {
    uint8_t nb_neighbours = 0;
    struct sockaddr* neighbours = malloc(MAX_NEIGHBOURS * sizeof(struct sockaddr));

    for (int i = 0; i < MAX_NEIGHBOURS; i++) {
        if (server->neighbours[i] != -1) {
            socklen_t len = sizeof(*(neighbours + nb_neighbours));
            getpeername(server->neighbours[i], neighbours + nb_neighbours, &len);
            nb_neighbours++;
        }
    }

    send_neighbours_list(socket, neighbours, nb_neighbours);
}


void join(const char* ip, const char* port, list_t* sockets) {
    int res = send_join_request(ip, port, 0);
    if (res == -1) {
        return;
    }

    list_push_back(sockets, &res);

    const_free(ip);
    const_free(port);
}


int handle_neighbours(server_t* server) {
    return 0;
}


int handle_client(server_t* server) {
    return 0;
}


void* add_new_socket(void* socket) {
    int* copy = malloc(sizeof(int));
    *copy = *(int*)socket;
    return copy;
}


void clear_server(server_t* server) {
    close(server->listening_socket);
    close(server->client_socket);

    for (int i = 0; i < MAX_NEIGHBOURS; ++i) {
        close(server->neighbours[i]);
        server->neighbours[i] = -1;
    }

    for (cell_t* head = server->awaiting_sockets->head; head != NULL; head = head->next) {
        close(*(int*)head);
    }

    list_destroy(&(server->awaiting_sockets));
}


void answer_join_request(int socket) {

}


void handle_sigint(int sigint) {
    UNUSED(sigint);
    _loop = 0;
}
