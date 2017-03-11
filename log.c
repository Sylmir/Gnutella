#include <assert.h>
#include <stdarg.h>
#include <stdio.h>

#include "common.h"
#include "log.h"
#include "util.h"


/*
 * Prototype of a log function. file indicates where we will print the message
 * message. Internally, the functions may decorate message with additional
 * content, such as colors and the log level.
 */
typedef void(*log_function_t)(FILE* file, const char* message);


/*
 * Internal log function. Creates a string based on format and va, then call
 * log_fn passing file and the newly created string as parameters.
 */
static void log_internal(log_function_t log_fn, const char* format,
                         FILE* file, va_list va);


/* Returns the function associated with a given log_level. */
static log_function_t get_log_function_by_level(int log_level);


/* Returns the file associated with a given log_level. */
static FILE* get_log_file_by_level(int log_level);


/* I guess these functions are self explanatory... */
static void log_info(FILE* file, const char* message);
static void log_warning(FILE* file, const char* message);
static void log_critical(FILE* file, const char* message);
static void log_fatal(FILE* file, const char* message);


/* Maximum length of a message we want to log. */
#define MAX_BUFFER_LENGTH 4096
/* As string. Note that this MUST be the same as MAX_BUFFER_LENGTH. */
#define STR_MAX_BUFFER_LENGTH "4096"

#ifdef DEBUG

void debug_assert(bool condition, const char* format, ...) {
    if (!condition) {
        va_list va;
        va_start(va, format);
        vprintf(format, va);
        va_end(va);

        assert(condition);
    }
}


void debug_file_assert(bool condition, FILE* file, const char* format, ...) {
    if (!condition) {
        va_list va;
        va_start(va, format);
        vfprintf(file, format, va);
        va_end(va);

        assert(condition);
    }
}

#else

void debug_assert(bool condition, const char* format, ...) {
    UNUSED(condition);
    UNUSED(format);
}


void debug_file_assert(bool condition, FILE* file, const char* format, ...) {
    UNUSED(condition);
    UNUSED(file);
    UNUSED(format);
}

#endif /* DEBUG */

int applog(int log_level, const char* format, ...) {
    if (log_level >= LOG_LEVEL_MAX || log_level < 0 || format == NULL) {
        return -1;
    }

    va_list va;
    va_start(va, format);
    FILE* log_file = get_log_file_by_level(log_level);
    log_internal(get_log_function_by_level(log_level), format,
                 log_file, va);
    fflush(log_file);
    va_end(va);

    return 0;
}


int log_to_file(int log_level, FILE* file, const char* format, ...) {
    if (log_level >= LOG_LEVEL_MAX || log_level < 0 || format == NULL) {
        return -1;
    }

    va_list va;
    va_start(va, format);
    log_internal(get_log_function_by_level(log_level), format, file, va);
    fflush(file);
    va_end(va);

    return 0;
}


void log_internal(log_function_t log_fn, const char* format,
                  FILE* file, va_list va) {
    assert(log_fn != NULL);
    assert(file != NULL);

    char buffer[MAX_BUFFER_LENGTH];
    vsnprintf(buffer, MAX_BUFFER_LENGTH, format, va);

    log_fn(file, buffer);
}


log_function_t get_log_function_by_level(int log_level) {
    switch (log_level) {
    case LOG_LEVEL_INFO:
        return log_info;

    case LOG_LEVEL_WARNING:
        return log_warning;

    case LOG_LEVEL_ERROR:
        return log_critical;

    case LOG_LEVEL_FATAL:
        return log_fatal;

    default:
        return NULL;
    }
}


FILE* get_log_file_by_level(int log_level) {
    switch (log_level) {
    case LOG_LEVEL_INFO:
        return stdout;

    case LOG_LEVEL_WARNING:
        return stderr;

    case LOG_LEVEL_ERROR:
        return stderr;

    case LOG_LEVEL_FATAL:
        return stderr;

    default:
        return NULL;
    }
}

#ifndef DEBUG

void log_info(FILE* file, const char* message) {
    UNUSED(file);
    UNUSED(message);
}


void log_warning(FILE* file, const char* message) {
    UNUSED(file);
    UNUSED(message);
}


void log_critical(FILE* file, const char* message) {
    UNUSED(file);
    UNUSED(message);
}


void log_fatal(FILE* file, const char* message) {
    UNUSED(file);
    UNUSED(message);
}

#else

void log_info(FILE* file, const char* message) {
    const char* format = "\033[0;38;2;0;204;204m[Info]: %s";
    fprintf(file, format, message);
    fprintf(file, "\033[0;39m");
}


void log_warning(FILE* file, const char* message) {
    const char* format = "\033[0;38;2;153;51;255m[Warning]: %s";
    fprintf(file, format, message);
    fprintf(file, "\033[0;39m");
}


void log_critical(FILE* file, const char* message) {
    const char* format = "\033[0;38;2;255;0;0m[Error]: %s";
    fprintf(file, format, message);
    fprintf(file, "\033[0;39m");
}


void log_fatal(FILE* file, const char* message) {
    const char* format = "\033[0;38;2;0;0;0;48;2;255;255;255m[Fatal]: %s";
    fprintf(file, format, message);
    fprintf(file, "\033[0;39m");
    fprintf(file, "\033[0;49m");
}

#endif /* DEBUG */
