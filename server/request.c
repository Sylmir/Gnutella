#include <stdlib.h>
#include <string.h>

#include "request.h"

void* add_new_request(void* req) {
    request_t* request = (request_t*)req;

    request_t* new_request = malloc(sizeof(request_t));
    new_request->type = request->type;
    new_request->request = request->request;
    return new_request;
}


void* add_new_search_request_log(void* log_request) {
    search_request_log_t* request = (search_request_log_t*)log_request;

    search_request_log_t* new_request = malloc(sizeof(search_request_log_t));
    memcpy(new_request, request, sizeof(search_request_log_t));

    return new_request;
}
