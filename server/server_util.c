#define _GNU_SOURCE

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>

#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/types.h>

#include "log.h"
#include "packets_defines.h"
#include "server_internal.h"
#include "util.h"


// Handle SMSG_NEIGHBOURS (Client)
void extract_neighbour_from_response(socket_t s, char** ip, char** port) {
    uint8_t ip_len;
    read_from_fd(s, &ip_len, sizeof(uint8_t));

    *ip = malloc(ip_len + 1);
    read_from_fd(s, *ip, ip_len);
    (*ip)[ip_len] = '\0';

    uint8_t port_len;
    read_from_fd(s, &port_len, sizeof(uint8_t));

    *port = malloc(port_len + 1);
    read_from_fd(s, *port, port_len);
    (*port)[port_len] = '\0';
}


void* add_new_socket(void* s) {
    int* copy = malloc(sizeof(int));
    *copy = *(int*)s;
    return copy;
}


// Build data for SMSG_NEIGHBOURS (Server)
void compute_and_send_neighbours(server_t* server, socket_t s) {
    uint8_t nb_neighbours = 0;
    char* ips[MAX_NEIGHBOURS] = { NULL };
    char* ports[MAX_NEIGHBOURS] = { NULL };

    for (int i = 0; i < MAX_NEIGHBOURS; i++) {
        if (server->neighbours[i].sock != -1) {
            struct sockaddr addr;
            socklen_t len = sizeof(addr);
            getpeername(server->neighbours[i].sock, &addr, &len);
            ips[nb_neighbours] = malloc(INET6_ADDRSTRLEN + 1);

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

    for (int i = 0; i < nb_neighbours; ++i) {
        free(ips[i]);
    }
}


void add_neighbour(server_t* server, socket_t s, const char *contact_port) {
    if (server->nb_neighbours == MAX_NEIGHBOURS) {
        return;
    }

    char* ip = extract_ip_from_socket_s(s, 1);
    applog(LOG_LEVEL_INFO, "[Client] Adding neighbour %s, contact %s (%d)\n",
                           ip, contact_port, s);
    free(ip);

    for (int i = 0; i < MAX_NEIGHBOURS; i++) {
        if (server->neighbours[i].sock == -1) {
            server->neighbours[i].sock = s;
            set_string(&(server->neighbours[i].port), contact_port);
            ++server->nb_neighbours;
            return;
        }
    }

    applog(LOG_LEVEL_ERROR, "[Client] Neighbour not added !\n");
}


