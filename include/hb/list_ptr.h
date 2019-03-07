#ifndef HB_LIST_PTR_H
#define HB_LIST_PTR_H

#include <stdint.h>
#include <stddef.h>

typedef struct hb_list_ptr_s {
	uintptr_t *data;
	uint64_t capacity;
	uint64_t index;
} hb_list_ptr_t;


int hb_list_ptr_setup(hb_list_ptr_t *list, size_t capacity);
void hb_list_ptr_cleanup(hb_list_ptr_t *list);
int hb_list_ptr_push_back(hb_list_ptr_t *list, void *item);
int hb_list_ptr_pop_back(hb_list_ptr_t *list, void **item);
uint64_t hb_list_ptr_count(hb_list_ptr_t *list);
void hb_list_ptr_clear(hb_list_ptr_t *list);
void *hb_list_ptr_get(hb_list_ptr_t *list, uint64_t index);
int hb_list_ptr_remove(hb_list_ptr_t *list, size_t index);


#endif