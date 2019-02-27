#ifndef HB_LIST_H
#define HB_LIST_H

#include <stdint.h>


typedef struct hb_list_s {
	void *priv_impl;
	void *priv_data;
	size_t capacity;
	size_t item_size;
} hb_list_t;


int hb_list_setup(hb_list_t *list, size_t capacity, size_t item_size);
void hb_list_cleanup(hb_list_t *list);
int hb_list_push_back(hb_list_t *list, void *item, size_t *out_index);
int hb_list_pop_back(hb_list_t *list, void **item);
int hb_list_count(hb_list_t *list, size_t *out_count);
int hb_list_clear(hb_list_t *list);
int hb_list_get(hb_list_t *list, size_t index, void **out_item);
int hb_list_remove(hb_list_t *list, size_t index);


#endif