#define _GNU_SOURCE

#include <assert.h>
#include <errno.h>
#include <signal.h>
#include <stdlib.h>
#include <string.h>

#include <fcntl.h>
#include <netdb.h>
#include <poll.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#include "client.h"
#include "common.h"
#include "log.h"
#include "networking.h"
#include "packets_defines.h"
#include "server.h"
#include "util.h"


/* The structure to represent the client. */
typedef struct client_s {
    /* Socket to communicate with the associated server. */
    int server_socket;
} client_t;


/*
 * Performs the handshake with the local server. This is done by writing
 * CMSG_INT_HANDSHAKE in the server socket and then waiting for the server to
 * answer with SMSG_INT_HANDSHAKE.
 */
ERROR_CODES_USUAL static int handshake(const client_t* client);

/* Max number of times we check for the server state. */
#define MAX_SERVER_UP_CHECK 3
/* Timeout when we check for the server state. */
#define SERVER_UP_CHECK_TIMEOUT 1
#define SERVER_UP_CHECK_TIMEOUT_MS SERVER_UP_CHECK_TIMEOUT * IN_MILLISECONDS


int run_client(const char* connection_port) {
    client_t client;

    int res = attempt_connect_to("127.0.0.1", connection_port == NULL ? SERVER_LISTEN_PORT : connection_port,
                                 &(client.server_socket), MAX_SERVER_UP_CHECK,
                                 SERVER_UP_CHECK_TIMEOUT);
    if (res == -1) {
        applog(LOG_LEVEL_ERROR, "[Client] Serveur innaccessible. Extinction.\n");
        return EXIT_FAILURE;
    }

    res = handshake(&client);
    if (res == -1) {
        return -1;
    }

    applog(LOG_LEVEL_INFO, "[Client] Serveur OK (Handshake).\n");

    close(client.server_socket);

    return EXIT_SUCCESS;
}


int handshake(const client_t* client) {
    opcode_t data = CMSG_INT_HANDSHAKE;
    write_to_fd(client->server_socket, &data, PKT_ID_SIZE);

    opcode_t server_data = 0;
    read_from_fd(client->server_socket, &server_data, PKT_ID_SIZE);

    if (server_data != SMSG_INT_HANDSHAKE) {
        return -1;
    }

    return 0;
}
