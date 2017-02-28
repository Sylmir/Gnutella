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
#define ARGV_SU_LENGTH 1
/* Index of he expected "-s" argument of main. */
#define ARGV_SU_INDEX 1
/* Command to un as first machine. */
#define SU_VALUE "-s"


/* Numbe of argments when we want to contact someone specific. */
#define ARGV_CONTACT_POINT_LENGTH 3
/* Index of the expected IP argument of main. */
#define ARGV_IP_INDEX 1
/* Index of the expected port argument of main. */
#define ARGV_PORT_INDEX 2

/* Print the correct way to call the application on the standard output. */
static void usage();


/* Ensure IP and port are correct. */
static int ensure_ip_port(const char* ip, const char* port);

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
        char* ip = NULL;
        char* port = NULL;

        if (argc == ARGV_SU_LENGTH) {
            if (strcmp(argv[ARGV_SU_LENGTH], SU_VALUE) == 0) {
                applog(LOG_LEVEL_INFO, "[Server] On est la première machine.\n");
                first_node = 1;
            } else {
                usage();
                kill(getppid(), SIGINT);
                exit(EXIT_FAILURE);
            }
        } else if (argc == ARGV_CONTACT_POINT_LENGTH) {
            if (ensure_ip_port(argv[ARGV_IP_INDEX], argv[ARGV_PORT_INDEX]) == 1) {
                ip = argv[ARGV_IP_INDEX];
                port = argv[ARGV_PORT_INDEX];
                applog(LOG_LEVEL_INFO, "[Server] Contact : %s:%s.\n",
                                       ip, port);
            } else {
                usage();
                kill(getppid(), SIGINT);
                exit(EXIT_FAILURE);
            }
        }
        return run_server(first_node, ip, port);
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
           "\t./%s [%s | ip port]\n", SU_VALUE, EXEC_NAME);
    printf("\tIf %s is specified, all forwarding aguments are ignored and the "
           "server identifies itself as the first machine in the network (and "
           "wont seach for neighbours) and will be used as contact point.\n",
           SU_VALUE);
    printf("\tIf ip and port are specified (both arguments must be) the server "
           "will contact ip:port to get it's first neighbours.\n");
    printf("If no arguments are specified, the server will look in the hosts "
           "files provided with the applicaion to try to find contact points.\n");
}


int ensure_ip_port(const char* ip, const char* aport) {
    char* tmp;
    long port = strtol(aport, &tmp, 10);
    if (*tmp != '\0') {
        return 0;
    }

    if (port < 1025 || port > 65535) {
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
