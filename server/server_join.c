#define _GNU_SOURCE

#include <assert.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include <arpa/inet.h>
#include <poll.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#include "common.h"
#include "log.h"
#include "networking.h"
#include "packets_defines.h"
#include "server_internal.h"
#include "util.h"


/*
 * Ensure that we are not adding the same IP again inside our list of neighbours.
 * This is achieved by looping over the array of neighbours and checking port
 * and ip port each, until we're done.
 *
 * The function return 0 if the IP:port is present, 1 if we can safely add.
 */
static int ensure_unique_ip(const server_t* server, const char* ip, const char* port);


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
    applog(LOG_LEVEL_INFO, "[Client] Joining network through %s:%s\n", ip, port);

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

    applog(LOG_LEVEL_INFO, "[Client] Received SMSG_NEIGHBOURS\n");


    /* Store the sockets we send CMSG_JOIN_REQUEST through. */
    list_t* awaiting = list_create(compare_ints, add_new_socket);

    uint8_t nb_neighbours;
    read_from_fd(socket, &nb_neighbours, sizeof(uint8_t));

    applog(LOG_LEVEL_INFO, "[Client] Received %d neighbours from %s:%s\n",
                           nb_neighbours, ip, port);


    char *current_ip, *current_port;
    for (int i = 0; i < nb_neighbours; i++) {
        extract_neighbour_from_response(socket, &current_ip, &current_port);
        applog(LOG_LEVEL_INFO, "[Client] Received neighbour %s:%s\n",
                               current_ip, current_port);
        join(server, current_ip, current_port, awaiting, 0);
    }

    close(socket);

    if (nb_neighbours < MAX_NEIGHBOURS) {
        applog(LOG_LEVEL_INFO, "[Client] Asking contact point to join (remote %s:%s)\n",
                               ip, port);
        if (nb_neighbours == 0) {
            join(server, ip, port, awaiting, 1);
        } else {
            join(server, ip, port, awaiting, 0);
        }
    }

    handle_join_responses(server, awaiting);

    if (nb_neighbours > 0 && server->nb_neighbours < MIN_NEIGHBOURS) {
        for (cell_t* cell = awaiting->head;
             cell != NULL && server->nb_neighbours < MIN_NEIGHBOURS;
             cell = cell->next) {
            extract_ip_port_from_socket_s(*(int*)cell->data, &current_ip, &current_port, 1);
            join_network_through(server, current_ip, current_port);
            free(current_ip);
            free(current_port);
        }
    }

    list_destroy(&awaiting);

    return 0;
}


void join(server_t* server, const char* ip, const char* port, list_t* sockets, int force) {
    applog(LOG_LEVEL_INFO, "[Client] Sending join request to %s:%s\n", ip, port);
    if (ensure_unique_ip(server, ip, port) == 0) {
        applog(LOG_LEVEL_INFO, "[Client] %s:%s already a neighbour.\n", ip, port);
        return;
    }

    int res = send_join_request(server, ip, port, force);
    if (res == -1) {
        return;
    }

    applog(LOG_LEVEL_INFO, "[Client] Successfully sent join request to %s:%s\n",
                           ip, port);

    list_push_back(sockets, &res);

    const_free(ip);
    const_free(port);
}


void handle_join_responses(server_t* server, list_t* targets) {
    for (cell_t* head = targets->head; head != NULL; head = head->next) {
        int res = handle_join_response(*(socket_t*)head->data);
        if (res == 1) {
            /* Continue handling of SMSG_JOIN. */
            uint8_t port_length;
            read_from_fd(*(socket_t*)head->data, &port_length, sizeof(uint8_t));

            char* port = malloc(port_length + 1);
            read_from_fd(*(socket_t*)head->data, port, port_length);
            port[port_length] = '\0';

            add_neighbour(server, *(socket_t*)head->data, port);

            free(port);
        } else {
            close(*(socket_t*)head->data);
        }
    }
}


// Handle SMSG_JOIN (Client)
int handle_join_response(socket_t s) {
    opcode_t opcode;
    read_from_fd(s, &opcode, PKT_ID_SIZE);

    assert(opcode == SMSG_JOIN);

    applog(LOG_LEVEL_INFO, "[Client] Received SMSG_JOIN\n");

    uint8_t answer;
    read_from_fd(s, &answer, sizeof(uint8_t));

    return answer;
}


#define JOIN_CHANCE 50
#define JOIN_CHANCE_MOD 100


// Handle CMSG_JOIN (Server)
int handle_join_request(server_t* server, socket_t sock) {
    uint8_t rescue;
    read_from_fd(sock, &rescue, sizeof(uint8_t));

    uint8_t port_length;
    read_from_fd(sock, &port_length, sizeof(uint8_t));

    char* port = malloc(port_length + 1);
    read_from_fd(sock, port, port_length);
    port[port_length] = '\0';

    uint8_t answer = 0;

    if (server->nb_neighbours == MAX_NEIGHBOURS) {
        answer = 0;
    } else {
        if (rescue == 1) {
            answer = 1;
        } else {
            if (rand() % JOIN_CHANCE_MOD < JOIN_CHANCE) {
                answer = 1;
            } else {
                answer = 0;
            }
        }
    }

    answer_join_request(server, sock, answer);

    if (answer == 1) {
        add_neighbour(server, sock, port);
        free(port);
    }

    return answer;
}


int ensure_unique_ip(const server_t* server, const char* ip, const char* port) {
    for (int i = 0; i < MAX_NEIGHBOURS; i++) {
        if (server->neighbours[i].sock != -1) {
            char* sock_ip, *sock_port;
            extract_ip_port_from_socket_s(server->neighbours[i].sock,
                                          &sock_ip, &sock_port, 1);
            if (strcmp(sock_ip, ip) == 0 && strcmp(sock_port, port) == 0) {
                /* Damn... No unique_ptr to help us clean memory. :( */
                free(sock_ip);
                free(sock_port);
                return 0;
            }

            free(sock_ip);
            free(sock_port);
        }
    }

    return 1;
}
