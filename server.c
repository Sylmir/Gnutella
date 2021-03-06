#define _GNU_SOURCE

#include <assert.h>
#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

#include <arpa/inet.h>
#include <fcntl.h>
#include <netdb.h>
#include <poll.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/types.h>
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
 * Minimum amount of time (in milliseconds) the server has to spend inside a loop.
 * This allows us to relieve the work on the CPU.
 */
#define LOOP_MIN_DURATION 50


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
 * If there is nothing to read, the function returns 0, otherwise it return one
 * of AWAIT_-family values to indicate if we must close the socket and remove it
 * from the awaiting list, or only remove it from the list.
 */
static int handle_awaiting_socket(server_t* server, int socket);

/* Do nothing. */
#define AWAIT_EMPTY 0
/* Close and remove. */
#define AWAIT_CLOSE 1
/* Don't close and remove. */
#define AWAIT_KEEP 2


/*
 * Loop over the neighbours sockets and answers to their requests if any. If the
 * other extremity of a socket has been closed, we set it to -1 and decrease our
 * number of neighbours by one.
 *
 * The function returns 0 to indicate nothing special happened, or 1 if we ran
 * out of neighbours. In that case, we shutdown.
 */
static int handle_neighbours(server_t* server);


/*
 * Handle a single neighbour. The function returns 1 to indicate we must remove
 * the neighbour from the list of neighbours, and 0 if we have nothing special
 * to do.
 */
static int handle_neighbour(server_t* server, socket_contact_t* neighbour);


/*
 * Display a list of our current neighbours on the standard output stream. Since
 * this comes at a cost, this function should not be used too frequently and only
 * for debugging purposes.
 */
static void display_neighbours(const server_t* server);


/*
 * Check if the client has something to say, and handle the requests if any. If
 * the client sent CMSG_INT_EXIT, the function return 1 to indicate we must stop.
 * Otherwise it return 0.
 */
static int handle_client(server_t* server);

#define HANDLE_CLIENT_TIMEOUT 10


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
static void handle_accept_result(server_t* server, int result);


/*
 * Loop over the pending requests and execute them.
 */
static void handle_pending_requests(server_t* server);


/*
 * Handle a pending request.
 */
static void handle_pending_request(server_t* server, request_t* request);


/*
 * Update all the timers inside the list of logs to remove obsolete logs. diff
 * represent the time in milliseconds the server took to perform it's previous
 * loop.
 *
 * Entries are removed when their internal timer reaches 0. This, obviously, is
 * not a perfect solution, as the rules of the Internet could lead us to reply
 * to an already answered request because the request came with so much delay
 * that we already removed the log.
 */
static void update_log_timers(server_t* server, long int diff);


/*
 * Loop over the sockets that have a download initiated through them and see
 * if there is something to read on them. If so, handle the answer and close the
 * socket once it is done.
 */
static void handle_pending_downloads(server_t* server);


/*
 * Handle one specific download. The function returns 1 if the download is over
 * and the socket can safely be closed, otherwise it returns 0.
 */
static int handle_pending_download(server_t* server, int sock);


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
    server.self_ip          = NULL;

    char host_name[NI_MAXHOST], port_number[NI_MAXSERV];
    int listening_socket = create_listening_socket(listen_port == NULL ? SERVER_LISTEN_PORT : listen_port,
                                                   MAX_PENDING_REQUESTS,
                                                   host_name, port_number);
    if (listening_socket < 0) {
        applog(LOG_LEVEL_FATAL, "[Server] Impossible de créer la socket "
                                "d'écoute. Extinction.\n");
        exit(EXIT_FAILURE);
    } else {
        applog(LOG_LEVEL_INFO, "[Server] Listening on %s:%s.\n",
                               host_name, port_number);
    }

    server.listening_socket = listening_socket;

    if (first_machine == 0) {
        int res = join_network(&server, ip, port);

        if (res == -1) {
            applog(LOG_LEVEL_FATAL, "[Client] Impossible d'acquérir des voisins. "
                                    "Extinction.\n");
            close(server.listening_socket);
            exit(EXIT_FAILURE);
        }
    }

    server.awaiting_sockets = list_create(compare_ints, add_new_socket);
    server.pending_requests = list_create(NULL, add_new_request);
    server.received_search_requests = list_create(NULL, add_new_search_request_log);
    server.pending_downloads = list_create(NULL, add_new_socket);
    signal(SIGINT, handle_sigint);
    loop(&server);
    leave_network(&server);

    clear_server(&server);
    return EXIT_SUCCESS;
}


int loop(server_t* server) {
    int print_timer = 10 * IN_MILLISECONDS;
    int time_diff = LOOP_MIN_DURATION;
    while (_loop == 1) {
        struct timespec begin;
        clock_gettime(CLOCK_REALTIME, &begin);

        struct sockaddr client_addr;
        socklen_t client_addr_len = sizeof(client_addr);
        int res = attempt_accept(server->listening_socket, ACCEPT_TIMEOUT,
                                 &client_addr, &client_addr_len);

        handle_accept_result(server, res);
        handle_awaiting_sockets(server);

        if (print_timer <= time_diff) {
            display_neighbours(server);
            print_timer = 10000;
        } else {
            print_timer -= time_diff;
        }

        handle_neighbours(server);
        handle_pending_downloads(server);

        if (handle_client(server) == 1) {
            _loop = 0;
        }

        /*
         * If we don't have at least one neighbour, sending a request accross
         * the network has no sense. Moreover, it means we don't know server->self_ip,
         * and since the packets rely on it... Niah...
         */
        if (server->nb_neighbours != 0) {
            handle_pending_requests(server);
        }

        update_log_timers(server, time_diff);

        time_diff = elapsed_time_since(&begin);
        if (time_diff < LOOP_MIN_DURATION) {
            millisleep(LOOP_MIN_DURATION - time_diff);
            time_diff = LOOP_MIN_DURATION;
        }
    }

    return 0;
}


void handle_accept_result(server_t *server, int result) {
    if (result == ACCEPT_ERR_TIMEOUT) {
        return;
    }

    if (result == -1) {
        applog(LOG_LEVEL_ERROR, "[Server] Erreur durant accept() : %s.\n",
               strerror(errno));
        return;
    }

    int socket = result;
    char* ip = NULL, *port = NULL;
    extract_ip_port_from_socket_s(socket, &ip, &port, 1);
    if (ip == NULL) {
        applog(LOG_LEVEL_WARNING, "[Server] inet_ntop failed. Erreur : %s.\n",
               strerror(errno));

        if (errno == EAFNOSUPPORT) {
            close(socket);
        }
    } else {
        /* Check if we can handshake. */
        applog(LOG_LEVEL_INFO, "[Server] Connexion acceptée depuis %s:%s.\n",
               ip, port);

        if (strcmp(ip, "127.0.0.1") == 0 ||
            strcmp(ip, "0000:0000:0000:0000:0000:0000:0000:0001") == 0) {
            int handshake_result = handshake(server, socket);
            handle_handshake_result(server, handshake_result);
            free(ip);
            free(port);
            return;
        } else {
            handle_new_socket(server, socket);
        }
    }

    free(ip);
    free(port);
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
        applog(LOG_LEVEL_INFO, "[Server] Client OK (Handshake).\n");
        break;

    case HANDSHAKE_BAD_OPCODE:
        applog(LOG_LEVEL_FATAL, "[Server] Mauvais opcode dans le handshake. "
                               "Arrêt.\n");
        clear_server(server);
        kill(getppid(), SIGINT);
        exit(EXIT_FAILURE);
        break;

    case HANDSHAKE_ALREADY_SHAKED:
        applog(LOG_LEVEL_WARNING, "[Server] Multiple handshake.\n");
        break;
    }
}


void handle_awaiting_sockets(server_t* server) {
    cell_t* prev = NULL;
    for (cell_t* head = server->awaiting_sockets->head; head != NULL; ) {
        int res = handle_awaiting_socket(server, *(int*)head->data);
        if (res == AWAIT_EMPTY) {
            prev = head;
            head = head->next;
        } else {
            if (res == AWAIT_CLOSE) {
                close(*(int*)head->data);
            }

            list_pop_at(server->awaiting_sockets, &prev, &head);
        }
    }
}


int handle_awaiting_socket(server_t* server, int socket) {
    struct pollfd poller;
    int res = poll_fd(&poller, socket, POLLIN, AWAIT_TIMEOUT);
    if (res == 0) {
        return AWAIT_EMPTY;
    }

    opcode_t opcode;
    read_from_fd(socket, &opcode, PKT_ID_SIZE);

    switch (opcode) {
    case CMSG_NEIGHBOURS:
        applog(LOG_LEVEL_INFO, "[Client] Received CMSG_NEIGHBOURS\n");
        compute_and_send_neighbours(server, socket);
        break;

    case CMSG_JOIN:
        applog(LOG_LEVEL_INFO, "[Client] Received CMSG_JOIN\n");
        if (handle_join_request(server, socket) == 0) {
            return AWAIT_CLOSE;
        } else {
            return AWAIT_KEEP;
        }
        break;

    case CMSG_DOWNLOAD:
        handle_remote_download_request(server, socket);
        return AWAIT_KEEP;

    default:
        break;
    }

    return AWAIT_CLOSE;
}


int handle_neighbours(server_t* server) {
    for (int i = 0; i < MAX_NEIGHBOURS; i++) {
        if (server->neighbours[i].sock != -1) {
            int remove = handle_neighbour(server, server->neighbours + i);
            if (remove == 1) {
                int res = handle_leave(server, server->neighbours + i);
                if (res == 1) {
                    return 1;
                }
            }
        }
    }

    return 0;
}


int handle_neighbour(server_t* server, socket_contact_t* neighbour) {
    int sock = neighbour->sock;
    struct pollfd poller;
    int poll_res = poll_fd(&poller, sock, POLLIN, AWAIT_TIMEOUT);

    if (poll_res == 0) {
        return 0;
    }

    opcode_t opcode;
    read_from_fd(sock, &opcode, PKT_ID_SIZE);

    switch (opcode) {
    case CMSG_SEARCH_REQUEST:
        handle_remote_search_request(server, sock);
        break;

    case CMSG_LEAVE:
        applog(LOG_LEVEL_INFO, "[Server] Received CMSG_LEAVE\n");
        return 1;

    case SMSG_SEARCH_REQUEST:
        handle_remote_search_answer(server, neighbour->sock);
        break;
    }

    return 0;
}


void display_neighbours(const server_t* server) {
    applog(LOG_LEVEL_INFO, "[Client] Displaying neighbours\n");
    for (int i = 0; i < MAX_NEIGHBOURS; i++) {
        if (server->neighbours[i].sock != -1) {
            char ip[INET6_ADDRSTRLEN];
            char port[6];
            extract_ip_port_from_socket(server->neighbours[i].sock, ip, port, 1);

            applog(LOG_LEVEL_INFO, "[Client] Neighbour: %s:%s, contact = %s\n",
                   ip, port, server->neighbours[i].port);
        }
    }
}


int handle_client(server_t* server) {
    struct pollfd poller;
    int res = poll_fd(&poller, server->client_socket, POLLIN, HANDLE_CLIENT_TIMEOUT);

    if (res == 0) {
        return 0;
    }

    opcode_t opcode;
    read_from_fd(server->client_socket, &opcode, PKT_ID_SIZE);

    switch (opcode) {
    case CMSG_INT_EXIT:
        applog(LOG_LEVEL_INFO, "[Local Server] Received CMSG_INT_EXIT\n");
        leave_network(server);
        return 1;

    case CMSG_INT_SEARCH:
        applog(LOG_LEVEL_INFO, "[Local Server] Received CMSG_INT_SEARCH\n");
        handle_local_search_request(server);
        return 0;

    case CMSG_INT_DOWNLOAD:
        handle_local_download_request(server);
        return 0;

    default:
        return 0;
    }
}


void handle_pending_requests(server_t* server) {
    cell_t* prev = NULL, *head = server->pending_requests->head;
    while (head != NULL) {
        request_t* request = (request_t*)head->data;
        handle_pending_request(server, request);
        list_pop_at(server->pending_requests, &prev, &head);
    }
}


void handle_pending_request(server_t* server, request_t* request) {
    switch (request->type) {
    case REQUEST_SEARCH_LOCAL:
        answer_local_search_request(server, request);
        break;

    case REQUEST_SEARCH_REMOTE:
        answer_remote_search_request(server, request);
        break;

    case REQUEST_DOWNLOAD_LOCAL:
        answer_local_download_request(server, request);
        break;

    case REQUEST_DOWNLOAD_REMOTE:
        answer_remote_download_request(server, request);
        break;
    }
}


void update_log_timers(server_t* server, long int diff) {
    cell_t* prev = NULL, *head = server->received_search_requests->head;
    while (head != NULL) {
        search_request_log_t* log_request = (search_request_log_t*)head->data;
        if (log_request->delete_timer <= diff) {
            list_pop_at(server->received_search_requests, &prev, &head);
        } else {
            prev = head;
            head = head->next;
        }
    }
}


void handle_pending_downloads(server_t* server) {
    cell_t* prev = NULL, *head = server->pending_downloads->head;
    while (head != NULL) {
        int remove = handle_pending_download(server, *(int*)head->data);
        if (remove) {
            list_pop_at(server->pending_downloads, &prev, &head);
        } else {
            prev = head;
            head = head->next;
        }
    }
}


static int handle_pending_download(server_t* server, int sock) {
    struct pollfd poller;
    int res = poll_fd(&poller, sock, POLLIN, AWAIT_TIMEOUT);
    if (res == 1) {
        opcode_t opcode;
        read_from_fd(sock, &opcode, PKT_ID_SIZE);

        assert(opcode == SMSG_DOWNLOAD);
        handle_remote_download_answer(server, sock);
        return 1;
    }

    return 0;
}


void clear_server(server_t* server) {
    close(server->listening_socket);
    close(server->client_socket);

    for (int i = 0; i < MAX_NEIGHBOURS; ++i) {
        if (server->neighbours[i].sock != -1) {
            close(server->neighbours[i].sock);
            free(server->neighbours[i].port);
        }
        server->neighbours[i].sock = -1;
    }

    for (cell_t* head = server->awaiting_sockets->head; head != NULL; head = head->next) {
        close(*(int*)head);
    }

    for (cell_t* received_head = server->received_search_requests->head;
         received_head != NULL; received_head = received_head->next) {
        search_request_log_t* log = (search_request_log_t*)received_head->data;
        const_free(log->filename);
        const_free(log->source_ip);
        const_free(log->source_port);
    }
    list_destroy(&(server->received_search_requests));

    for (cell_t* awaiting_head = server->awaiting_sockets->head;
         awaiting_head != NULL; awaiting_head = awaiting_head->next) {
        int sock = *(int*)awaiting_head->data;
        close(sock);
    }
    list_destroy(&(server->awaiting_sockets));

    for (cell_t* pending_requests_head = server->pending_requests->head;
         pending_requests_head != NULL; pending_requests_head = pending_requests_head->next) {
        request_t* request = (request_t*)pending_requests_head->data;

        switch (request->type) {
        case REQUEST_DOWNLOAD_LOCAL: {
            download_request_t* download = (download_request_t*)request->request;
            free(download->filename);
            free(download->ip);
            free(download->port);
            break;
        }

        case REQUEST_DOWNLOAD_REMOTE: {
            remote_download_request_t* download = (remote_download_request_t*)request->request;
            free(download->filename);
            close(download->socket);
            break;
        }

        case REQUEST_SEARCH_LOCAL: {
            local_search_request_t* search = (local_search_request_t*)request->request;
            const_free(search->name);
            break;
        }

        case REQUEST_SEARCH_REMOTE: {
            search_request_t* search = (search_request_t*)request->request;
            free(search->filename);
            for (int i = 0; i < search->nb_ips; i++) {
                free(search->ips[i]);
                free(search->ports[i]);
            }
            free(search->ips);
            free(search->ip_source);
            free(search->ports);
            free(search->port_source);
            close(search->source_sock);
            break;
        }

        default:
            break;
        }

        free(request->request);
    }
    list_destroy(&(server->pending_requests));

    for (cell_t* pending_downloads_head = server->pending_downloads->head;
         pending_downloads_head != NULL; pending_downloads_head = pending_downloads_head->next) {
        close(*(int*)pending_downloads_head->data);
    }
    list_destroy(&(server->pending_downloads));
}


void handle_sigint(int sigint) {
    UNUSED(sigint);
    _loop = 0;
}
