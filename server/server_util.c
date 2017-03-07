#define _GNU_SOURCE

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>

#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/types.h>

#include "packets_defines.h"
#include "server_internal.h"
#include "util.h"


void extract_neighbour_from_response(socket_t s, char** ip, char** port) {
    uint8_t ip_len;
    read_from_fd(s, &ip_len, sizeof(uint8_t));

    *ip = malloc(ip_len);
    read_from_fd(s, *ip, ip_len);

    uint8_t port_len;
    read_from_fd(s, &port_len, sizeof(uint8_t));

    *port = malloc(port_len);
    read_from_fd(s, *port, port_len);
}


void* add_new_socket(void* s) {
    int* copy = malloc(sizeof(int));
    *copy = *(int*)s;
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


void compute_and_send_neighbours(server_t* server, socket_t s) {
    uint8_t nb_neighbours = 0;
    char* ips[MAX_NEIGHBOURS] = { NULL };
    char* ports[MAX_NEIGHBOURS] = { NULL };

    for (int i = 0; i < MAX_NEIGHBOURS; i++) {
        if (server->neighbours[i].sock != -1) {
            struct sockaddr addr;
            socklen_t len = sizeof(addr);
            getpeername(server->neighbours[i].sock, &addr, &len);
            ips[nb_neighbours] = malloc(INET6_ADDRSTRLEN);

            if (addr.sa_family == AF_INET) {
                inet_ntop(addr.sa_family, &((struct sockaddr_in*)&addr)->sin_addr, ips[nb_neighbours], len);
            } else {
                inet_ntop(addr.sa_family, &((struct sockaddr_in6*)&addr)->sin6_addr, ips[nb_neighbours], len);
            }
            ports[nb_neighbours] = server->neighbours[i].port;
            nb_neighbours++;
        }
    }

    send_neighbours_list(s, ips, ports, nb_neighbours);
}


void add_neighbour(server_t* server, socket_t s, const char *contact_port) {
    if (server->nb_neighbours == MAX_NEIGHBOURS) {
        return;
    }

    for (int i = 0; i < MAX_NEIGHBOURS; i++) {
        if (server->neighbours[i].sock == -1) {
            server->neighbours[i].sock = s;
            set_string(&(server->neighbours[i].port), contact_port);
            ++server->nb_neighbours;
            return;
        }
    }
}
