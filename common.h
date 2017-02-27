#ifndef COMMON_H
#define COMMON_H

/* Common constants, defines and functions to use accross the whole project. */


/* Port on which the server will listen and to which clients will talk. */
/** @todo Move to a .ini file we can parse. */
#define CONTACT_PORT "10000"

/* IP which will be used as the contact point. */
/** @todo Move to a .ini file we can parse. */
/** @todo Add support for IPv4 / IPv6 ? */
#define CONTACT_POINT "92.94.169.89"


/* The name of the executable. */
#define EXEC_NAME "network"


/* Mark argument as unused. */
#define UNUSED(arg) (void)arg

/*
 * Dummy macro to indicate that the function will return 0 on success, -1 on
 * failure.
 */
#define ERROR_CODES_USUAL

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
