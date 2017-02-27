#define _GNU_SOURCE

#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#include "client.h"
#include "common.h"
#include "log.h"
#include "server.h"


/* The minimum number of values we must find in argv. */
#define ARGV_MINIMUM_LENGTH 1

/* Print the correct way to call the application on the standard output. */
static void usage();


/******************************************************************************/


/*
 * The application works as both a client and a server so we need a way to
 * know which is which. So, the first argument of argv will either be :
 *  - "server" in which case the program will run as server
 *  - "client" in which case the program will run as client
 *
 * If the application is the client, it will first try to establish contact
 * with the contact point, so it can join the network.
 *
 * If the client is the server, it will listen on the contact port for further
 * requests.
 */
int main(int argc, char** argv) {
    if (argc < ARGV_MINIMUM_LENGTH) {
        fprintf(stderr, "Not enough arguments supplied.\n");
        usage();
        return EXIT_NOT_ENOUGH_ARGUMENTS;
    }

    pid_t server_pid = fork();
    if (server_pid < 0) {
        applog(LOG_LEVEL_FATAL, "Erreur lors du fork pour créer le serveur. "
                                "Extinction.\n");
        return EXIT_CLIENT_NO_FORK;
    } else if (server_pid == 0) {
        return run_server();
    } else {
        int res = run_client();
        if (res == -1) {
            kill(server_pid, SIGINT);
            return EXIT_FAILURE;
        }

        int status;
        waitpid(server_pid, &status, 0);
        return res;
    }
}


void usage() {
    printf("Usage: \n"
           "\t./%s", EXEC_NAME);
}
