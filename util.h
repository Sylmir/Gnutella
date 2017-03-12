#ifndef UTIL_H
#define UTIL_H

#include <stdlib.h>

#include "common.h"
#include "list.h"


struct pollfd;
struct timespec;

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


void free_reset(void* ptr);


void const_free_reset(const void* ptr);


/*
 * Extract the port the socket is bound to and store it as a string inside
 * target. If target is not long enough, the behaviour is undefined. All content
 * in target is overwritten.
 *
 * The function will also add a trailing '\0'. Ideally, the length of target
 * should be at least 6 (5 for the port, 1 for the '\0').
 */
void extract_port_from_socket(int socket, char* target, int remote);


/*
 * Extract the port the socket is bound to and return it as a mallocated string.
 * This function guarantees that the returned buffer will be long enough to
 * hold any port.
 *
 * It also adds the trailing '\0'. How convenient.
 */
char* extract_port_from_socket_s(int socket, int remote);


/*
 * Extract the IP from a socket and store it as a string inside target. Is target
 * it not long enough, the behaviour is undefined. All content in target is
 * overwritten.
 *
 * The function will also add a trailing '\0'. Ideally, the length of target
 * should be at least INET6_ADDRSTRLEN to handle both IPv4 and IPv6 (and to
 * put the '\0').
 */
void extract_ip_from_socket(int socket, char* target, int remote);


/*
 * Extract the IP from a socket and return it as a mallocated string. The function
 * guarantees that the returned buffer will be long enough to hold both IPv4 and
 * IPv6 adress.
 *
 * It also adds the trailing '\0'. Man this is useful. Too bad we don't have C++11
 * std::unique_ptr to remove that annoying free from our minds...
 */
char* extract_ip_from_socket_s(int socket, int remote);


/*
 * Extract both IP and port from a socket and store them as strings inside
 * (respectively) ip and port. These buffer must satisfy the criteria from both
 * extract_ip_from_socket and extract_port_from_socket to ensure the behaviour
 * is defined.
 *
 * All guarantees from the aforementionned functions are kept.
 */
void extract_ip_port_from_socket(int socket, char* ip, char* port, int remote);


/*
 * Extract both IP and port from a socket and store them as strings inside
 * the (in-function) mallocated buffers *ip and *port. This function has the
 * same level of guarantees and the same requirements as the extract_*_s
 * functions above.
 */
void extract_ip_port_from_socket_s(int socket, char** ip, char** port, int remote);


/*
 * Returned the elapsed time in milliseconds between now and begin.
 */
int elapsed_time_since(const struct timespec* begin);


/*
 * Sleep in milliseconds.
 */
void millisleep(int milliseconds);


#endif /* UTIL_H */
