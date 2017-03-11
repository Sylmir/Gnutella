#define _GNU_SOURCE

#include <stdint.h>

#include "log.h"
#include "server_internal.h"
#include "util.h"

void handle_local_search_request(server_t* server) {
    uint8_t name_len;
    read_from_fd(server->client_socket, &name_len, sizeof(uint8_t));

    char* name = malloc(name_len + 1);
    read_from_fd(server->client_socket, name, name_len);
    name[name_len] = '\0';

    applog(LOG_LEVEL_INFO, "[Local Server] Searching file %s\n", name);

    request_t main_request;
    main_request.type = REQUEST_SEARCH_LOCAL;

    local_search_request_t* request = malloc(sizeof(local_search_request_t));
    request->name = name;

    main_request.request = request;

    list_push_back(server->pending_requests, &main_request);
}
