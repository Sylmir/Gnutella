#ifndef LIST_H
#define LIST_H

typedef int(*compare_fn)(void* lhs, void* rhs);


typedef struct cell_s {
    struct cell_s* next;
    void* data;
} cell_t;


typedef struct list_s {
    cell_t* head;
    compare_fn compare;
} list_t;


list_t* list_create(compare_fn compare);
void list_push_back(list_t* list, void* data);
void list_pop(list_t* list, void* data, int once);
void list_pop_at(cell_t** at);
void list_clear(list_t* list);
void list_destroy(list_t** list);

#endif /* LIST_H */
