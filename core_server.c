#define _GNU_SOURCE

#include <errno.h>
#include <signal.h>
#include <string.h>

#include <netdb.h>
#include <poll.h>
#include <unistd.h>

#include "common.h"
#include "core_server.h"
#include "list.h"
#include "log.h"
#include "networking.h"
#include "packets_defines.h"
#include "server_common.h"
#include "util.h"


/* Continue the server loop. */
sig_atomic_t _loop = 1;


typedef struct core_server_s {
    /* Socket we use to listen to new connections. */
    int listening_socket;
    /* Our direct neighbours (virtually handled, since we are not in the network
     * we just send them each time someone asks for neighbours. */
    int neighbours[MAX_NEIGHBOURS];
    /* Awaiting sockets (i.e, we haven't received the request for neighbours
     * yet). */
    list_t* awaiting_sockets;
    /* Number of active neighbours (updated fairly frequently). */
    int active_neighbours;
} core_server_t;


/*
 * Perform the main loop, i.e accept for connection and sends people a list of
 * neighbours.
 */
static void loop(core_server_t* server);


/*
 * SIGINT handler function. Set (atomically) _loop to false so we can stop the
 * core server.
 */
static void handle_sigint(int sigint);


/*
 * Handle a newly received (and valid) socket.
 */
static void handle_new_socket(core_server_t* server, int socket,
                              const struct sockaddr* addr,
                              socklen_t addr_len);


/*
 * Check if we received something on one of the awaiting sockets. If we did
 * receive something, ensure it is a request for neighbours, and if it is,
 * respond to it.
 */
static void handle_awaiting_sockets(core_server_t* server);


/*
 * Ensure we have the right message in the socket.
 */
ERROR_CODES_BOOL static int ensure_opcode(int socket);


/*
 * Compute a list of neighbours and then send it to the peer identified by
 * socket.
 */
static void compute_and_send_neighbours_list(core_server_t* server, int socket);


/*
 * Check the status of the neighbours to ensure we are not keeping count of
 * departed clients. If a client has exited, we update our list.
 */
static void check_neighbours(core_server_t* server);

/* Delay in milliseconds to check if the other end of the socket is ready. */
#define CHECKER_TIMEOUT 1


/*
 * Close the listening_socket and the awaiting sockets. After that, the server
 * is ready to shut down gracefully.
 */
static void clear_server(core_server_t* server);


/*
 * Add the (virtual) neighbour identified by socket as one of our neighbours.
 */
static void add_neighbour(core_server_t* server, int socket);


/* Maximum number of pending requests on the listening socket. */
#define MAX_PENDING_REQUESTS 50


/******************************************************************************/


static int run_core_server() {
    core_server_t core_server;
    char host_name[NI_MAXHOST], port_number[NI_MAXSERV];
    int listening_socket = create_listening_socket(CONTACT_PORT,
                                                   MAX_PENDING_REQUESTS,
                                                   host_name, port_number);

    if (listening_socket < 0) {
        return EXIT_FAILURE;
    }

    for (int i = 0; i < MAX_NEIGHBOURS; ++i) {
        core_server.neighbours[i] = -1;
    }

    core_server.listening_socket = listening_socket;
    core_server.awaiting_sockets = list_create(compare_ints);

    loop(&core_server);

    clear_server(&core_server);
    return 0;
}


void loop(core_server_t* server) {
    while (_loop) {
        struct sockaddr addr;
        socklen_t addr_len = sizeof(addr);
        int socket = attempt_accept(server->listening_socket, ACCEPT_TIMEOUT,
                                    &addr, &addr_len);
        if (socket < 0) {
            if (socket == -1) {
                applog(LOG_LEVEL_ERROR, "[Core server] accept error %s.\n",
                                    strerror(errno));
            }
            continue;
        }

        handle_new_socket(server, socket, &addr, addr_len);
        handle_awaiting_sockets(server);
        check_neighbours(server);
    }
}


void handle_new_socket(core_server_t* server, int socket,
                       const struct sockaddr* addr, socklen_t addr_len) {
    in_port_t port;
    const char* ip = extract_ip_classic(addr, &port);
    applog(LOG_LEVEL_INFO, "[Core server] Accepted connection from %s:%d.\n",
                           ip, port);

    /* I bet you didn't see that coming ? *sigh* */
    int* copy = malloc(sizeof(int));
    *copy = socket;
    list_push_back(server->awaiting_sockets, copy);
}


void handle_awaiting_sockets(core_server_t* server) {
    cell_t* prev = NULL;
    for (cell_t* cell = server->awaiting_sockets->head; cell != NULL; ) {
        struct pollfd poller;
        poller.fd = *(int*)cell->data;
        poller.events = POLLIN;
        poller.revents = 0;

        int res = poll(&poller, 1, ACCEPT_TIMEOUT);
        if (res == 1) {
            /* POLLHUP says that the client close it's extremity. So why should
             * we answer ?
             */
            if ((poller.revents & POLLHUP) != 0) {
                close(poller.fd);
                list_pop_at(&prev, &cell);
            } else if ((poller.revents & POLLIN) != 0) {
                if (ensure_opcode(poller.fd)) {
                    compute_and_send_neighbours_list(server, poller.fd);
                    close(poller.fd);
                    list_pop_at(&prev, &cell);
                }
            }
        } else {
            prev = cell;
            cell = cell->next;
        }
    }
}


int ensure_opcode(int socket) {
    opcode_t opcode;
    read_from_fd(socket, &opcode, PKT_ID_SIZE);

    if (opcode != CMSG_NEIGHBOURS_REQUEST) {
        char dummy[4096];
        read_from_fd(socket, dummy, 4096);
        return 0;
    }

    return 1;
}


void compute_and_send_neighbours_list(core_server_t* server, int socket) {
    int nb_neighbours = 0;
    struct sockaddr* neighbours = malloc(MAX_NEIGHBOURS * sizeof(struct sockaddr));

    for (int i = 0; i < MAX_NEIGHBOURS; ++i) {
        if (server->neighbours[i] != -1) {
            socklen_t socklen;
            getpeername(server->neighbours[i], neighbours + nb_neighbours, &socklen);
            nb_neighbours++;
        }
    }

    send_neighbours_list(socket, neighbours, nb_neighbours);

    add_neighbour(server, socket);
}


void add_neighbour(core_server_t* server, int socket) {
    if (server->active_neighbours == MAX_NEIGHBOURS) {
        return;
    }

    int i = 0;
    while (i < MAX_NEIGHBOURS && server->neighbours[i] != -1) {
        i++;
    }

    server->neighbours[i] = socket;
    server->active_neighbours++;
}


void check_neighbours(core_server_t* server) {
    for (int i = 0; i < MAX_NEIGHBOURS; i++) {
        if (server->neighbours[i] != -1) {
            struct pollfd checker;
            checker.fd = server->neighbours[i];
            checker.events = checker.revents = 0;
            poll(&checker, 1, CHECKER_TIMEOUT);

            if ((checker.revents & POLLHUP) != 0) {
                server->active_neighbours--;
                server->neighbours[i] = -1;
            }
        }
    }
}


void clear_server(core_server_t* server) {
    close(server->listening_socket);
    for (cell_t* head = server->awaiting_sockets->head; head != NULL; head = head->next) {
        close(*(int*)head->data);
    }

    list_destroy(&(server->awaiting_sockets));
}


int main() {
    signal(SIGINT, handle_sigint);
    return run_core_server();
}


void handle_sigint(int sigint) {
    UNUSED(sigint);
    const char* msg= "Arrêt.\n";
    write(STDOUT_FILENO, msg, strlen(msg));
    _loop = 0;
}
