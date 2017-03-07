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
    char t = 'a';
    char toto[5] = "Hello";
    printf("%s, %d, %c\n", toto, toto[5], t);

    return EXIT_SUCCESS;
}
