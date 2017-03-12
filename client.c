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
#include "client_defines.h"
#include "client_internal.h"
#include "common.h"
#include "list.h"
#include "log.h"
#include "networking.h"
#include "packets_defines.h"
#include "server.h"
#include "util.h"


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


/*
 * Main client loop.
 */
static int loop(client_t* client);


/*
 * Notify the server that we are exiting and subsequently close the socket to
 * communicate with the server. After a call to this function, the client is
 * ready to exit and should usually wait for the server to shut down.
 */
static void clear_client(client_t* client);


/*
 * Write the exit packet and send it to the local server.
 */
static void write_exit_packet(client_t* client);


/*
 * Display the help on the standard output. This includes the different commands
 * the user can type in and how to use them.
 */
static void display_help();


/*
 * Read a line on the standard input and return it as a mallocated string.
 */
static char* read_command();


/*
 * Read the new packets sent by the local servent.
 */
static void handle_servent(client_t* client);


/*
 * Handle the command passed as a parameter.
 */
static void handle_command(client_t* client, char* command);


/*
 * Handle the download of a file.
 */
static void handle_download(client_t* client);


#define SEARCH_COMMAND "search"
#define DOWNLOAD_COMMAND "download"
#define LOOKUP_COMMAND "lookup"
#define HELP_COMMAND "help"
#define EXIT_COMMAND "exit"
static const char* commands[][2] = {
    { HELP_COMMAND, "Affiche l'aide." },
    { SEARCH_COMMAND " nom_de_fichier", "Demande la recherche du fichier <nom_de_fichier>." },
    { LOOKUP_COMMAND " nom_de_fichier", "Demander la liste des machines qui possèdent le fichier <nom_de_fichier>." },
    { DOWNLOAD_COMMAND " nom_de_fichier [ip port]", "Effectue le téléchargement du fichier <nom_de_fichier> depuis la machine d'IP <ip> sur le port <port>.\n"
                                           "\tSi IP ou port n'est pas spécifié, une recherche est effectuée, puis une machine est sélectionnée au hasard parmis les candidates." },
    { EXIT_COMMAND, "Quitte l'application." },
    { NULL, NULL }
};


/******************************************************************************/


int run_client(const char* connection_port) {
    client_t client;

    int res = attempt_connect_to("127.0.0.1", connection_port == NULL ? SERVER_LISTEN_PORT : connection_port,
                                 &(client.server_socket), MAX_SERVER_UP_CHECK,
                                 SERVER_UP_CHECK_TIMEOUT);
    if (res == -1) {
        applog(LOG_LEVEL_ERROR, "[User] Serveur innaccessible. Extinction.\n");
        return EXIT_FAILURE;
    }

    res = handshake(&client);
    if (res == -1) {
        return -1;
    }

    applog(LOG_LEVEL_INFO, "[User] Serveur OK (Handshake).\n");
    client.machines_by_files = list_create(NULL, create_lookup);
    loop(&client);

    clear_client(&client);

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


int loop(client_t* client) {
    int continue_loop = 1;
    display_help();
    while (continue_loop == 1) {
        printf("@gnutella$ ");
        char* temp = read_command();
        char* command = malloc(strlen(temp));
        strncpy(command, temp, strlen(temp) - 1);
        command[strlen(temp) - 1] = '\0';
        if (strcmp(command, "") == 0) {
            applog(LOG_LEVEL_WARNING, "Commande vide\n");
            continue;
        } else if (strcmp(command, EXIT_COMMAND) == 0) {
            continue_loop = 0;
        }

        handle_command(client, command);
        handle_servent(client);
        free(temp);
        free(command);
    }
    return 0;
}


void clear_client(client_t* client) {
    write_exit_packet(client);

    close(client->server_socket);
    client->server_socket = -1;


}


void write_exit_packet(client_t* client) {
    opcode_t opcode = CMSG_INT_EXIT;
    write_to_fd(client->server_socket, &opcode, PKT_ID_SIZE);

    applog(LOG_LEVEL_INFO, "[User] Sent CMSG_INT_EXIT\n");
}


void handle_servent(client_t* client) {
    struct pollfd poller;
    int res = poll_fd(&poller, client->server_socket, POLLIN, SERVENT_AWAIT_TIMEOUT);

    if (res == 1) {
        opcode_t opcode;
        read_from_fd(client->server_socket, &opcode, PKT_ID_SIZE);

        switch (opcode) {
        case SMSG_INT_SEARCH:
            handle_search_answer(client);
            break;

        default:
            break;
        }
    }
}


/******************************************************************************/


void display_help() {
    int i = 0;
    while (commands[i][0] != NULL) {
        printf("%s\n\t%s\n\n", commands[i][0], commands[i][1]);
        i++;
    }
}


char* read_command() {
    char* command = NULL;
    size_t length = 0;
    getline(&command, &length, stdin);
    return command;
}


void handle_command(client_t* client, char* command) {
    if (strcmp(command, HELP_COMMAND) == 0) {
        display_help();
        return;
    }

    char* command_name = strtok(command, " ");
    if (strcmp(command_name, DOWNLOAD_COMMAND) == 0) {
        handle_download(client);
    } else if (strcmp(command_name, SEARCH_COMMAND) == 0) {
        handle_search(client);
    } else if (strcmp(command_name, LOOKUP_COMMAND) == 0) {
        handle_lookup(client);
    }
}


static void handle_download(client_t* client) {

}
