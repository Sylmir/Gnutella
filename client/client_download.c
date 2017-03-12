#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "client_internal.h"
#include "log.h"
#include "packets_defines.h"
#include "util.h"


void handle_download(client_t* client) {
    char* ip = strtok(NULL, " ");
    if (ip == NULL) {
        printf("Invalid command\n");
        return;
    }

    char* port = strtok(NULL, " ");
    if (port == NULL) {
        printf("Invalid command\n");
        return;
    }

    char* file = strtok(NULL, "\0");
    if (file == NULL) {
        printf("Invalid command\n");
        return;
    }

    applog(LOG_LEVEL_INFO, "Downloading file %s from %s:%s\n", file, ip, port);

    void* data = malloc(PKT_ID_SIZE + sizeof(uint8_t) + strlen(ip) +
                        sizeof(uint8_t) + strlen(port) +
                        sizeof(uint8_t) + strlen(file));
    char* ptr = data;

    opcode_t opcode = CMSG_INT_DOWNLOAD;
    write_to_packet(&ptr, &opcode, PKT_ID_SIZE);

    uint8_t ip_length = strlen(ip);
    write_to_packet(&ptr, &ip_length, sizeof(uint8_t));
    write_to_packet(&ptr, ip, ip_length);

    uint8_t port_length = strlen(port);
    write_to_packet(&ptr, &port_length, sizeof(uint8_t));
    write_to_packet(&ptr, port, port_length);

    uint8_t filename_length = strlen(file);
    write_to_packet(&ptr, &filename_length, sizeof(uint8_t));
    write_to_packet(&ptr, file, filename_length);

    write_to_fd(client->server_socket, data, (intptr_t)ptr - (intptr_t)data);

    free(data);
}


void handle_download_answer(client_t* client) {
    uint8_t code;
    read_from_fd(client->server_socket, &code, sizeof(uint8_t));

    if (code != ANSWER_CODE_REMOTE_FOUND) {
        uint8_t ip_length, port_length, filename_length;
        read_from_fd(client->server_socket, &ip_length, sizeof(uint8_t));

        char* ip = malloc(ip_length + 1);
        read_from_fd(client->server_socket, ip, ip_length);

        read_from_fd(client->server_socket, &port_length, sizeof(uint8_t));

        char* port = malloc(port_length + 1);
        read_from_fd(client->server_socket, port, port_length);

        read_from_fd(client->server_socket, &filename_length, sizeof(uint8_t));

        char* filename = malloc(filename_length + 1);
        read_from_fd(client->server_socket, filename, filename_length);

        ip[ip_length] = '\0';
        port[port_length] = '\0';
        filename[filename_length] = '\0';

        switch (code) {
        case ANSWER_CODE_LOCAL:
            printf("Le fichier %s se trouve déjà sur la machine locale.\n", filename);
            break;

        case ANSWER_CODE_REMOTE_NOT_FOUND:
            printf("La machine %s:%s ne possède pas le fichier %s.\n", ip, port, filename);
            break;

        case ANSWER_CODE_REMOTE_OFFLINE:
            printf("La machine %s:%s est hors ligne.\n", ip, port);
            break;

        default:
            break;
        }

        free(ip);
        free(port);
        free(filename);
    } else {
        uint8_t filename_length;
        read_from_fd(client->server_socket, &filename_length, sizeof(uint8_t));

        char* filename = malloc(filename_length + 1);
        read_from_fd(client->server_socket, filename, filename_length);
        filename[filename_length] = '\0';

        printf("Le fichier %s a été téléchargé.\n", filename);

        free(filename);
    }

}
