#ifndef REQUEST_H
#define REQUEST_H

#include <stdint.h>

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
    /* Socket from which the request came from (ensure we don't loop). */
    int source_sock;

    /* Fields of the request. See packets_doc.h for more informations. */
    char* ip_source;
    char* port_source;
    char* filename;
    uint8_t ttl;
    uint8_t nb_ips;
    char** ips;
    char** ports;
} search_request_t;


/*
 * Structure to hold the informations relative to a given research.
 */
typedef struct search_request_log_s {
    /* The amount of time in milliseconds we will wait until the entry is removed. */
   long int delete_timer;
   /* The IP from which the request originated. */
   const char* source_ip;
   /* The port from which the request originated. */
   const char* source_port;
   /* The file searched. */
   const char* filename;
} search_request_log_t;


/*
 * Add a coyp-in-heap of req inside a list (creating function).
 */
LIST_CREATE_FN void* add_new_request(void* req);


/*
 * Add a copy-in-heap of log inside a list (creating function).
 */
LIST_CREATE_FN void* add_new_search_request_log(void* log_request);

#endif /* REQUEST_H */
