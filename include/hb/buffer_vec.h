#ifndef HB_BUFFER_VEC_H
#define HB_BUFFER_VEC_H

#include <stdint.h>

#include "hb/thread.h"


// forwards
typedef struct hb_buffer_s hb_buffer_t;


typedef struct hb_buffer_pool_s {
	uint64_t blocks_inuse;
	uint64_t bytes_inuse;
	uint64_t block_size;
	uint64_t block_count;
	hb_buffer_t *buffer_array;
	uint8_t *allocation;
	hb_list_ptr_t buffer_list_free;
	hb_mutex_t mtx;
} hb_buffer_pool_t;


int hb_buffer_vec_setup(hb_buffer_pool_t *pool, uint64_t block_count, uint64_t block_size);
void hb_buffer_vec_cleanup(hb_buffer_pool_t *pool);
void hb_buffer_vec_heap(hb_buffer_pool_t *pool, void **out_heap, uint64_t *out_block_count, uint64_t *out_block_size);

int hb_buffer_vec_lock(hb_buffer_pool_t *pool);
int hb_buffer_vec_unlock(hb_buffer_pool_t *pool);

int hb_buffer_vec_acquire(hb_buffer_pool_t *pool, hb_buffer_t **out_buffer);
int hb_buffer_vec_release(hb_buffer_pool_t *pool, hb_buffer_t *buffer);


#endif