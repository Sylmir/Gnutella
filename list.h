#ifndef LIST_H
#define LIST_H

typedef int(*compare_fn)(void* lhs, void* rhs);
typedef void*(*create_fn)(void* data);


/*
 * Dummy macro to identfy that a function is used as a compare_fn inside a list.
 */
#define LIST_COMPARE_FN


/*
 * Dummy macro to identify that a function is used as a create_fn inside a list.
 */
#define LIST_CREATE_FN

typedef struct cell_s {
    struct cell_s* next;
    void* data;
} cell_t;


typedef struct list_s {
    cell_t* head;
    compare_fn compare;
    create_fn create;
} list_t;


list_t* list_create(compare_fn compare, create_fn create);
void list_push_back(list_t* list, void* value);
void list_push_back_no_create(list_t* list, void* data);
void list_pop(list_t* list, void* data, int once);
void list_pop_at(list_t* list, cell_t** prev, cell_t** at);
void list_clear(list_t* list);
void list_destroy(list_t** list);

#endif /* LIST_H */
