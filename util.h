#ifndef UTIL_H
#define UTIL_H

#include <stdlib.h>

#include "common.h"
#include "list.h"


struct pollfd;

/*
 * Write the content of buffer up to size bytes in the file designed by fd.
 * If an error occurs during one write, the function returns -1, otherwise
 * it returns 0.
 *
 * Note: if buffer isn't at least size bytes large, the behaviour is undefined.
 *
 * See also: write(2).
 */
ERROR_CODES_USUAL int write_to_fd(int fd, void* buffer, size_t size);


/*
 * Read up to size bytes from the file designed by fd and stores it in buffer.
 * If an error occurs during one read, the function returns -1, otherwise it
 * returns 0.
 *
 * Note: if buffer isn't at least size bytes large, the behaviour is undefined.
 *
 * See also: read(2).
 */
ERROR_CODES_USUAL int read_from_fd(int fd, void* buffer, size_t size);


/*
 * Compare two ints.
 */
LIST_COMPARE_FN int compare_ints(void* lhs, void* rhs);


/*
 * Convert an int into a string. For this operation to work, dst must be a buffer
 * of at least number_of_digits_in(src) + 1 long.
 */
void int_to_string(int src, char* dst);


/*
 * Convert an int into a string and return the resulting conversion. The returned
 * buffer is malloced and must subsequently be freed. This function guarantees
 * that the buffer will be long enough to hold any integer.
 */
const char* int_to_cstring(int src);


/*
 * Return the number of digits in src. Note that if src is a strict negative
 * integer, the function won't add one to take the '-' into account.
 */
int get_number_of_digits_in(int src);


/*
 * Version of free that takes a const pointer instead of a non-const pointer.
 * Because I'm tired of casting my strings from const char* to char*.
 * Stupid GCC.
 */
void const_free(const void* ptr);


/*
 * Perform a poll by filling poller with fd and events. The function subsequently
 * calls poll() on poller, using timeout as it's timeout parameter. The function
 * then returns the same as poll(poller, 1, timeout).
 */
int poll_fd(struct pollfd* poller, int fd, int events, int timeout);


/*
 * Free a pointer if it is not null.
 */
void free_not_null(const void* ptr);


/*
 * Check if a port is valid.
 */
int check_port(const char* port);


/*
 * Check if an IP is valid.
 */
int check_ip(const char* ip);


/*
 * Allocate a block of at least strlen(content) bytes of memory and set *dest
 * to point to the first byte of that block. Then, fills this block with content.
 *
 * Why isn't such a thing inside the C library ?
 */
void set_string(char** dest, const char* content);


void free_reset(void** ptr);


void const_free_reset(const void** ptr);


/*
 * Extract the port the socket is bound to and store it as a string inside
 * target. If target is not long enough, the behaviour is undefined. All content
 * in target is overwritten.
 *
 * The function will also add a trailing '\0'. Ideally, the length of target
 * should be at least 6 (5 for the port, 1 for the '\0').
 */
void extract_port_from_socket(int socket, char* target);


/*
 * Extract the port the socket is bound to and return it as a mallocated string.
 * This function guarantees that the returned buffer will be long enough to
 * hold any port.
 *
 * It also adds the trailing '\0'. How convenient.
 */
char* extract_port_from_socket_s(int socket);

#endif /* UTIL_H */
