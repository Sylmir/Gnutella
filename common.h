#ifndef COMMON_H
#define COMMON_H

/* Common constants, defines and functions to use accross the whole project. */


/* The name of the executable. */
#define EXEC_NAME "network"


/* Mark argument as unused. */
#define UNUSED(arg) (void)arg

/*
 * Dummy macro to indicate that the function will return 0 on success, -1 on
 * failure.
 */
#define ERROR_CODES_USUAL


/*
 * Dummy macro to indicate that the function will return 0 on failure, 1 on
 * success.
 */
#define ERROR_CODES_BOOL


/* Any error linked to I/O or libc. */
#define EXIT_LIBC_ERROR                 1
/* Not enough arguments supplied to main. */
#define EXIT_NOT_ENOUGH_ARGUMENTS       2
/* Client is unable to fork into the server. */
#define EXIT_CLIENT_NO_FORK             3


/* Maximum number of neighbours. */
#define MAX_NEIGHBOURS 5


/* Convert seconds in milliseconds (thanks Trinity). */
#define IN_MILLISECONDS 1000

#endif /* COMMON_H */
