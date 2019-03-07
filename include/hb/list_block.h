#ifndef HB_LIST_BLOCK_H
#define HB_LIST_BLOCK_H

#include <stdint.h>
#include <stddef.h>

typedef struct hb_list_block_s {
	void *priv_impl;
	void *priv_data;
	uint64_t capacity;
	uint64_t item_size;
} hb_list_block_t;


int hb_list_block_setup(hb_list_block_t *list, uint64_t capacity, uint64_t item_size);
void hb_list_block_cleanup(hb_list_block_t *list);
int hb_list_block_push_back(hb_list_block_t *list, void **item, uint64_t *out_index);
int hb_list_block_pop_back(hb_list_block_t *list, void **item);
int hb_list_block_count(hb_list_block_t *list, uint64_t *out_count);
int hb_list_block_clear(hb_list_block_t *list);
int hb_list_block_get(hb_list_block_t *list, uint64_t index, void **out_item);
int hb_list_block_remove(hb_list_block_t *list, uint64_t index);


#endif