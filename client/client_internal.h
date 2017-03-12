#ifndef CLIENT_INTERNAL_H
#define CLIENT_INTERNAL_H


#include "list.h"


/* The structure to represent the client. */
typedef struct client_s {
    /* Socket to communicate with the associated server. */
    int server_socket;
    /* List of files and the machines that possess them. */
    list_t* machines_by_files;
} client_t;

typedef struct machine_s {
    /* IP of the machine. */
    const char* ip;
    /* Port to contact the machine. */
    const char* port;
} machine_t;


/* Hold the machines that have a given fail. */
typedef struct file_lookup_s {
    /* Name of the file. */
    const char* filename;
    /* List of machines that have the file. */
    list_t* machines;
} file_lookup_t;


/*******************************************************************************
 * File search
 */


/*
 * Handle the search of a file.
 */
void handle_search(client_t* client);


/*
 * Handle the response (SMSG_INT_SEARCH) to a request. After reading the packet,
 * we create a new record or add the infos to an existing one (this prevents
 * machines duplication).
 */
void handle_search_answer(client_t* client);


/*******************************************************************************
 * File lookup
 */


/*
 * Search if the client has already a list of machines for the given file. Return
 * 0 if there is no record, 1 if there is one. If there is one, *dest is set to
 * point to the record.
 *
 * There is no need to malloc the underlying pointer, the function will set it.
 * Also, there is no need to free the underlying pointer, no memory allocation
 * is performed.
 */
int has_file_record(client_t* client, const char* filename, file_lookup_t** dest);


/*
 * Check if record contains a record (ahah, record in record) for the given
 * machine. If there is a record, the function returns 1, otherwise it
 * returns 1.
 */
int has_file_machine_record(const file_lookup_t* record, const char* ip, const char* port);


/*
 * Display the informations about the machine that possess a given file.
 */
void handle_lookup(client_t* client);


/*
 * Create a copy-in-heap of machine and return it.
 */
LIST_CREATE_FN void* create_machine(void* machine);


/*
 * Create a copy-in-heap of lookup and return it.
 */
LIST_CREATE_FN void* create_lookup(void* lookup);


/*******************************************************************************
 * File download
 */


/*
 * Handle the download of a file.
 */
void handle_download(client_t* client);


#endif /* CLIENT_INTERNAL_H */
