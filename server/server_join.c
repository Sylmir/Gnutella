#define _GNU_SOURCE

#include <assert.h>
#include <stdint.h>
#include <stdlib.h>

#include <poll.h>
#include <unistd.h>

#include "common.h"
#include "log.h"
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
    applog(LOG_LEVEL_INFO, "join_network: %s:%s\n", ip, port);

    int socket = send_neighbours_request(ip, port);
    if (socket == -1) {
        return -1;
    }

    opcode_t opcode;
    read_from_fd(socket, &opcode, PKT_ID_SIZE);

    applog(LOG_LEVEL_INFO, "join_network: opcode %d\n", opcode);

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
        applog(LOG_LEVEL_INFO, "join_network: remote %s:%s\n", current_ip, current_port);
        join(server, current_ip, current_port, awaiting);
    }

    close(socket);

    if (nb_neighbours < MAX_NEIGHBOURS) {
        /* extract_neighbour_from_socket(socket, &current_ip, &current_port);
        applog(LOG_LEVEL_INFO, "join_network: contact %s:%s\n", current_ip, current_port); */
        join(server, ip, port, awaiting);
    }

    handle_join_responses(server, awaiting);

    list_destroy(&awaiting);

    return 0;
}


void join(server_t* server, const char* ip, const char* port, list_t* sockets) {
    applog(LOG_LEVEL_INFO, "join: %s:%s\n", ip, port);
    int res = send_join_request(server, ip, port, 0);
    if (res == -1) {
        return;
    }

    applog(LOG_LEVEL_INFO, "join: success %d\n", res);

    list_push_back(sockets, &res);

    const_free(ip);
    const_free(port);
}


void handle_join_responses(server_t* server, list_t* targets) {
    for (cell_t* head = targets->head; head != NULL; head = head->next) {
        int res = handle_join_response(*(socket_t*)head->data);
        if (res == 1) {
            char* port = NULL;
            add_neighbour(server, *(socket_t*)head->data, port);
        } else {
            close(*(socket_t*)head->data);
        }
    }
}


int handle_join_response(socket_t s) {
    opcode_t opcode;
    read_from_fd(s, &opcode, PKT_ID_SIZE);

    assert(opcode == SMSG_JOIN);

    uint8_t answer;
    read_from_fd(s, &answer, sizeof(uint8_t));

    return answer;
}
