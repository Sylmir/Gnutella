#define _GNU_SOURCE

#include <assert.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include <arpa/inet.h>
#include <dirent.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#include "log.h"
#include "networking.h"
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
 * Clean a search request, i.e free the memory allocated.
 */
static void clean_search_request(search_request_t* request);


/*
 * If we are the source sender and we received the packet, forward it immediately
 * to our local client.
 */
static void forward_to_local(server_t* server, search_request_t* request);


/*
 * Send the results of one search to our local client.
 */
static void send_search_answer_to_client(server_t* server, const char* filename,
                                         uint8_t nb_ips, char** ips, char** ports);


/*
 * Clean a download request, i.e free the memory allocate.
 */
static void clean_download_request(download_request_t* request);


/*
 * Build the header of the download response packet.
 */
static void build_download_answer_header(char*** work_ptr,
                                         download_request_t* request,
                                         smsg_int_download_answer_codes_t code);


/*
 * Build and send a response to the local client when the only thing we write
 * in the packet is an error code.
 */
static void send_download_error_response(server_t* server, download_request_t* request,
                                         smsg_int_download_answer_codes_t code);


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
    read_from_fd(sock, port_source, source_port_len);
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

    applog(LOG_LEVEL_INFO, "[Client] Searching file %s\n", local_request->name);

    int has_file = search_file(local_request->name);

    applog(LOG_LEVEL_INFO, "[Client] Found file = %d\n", has_file);

    /*
     * Packet ID + length IP + IP + length port + port + length filename +
     * filename + ttl + nb_ips.
     */
    void* packet = malloc(PKT_ID_SIZE + sizeof(uint8_t) + INET6_ADDRSTRLEN +
                          sizeof(uint8_t) + 5 + sizeof(uint8_t) + 255 + 1 + 1 +
                          1 + INET6_ADDRSTRLEN + 1 + 5);
    char** data = (char**)&packet;

    opcode_t opcode;
    if (has_file == 1) {
        opcode = SMSG_INT_SEARCH;
    } else {
        opcode = CMSG_SEARCH_REQUEST;
    }

    write_to_packet(data, &opcode, PKT_ID_SIZE);

    if (has_file == 0) {
        const char* local_ip = server->self_ip;
        uint8_t ip_length = strlen(local_ip);

        write_to_packet(data, &ip_length, sizeof(uint8_t));
        write_to_packet(data, local_ip, ip_length);

        char* port = extract_port_from_socket_s(server->listening_socket, 0);
        uint8_t port_length = strlen(port);

        write_to_packet(data, &port_length, sizeof(uint8_t));
        write_to_packet(data, port, port_length);

        free(port);
    }

    uint8_t filename_length = strlen(local_request->name);
    write_to_packet(data, &filename_length, sizeof(uint8_t));
    write_to_packet(data, local_request->name, filename_length);

    if (has_file == 0) {
        uint8_t ttl = DEFAULT_TTL;
        write_to_packet(data, &ttl, sizeof(uint8_t));
    }

    write_to_packet(data, &has_file, sizeof(uint8_t));

    if (has_file == 0) {
        broadcast_packet(server, packet, (intptr_t)*data - (intptr_t)packet);
    } else {
        uint8_t self_length = strlen(server->self_ip);
        write_to_packet(data, &self_length, sizeof(uint8_t));
        write_to_packet(data, server->self_ip, self_length);

        char* port = extract_port_from_socket_s(server->listening_socket, 0);
        uint8_t port_length = strlen(port);

        write_to_packet(data, &port_length, sizeof(uint8_t));
        write_to_packet(data, port, port_length);

        write_to_fd(server->client_socket, packet, (intptr_t)*data - (intptr_t)packet);
    }

    free(packet);
    free(local_request);
}


void answer_remote_search_request(server_t* server, request_t* request) {
    search_request_t* local_request = (search_request_t*)request->request;

    /* If we are the source machine, forward to local. */
    if (strcmp(server->self_ip, local_request->ip_source) == 0) {
        forward_to_local(server, local_request);
        return;
    }

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
    char** ptr = (char**)&packet;
    opcode_t opcode;
    if (server_answer == 0) {
        opcode = CMSG_SEARCH_REQUEST;
    } else {
        opcode = SMSG_SEARCH_REQUEST;
    }

    write_to_packet(ptr, &opcode, PKT_ID_SIZE);

    if (server_answer == 0) {
        uint8_t length = strlen(local_request->ip_source);
        write_to_packet(ptr, &length, sizeof(uint8_t));
        write_to_packet(ptr, local_request->ip_source, length);

        uint8_t port_length = strlen(local_request->port_source);
        write_to_packet(ptr, &port_length, sizeof(uint8_t));
        write_to_packet(ptr, local_request->port_source, port_length);
    }

    uint8_t filename_length = strlen(local_request->filename);
    write_to_packet(ptr, &filename_length, sizeof(uint8_t));
    write_to_packet(ptr, local_request->filename, filename_length);

    if (server_answer == 0) {
        assert(local_request->ttl > 0);
        uint8_t ttl = local_request->ttl - 1;
        write_to_packet(ptr, &ttl, sizeof(uint8_t));
    }

    int has_file = 0;
    if (unique == 1) {
        if (search_file(local_request->filename) == 1) {
            has_file = 1;
        }
    }

    uint8_t nb_ips = local_request->nb_ips + (has_file == 1 ? 1 : 0);
    write_to_packet(ptr, &nb_ips, sizeof(uint8_t));

    for (int i = 0; i < local_request->nb_ips; i++) {
        uint8_t ip_length = strlen(local_request->ips[i]);
        uint8_t port_length = strlen(local_request->ports[i]);

        write_to_packet(ptr, &ip_length, sizeof(uint8_t));
        write_to_packet(ptr, local_request->ips[i], ip_length);

        write_to_packet(ptr, &port_length, sizeof(uint8_t));
        write_to_packet(ptr, local_request->ports[i], port_length);
    }

    if (has_file == 1) {
        uint8_t ip_length = strlen(server->self_ip);
        const char* local_port = extract_port_from_socket_s(server->listening_socket, 0);
        uint8_t port_length = strlen(local_port);

        write_to_packet(ptr, &ip_length, sizeof(uint8_t));
        write_to_packet(ptr, server->self_ip, ip_length);

        write_to_packet(ptr, &port_length, sizeof(uint8_t));
        write_to_packet(ptr, local_port, port_length);
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

    broadcast_packet_to(sockets, nb_sockets, packet, (intptr_t)*ptr - (intptr_t)packet);

    free(packet);
    clean_search_request(local_request);
}


void answer_local_download_request(server_t* server, request_t* request) {
    download_request_t* download = (download_request_t*)request->request;

    if (search_file(download->filename) == 1) {
        send_download_error_response(server, download, ANSWER_CODE_LOCAL);
        return;
    }

    int sock = -1;
    int res = connect_to(download->ip, download->port, &sock);

    if (res != CONNECT_OK) {
        send_download_error_response(server, download, ANSWER_CODE_REMOTE_OFFLINE);
        return;
    }

    list_push_back(server->pending_downloads, &sock);

    void* packet = malloc(PKT_ID_SIZE + sizeof(uint8_t) + strlen(download->filename));
    char** ptr = (char**)&packet;

    opcode_t opcode = CMSG_DOWNLOAD;
    write_to_packet(ptr, &opcode, PKT_ID_SIZE);

    uint8_t filename_length = strlen(download->filename);
    write_to_packet(ptr, &filename_length, sizeof(uint8_t));
    write_to_packet(ptr, download->filename, filename_length);

    write_to_fd(sock, packet, (intptr_t)*ptr - (intptr_t)packet);

    clean_download_request(download);
    free(packet);
}


void answer_remote_download_request(server_t* server, request_t* request) {
    UNUSED(server);

    remote_download_request_t* download = (remote_download_request_t*)request->request;

    int has_file = search_file(download->filename);
    if (has_file == 0) {
        void* packet = malloc(PKT_ID_SIZE +
                              sizeof(uint8_t) + INET6_ADDRSTRLEN +
                              sizeof(uint8_t) + 5 +
                              sizeof(uint8_t) + strlen(download->filename) +
                              sizeof(uint8_t));
        char** ptr = (char**)&packet;
        char* ip, *port;
        extract_ip_port_from_socket_s(download->socket, &ip, &port, 0);

        opcode_t opcode = SMSG_DOWNLOAD;
        uint8_t ip_length = strlen(ip), port_length = strlen(port);
        uint8_t name_length = strlen(download->filename);
        uint8_t result = ANSWER_CODE_REMOTE_NOT_FOUND;

        write_to_packet(ptr, &opcode, PKT_ID_SIZE);
        write_to_packet(ptr, &ip_length, sizeof(uint8_t));
        write_to_packet(ptr, ip, strlen(ip));
        write_to_packet(ptr, &port_length, sizeof(uint8_t));
        write_to_packet(ptr, port, strlen(port));
        write_to_packet(ptr, &name_length, sizeof(uint8_t));
        write_to_packet(ptr, download->filename, strlen(download->filename));
        write_to_packet(ptr, &result, sizeof(uint8_t));

        write_to_fd(download->socket, packet, (intptr_t)*ptr - (intptr_t)packet);

        free(ip);
        free(port);
        free(packet);
        /* JE HAIS CETTE LIGNE, POURQUOI JE DOIS EXTRAIRE L'IP ET LE PORT ENCORE UNE FOIS ? */
        close(download->socket);
        free(download->filename);
        free(download);

        return;
    }

    char* full_name = malloc(strlen(SEARCH_DIRECTORY) + 1 + strlen(download->filename) + 1);
    sprintf(full_name, "%s/%s", SEARCH_DIRECTORY, download->filename);
    int file = open(full_name, O_RDONLY, 0666);
    free(full_name);

    uint32_t length = 0;
    void* buffer = malloc(100);
    int _read = 0;

    do {
        _read = read(file, buffer + length, 100);

        if (_read == 0) {
            break;
        }

        void* copy = malloc(length);
        memcpy(copy, buffer, length);

        length += _read;

        buffer = realloc(buffer, length);
        memcpy(buffer, copy, length);

        free(copy);
    } while (1);

    void* packet = malloc(PKT_ID_SIZE + sizeof(uint8_t) + sizeof(uint32_t) + length);
    char** ptr = (char**)&packet;

    opcode_t opcode = SMSG_DOWNLOAD;
    write_to_packet(ptr, &opcode, PKT_ID_SIZE);

    uint8_t answer = ANSWER_CODE_REMOTE_FOUND;
    write_to_packet(ptr, &answer, sizeof(uint8_t));

    uint8_t filename_length = strlen(download->filename);
    write_to_packet(ptr, &filename_length, sizeof(uint8_t));
    write_to_packet(ptr, download->filename, filename_length);

    write_to_packet(ptr, &length, sizeof(uint32_t));
    write_to_packet(ptr, buffer, length);

    write_to_fd(download->socket, packet, (intptr_t)*ptr - (intptr_t)packet);

    free(packet);
    close(download->socket);
    free(download->filename);
    free(download);
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

    applog(LOG_LEVEL_INFO, "[Client] Statut du fils : %d\n", status);

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

    send_search_answer_to_client(server, filename, nb_ips, ips, ports);
}


void clean_search_request(search_request_t* request) {

    free(request->filename);
    for (int i = 0; i < request->nb_ips; i++) {
        free(request->ips[i]);
        free(request->ports[i]);
    }
    free(request->ips);
    free(request->ip_source);
    free(request->ports);
    free(request->port_source);
    free(request);
}


void forward_to_local(server_t* server, search_request_t* request) {
    send_search_answer_to_client(server, request->filename, request->nb_ips,
                                 request->ips, request->ports);
    clean_search_request(request);
}


void send_search_answer_to_client(server_t* server, const char* filename,
                                  uint8_t nb_ips, char** ips, char** ports) {
    int target = server->client_socket;
    void* packet = malloc(PKT_ID_SIZE + sizeof(uint8_t) + strlen(filename) +
                          nb_ips *
                            (sizeof(uint8_t) + INET6_ADDRSTRLEN +
                             sizeof(uint8_t) + 5));
    char** ptr = (char**)&packet;

    opcode_t opcode = SMSG_INT_SEARCH;
    write_to_packet(ptr, &opcode, PKT_ID_SIZE);

    uint8_t filename_length = strlen(filename);
    write_to_packet(ptr, &filename_length, sizeof(uint8_t));
    write_to_packet(ptr, filename, filename_length);

    write_to_packet(ptr, &nb_ips, sizeof(uint8_t));

    for (int i = 0; i < nb_ips; i++) {
        uint8_t ip_length = strlen(ips[i]), port_length = strlen(ports[i]);

        write_to_packet(ptr, &ip_length, sizeof(uint8_t));
        write_to_packet(ptr, ips[i], ip_length);

        write_to_packet(ptr, &port_length, sizeof(uint8_t));
        write_to_packet(ptr, ports[i], port_length);
    }

    write_to_fd(target, packet, (intptr_t)*ptr - (intptr_t)packet);

    free(packet);
    for (int i = 0; i < nb_ips; i++) {
        free(ips[i]);
        free(ports[i]);
    }

    free(ips);
    free(ports);
}


void clean_download_request(download_request_t* request) {
    free(request->filename);
    free(request->ip);
    free(request->port);
    free(request);
}


void send_download_error_response(server_t* server, download_request_t* request,
                                         smsg_int_download_answer_codes_t code) {
    void* packet = malloc(PKT_ID_SIZE + sizeof(uint8_t) + strlen(request->filename) +
                          sizeof(uint8_t) + strlen(request->ip) +
                          sizeof(uint8_t) + strlen(request->port) +
                          sizeof(uint8_t));
    char** ptr = (char**)&packet;
    build_download_answer_header(&ptr, request, code);

    write_to_fd(server->client_socket, packet, (intptr_t)*ptr - (intptr_t)packet);

    free(packet);
    clean_download_request(request);
}


void build_download_answer_header(char*** work_ptr,
                                  download_request_t* request,
                                  smsg_int_download_answer_codes_t code) {
    char** ptr = *work_ptr;

    opcode_t opcode = SMSG_INT_DOWNLOAD;
    uint8_t ip_length = strlen(request->ip);
    uint8_t port_length = strlen(request->port);
    uint8_t filename_length = strlen(request->filename);

    write_to_packet(ptr, &opcode, PKT_ID_SIZE);

    write_to_packet(ptr, &ip_length, sizeof(uint8_t));
    write_to_packet(ptr, request->ip, ip_length);

    write_to_packet(ptr, &port_length, sizeof(uint8_t));
    write_to_packet(ptr, request->port, port_length);

    write_to_packet(ptr, &filename_length, sizeof(uint8_t));
    write_to_packet(ptr, request->filename, filename_length);

    write_to_packet(ptr, &code, sizeof(uint8_t));

    *work_ptr = ptr;
}


void handle_remote_download_answer(server_t* server, int sock) {
    uint8_t answer;
    read_from_fd(sock, &answer, sizeof(uint8_t));

    if (answer == ANSWER_CODE_REMOTE_NOT_FOUND) {
        download_request_t* request = malloc(sizeof(download_request_t));
        uint8_t ip_length, port_length, filename_length;

        read_from_fd(sock, &ip_length, sizeof(uint8_t));

        request->ip = malloc(ip_length + 1);
        read_from_fd(sock, request->ip, ip_length);
        request->ip[ip_length] = '\0';

        read_from_fd(sock, &port_length, sizeof(uint8_t));

        request->port = malloc(port_length + 1);
        read_from_fd(sock, request->port, port_length);
        request->port[port_length] = '\0';

        read_from_fd(sock, &filename_length, sizeof(uint8_t));

        request->filename = malloc(filename_length + 1);
        read_from_fd(sock, request->filename, filename_length);
        request->filename[filename_length] = '\0';

        send_download_error_response(server, request, ANSWER_CODE_REMOTE_NOT_FOUND);

        return;
    }

    uint8_t filename_length;
    read_from_fd(sock, &filename_length, sizeof(uint8_t));

    char* filename = malloc(filename_length + 1);
    read_from_fd(sock, filename, filename_length);
    filename[filename_length] = '\0';

    uint8_t file_length;
    read_from_fd(sock, &file_length, sizeof(uint8_t));

    void* buffer = malloc(file_length);
    read_from_fd(sock, buffer, file_length);

    char* pathname = malloc(strlen(SEARCH_DIRECTORY + 1 + filename_length + 1));
    sprintf(pathname, "%s/%s", SEARCH_DIRECTORY, filename);

    int new_file = open(pathname, O_WRONLY, 0666);
    write_to_fd(new_file, buffer, file_length);

    free(buffer);
    free(pathname);

    void* packet = malloc(PKT_ID_SIZE + sizeof(uint8_t) + sizeof(uint8_t) + strlen(filename));
    char** ptr = (char**)&packet;

    opcode_t opcode = SMSG_INT_DOWNLOAD;
    write_to_packet(ptr, &opcode, PKT_ID_SIZE);

    smsg_int_download_answer_codes_t code = ANSWER_CODE_REMOTE_FOUND;
    write_to_packet(ptr, &code, sizeof(uint8_t));
    write_to_packet(ptr, &filename_length, sizeof(uint8_t));
    write_to_packet(ptr, filename, filename_length);

    write_to_fd(server->client_socket, packet, (intptr_t)*ptr - (intptr_t)packet);

    free(filename);
    free(packet);
}


void handle_remote_download_request(server_t* server, int sock) {
    uint8_t name_length;
    read_from_fd(sock, &name_length, sizeof(uint8_t));

    char* filename = malloc(name_length + 1);
    read_from_fd(sock, filename, name_length);
    filename[name_length] = '\0';

    remote_download_request_t* request = malloc(sizeof(remote_download_request_t));
    request->socket = sock;
    request->filename = filename;

    request_t main_request;
    main_request.type = REQUEST_DOWNLOAD_REMOTE;
    main_request.request = request;

    list_push_back(server->pending_requests, &main_request);
}
