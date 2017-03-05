#define _GNU_SOURCE

#include <assert.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <arpa/inet.h>
#include <sys/socket.h>

#include "packets_defines.h"
#include "server_common.h"
#include "util.h"


const char* extract_ip(const struct sockaddr_storage* addr, in_port_t* port) {
    char* ip                = NULL;
    void* target            = NULL;
    socklen_t target_size   = 0;

    if (addr->ss_family == AF_INET) {
        ip = malloc(INET_ADDRSTRLEN);
        target = &((struct sockaddr_in*)addr)->sin_addr;
        target_size = INET_ADDRSTRLEN;
        *port = ((const struct sockaddr_in*)addr)->sin_port;
    } else if (addr->ss_family == AF_INET6) {
        ip = malloc(INET6_ADDRSTRLEN);
        target = &((struct sockaddr_in6*)addr)->sin6_addr;
        target_size = INET6_ADDRSTRLEN;
        *port = ((const struct sockaddr_in6*)addr)->sin6_port;
    }

    return inet_ntop(addr->ss_family, target, ip,
                     target_size);
}


const char* extract_ip_classic(const struct sockaddr* addr, in_port_t* port) {
    char* ip                = NULL;
    void* target            = NULL;
    socklen_t target_size   = 0;

    if (addr->sa_family == AF_INET) {
        ip = malloc(INET_ADDRSTRLEN);
        target = &((struct sockaddr_in*)addr)->sin_addr;
        target_size = INET_ADDRSTRLEN;
        *port = ((const struct sockaddr_in*)addr)->sin_port;
    } else if (addr->sa_family == AF_INET6) {
        ip = malloc(INET6_ADDRSTRLEN);
        target = &((struct sockaddr_in6*)addr)->sin6_addr;
        target_size = INET6_ADDRSTRLEN;
        *port = ((const struct sockaddr_in6*)addr)->sin6_port;
    } else {
        printf("Famille = %d\n", addr->sa_family);
        assert(0);
    }

    return inet_ntop(addr->sa_family, target, ip,
                     target_size);
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
