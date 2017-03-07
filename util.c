#define _GNU_SOURCE

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <arpa/inet.h>
#include <poll.h>
#include <sys/socket.h>
#include <unistd.h>

#include "util.h"


int write_to_fd(int fd, void* buffer, size_t size) {
    size_t written = 0;
    while (written < size) {
        ssize_t res = write(fd, buffer + written, size - written);
        if (res == -1) {
            return -1;
        }
        written += res;
    }

    return 0;
}


int read_from_fd(int fd, void* buffer, size_t size) {
    size_t _read = 0;
    while (_read < size) {
        ssize_t res = read(fd, buffer + _read, size - _read);
        if (res == -1) {
            return -1;
        } else if (res == 0) {
            return 0;
        }
        _read += res;
    }

    return 0;
}


int compare_ints(void* lhs, void* rhs) {
    int a = *(int*)lhs;
    int b = *(int*)rhs;

    if (a == b) {
        return 0;
    } else if (a < b) {
        return -1;
    } else {
        return 1;
    }
}


void int_to_string(int src, char* dst) {
    sprintf(dst, "%u", src);
}


const char* int_to_cstring(int src) {
    int length  = get_number_of_digits_in(src);
    if (src < 0) {
        ++length;
    }

    char* result = malloc(length);
    int_to_string(src, result);
    return result;
}


int get_number_of_digits_in(int src) {
    int nb_digits = 1; /* Every single number is at least one digit long. */
    while (abs(src) >= 10) {
        src /= 10;
        nb_digits++;
    }

    return nb_digits;
}


void const_free(const void* ptr) {
    free((void*)ptr);
}


int poll_fd(struct pollfd* poller, int fd, int events, int timeout) {
    poller->fd = fd;
    poller->events = events;
    poller->revents = 0;

    return poll(poller, 1, timeout);
}


void free_not_null(const void* ptr) {
    if (ptr != NULL) {
        const_free(ptr);
    }
}


int check_port(const char* port) {
    char* end_ptr;
    long long_port = strtol(port, &end_ptr, 10);

    if (*end_ptr != '\0') {
        return 0;
    }

    return long_port > 1024 && long_port < 65536;
}


int check_ip(const char* ip) {
    struct sockaddr_in v4;
    int res = inet_pton(AF_INET, ip, &v4);
    if (res == 1) {
        return 1;
    }

    struct sockaddr_in6 v6;
    res = inet_pton(AF_INET6, ip, &v6);
    if (res == 1) {
        return 1;
    }

    return 0;
}


void set_string(char** target, const char* content) {
    *target = malloc(strlen(content));
    strcpy(*target, content);
}


void free_reset(void** ptr) {
    if (*ptr != NULL) {
        free(*ptr);
        *ptr = NULL;
    }
}


void const_free_reset(const void **ptr) {
    if (*ptr != NULL) {
        const_free(*ptr);
        *ptr = NULL;
    }
}


void extract_port_from_socket(int socket, char* target) {
    struct sockaddr addr;
    socklen_t addr_len = sizeof(addr);

    getpeername(socket, &addr, &addr_len);

    if (addr.sa_family == AF_INET) {
        struct sockaddr_in* v4 = (struct sockaddr_in*)&addr;
        int_to_string(v4->sin_port, target);
    } else if (addr.sa_family == AF_INET6) {
        struct sockaddr_in6* v6 = (struct sockaddr_in6*)&addr;
        int_to_string(v6->sin6_port, target);
    }
}


char* extract_port_from_socket_s(int socket) {
    char* target = malloc(6);
    extract_port_from_socket(socket, target);
    return target;
}
