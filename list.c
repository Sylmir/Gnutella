#include <stdlib.h>

#include "list.h"

list_t* list_create(compare_fn compare) {
    list_t* list = malloc(sizeof(list_t));
    list->compare = compare;
    list->head = NULL;
    return list;
}


void list_push_back(list_t* list, void* data) {
    cell_t* head = list->head;
    cell_t* prev = NULL;

    while (head != NULL) {
        prev = head;
        head = head->next;
    }

    if (prev == NULL) {
        list->head = malloc(sizeof(cell_t));
        list->head->data = data;
        list->head->next = NULL;
    } else {
        prev->next = malloc(sizeof(cell_t));
        prev->next->data = data;
        prev->next->next = NULL;
    }
}


void list_pop(list_t* list, void* data, int once) {
    cell_t* prev = NULL;
    for (cell_t* head = list->head; head != NULL; ) {
        if (list->compare(data, head->data) == 1) {
            free(head->data);

            if (prev == NULL) {
                list->head = head->next;
            } else {
                prev->next = head->next;

            }

            cell_t* next = head->next;
            free(head);
            head = next;

            if (once) {
                return;
            }
        }

        prev = head;
        head = head->next;
    }
}


void list_pop_at(cell_t** at) {
    free((*at)->data);
    cell_t* copy_at = *at;
    *at = (*at)->next;
    free(copy_at);
}


void list_clear(list_t* list) {
    for (cell_t* head = list->head; head != NULL; ) {
        free(head->data);
        cell_t* next = head->next;
        free(head);
        head = next;
    }
}


void list_destroy(list_t** list) {
    list_clear(*list);
    free(*list);
    *list = NULL;
}