#ifndef HB_BUFFER_POOL_H
#define HB_BUFFER_POOL_H

#include <stdint.h>

#include "aws/common/byte_order.h"
#include "aws/common/byte_buf.h"

#include "hb/mutex.h"
#include "hb/queue_spsc.h"


typedef struct hb_buffer_s hb_buffer_t;


typedef struct hb_buffer_pool_s {
	uint64_t blocks_inuse;
	uint64_t bytes_inuse;
	uint64_t block_size;
	uint64_t block_count;
	hb_buffer_t *hb_buffers;
	uint8_t *allocation;
	hb_queue_spsc_t hb_buffers_free;
} hb_buffer_pool_t;


int hb_buffer_pool_setup(hb_buffer_pool_t *pool, uint64_t block_count, uint64_t block_size);
void hb_buffer_pool_cleanup(hb_buffer_pool_t *pool);
void hb_buffer_pool_heap(hb_buffer_pool_t *pool, void **out_heap, uint64_t *out_block_count, uint64_t *out_block_size);

int hb_buffer_pool_lock(hb_buffer_pool_t *pool);
int hb_buffer_pool_unlock(hb_buffer_pool_t *pool);

int hb_buffer_pool_acquire(hb_buffer_pool_t *pool, hb_buffer_t **out_buffer);
int hb_buffer_pool_release(hb_buffer_pool_t *pool, hb_buffer_t *buffer);


#endif