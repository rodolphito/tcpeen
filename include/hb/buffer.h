#ifndef HB_BUFFER_H
#define HB_BUFFER_H

#include <stdint.h>

#include "aws/common/byte_order.h"
#include "aws/common/byte_buf.h"

#include "hb/thread.h"
#include "hb/list.h"


typedef struct hb_buffer_s {
	struct aws_byte_buf buf;
	struct aws_byte_cursor pos;
} hb_buffer_t;


typedef struct hb_buffer_pool_s {
	uint64_t blocks_inuse;
	uint64_t bytes_inuse;
	uint64_t block_size;
	uint64_t block_count;
	hb_buffer_t *buffer_array;
	uint8_t *allocation;
	hb_list_t buffer_list_free;
	hb_mutex_t mtx;
} hb_buffer_pool_t;


int hb_buffer_setup(hb_buffer_t *buffer);
int hb_buffer_add_length(hb_buffer_t *buffer, size_t len);
int hb_buffer_remaining_length(hb_buffer_t *buffer, size_t *out_remaining);
int hb_buffer_write_ptr(hb_buffer_t *buffer, uint8_t *out_write_ptr, size_t *out_write_len);
int hb_buffer_read_ptr(hb_buffer_t *buffer, uint8_t *out_read_ptr, size_t *out_read_len);


int hb_buffer_pool_setup(hb_buffer_pool_t *pool, uint64_t block_count, uint64_t block_size);
void hb_buffer_pool_cleanup(hb_buffer_pool_t *pool);

int hb_buffer_pool_lock(hb_buffer_pool_t *pool);
int hb_buffer_pool_unlock(hb_buffer_pool_t *pool);

int hb_buffer_pool_acquire(hb_buffer_pool_t *pool, hb_buffer_t **out_buffer);
int hb_buffer_pool_release(hb_buffer_pool_t *pool, hb_buffer_t *buffer);


#endif