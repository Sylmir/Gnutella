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
static int ensure_absent_ip(const server_t* server, const char* ip, const char* port);


int join_network(server_t* server, const char* ip, const char* port) {
    int res;

    if (ip == NULL) {
        if (port == NULL) {
            res = join_network_through(server, CONTACT_POINT, CONTACT_PORT, JOIN_MAX_ATTEMPTS);
        } else {
            res = join_network_through(server, CONTACT_POINT, port, JOIN_MAX_ATTEMPTS);
        }
    } else {
        if (port == NULL) {
            res = join_network_through(server, ip, CONTACT_PORT, JOIN_MAX_ATTEMPTS);
        } else {
            res = join_network_through(server, ip, port, JOIN_MAX_ATTEMPTS);
        }
    }

    return res;
}


int join_network_through(server_t* server, const char* ip, const char* port,
                         int nb_attempts) {
    if (nb_attempts < 0) {
        return 0;
    }

    printf("toto\n");
    applog(LOG_LEVEL_INFO, "[Client] Joining network through %s:%s\n", ip, port);

    int socket = send_neighbours_request(ip, port);
    if (socket == -1) {
        return -1;
    }

    if (server->self_ip == NULL) {
        char* self_ip, *self_port;
        extract_ip_port_from_socket_s(socket, &self_ip, &self_port, 0);
        applog(LOG_LEVEL_INFO, "Deduced self IP: %s\n", self_ip);
        server->self_ip = self_ip;
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


    char ips[MAX_NEIGHBOURS][INET6_ADDRSTRLEN + 1];
    char ports[MAX_NEIGHBOURS][6];
    int index = 0;

    char *current_ip, *current_port;
    for (int i = 0; i < nb_neighbours; i++) {
        extract_neighbour_from_response(socket, &current_ip, &current_port);
        applog(LOG_LEVEL_INFO, "[Client] Received neighbour %s:%s\n",
                               current_ip, current_port);
        if (server->self_ip != NULL) {
            if(strcmp(current_ip, server->self_ip) == 0) {
                applog(LOG_LEVEL_WARNING, "[Client] Received ourselves as neighbour. Ignoring.\n");
                continue;
            }
        }

        strcpy(ips[index], current_ip);
        strcpy(ports[index], current_port);
        index++;

        join(server, current_ip, current_port, awaiting, 0);
    }

    close(socket);

    if (nb_neighbours < MAX_NEIGHBOURS) {
        applog(LOG_LEVEL_INFO, "[Client] Asking contact point to join (remote %s:%s)\n",
                               ip, port);

        strcpy(ips[index], ip);
        strcpy(ports[index], port);
        index++;

        char* contact_ip = malloc(strlen(ip));
        strcpy(contact_ip, ip);

        char* contact_port = malloc(strlen(ip));
        strcpy(contact_port, port);

        if (nb_neighbours == 0) {
            join(server, contact_ip, contact_port, awaiting, 1);
        } else {
            join(server, contact_ip, contact_port, awaiting, 0);
        }
    }

    handle_join_responses(server, awaiting);

    if (nb_neighbours > 0 && server->nb_neighbours < MIN_NEIGHBOURS) {
        for (int i = 0; i < index; i++) {
            printf("Voisins supplémentaires sur %s:%s\n", ips[i], ports[i]);
            join_network_through(server, ips[i], ports[i], nb_attempts - 1);
        }
    }

    list_destroy(&awaiting);

    return 0;
}


void join(server_t* server, const char* ip, const char* port, list_t* sockets, int force) {
    applog(LOG_LEVEL_INFO, "[Client] Sending join request to %s:%s\n", ip, port);
    if (ensure_absent_ip(server, ip, port) == 0) {
        applog(LOG_LEVEL_INFO, "[Client] %s:%s already a neighbour.\n", ip, port);
        return;
    }

    int res = send_join_request(server, ip, port, force);
    if (res == -1) {
        return;
    }

    applog(LOG_LEVEL_INFO, "[Client] Successfully sent join request to %s:%s (%d)\n",
                           ip, port, res);

    list_push_back(sockets, &res);

    const_free(ip);
    const_free(port);
}


void handle_join_responses(server_t* server, list_t* targets) {
    for (cell_t* head = targets->head; head != NULL; head = head->next) {
        int res = handle_join_response(*(int*)head->data);
        applog(LOG_LEVEL_INFO, "[Client] SMSG_JOIN (%d) => %d\n", *(int*)head->data, res);
        if (res == 1) {
            /* Continue handling of SMSG_JOIN. */
            uint8_t port_length;
            read_from_fd(*(int*)head->data, &port_length, sizeof(uint8_t));

            char* port = malloc(port_length + 1);
            read_from_fd(*(int*)head->data, port, port_length);
            port[port_length] = '\0';

            add_neighbour(server, *(int*)head->data, port);

            free(port);
        } else {
            /* Because other extremity is already closed. */
            close(*(int*)head->data);
        }
    }
}


// Handle SMSG_JOIN (Client)
int handle_join_response(int s) {
    opcode_t opcode;
    read_from_fd(s, &opcode, PKT_ID_SIZE);

    assert(opcode == SMSG_JOIN);

    applog(LOG_LEVEL_INFO, "[Client] Received SMSG_JOIN (%d)\n", s);

    uint8_t answer;
    read_from_fd(s, &answer, sizeof(uint8_t));

    return answer;
}


#define JOIN_CHANCE 50
#define JOIN_CHANCE_MOD 100


// Handle CMSG_JOIN (Server)
int handle_join_request(server_t* server, int sock) {
    if (server->self_ip == NULL) {
        server->self_ip = extract_ip_from_socket_s(sock, 0);
        applog(LOG_LEVEL_INFO, "[Server] Deduced self IP : %s\n", server->self_ip);
    }

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
    }

    free(port);

    return answer;
}


int ensure_absent_ip(const server_t* server, const char* ip, const char* port) {
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


int handle_leave(server_t* server, socket_contact_t* departed) {
    close(departed->sock);
    departed->sock = -1;
    free_reset(&(departed->port));

    --server->nb_neighbours;

    if (server->nb_neighbours < MIN_NEIGHBOURS) {
        for (int i = 0; i < MAX_NEIGHBOURS; i++) {
            if (server->neighbours[i].sock != -1) {
                char* contact_ip, *contact_port;
                extract_ip_port_from_socket_s(server->neighbours[i].sock,
                                              &contact_ip, &contact_port, 1);

                /*
                 * Technically this means the other end is closed, so we could
                 * remove it right now, but why complicate the function with
                 * recursion that might make us loop weirdly ?
                 */
                if (strcmp(contact_ip, "") == 0 || strcmp(contact_port, "") == 0) {
                    free(contact_ip);
                    free(contact_port);
                    continue;
                }

                join_network_through(server, contact_ip, contact_port, JOIN_MAX_ATTEMPTS);
                free(contact_ip);
                free(contact_port);
                break;
            }
        }
    }

    if (server->nb_neighbours == 0) {
        return 1;
    }

    return 0;
}
