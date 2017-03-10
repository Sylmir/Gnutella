#include <stdlib.h>

#include "request.h"

void* add_new_request(void* req) {
    request_t* request = (request_t*)req;

    request_t* new_request = malloc(sizeof(request_t));
    new_request->type = request->type;
    new_request->request = request->request;
    return new_request;
}
