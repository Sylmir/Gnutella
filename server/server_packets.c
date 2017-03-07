#define _GNU_SOURCE

#include <assert.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include <arpa/inet.h>
#include <sys/socket.h>

#include "log.h"
#include "networking.h"
#include "packets_defines.h"
#include "server_internal.h"
#include "util.h"

#define JOIN_CHANCE 80
#define JOIN_CHANCE_MOD 100

void answer_join_request(socket_t s) {
    uint8_t answer;
    if (rand() % JOIN_CHANCE_MOD < JOIN_CHANCE) {
        answer = 1;
    } else {
        answer = 0;
    }

    void* data = malloc(PKT_ID_SIZE + sizeof(uint8_t));
    char* ptr = data;

    *(opcode_t*)ptr = SMSG_JOIN;
    ptr += PKT_ID_SIZE;

    *(uint8_t*)ptr = answer;
    ptr += sizeof(uint8_t);

    write_to_fd(s, data, (intptr_t)ptr - (intptr_t)answer);
}


int send_neighbours_request(const char* ip, const char* port) {
    applog(LOG_LEVEL_INFO, "send_neighbours_request: %s:%s\n", ip, port);
    int socket = -1;
    int res = connect_to(ip, port, &socket);
    if (res != CONNECT_OK) {
        return -1;
    }

    opcode_t opcode = CMSG_NEIGHBOURS;
    write_to_fd(socket, &opcode, PKT_ID_SIZE);

    return socket;
}


int send_join_request(server_t* server, const char* ip,
                      const char* port, uint8_t rescue) {
    int socket = -1;
    int res = connect_to(ip, port, &socket);
    if (res != 0) {
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


void send_neighbours_list(socket_t s, char **ips, char **ports,
                          uint8_t nb_neighbours) {
    void* data = malloc(PKT_ID_SIZE + sizeof(uint8_t) +
                        nb_neighbours * (INET6_ADDRSTRLEN + 5 + 2 * sizeof(uint8_t)));
    char* ptr = (char*)data;
    *(opcode_t*)ptr = SMSG_NEIGHBOURS;
    ptr += PKT_ID_SIZE;

    *(uint8_t*)ptr = nb_neighbours;
    ptr += sizeof(uint8_t);

    for (int i = 0; i < nb_neighbours; i++) {
        const char* ip = ips[i];

        *(uint8_t*)ptr = strlen(ip);
        ptr += sizeof(uint8_t);

        memcpy(ptr, ip, strlen(ip));
        ptr += strlen(ip);

        const char* port = ports[i];
        *(uint8_t*)ptr = strlen(port);
        ptr += sizeof(uint8_t);

        memcpy(ptr, port, strlen(port));
        ptr += strlen(port);
    }

    ptrdiff_t length = (ptrdiff_t)ptr - (ptrdiff_t)data;
    assert(length >= 0);

    write_to_fd(s, data, (size_t)length);

    free(data);
}
