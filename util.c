#define _GNU_SOURCE

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include <arpa/inet.h>
#include <netdb.h>
#include <poll.h>
#include <sys/socket.h>
#include <sys/resource.h>
#include <sys/time.h>
#include <sys/types.h>
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
    *target = malloc(strlen(content) + 1);
    strcpy(*target, content);
}


void free_reset(void* ptr) {
    void** _ptr = (void**)ptr;
    if (*_ptr != NULL) {
        free(*_ptr);
        *_ptr = NULL;
    }
}


void const_free_reset(const void* ptr) {
    void** _ptr = (void**)ptr;
    if (*_ptr != NULL) {
        const_free(*_ptr);
        *_ptr = NULL;
    }
}


void extract_port_from_socket(int socket, char* target, int remote) {
    struct sockaddr_storage addr;
    socklen_t addr_len = sizeof(addr);

    if (remote == 1) {
        getpeername(socket, (struct sockaddr*)&addr, &addr_len);
    } else {
        getsockname(socket, (struct sockaddr*)&addr, &addr_len);
    }

    if (addr.ss_family == AF_INET) {
        struct sockaddr_in* v4 = (struct sockaddr_in*)&addr;
        int_to_string(ntohs(v4->sin_port), target);
    } else if (addr.ss_family == AF_INET6) {
        struct sockaddr_in6* v6 = (struct sockaddr_in6*)&addr;
        int_to_string(ntohs(v6->sin6_port), target);
    }
}


char* extract_port_from_socket_s(int socket, int remote) {
    char* target = malloc(6);
    extract_port_from_socket(socket, target, remote);
    return target;
}


void extract_ip_from_socket(int socket, char* target, int remote) {
    struct sockaddr_storage addr;
    socklen_t addr_len = sizeof(addr);

    if (remote == 1) {
        getpeername(socket, (struct sockaddr*)&addr, &addr_len);
    } else {
        getsockname(socket, (struct sockaddr*)&addr, &addr_len);
    }

    if (addr.ss_family == AF_INET) {
        struct sockaddr_in* v4 = (struct sockaddr_in*)&addr;
        inet_ntop(addr.ss_family, &(v4->sin_addr), target, INET_ADDRSTRLEN);
    } else if (addr.ss_family == AF_INET6) {
        struct sockaddr_in6* v6 = (struct sockaddr_in6*)&addr;
        inet_ntop(addr.ss_family, &(v6->sin6_addr), target, INET6_ADDRSTRLEN);
    }
}


char* extract_ip_from_socket_s(int socket, int remote) {
    char* target = malloc(INET6_ADDRSTRLEN);
    extract_ip_from_socket(socket, target, remote);
    return target;
}


void extract_ip_port_from_socket(int socket, char* ip, char* port, int remote) {
    struct sockaddr_storage addr;
    socklen_t addr_len = sizeof(addr);

    if (remote == 1) {
        getpeername(socket, (struct sockaddr*)&addr, &addr_len);
    } else {
        getsockname(socket, (struct sockaddr*)&addr, &addr_len);
    }

    if (addr.ss_family == AF_INET) {
        struct sockaddr_in* v4 = (struct sockaddr_in*)&addr;
        int_to_string(ntohs(v4->sin_port), port);
        inet_ntop(addr.ss_family, &(v4->sin_addr), ip, INET_ADDRSTRLEN);
    } else if (addr.ss_family == AF_INET6) {
        struct sockaddr_in6* v6 = (struct sockaddr_in6*)&addr;
        int_to_string(ntohs(v6->sin6_port), port);
        inet_ntop(addr.ss_family, &(v6->sin6_addr), ip, INET6_ADDRSTRLEN);
    }
}



void extract_ip_port_from_socket_s(int socket, char** ip, char** port, int remote) {
    *ip = malloc(INET6_ADDRSTRLEN);
    *port = malloc(6);
    extract_ip_port_from_socket(socket, *ip, *port, remote);
}


int elapsed_time_since(const struct timespec* begin) {
    struct timespec now;
    clock_gettime(CLOCK_REALTIME, &now);

    long int sec_diff = now.tv_sec - begin->tv_sec;
    long int nano_diff = now.tv_nsec - begin->tv_nsec;

    /* If less than one second was elapsed. */
    if (nano_diff < 0) {
        --sec_diff;
        nano_diff += 1000000000;
    }

    return sec_diff * 1000 + (nano_diff / 1000 / 1000);
}


void millisleep(int milliseconds) {
    struct timespec timer;
    div_t res = div(milliseconds, 1000);
    timer.tv_sec = res.quot;
    timer.tv_nsec = res.rem * 1000 * 1000;

    nanosleep(&timer, NULL);
}
