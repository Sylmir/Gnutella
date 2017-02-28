#ifndef SERVER_H
#define SERVER_H

/* Port the server will listen to get requests. */
#define SERVER_LISTEN_PORT "10001"


int run_server(int first_machine, const char* ip, const char* port);

#endif /* SERVER_H */
