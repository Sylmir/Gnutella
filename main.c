#define _GNU_SOURCE

#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#include "client.h"
#include "common.h"
#include "log.h"
#include "server.h"


/* The minimum number of values we must find in argv. */
#define ARGV_MINIMUM_LENGTH 1


/* Number of arguments when we want to be super peer. */
#define ARGV_SU_LENGTH 3
/* Index of the expected "-s" argument of main. */
#define ARGV_SU_INDEX 1
/* Index of the expected port number. */
#define ARGV_SU_PORT_INDEX 2
/* Command to run as first machine. */
#define SU_VALUE "-s"


/* Number of argments when we want to contact someone specific. */
#define ARGV_CONTACT_POINT_LENGTH 4
/* Index of the expected "-c" argument of main. */
#define ARGV_CONTACT_INDEX 1
/* Index of the expected IP argument of main. */
#define ARGV_IP_INDEX 2
/* Index of the expected port argument of main. */
#define ARGV_PORT_INDEX 3
/* Command to send an IP:port to get the first neighbours. */
#define CONTACT_VALUE "-c"

/* Print the correct way to call the application on the standard output. */
static void usage();


/* Ensure IP and port are correct. */
static int ensure_ip_port(const char* ip, const char* port);


/* Ensure port is correct. */
static int ensure_port(const char* port);


/* Display msg on the error output, kill the client and exit. */
static void server_error(const char* msg);


static const char* extract_listening_port(int argc, const char** argv);

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
        int first_node = 0;
        const char* ip = NULL;
        const char* port = NULL;
        const char* listening_port = NULL;

        if (argc > 1) {
            if (strcmp(argv[ARGV_SU_INDEX], SU_VALUE) == 0) {
                first_node = 1;
                listening_port = extract_listening_port(argc, (const char**)argv);
                if (listening_port == NULL) {
                    printf("Port invalide, default utilisé.\n");
                }
            } else if (strcmp(argv[ARGV_CONTACT_INDEX], CONTACT_VALUE) == 0) {
                if (argc == ARGV_CONTACT_POINT_LENGTH) {
                    if (ensure_ip_port(argv[ARGV_IP_INDEX], argv[ARGV_PORT_INDEX]) == 1) {
                        ip = argv[ARGV_IP_INDEX];
                        port = argv[ARGV_PORT_INDEX];
                        applog(LOG_LEVEL_INFO, "[Server] Contact : %s:%s.\n",
                                               ip, port);
                    } else {
                        server_error("IP ou port invalide.\n");
                    }
                }
            }
        }

        return run_server(first_node, listening_port, ip, port);
    } else {
        int res = run_client(extract_listening_port(argc, (const char**)argv));
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
           "\t./%s [%s port | ip port]\n", EXEC_NAME, SU_VALUE);
    printf("\tIf %s is specified, the forwarding agument represent the port on "
           "which the server will listen. it then identifies itself as the first "
           "machine in the network (and won't seach for neighbours) and will be "
           "used as contact point.\n", SU_VALUE);
    printf("\tIf ip and port are specified (both arguments must be) the server "
           "will contact ip:port to get it's first neighbours.\n");
    printf("If no arguments are specified, the server will look in the hosts "
           "files provided with the applicaion to try to find contact points.\n");
}


int ensure_ip_port(const char* ip, const char* port) {
    if (ensure_port(port) == 0) {
        return 0;
    }

    struct sockaddr_in v4;
    struct sockaddr_in6 v6;

    int res = inet_pton(AF_INET, ip, &v4);
    if (res == 1) {
        return 1;
    }

    res = inet_pton(AF_INET6, ip, &v6);
    if (res == 1) {
        return 1;
    }

    return 0;
}


int ensure_port(const char* port) {
    char* tmp;
    long l_port = strtol(port, &tmp, 10);
    if (*tmp != '\0') {
        return 0;
    }

    if (l_port < 1025 || l_port > 65535) {
        return 0;
    }

    return 1;
}


void server_error(const char* msg) {
    if (msg != NULL) {
        fprintf(stderr, msg);
    }

    usage();
    kill(getppid(), SIGINT);
    exit(EXIT_FAILURE);
}


const char* extract_listening_port(int argc, const char** argv) {
    if (argc != ARGV_SU_LENGTH) {
        return NULL;
    }

    if (ensure_port(argv[ARGV_SU_PORT_INDEX]) == 0) {
        return NULL;
    }

    return argv[ARGV_SU_PORT_INDEX];
}
