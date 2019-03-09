#ifndef HB_BUFFER_H
#define HB_BUFFER_H

#include <stdint.h>

#include "aws/common/byte_order.h"
#include "aws/common/byte_buf.h"

#include "hb/mutex.h"
#include "hb/list_ptr.h"

// forwards
typedef struct hb_buffer_pool_s hb_buffer_pool_t;

typedef struct hb_buffer_s {
	void *priv;
	uint64_t id;
	hb_buffer_pool_t *pool;
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
	hb_list_ptr_t buffer_list_free;
	hb_mutex_t mtx;
} hb_buffer_pool_t;


int hb_buffer_setup(hb_buffer_t *buffer);
int hb_buffer_set_length(hb_buffer_t *buffer, size_t len);

int hb_buffer_read(hb_buffer_t *buffer, uint8_t *out_buffer, size_t len);
int hb_buffer_read_u8(hb_buffer_t *buffer, uint8_t *out_val);
int hb_buffer_read_be16(hb_buffer_t *buffer, uint16_t *out_val);
int hb_buffer_read_be32(hb_buffer_t *buffer, uint32_t *out_val);
int hb_buffer_read_be64(hb_buffer_t *buffer, uint64_t *out_val);
int hb_buffer_write(hb_buffer_t *buffer, uint8_t *out_buffer, size_t len);
int hb_buffer_write_u8(hb_buffer_t *buffer, uint8_t val);
int hb_buffer_write_be16(hb_buffer_t *buffer, uint16_t val);
int hb_buffer_write_be32(hb_buffer_t *buffer, uint32_t val);
int hb_buffer_write_be64(hb_buffer_t *buffer, uint64_t val);

void hb_buffer_read_reset(hb_buffer_t *buffer);
void hb_buffer_write_reset(hb_buffer_t *buffer);
void hb_buffer_reset(hb_buffer_t *buffer);

size_t hb_buffer_length(hb_buffer_t *buffer);
int hb_buffer_set_length(hb_buffer_t *buffer, size_t len);
size_t hb_buffer_remaining_length(hb_buffer_t *buffer);

void hb_buffer_write_ptr(hb_buffer_t *buffer, uint8_t **out_write_ptr, size_t *out_write_len);
void hb_buffer_read_ptr(hb_buffer_t *buffer, uint8_t **out_read_ptr, size_t *out_read_len);

int hb_buffer_release(hb_buffer_t *buffer);


int hb_buffer_pool_setup(hb_buffer_pool_t *pool, uint64_t block_count, uint64_t block_size);
void hb_buffer_pool_cleanup(hb_buffer_pool_t *pool);
void hb_buffer_pool_heap(hb_buffer_pool_t *pool, void **out_heap, uint64_t *out_block_count, uint64_t *out_block_size);

int hb_buffer_pool_lock(hb_buffer_pool_t *pool);
int hb_buffer_pool_unlock(hb_buffer_pool_t *pool);

int hb_buffer_pool_acquire(hb_buffer_pool_t *pool, hb_buffer_t **out_buffer);
int hb_buffer_pool_release(hb_buffer_pool_t *pool, hb_buffer_t *buffer);


#endif