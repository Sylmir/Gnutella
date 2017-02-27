#define _GNU_SOURCE

#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <arpa/inet.h>
#include <netdb.h>
#include <features.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#include "log.h"
#include "util.h"

int main() {
    const char* ip = "92.94.169.89";
    const char* port = "10000";

    struct addrinfo hints;
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE;
    hints.ai_protocol = 0;

    struct addrinfo* result;
    int res = getaddrinfo(NULL, port, &hints, &result);
    assert(res == 0);

    struct addrinfo* rp = result;
    int s = socket(rp->ai_family, rp->ai_socktype, rp->ai_protocol);
    assert(s != -1);
    int yes = 1;
    setsockopt(s, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes));
    res = bind(s, rp->ai_addr, rp->ai_addrlen);
    assert(res == 0);
    listen(s, 10);
    printf("listen\n");
    char localhost[100];
    gethostname(localhost, 100);
    printf("je suis %s\n", localhost);
    freeaddrinfo(result);

    struct addrinfo* res_local;
    res = getaddrinfo(ip, port, &hints, &res_local);
    assert(res == 0);

    rp = res_local;
    char name[100];
    const char* test = inet_ntop(rp->ai_family, &((struct sockaddr_in*)rp->ai_addr)->sin_addr, name, 100);
    printf("%s\n", test);
    int sl = socket(rp->ai_family, rp->ai_socktype, rp->ai_protocol);
    assert(sl != -1);
    printf("Connexion.\n");
    res = connect(sl, rp->ai_addr, rp->ai_addrlen);
    printf("res = %d, errno = %s\n", res, strerror(errno));
    sleep(10);

    return EXIT_SUCCESS;
}
