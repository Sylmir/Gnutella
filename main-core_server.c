#define _GNU_SOURCE

#include <signal.h>

#include "common.h"
#include "core_server.h"
#include "networking.h"
#include "util.h"


/* Continue the server loop. */
sig_atomic_t _loop = 1;


typedef struct core_server_s {
    int listening_socket;
} core_server_t;


/*
 * Perform the main loop, i.e accept for connection and sends people a list of
 * neighbours.
 */
static void loop(core_server_t* server);


/*
 * SIGINT handler function. Set (atomically) _loop to false so we can stop the
 * core server.
 */
static void handle_sigint(int sigint);


/* Maximum number of pending requests on the listening socket. */
#define MAX_PENDING_REQUESTS 50


/******************************************************************************/


static int run_core_server() {
    core_server_t core_server;
    int listening_socket = create_listening_socket(CONTACT_PORT,
                                                   MAX_PENDING_REQUESTS);

    if (listening_socket < 0) {
        return EXIT_FAILURE;
    }

    core_server.listening_socket = listening_socket;
    loop(&core_server);

    return 0;
}


void loop(core_server_t* server) {
    while (_loop) {

    }
}


int main() {
    signal(SIGINT, handle_sigint);
    return run_core_server();
}


void handle_sigint(int sigint) {
    UNUSED(sigint);
    _loop = 0;
}
