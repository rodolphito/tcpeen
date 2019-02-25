#ifndef HB_BUFFER_H
#define HB_BUFFER_H

#include <stdint.h>

#include "hb/thread.h"

typedef enum hb_buffer_state_e {
	HB_BUFFER_FREE,
	HB_BUFFER_INUSE,
} hb_buffer_state_t;

typedef struct hb_buffer_meta_s {
	uint64_t id;
	uint64_t length;
	uint64_t capacity;
	uint8_t state;
} hb_buffer_meta_t;

typedef struct hb_buffer_s {
	hb_buffer_meta_t *meta;
	uint8_t *buf;
} hb_buffer_t;

typedef struct hb_buffer_pool_s {
	uint64_t blocks_inuse;
	uint64_t bytes_inuse;
	uint64_t block_size;
	uint64_t block_count;
	uint64_t next;
	uint8_t *allocation;
	hb_buffer_t *buffer_list;
	hb_buffer_meta_t *meta_list;
	hb_mutex_t mtx;
} hb_buffer_pool_t;


int hb_buffer_pool_setup(hb_buffer_pool_t *pool, uint64_t block_count, uint64_t block_size);
void hb_buffer_pool_cleanup(hb_buffer_pool_t *pool);

int hb_buffer_pool_lock(hb_buffer_pool_t *pool);
int hb_buffer_pool_unlock(hb_buffer_pool_t *pool);

// everything below here requires the buffer pool to be locked
int hb_buffer_pool_acquire(hb_buffer_pool_t *pool, hb_buffer_t **out_buffer);
void hb_buffer_pool_release(hb_buffer_pool_t *pool, hb_buffer_t *buffer);


#endif