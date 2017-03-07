#define _GNU_SOURCE

#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#include "client.h"
#include "common.h"
#include "log.h"
#include "server.h"
#include "util.h"


/* The minimum number of values we must find in argv. */
#define ARGV_MINIMUM_LENGTH 1


/*******************************************************************************
 * Boot argv.
 */


/* Parameter to show help. */
#define HELP_SHORT "-h"
#define HELP_LONG "--help"


/*******************************************************************************
 * Client argv.
 */


typedef struct client_argv_s {
    char* contact_port;

    char* error;
} client_argv_t;

/*
 * See LISTEN_SHORT and LISTEN_LONG, as they also define the port we contact to
 * dialogue with the servent.
 */


/*******************************************************************************
 * Server argv.
 */


typedef struct server_argv_s {
    int first_machine;
    char* contact_ip;
    char* contact_port;
    char* listen_port;

    char* error;
} server_argv_t;

/* Parameter to run as first machine. */
#define FIRST_MACHINE_SHORT "-f"
#define FIRST_MACHINE_LONG "--first"


/* Command to set an IP:port to get the first neighbours. */
#define CONTACT_POINT_SHORT "-c"
#define CONTACT_POINT_LONG "--contact"


/* Command to set our listening port. */
#define LISTEN_SHORT "-l"
#define LISTEN_LONG "--listen"


/* Print the correct way to call the application on the standard output. */
static void usage();


/*
 * Handle argv according to phase (boot, client, server). context will store the
 * informations retrieved.
 */
static void handle_argv(int argc, char** argv, int phase, void* context);

#define PHASE_BOOT 0
#define PHASE_CLIENT 1
#define PHASE_SERVER 2


static void handle_argv_boot(int argc, char** argv, void* context);
static void handle_argv_client(int argc, char** argv, void* context);
static void handle_argv_server(int argc, char** argv, void* context);


static void clear_client_argv(client_argv_t* argv);
static void clear_server_argv(server_argv_t* argv);


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

    srand((unsigned int)time(NULL));

    handle_argv(argc, argv, PHASE_BOOT, NULL);

    pid_t server_pid = fork();
    if (server_pid < 0) {
        applog(LOG_LEVEL_FATAL, "Erreur lors du fork pour créer le serveur. "
                                "Extinction.\n");
        return EXIT_CLIENT_NO_FORK;
    } else if (server_pid == 0) {
        server_argv_t infos;
        infos.first_machine = 0;
        infos.contact_ip = infos.contact_port = infos.listen_port = infos.error = NULL;

        handle_argv(argc, argv, PHASE_SERVER, &infos);

        if (infos.error != NULL) {
            fprintf(stderr, infos.error);
            usage();
            kill(getppid(), SIGINT); /* Kill client. */
            exit(EXIT_FAILURE);
        }

        int res = run_server(infos.first_machine, infos.listen_port,
                             infos.contact_ip, infos.contact_port);

        clear_server_argv(&infos);

        return res;
    } else {
        client_argv_t infos;
        infos.contact_port = infos.error = NULL;

        handle_argv(argc, argv, PHASE_CLIENT, &infos);

        if (infos.error != NULL) {
            fprintf(stderr, infos.error);
            usage();
            kill(server_pid, SIGINT); /* Kill server. */
            exit(EXIT_FAILURE);
        }

        printf("[Client] Port d'écoute : %s\n", infos.contact_port);
        int res = run_client(infos.contact_port);
        if (res == -1) {
            kill(server_pid, SIGINT);
            return EXIT_FAILURE;
        }

        clear_client_argv(&infos);
        int status;
        waitpid(server_pid, &status, 0);
        return res;
    }
}


void usage() {
    printf("Usage:\n");
    printf("./%s [%s | %s] [%s || %s] [%s port || %s port] [%s ip port || %s ip port]\n",
           EXEC_NAME, HELP_SHORT, HELP_LONG, FIRST_MACHINE_SHORT, FIRST_MACHINE_LONG,
           LISTEN_SHORT, LISTEN_LONG, CONTACT_POINT_SHORT, CONTACT_POINT_LONG);
    printf("\t%s / %s Display the present help and exit.\n", HELP_SHORT, HELP_LONG);
    printf("\t%s / %s Run this application as first machine. It means the servent "
           "won't search for neighbours.\n", FIRST_MACHINE_SHORT, FIRST_MACHINE_LONG);
    printf("\t%s / %s port Force the servent to listen on the given port.\n",
           LISTEN_SHORT, LISTEN_LONG);
    printf("\t%s / %s ip port Force the servent to contact this given IP and port "
           "to join the network.\n", CONTACT_POINT_SHORT, CONTACT_POINT_LONG);
}


void handle_argv(int argc, char **argv, int phase, void *context) {
    switch (phase) {
    case PHASE_BOOT:
        handle_argv_boot(argc, argv, context);
        break;

    case PHASE_CLIENT:
        handle_argv_client(argc, argv, context);
        break;

    case PHASE_SERVER:
        handle_argv_server(argc, argv, context);
        break;

    default:
        break;
    }
}


void handle_argv_boot(int argc, char** argv, void* context) {
    for (int i = 0; i < argc; ) {
        int increment = 1;
        const char* value = argv[i];

        if (strcmp(value, HELP_LONG) == 0 ||
            strcmp(value, HELP_SHORT) == 0) {
            usage();
            exit(EXIT_SUCCESS);
        }

        i += increment;
    }
}


void handle_argv_client(int argc, char** argv, void* context) {
    client_argv_t* infos = (client_argv_t*)context;
    for (int i = 0; i < argc; ) {
        int increment = 1;
        const char* value = argv[i];

        if (strcmp(value, LISTEN_LONG) == 0 ||
            strcmp(value, LISTEN_SHORT) == 0) {
            if (argc < i + 1) {
                set_string(&infos->error, "Not enough parameters for internal contact port.\n");
                return;
            }

            if (check_port(argv[i + 1]) == 0) {
                set_string(&infos->error, "Invalid internal contact port.\n");
                return;
            }
            set_string(&infos->contact_port, argv[i + 1]);

            increment = 2;
        }

        i += increment;
    }
}


void handle_argv_server(int argc, char** argv, void* context) {
    server_argv_t* infos = (server_argv_t*)context;
    for (int i = 0; i < argc; ) {
        int increment = 1;
        const char* value = argv[i];

        if (strcmp(value, FIRST_MACHINE_LONG) == 0 ||
            strcmp(value, FIRST_MACHINE_SHORT) == 0) {
            infos->first_machine = 1;
        } else if (strcmp(value, CONTACT_POINT_LONG) == 0 ||
                   strcmp(value, CONTACT_POINT_SHORT) == 0) {
            if (argc < i + 2) {
                set_string(&infos->error, "Not enough parameters for contact point.\n");
                return;
            }

            if (check_ip(argv[i + 1]) == 0) {
                set_string(&infos->error, "Invalid contact IP.\n");
                return;
            }

            if (check_port(argv[i + 2]) == 0) {
                set_string(&infos->error, "Invalid contact port.\n");
                return;
            }

            set_string(&infos->contact_ip, argv[i + 1]);
            set_string(&infos->contact_port, argv[i + 2]);

            increment = 3;
        } else if (strcmp(value, LISTEN_LONG) == 0 ||
                   strcmp(value, LISTEN_SHORT) == 0) {
            if (argc < i + 1) {
                set_string(&infos->error, "Not enough parameters for listening port.\n");
                return;
            }

            if (check_port(argv[i + 1]) == 0) {
                set_string(&infos->error, "Invalid listening port.\n");
                return;
            }

            set_string(&infos->listen_port, argv[i + 1]);

            increment = 2;
        }

        i += increment;
    }
}


void clear_client_argv(client_argv_t* argv) {
    free_not_null(argv->contact_port);
    free_not_null(argv->error);
}


void clear_server_argv(server_argv_t* argv) {
    free_not_null(argv->contact_ip);
    free_not_null(argv->contact_port);
    free_not_null(argv->error);
    free_not_null(argv->listen_port);
}
