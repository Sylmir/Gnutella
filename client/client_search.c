#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "client_internal.h"
#include "log.h"
#include "packets_defines.h"
#include "util.h"

void handle_search(client_t* client) {
    const char* name = strtok(NULL, "\0");
    if (name == NULL) {
        log_to_file(LOG_LEVEL_ERROR, stdout, "Erreur dans la commande de recherche. "
                                             "Tapez \"help\" pour vérifier la syntaxe.\n");
        return;
    } else {
        applog(LOG_LEVEL_INFO, "Recherche du fichier %s\n", name);
    }

    void* data = malloc(PKT_ID_SIZE + sizeof(uint8_t) + strlen(name));
    char* ptr = data;
    *(opcode_t*)ptr = CMSG_INT_SEARCH;
    ptr += PKT_ID_SIZE;

    *(uint8_t*)ptr = strlen(name);
    ptr += sizeof(uint8_t);

    memcpy(ptr, name, strlen(name));
    ptr += strlen(name);

    write_to_fd(client->server_socket, data, (intptr_t)ptr - (intptr_t)data);

    free(data);
}


void handle_search_answer(client_t* client) {
    uint8_t filename_length;
    read_from_fd(client->server_socket, &filename_length, sizeof(uint8_t));

    char* filename = malloc(filename_length + 1);
    read_from_fd(client->server_socket, filename, filename_length);
    filename[filename_length] = '\0';


    file_lookup_t* lookup;
    int has_file = 1;
    if (has_file_record(client, filename, &lookup) == 0) {
        has_file = 0;
        lookup = malloc(sizeof(file_lookup_t));
        lookup->filename = filename;
        lookup->machines = list_create(NULL, create_machine);
        list_push_back_no_create(client->machines_by_files, lookup);
    }

    uint8_t nb_ips;
    read_from_fd(client->server_socket, &nb_ips, sizeof(uint8_t));

    for (int i = 0; i < nb_ips; i++) {
        uint8_t ip_length, port_length;
        read_from_fd(client->server_socket, &ip_length, sizeof(uint8_t));

        char* ip = malloc(ip_length + 1);
        read_from_fd(client->server_socket, ip, ip_length);
        ip[ip_length] = '\0';

        read_from_fd(client->server_socket, &port_length, sizeof(uint8_t));

        char* port = malloc(port_length + 1);
        read_from_fd(client->server_socket, port, port_length);
        port[port_length] = '\0';

        machine_t* machine = malloc(sizeof(machine_t));
        machine->ip = ip;
        machine->port = port;

        if (has_file == 1) {
            if (has_file_machine_record(lookup, ip, port) == 0) {
                list_push_back_no_create(lookup->machines, machine);
            }
        } else {

            list_push_back_no_create(lookup->machines, machine);
        }
    }
}
