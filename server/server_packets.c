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
void answer_join_request(server_t* server, int s, uint8_t join) {
    void* data = malloc(PKT_ID_SIZE + sizeof(uint8_t) + sizeof(uint8_t) + 5);
    char** ptr = (char**)&data;

    opcode_t opcode = SMSG_JOIN;
    write_to_packet(ptr, &opcode, PKT_ID_SIZE);
    write_to_packet(ptr, &join, sizeof(uint8_t));

    if (join == 1) {
        // Trailing '\0' not included
        char listening_port[6];
        extract_port_from_socket(server->listening_socket, listening_port, 0);

        uint8_t port_length = strlen(listening_port);
        write_to_packet(ptr, &port_length, sizeof(uint8_t));
        write_to_packet(ptr, listening_port, port_length);
    }

    write_to_fd(s, data, (intptr_t)*ptr - (intptr_t)data);

    applog(LOG_LEVEL_INFO, "[Server] Sent SMSG_JOIN\n");

    free(data);
}


// CMSG_NEIGHBOURS (C -> S)
int send_neighbours_request(const char* ip, const char* port) {
    applog(LOG_LEVEL_INFO, "[Client] Sending neighbours request to %s:%s\n",
                           ip, port);
    int socket = -1;
    int res = connect_to(ip, port, &socket);
    if (res != CONNECT_OK) {
        return -1;
    }

    opcode_t opcode = CMSG_NEIGHBOURS;
    write_to_fd(socket, &opcode, PKT_ID_SIZE);

    applog(LOG_LEVEL_INFO, "[Client] Sent CMSG_NEIGHBOURS\n");

    return socket;
}


// CMSG_JOIN (C -> S)
int send_join_request(server_t* server, const char* ip,
                      const char* port, uint8_t rescue) {
    int socket = -1;
    int res = connect_to(ip, port, &socket);
    if (res != CONNECT_OK) {
        return -1;
    }

    void* data = malloc(PKT_ID_SIZE + sizeof(uint8_t) + sizeof(uint8_t) + 5);
    char** ptr = (char**)&data;

    opcode_t opcode = CMSG_JOIN;
    write_to_packet(ptr, &opcode, PKT_ID_SIZE);
    write_to_packet(ptr, &rescue, sizeof(uint8_t));

    char listening_port[6];
    extract_port_from_socket(server->listening_socket, listening_port, 0);

    uint8_t port_length = strlen(listening_port);
    write_to_packet(ptr, &port_length, sizeof(uint8_t));
    write_to_packet(ptr, listening_port, port_length);

    write_to_fd(socket, data, (intptr_t)*ptr - (intptr_t)data);

    applog(LOG_LEVEL_INFO, "[Client] Sent CMSG_JOIN\n");

    free(data);

    return socket;
}


// SMSG_NEIGHBOURS (S -> C)
void send_neighbours_list(int s, char **ips, char **ports,
                          uint8_t nb_neighbours) {
    void* data = malloc(PKT_ID_SIZE + sizeof(uint8_t) +
                        nb_neighbours * (INET6_ADDRSTRLEN + 5 + 2 * sizeof(uint8_t)));
    char** ptr = (char**)&data;

    opcode_t opcode = SMSG_NEIGHBOURS;
    write_to_packet(ptr, &opcode, PKT_ID_SIZE);
    write_to_packet(ptr, &nb_neighbours, sizeof(uint8_t));

    // Trailing '\0' not written
    for (int i = 0; i < nb_neighbours; i++) {
        const char* ip = ips[i];
        uint8_t ip_length = strlen(ip);

        const char* port = ports[i];
        uint8_t port_length = strlen(port);

        write_to_packet(ptr, &ip_length, sizeof(uint8_t));
        write_to_packet(ptr, ip, ip_length);
        write_to_packet(ptr, &port_length, sizeof(uint8_t));
        write_to_packet(ptr, port, port_length);
    }

    write_to_fd(s, data, (intptr_t)*ptr - (intptr_t)data);

    applog(LOG_LEVEL_INFO, "[Server] Sent SMSG_NEIGHBOURS\n");

    free(data);
}


void broadcast_packet(server_t* server, void* packet, size_t size) {
    for (int i = 0; i < MAX_NEIGHBOURS; ++i) {
        if (server->neighbours[i].sock != -1) {
            write_to_fd(server->neighbours[i].sock, packet, size);
        }
    }
}


void broadcast_packet_to(int* sockets, int nb_sockets, void* packet, size_t size) {
    for (int i = 0; i < nb_sockets; i++) {
        write_to_fd(sockets[i], packet, size);
    }
}


void leave_network(server_t* server) {
    opcode_t opcode = CMSG_LEAVE;
    broadcast_packet(server, &opcode, PKT_ID_SIZE);
}
