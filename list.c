#include <stdlib.h>

#include "list.h"

static int dummy_compare(void* a, void* b) {
    return a == b;
}

list_t* list_create(compare_fn compare, create_fn create) {
    list_t* list = malloc(sizeof(list_t));
    if (create != NULL) {
        list->compare = compare;
    } else {
        list->compare = dummy_compare;
    }
    list->create = create;
    list->head = NULL;
    return list;
}


void list_push_back(list_t* list, void* value) {
    list_push_back_no_create(list, list->create(value));
}


void list_push_back_no_create(list_t *list, void* data) {
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


void list_pop_at(list_t *list, cell_t **prev, cell_t** at) {
    if (*prev != NULL) {
        (*prev)->next = (*at)->next;
    } else {
        if ((*at)->next == NULL) {
            list->head = NULL;
        }
    }

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
