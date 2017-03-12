#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "client_internal.h"
#include "list.h"

int has_file_record(client_t* client, const char* filename, file_lookup_t** dest) {
    cell_t* head = client->machines_by_files->head;
    while (head != NULL) {
        file_lookup_t* entry = (file_lookup_t*)head->data;
        if (strcmp(entry->filename, filename) == 0) {
            *dest = entry;
            return 1;
        }

        head = head->next;
    }

    return 0;
}


int has_file_machine_record(const file_lookup_t* record, const char* ip, const char* port) {
    cell_t* head = record->machines->head;
    while (head != NULL) {
        machine_t* entry = (machine_t*)head->data;
        if (strcmp(entry->ip, ip) == 0 && strcmp(entry->port, port) == 0) {
            return 1;
        }

        head = head->next;
    }

    return 0;
}


void handle_lookup(client_t* client) {
    char* file = strtok(NULL, "\0");
    if (file == NULL) {
        printf("Commande invalide\n");
        return;
    }

    file_lookup_t* entry;

    if (has_file_record(client, file, &entry) == 1) {
        printf("Machines possédant le fichier %s:\n", file);

        cell_t* head = entry->machines->head;
        while (head != NULL) {
            machine_t* machine = (machine_t*)head->data;
            printf("\t%s:%s\n", machine->ip, machine->port);
            head = head->next;
        }
        printf("\n");
    } else {
        printf("Aucune machine ne possède le fichier %s\n", file);
    }
}


LIST_CREATE_FN void* create_machine(void* machine){
    machine_t* _machine = (machine_t*)machine;

    machine_t* new_machine = malloc(sizeof(machine_t));
    new_machine->ip = _machine->ip;
    new_machine->port = _machine->port;

    return new_machine;
}


LIST_CREATE_FN void* create_lookup(void* lookup) {
    file_lookup_t* _lookup = (file_lookup_t*)lookup;

    file_lookup_t* new_lookup = malloc(sizeof(file_lookup_t));
    new_lookup->filename = _lookup->filename;
    new_lookup->machines = _lookup->machines;

    return new_lookup;
}
