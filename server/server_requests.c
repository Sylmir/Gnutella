#define _GNU_SOURCE

#include <assert.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include <arpa/inet.h>
#include <dirent.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#include "log.h"
#include "packets_defines.h"
#include "server_internal.h"
#include "util.h"


/*
 * Fork the process into another process that will search the file filename
 * inside the directory containing the downloadable files, using dirent. If
 * a match is found, the function return 1, otherwise it returns 0. (Also returns
 * 0 if there was an error searching file).
 */
static int search_file(const char* filename);


/*
 * This function is called only by the child process when we are searching a file.
 * It uses dirent to parse the files in the directory where the downloadable files
 * are stored. If filename is found, 1 is written into write_fd, otherwise 0 is
 * written.
 */
static void search_file_fork(const char* filename, int write_fd);


/*
 * Ensure that the request we are treating is not a duplicate. log_entry contains
 * the data from the request ; it is then compared to the different log entries
 * present in the server list. If a match is found, the function return 0,
 * otherwise it returns 1, effectively indicating if the request is unique.
 */
static int check_unique_request(server_t* server, const search_request_t* request);


/*
 * Default depth when we are searching for a file on the network. Each time a
 * machine receives the packet (assuming it has not yet received it once),
 * it decreases the TTL by one. When the TTL reaches 0, the machine responds
 * to the original asker (by opening a direct connection).
 */
#define DEFAULT_TTL 10;


void handle_remote_search_request(server_t* server, int sock) {
    uint8_t source_ip_len;
    read_from_fd(sock, &source_ip_len, sizeof(uint8_t));

    char* ip_source = malloc(source_ip_len + 1);
    read_from_fd(sock, ip_source, source_ip_len);
    ip_source[source_ip_len] = '\0';

    uint8_t source_port_len;
    read_from_fd(sock, &source_port_len, sizeof(uint8_t));

    char* port_source = malloc(source_port_len + 1);
    read_from_fd(sock, &port_source, source_port_len);
    port_source[source_port_len] = '\0';

    uint8_t file_name_length;
    read_from_fd(sock, &file_name_length, sizeof(uint8_t));

    char* filename = malloc(file_name_length + 1);
    read_from_fd(sock, filename, file_name_length);
    filename[file_name_length] = '\0';

    uint8_t ttl;
    read_from_fd(sock, &ttl, sizeof(uint8_t));

    uint8_t nb_ips;
    read_from_fd(sock, &nb_ips, sizeof(uint8_t));

    char** ips = malloc(nb_ips * sizeof(char*));
    char** ports = malloc(nb_ips * sizeof(char*));

    for (int i = 0; i < nb_ips; i++) {
        uint8_t ip_len;
        read_from_fd(sock, &ip_len, sizeof(uint8_t));

        ips[i] = malloc(ip_len + 1);
        read_from_fd(sock, ips[i], ip_len);
        ips[i][ip_len] = '\0';

        uint8_t port_len;
        read_from_fd(sock, &port_len, sizeof(uint8_t));

        ports[i] = malloc(port_len + 1);
        read_from_fd(sock, ports[i], port_len);
        ports[i][port_len] = '\0';
    }

    request_t main_request;
    main_request.type = REQUEST_SEARCH_REMOTE;

    search_request_t* request = malloc(sizeof(search_request_t));
    request->filename   = filename;
    request->ips        = ips;
    request->ip_source  = ip_source;
    request->nb_ips     = nb_ips;
    request->ports      = ports;
    request->port_source = port_source;
    request->ttl        = ttl;

    main_request.request = request;

    list_push_back(server->pending_requests, &main_request);
}


void answer_local_search_request(server_t* server, request_t* request) {
    local_search_request_t* local_request = (local_search_request_t*)request->request;

    if (search_file(local_request->name) == 1) {
        return;
    }

    /*
     * Packet ID + length IP + IP + length port + port + length filename +
     * filename + ttl + nb_ips.
     */
    void* packet = malloc(PKT_ID_SIZE + sizeof(uint8_t) + INET6_ADDRSTRLEN +
                          sizeof(uint8_t) + 5 + sizeof(uint8_t) + 255 + 1 + 1);
    char* data = packet;

    *(opcode_t*)data = CMSG_SEARCH_REQUEST;
    data += PKT_ID_SIZE;

    const char* local_ip = server->self_ip;

    *(uint8_t*)data = strlen(local_ip);
    data += sizeof(uint8_t);

    memcpy(data, local_ip, strlen(local_ip));
    data += strlen(local_ip);

    char* port = extract_port_from_socket_s(server->listening_socket, 0);
    *(uint8_t*)data = strlen(port);
    data += sizeof(uint8_t);

    memcpy(data, port, strlen(port));
    data += strlen(port);

    *(uint8_t*)data = strlen(local_request->name);
    data += sizeof(uint8_t);

    memcpy(data, local_request->name, strlen(local_request->name));
    data += strlen(local_request->name);

    *(uint8_t*)data = DEFAULT_TTL;
    data += sizeof(uint8_t);

    *(uint8_t*)data = 0;
    data += sizeof(uint8_t);

    broadcast_packet(server, packet, (intptr_t)data - (intptr_t)packet);

    free(packet);
    free(local_request);
}


void answer_remote_search_request(server_t* server, request_t* request) {
    search_request_t* local_request = (search_request_t*)request->request;
    int unique = check_unique_request(server, local_request);
    int server_answer = 0;

    /*
     * Answer as soon as we can : either we already received the request, either
     * we cannot make it go any farther.
     */
    if (unique || local_request->ttl == 0) {
        server_answer = 1;
    }

    /* Basically, rebuild the packet. */
    void* packet = malloc(PKT_ID_SIZE + sizeof(uint8_t) + INET6_ADDRSTRLEN +
                          sizeof(uint8_t) + 5 + sizeof(uint8_t) + 255 + 1 + 1 +
                          local_request->nb_ips *
                            (sizeof(uint8_t) + INET6_ADDRSTRLEN +
                             sizeof(uint8_t) + 5));
    char* ptr = packet;
    if (server_answer == 0) {
        *(opcode_t*)ptr = CMSG_SEARCH_REQUEST;
    } else {
        *(opcode_t*)ptr = SMSG_SEARCH_REQUEST;
    }
    ptr += PKT_ID_SIZE;

    if (server_answer == 0) {
        uint8_t length = strlen(local_request->ip_source);
        *(uint8_t*)ptr = length;
        ptr += sizeof(uint8_t);

        memcpy(ptr, local_request->ip_source, length);
        ptr += length;

        uint8_t port_length = strlen(local_request->port_source);
        *(uint8_t*)ptr = port_length;
        ptr += sizeof(uint8_t);

        memcpy(ptr, local_request->port_source, port_length);
        ptr += port_length;
    }

    uint8_t filename_length = strlen(local_request->filename);
    *(uint8_t*)ptr = filename_length;
    ptr += sizeof(uint8_t);

    memcpy(ptr, local_request->filename, filename_length);
    ptr += filename_length;

    if (server_answer == 0) {
        assert(local_request->ttl > 0);
        *(uint8_t*)ptr = local_request->ttl - 1;
        ptr += sizeof(uint8_t);
    }

    int has_file = 0;
    if (unique == 1) {
        if (search_file(local_request->filename) == 1) {
            has_file = 1;
        }
    }

    *(uint8_t*)ptr = local_request->nb_ips + (has_file == 1 ? 1 : 0);
    ptr += sizeof(uint8_t);

    for (int i = 0; i < local_request->nb_ips; i++) {
        uint8_t ip_length = strlen(local_request->ips[i]);
        uint8_t port_length = strlen(local_request->ports[i]);

        *(uint8_t*)ptr = ip_length;
        ptr += sizeof(uint8_t);

        strcpy(ptr, local_request->ips[i]);
        ptr += ip_length;

        *(uint8_t*)ptr = port_length;
        ptr += sizeof(uint8_t);

        strcpy(ptr, local_request->ports[i]);
        ptr += port_length;
    }

    if (has_file == 1) {
        uint8_t ip_length = strlen(server->self_ip);
        const char* local_port = extract_port_from_socket_s(server->listening_socket, 0);
        uint8_t port_length = strlen(local_port);

        *(uint8_t*)ptr = ip_length;
        ptr += sizeof(uint8_t);

        strcpy(ptr, server->self_ip);
        ptr += ip_length;

        *(uint8_t*)ptr = port_length;
        ptr += sizeof(uint8_t);

        strcpy(ptr, local_port);
        ptr += port_length;
    }

    int* sockets = malloc(sizeof(int) * MAX_NEIGHBOURS);
    int nb_sockets = 0;
    for (int i = 0; i < MAX_NEIGHBOURS; ++i) {
        int sock = server->neighbours[i].sock;
        if (sock != -1 && sock != local_request->source_sock) {
            sockets[i] = sock;
            ++nb_sockets;
        }
    }

    broadcast_packet_to(sockets, nb_sockets, packet, (intptr_t)ptr - (intptr_t)packet);


    free(packet);
    free(local_request->filename);
    for (int i = 0; i < local_request->nb_ips; i++) {
        free(local_request->ips[i]);
        free(local_request->ports[i]);
    }
    free(local_request->ips);
    free(local_request->ip_source);
    free(local_request->ports);
    free(local_request->port_source);
    free(local_request);
}


void answer_local_download_request(server_t* server, request_t* request) {

}


void answer_remote_download_request(server_t* server, request_t* request) {

}


int search_file(const char* filename) {
    int fds[2];
    pipe(fds);

    pid_t find_pid = fork();
    if (find_pid == -1) {
        applog(LOG_LEVEL_FATAL, "[Server] Erreur lors du fork pour la recherche de fichier.\n");
        exit(EXIT_FAILURE);
    } else if (find_pid == 0) {
        close(fds[0]);
        search_file_fork(filename, fds[1]);
        exit(EXIT_SUCCESS);
    }

    close(fds[1]);
    int status = -1;
    waitpid(find_pid, &status, 0);

    signed char value;
    read(fds[0], &value, sizeof(signed char));
    close(fds[0]);

    return value;
}


void search_file_fork(const char* filename, int write_fd) {
    DIR* search_dir = opendir(SEARCH_DIRECTORY);
    if (search_dir == NULL) {
        mkdir(SEARCH_DIRECTORY, 0777);
        search_dir = opendir(SEARCH_DIRECTORY);
        assert(search_dir != NULL);
    }

    signed char found = 0;
    struct dirent* entry = readdir(search_dir);
    while (entry != NULL) {
        if (strcmp(entry->d_name, filename) == 0) {
            found = 1;
            break;
        }

        entry = readdir(search_dir);
    }

    write_to_fd(write_fd, &found, sizeof(signed char));
    closedir(search_dir);
}


int check_unique_request(server_t* server, const search_request_t* request) {
    cell_t* head = server->received_search_requests->head;
    while (head != NULL) {
        search_request_log_t* entry = (search_request_log_t*)head->data;
        if (strcmp(entry->filename, request->filename) == 0) {
            if (strcmp(entry->source_ip, request->ip_source) == 0) {
                if (strcmp(entry->source_port, request->port_source)) {
                    return 0;
                }
            }
        }

        head = head->next;
    }

    return 1;
}


void handle_remote_search_answer(server_t* server, int sock) {
    uint8_t filename_length;
    read_from_fd(sock, &filename_length, sizeof(uint8_t));

    char* filename = malloc(filename_length + 1);
    read_from_fd(sock, filename, filename_length);
    filename[filename_length] = '\0';

    uint8_t nb_ips;
    read_from_fd(sock, &nb_ips, sizeof(uint8_t));

    char** ips = malloc(sizeof(char*) * nb_ips);
    char** ports = malloc(sizeof(char*) * nb_ips);

    for (int i = 0; i < nb_ips; i++) {
        uint8_t ip_length, port_length;

        read_from_fd(sock, &ip_length, sizeof(uint8_t));
        ips[i] = malloc(ip_length + 1);
        read_from_fd(sock, ips[i], ip_length);

        read_from_fd(sock, &port_length, sizeof(uint8_t));
        ports[i] = malloc(port_length + 1);
        read_from_fd(sock, ports[i], port_length);
    }

    int target = server->client_socket;
    void* packet = malloc(PKT_ID_SIZE + sizeof(uint8_t) + strlen(filename) +
                          nb_ips *
                            (sizeof(uint8_t) + INET6_ADDRSTRLEN +
                             sizeof(uint8_t) + 5));
    char* ptr = packet;

    *(opcode_t*)ptr = SMSG_INT_SEARCH;
    ptr += PKT_ID_SIZE;

    *(uint8_t*)ptr = strlen(filename);
    ptr += sizeof(uint8_t);

    strcpy(ptr, filename);
    ptr += strlen(filename);

    *(uint8_t*)ptr = nb_ips;
    ptr += sizeof(uint8_t);

    for (int i = 0; i < nb_ips; i++) {
        *(uint8_t*)ptr = strlen(ips[i]);
        ptr += sizeof(uint8_t);

        strcpy(ptr, ips[i]);
        ptr += strlen(ips[i]);

        *(uint8_t*)ptr = strlen(ports[i]);
        ptr += sizeof(uint8_t);

        strcpy(ptr, ports[i]);
        ptr += strlen(ports[i]);
    }

    write_to_fd(target, packet, (intptr_t)ptr - (intptr_t)packet);

    free(packet);
    for (int i = 0; i < nb_ips; i++) {
        free(ips[i]);
        free(ports[i]);
    }

    free(ips);
    free(ports);
}
