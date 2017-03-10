#define _GNU_SOURCE

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include <arpa/inet.h>

#include "packets_defines.h"
#include "server_internal.h"
#include "util.h"


/*
 * Search the file filename inside the directory containing the downloadable
 * files. If a match is found, the function return 1, otherwise it returns
 * 0. (Also returns 0 if there was an error searching file).
 */
static int search_file(const char* filename);

static const uint8_t DEFAULT_TTL = 10;

void handle_local_search_request(server_t* server, request_t* request) {
    local_search_request_t* local_request = (local_search_request_t*)request->request;

    void* packet = malloc(PKT_ID_SIZE + sizeof(uint8_t) + INET6_ADDRSTRLEN +
                          sizeof(uint8_t) + UINT8_MAX + 4 + 1 + 1);
    char* data = packet;

    *(opcode_t*)data = CMSG_SEARCH_REQUEST;
    data += PKT_ID_SIZE;

    char local_ip[INET6_ADDRSTRLEN];
    extract_self_ip(local_ip);

    *(uint8_t*)data = strlen(local_ip);
    data += sizeof(uint8_t);

    memcpy(data, local_ip, strlen(local_ip));
    data += strlen(local_ip);

    *(uint8_t*)data = strlen(local_request->name);
    data += sizeof(uint8_t);

    memcpy(data, local_request->name, strlen(local_request->name));
    data += strlen(local_request->name);

    *(uint32_t*)data = server->packet_counter;
    data += sizeof(uint32_t);
    ++server->packet_counter;

    *(uint8_t*)data = DEFAULT_TTL;
    data += sizeof(uint8_t);

    *(uint8_t*)data = 0;
    data += sizeof(uint8_t);

    broadcast_packet(server, packet, (intptr_t)data - (intptr_t)packet);

    free(packet);
}


void handle_remote_search_request(server_t* server, request_t* request) {

}


void handle_local_download_request(server_t* server, request_t* request) {

}


void handle_remote_download_request(server_t* server, request_t* request) {

}
