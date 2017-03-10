#define _GNU_SOURCE

#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <arpa/inet.h>
#include <netdb.h>
#include <netinet/in.h>
#include <features.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#include "log.h"
#include "util.h"

int main() {
    char* test = malloc(INET6_ADDRSTRLEN);
    gethostname(test, INET6_ADDRSTRLEN);

    struct addrinfo filter;
    struct addrinfo* results;
    memset(&filter, 0, sizeof(struct addrinfo));
    getaddrinfo(test, NULL, &filter, &results);

    if (results->ai_family == AF_INET) {
        char* IP = malloc(INET_ADDRSTRLEN);
        struct sockaddr_in* v4 = (struct sockaddr_in*)results->ai_addr;
        inet_ntop(AF_INET, &v4->sin_addr, IP, INET_ADDRSTRLEN);
        printf("Toto\n");
        printf("IP = %s\n", IP);
        free(IP);
    } else {
        printf("Niah\n");
    }


    return EXIT_SUCCESS;
}
