#ifndef UTIL_H
#define UTIL_H

#include <stdlib.h>

#include "common.h"
#include "list.h"


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


void const_free(const void* ptr);
#endif /* UTIL_H */
