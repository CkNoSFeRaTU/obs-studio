typedef struct list_s list_t;

list_t * list_append(list_t *, void *);
list_t * list_insert(list_t *, void *);
list_t * list_delete(list_t *, void *);
list_t * list_get_first(list_t *);
list_t * list_get_next(list_t *);
void   * list_get_data(list_t *);

