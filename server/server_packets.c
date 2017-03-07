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


// SMSG_JOIN (S -> C)
void answer_join_request(server_t* server, socket_t s, uint8_t join) {
    void* data = malloc(PKT_ID_SIZE + sizeof(uint8_t) + sizeof(uint8_t) + 5);
    char* ptr = data;

    *(opcode_t*)ptr = SMSG_JOIN;
    ptr += PKT_ID_SIZE;

    *(uint8_t*)ptr = join;
    ptr += sizeof(uint8_t);

    if (join == 1) {
        char listening_port[6];
        extract_port_from_socket(server->listening_socket, listening_port);

        *(uint8_t*)ptr = strlen(listening_port);
        ptr += sizeof(uint8_t);

        memcpy(ptr, listening_port, strlen(listening_port));
        ptr += strlen(listening_port);
    }

    write_to_fd(s, data, (intptr_t)ptr - (intptr_t)data);

    free(data);
}


// CMSG_NEIGHBOURS (C -> S)
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


// CMSG_JOIN (C -> S)
int send_join_request(server_t* server, const char* ip,
                      const char* port, uint8_t rescue) {
    int socket = -1;
    int res = connect_to(ip, port, &socket);
    if (res != 0) {
        return -1;
    }

    void* data = malloc(PKT_ID_SIZE + sizeof(uint8_t) + sizeof(uint8_t) + 5);
    char* ptr = data;

    *(opcode_t*)ptr = CMSG_JOIN;
    ptr += PKT_ID_SIZE;

    *(uint8_t*)ptr = rescue;
    ptr += sizeof(uint8_t);

    char listening_port[6];
    extract_port_from_socket(server->listening_socket, listening_port);

    *(uint8_t*)ptr = strlen(listening_port);
    ptr += sizeof(uint8_t);

    memcpy(ptr, listening_port, strlen(listening_port));
    ptr += sizeof(strlen(listening_port));

    write_to_fd(socket, data, (intptr_t)ptr - (intptr_t)data);

    free(data);

    return socket;
}


// SMSG_NEIGHBOURS (S -> C)
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

    write_to_fd(s, data, (intptr_t)ptr - (intptr_t)data);

    free(data);
}
