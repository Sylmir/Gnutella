#ifndef SERVER_DEFINES_H
#define SERVER_DEFINES_H


/*
 * Maximum number of neighbours.
 */
#define MAX_NEIGHBOURS 5

/*
 * Time interval to check if we have an incoming request on the listening
 * socket. This is in milliseconds.
 */
#define ACCEPT_TIMEOUT 100


/*
 * Timeout (milliseconds) when we check if there is something to read on an
 * awaiting socket.
 */
#define AWAIT_TIMEOUT 10


/*
 * Port on which the server will listen and to which clients will talk.
 */
#define CONTACT_PORT "10000"


/*
 * IP which will be used as the contact point.
 */
#define CONTACT_POINT "134.214.88.230"

#endif /* SERVER_DEFINES_H */
