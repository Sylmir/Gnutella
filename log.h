#ifndef LOG_H
#define LOG_H

#include <stdbool.h>
#include <stdio.h>

#include "common.h"


/* Log system. */


/*
 * Performs an assertion on condition and displays the message format, formatted
 * like printf, on the standard output, if the assertion fails.
 *
 * Please note that calling this function will override the usual __FILE__ and
 * __LINE__ used by assert.
 *
 * Please also note that this function won't work if the DEBUG flag is not set
 * when compiling the application.
 */
void debug_assert(bool condition, const char* format, ...);


/*
 * Performs an assertion on condition and displays the message format, formatted
 * like printf, in the file "file", if the assertion fails.
 *
 * Please note that calling this function will override the usual __FILE__ and
 * __LINE__ used by assert.
 *
 * Please also note that this function won't work if the DEBUG flag is not set
 * when compiling the application.
 */
void debug_file_assert(bool condition, FILE* file, const char* format, ...);

/* Debug informations: variable content, trace of functions etc... */
#define LOG_LEVEL_DEBUG     0
/* Information: received message, sent message etc... */
#define LOG_LEVEL_INFO      1
/* Warning: minor errors. */
#define LOG_LEVEL_WARNING   2
/* Critical: application shouldn't shut down, but may be in an unstable state. */
#define LOG_LEVEL_ERROR     3
/* Fatal: application must shut down. */
#define LOG_LEVEL_FATAL     4
/* Maximum log level. */
#define LOG_LEVEL_MAX       5


/*
 * Display the message format, formatted like printf, in the most appropriate
 * standard flux, according to log_level.
 */
ERROR_CODES_USUAL int applog(int log_level, const char* format, ...);


/*
 * Display the message format, formatted like printf, in the file "file",
 * overriding usual considerations.
 */
ERROR_CODES_USUAL int log_to_file(int log_level, FILE* file,
                                  const char* format, ...);

#endif /* LOG_H */
