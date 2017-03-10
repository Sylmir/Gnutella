#ifndef REQUEST_H
#define REQUEST_H

#include "list.h"


/*
 * Enumerate the different type of request that are awaiting.
 */
typedef enum request_type_e {
    /* Searching for a file (request from local client). */
    REQUEST_SEARCH_LOCAL    = 0,
    /* Awaiting to download a file (request from local client). */
    REQUEST_DOWNLOAD_LOCAL  = 1,
    /* Searching for a file (request from remote). */
    REQUEST_SEARCH_REMOTE   = 2,
    /* Awaiting to upload a file (request from remote). */
    REQUEST_DOWNLOAD_REMOTE = 3,
} request_type_t;


/*
 * Represent a request.
 */
typedef struct request_s {
    /* The type of the request. */
    request_type_t type;
    /* Underlying data structure containing the informations. */
    void* request;
} request_t;


/*
 * Structure to hold a local search request.
 */
typedef struct local_search_request_s {
    const char* name;
} local_search_request_t;


/*
 * Structure to hold a remote search request.
 */
typedef struct search_request_s {
    void* header;
} search_request_t;


/*
 * Add a coyp-in-heap of req inside a list (creating function).
 */
LIST_CREATE_FN void* add_new_request(void* req);

#endif /* REQUEST_H */
