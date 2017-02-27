#define _GNU_SOURCE

#include <errno.h>
#include <string.h>

#include <netdb.h>
#include <poll.h>
#include <sys/socket.h>
#include <unistd.h>

#include "log.h"
#include "networking.h"


int connect_to(const char* ip, const char* port, int* sock) {
    struct addrinfo hints;
    struct addrinfo *result;
    char host_name[NI_MAXHOST], numeric_port[NI_MAXSERV];

    memset(&hints, 0, sizeof(struct addrinfo));
    hints.ai_family     = AF_UNSPEC;
    hints.ai_socktype   = SOCK_STREAM;
    hints.ai_addr       = NULL;
    hints.ai_addrlen    = 0;
    hints.ai_canonname  = NULL;
    hints.ai_flags      = 0;
    hints.ai_next       = NULL;
    hints.ai_protocol   = 0;

    int res = getaddrinfo(ip, port, &hints, &result);
    if (res != 0) {
        applog(LOG_LEVEL_ERROR, "[Client] Erreur lors de la recherche sur %s:%s."
                                " Erreur : %s.\n", gai_strerror(res));
        return CONNECT_ERROR_NO_ADDRINFO;
    }

    int has_valid_socket = 0;
    struct addrinfo* addr_info = result;
    while (addr_info != NULL && has_valid_socket != 1) {
        res = getnameinfo(addr_info->ai_addr, addr_info->ai_addrlen,
                          host_name, NI_MAXHOST, numeric_port, NI_MAXSERV,
                          NI_NUMERICHOST | NI_NUMERICSERV);

        if (res != 0) {
            applog(LOG_LEVEL_ERROR, "[Client] Erreur lors de la récupération "
                                    "des informations. Erreur : %s.\n",
                                    gai_strerror(res));
            return CONNECT_ERROR_NO_NAMEINFO;
        }

        int attempted_socket = socket(addr_info->ai_family, addr_info->ai_socktype,
                                      addr_info->ai_protocol);
        if (attempted_socket == -1) {
            applog(LOG_LEVEL_ERROR, "[Client] Erreur lors de la création de la "
                                    "socket. Erreur : %s.\n", strerror(errno));
            continue;
        }

        res = connect(attempted_socket, addr_info->ai_addr,
                      addr_info->ai_addrlen);
        if (res == 0) {
            has_valid_socket = 1;
            *sock = attempted_socket;
            applog(LOG_LEVEL_INFO, "[Client] Connexion en attente sur le serveur "
                                   "%s:%s.\n", host_name, numeric_port);
        } else {
            applog(LOG_LEVEL_ERROR, "[Client] Echec de la connexion. Erreur: %s."
                                    "\n", strerror(errno));
            close(attempted_socket);
            addr_info = addr_info->ai_next;
        }
    }

    freeaddrinfo(result);

    if (has_valid_socket == 0) {
        applog(LOG_LEVEL_ERROR, "[Client] Impossible de se connecter au serveur "
                                "%s:%s.\n", ip, port);
        return CONNECT_ERROR_NO_SOCKET;
    }

    return CONNECT_OK;
}


int attempt_connect_to(const char* ip, const char* port,
                       int* sock, int nb_attempt, int sleep_time) {
    for (int i = 0; i < nb_attempt; i++) {
        applog(LOG_LEVEL_INFO, "[Client] Tentative de connexion...\n");
        int res = connect_to(ip, port, sock);
        if (res != CONNECT_OK) {
            sleep(sleep_time);
            applog(LOG_LEVEL_ERROR, "[Client] Échec de la connexion.\n");
        } else {
            return 0;
        }
    }

    return -1;
}


int create_listening_socket(const char* port, int max_requests,
                            char* host_name, char* port_number) {
    struct addrinfo hints;
    struct addrinfo *result;

    memset(&hints, 0, sizeof(struct addrinfo));
    hints.ai_family     = AF_UNSPEC;
    hints.ai_socktype   = SOCK_STREAM;
    hints.ai_flags      = AI_PASSIVE;
    hints.ai_protocol   = 0;

    int res = getaddrinfo(NULL, port, &hints, &result);
    if (res != 0) {
        return CL_ERROR_NO_ADDRINFO;
    }

    struct addrinfo *addr_info = result;
    int has_valid_socket = 0;
    int selected_socket = 0;
    while (addr_info != NULL && has_valid_socket != 1) {
        /* Try a new address. */
        selected_socket = socket(addr_info->ai_family,
                                 addr_info->ai_socktype,
                                 addr_info->ai_protocol);

        if (selected_socket == -1) {
            continue;
        }

        /* Set some options. */
        int yes = 1;
        res = setsockopt(selected_socket, SOL_SOCKET, SO_REUSEADDR,
                         &yes, sizeof(int));

        if (res == -1) {
            continue;
        }

        /* Try to bind the socket. */
        res = bind(selected_socket, addr_info->ai_addr, addr_info->ai_addrlen);
        if (res == 0) {
            has_valid_socket = 1;

            res = getnameinfo(addr_info->ai_addr, addr_info->ai_addrlen,
                              host_name, NI_MAXHOST, port_number, NI_MAXSERV,
                              NI_NUMERICHOST | NI_NUMERICSERV);

            if (res != 0) {
                return CL_ERROR_NO_NAMEINFO;
            }
        } else {
            close(selected_socket);
            addr_info = addr_info->ai_next;
        }
    }

    freeaddrinfo(result);

    if (has_valid_socket == 0) {
        return CL_ERROR_NO_SOCKET;
    }

    res = listen(selected_socket, max_requests);
    if (res < 0) {
        return CL_ERROR_NO_LISTEN;
    }

    return selected_socket;
}

int attempt_accept(int listening_socket, int timeout,
                   struct sockaddr* addr, socklen_t* addr_len) {
    struct pollfd poller;
    poller.fd = listening_socket;
    poller.events = POLLIN;
    poller.revents = 0;

    int res = poll(&poller, 1, timeout);
    if (res == 1 && (poller.revents & POLLIN) != 0) {
        int accept_res = accept(listening_socket, addr, addr_len);
        if (accept_res == -1 && errno == EINVAL) {
            printf("addr_len = %d\n", *addr_len);
        }
        return accept_res;
    }

    return ACCEPT_ERR_TIMEOUT;
}
