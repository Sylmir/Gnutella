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
 * Default depth when we are searching for a file on the network. Each time a
 * machine receives the packet (assuming it has not yet received it once),
 * it decreases the TTL by one. When the TTL reaches 0, the machine responds
 * to the original asker (by opening a direct connection).
 */
static const uint8_t DEFAULT_TTL = 10;


void answer_local_search_request(server_t* server, request_t* request) {
    local_search_request_t* local_request = (local_search_request_t*)request->request;

    if (search_file(local_request->name) == 1) {
        return;
    }

    void* packet = malloc(PKT_ID_SIZE + sizeof(uint8_t) + INET6_ADDRSTRLEN +
                          sizeof(uint8_t) + UINT8_MAX + 4 + 1 + 1);
    char* data = packet;

    *(opcode_t*)data = CMSG_SEARCH_REQUEST;
    data += PKT_ID_SIZE;

    char local_ip[INET6_ADDRSTRLEN];
    extract_self_ip(local_ip);

    *(uint8_t*)data = strlen(local_ip);
    data += sizeof(uint8_t);

    memcpy(data, local_ip, strlen(local_ip));
    data += strlen(local_ip);

    *(uint8_t*)data = strlen(local_request->name);
    data += sizeof(uint8_t);

    memcpy(data, local_request->name, strlen(local_request->name));
    data += strlen(local_request->name);

    *(uint32_t*)data = server->packet_counter;
    data += sizeof(uint32_t);
    ++server->packet_counter;

    *(uint8_t*)data = DEFAULT_TTL;
    data += sizeof(uint8_t);

    *(uint8_t*)data = 0;
    data += sizeof(uint8_t);

    broadcast_packet(server, packet, (intptr_t)data - (intptr_t)packet);

    free(packet);
}


void answer_remote_search_request(server_t* server, request_t* request) {

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
