#define _GNU_SOURCE

#include <stdint.h>
#include <stdlib.h>

#include <poll.h>
#include <unistd.h>

#include "common.h"
#include "networking.h"
#include "packets_defines.h"
#include "server_internal.h"
#include "util.h"


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


void join(const char* ip, const char* port, list_t* sockets) {
    int res = send_join_request(ip, port, 0);
    if (res == -1) {
        return;
    }

    list_push_back(sockets, &res);

    const_free(ip);
    const_free(port);
}
