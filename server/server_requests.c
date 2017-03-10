#define _GNU_SOURCE

#include <stdint.h>
#include <stdlib.h>

#include <arpa/inet.h>

#include "packets_defines.h"
#include "server_internal.h"
#include "util.h"


static void search_file(const char* filename);

void handle_local_search_request(server_t* server, request_t* request) {
    local_search_request_t* local_request = (local_search_request_t*)request->request;



    void* packet = malloc(PKT_ID_SIZE + sizeof(uint8_t) + INET6_ADDRSTRLEN +
                          sizeof(uint8_t) + UINT8_MAX + 4 + 1 + 1);
}


void handle_remote_search_request(server_t* server, request_t* request) {

}


void handle_local_download_request(server_t* server, request_t* request) {

}


void handle_remote_download_request(server_t* server, request_t* request) {

}
