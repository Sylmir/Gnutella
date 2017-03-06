#define _GNU_SOURCE

#include <assert.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include <arpa/inet.h>
#include <sys/socket.h>

#include "networking.h"
#include "packets_defines.h"
#include "server_internal.h"
#include "util.h"

void answer_join_request(int socket) {

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


void send_neighbours_list(int socket, struct sockaddr *neighbours,
                          uint8_t nb_neighbours) {
    void* data = malloc(PKT_ID_SIZE + sizeof(uint8_t) +
                        nb_neighbours * (IP_VERSION_ID_SIZE + INET6_ADDRSTRLEN + sizeof(in_port_t)) + sizeof(uint8_t));
    char* ptr = (char*)data;
    *(opcode_t*)ptr = SMSG_NEIGHBOURS;
    ptr += PKT_ID_SIZE;

    *(uint8_t*)ptr = nb_neighbours;
    ptr += sizeof(uint8_t);

    for (int i = 0; i < nb_neighbours; i++) {
        if ((neighbours + i)->sa_family == AF_INET) {
            *(ip_version_id_t*)ptr = 4;
        } else if ((neighbours + i)->sa_family == AF_INET6) {
            *(ip_version_id_t*)ptr = 6;
        }
        ptr += IP_VERSION_ID_SIZE;

        in_port_t port;
        const char* ip = extract_ip_classic(neighbours + i, &port);

        *(uint8_t*)ptr = strlen(ip);
        ptr += sizeof(uint8_t);

        memcpy(ptr, ip, strlen(ip));
        ptr += strlen(ip);

        *(in_port_t*)ptr = port;
        ptr += sizeof(in_port_t);
    }

    ptrdiff_t length = (ptrdiff_t)ptr - (ptrdiff_t)data;
    assert(length >= 0);

    write_to_fd(socket, data, (size_t)length);

    free(data);
}
