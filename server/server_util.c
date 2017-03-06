#define _GNU_SOURCE

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>

#include <arpa/inet.h>
#include <sys/socket.h>

#include "packets_defines.h"
#include "server_internal.h"
#include "util.h"


void extract_neighbour_from_response(int socket, char** ip, char** port) {
    ip_version_id_t version;
    read_from_fd(socket, &version, IP_VERSION_ID_SIZE);

    uint8_t ip_len;
    read_from_fd(socket, &ip_len, sizeof(uint8_t));

    *ip = malloc(ip_len);
    read_from_fd(socket, *ip, ip_len);

    in_port_t iport;
    read_from_fd(socket, &iport, sizeof(in_port_t));

    *port = (char*)int_to_cstring(iport);
}


void extract_neighbour_from_socket(int socket, char** ip, char** port) {
    struct sockaddr addr;
    socklen_t len = sizeof(addr);

    getpeername(socket, &addr, &len);

    in_port_t iport;
    *ip = (char*)extract_ip_classic(&addr, &iport);
    *port = (char*)int_to_cstring(iport);
}


void* add_new_socket(void* socket) {
    int* copy = malloc(sizeof(int));
    *copy = *(int*)socket;
    return copy;
}


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


void compute_and_send_neighbours(server_t* server, int socket) {
    uint8_t nb_neighbours = 0;
    struct sockaddr* neighbours = malloc(MAX_NEIGHBOURS * sizeof(struct sockaddr));

    for (int i = 0; i < MAX_NEIGHBOURS; i++) {
        if (server->neighbours[i] != -1) {
            socklen_t len = sizeof(*(neighbours + nb_neighbours));
            getpeername(server->neighbours[i], neighbours + nb_neighbours, &len);
            nb_neighbours++;
        }
    }

    send_neighbours_list(socket, neighbours, nb_neighbours);
}
