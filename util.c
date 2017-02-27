#define _GNU_SOURCE

#include <stdlib.h>
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
