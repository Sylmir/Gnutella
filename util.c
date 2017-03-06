#define _GNU_SOURCE

#include <stdio.h>
#include <stdlib.h>

#include <poll.h>
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
