#ifndef HB_BUFFER_H
#define HB_BUFFER_H

#include <stdint.h>

#include "aws/common/byte_order.h"
#include "aws/common/byte_buf.h"

#include "hb/mutex.h"

// forwards
typedef struct hb_buffer_pool_s hb_buffer_pool_t;

typedef struct hb_buffer_span_s {
	size_t len;
	uint8_t *ptr;
} hb_buffer_span_t;

typedef struct hb_buffer_s {
	hb_buffer_pool_t *pool;
	struct aws_byte_buf buf;
	struct aws_byte_cursor pos;
	size_t capacity;
} hb_buffer_t;


int hb_buffer_setup(hb_buffer_t *buffer, uint8_t *src, size_t capacity);

int hb_buffer_read(hb_buffer_t *buffer, uint8_t *dst_buffer, size_t len);
int hb_buffer_read_seek(hb_buffer_t *buffer, hb_buffer_span_t *span);
int hb_buffer_read_skip(hb_buffer_t *buffer, size_t len);
int hb_buffer_read_u8(hb_buffer_t *buffer, uint8_t *out_val);
int hb_buffer_read_be16(hb_buffer_t *buffer, uint16_t *out_val);
int hb_buffer_read_be32(hb_buffer_t *buffer, uint32_t *out_val);
int hb_buffer_read_be64(hb_buffer_t *buffer, uint64_t *out_val);
int hb_buffer_read_buffer(hb_buffer_t *buffer, hb_buffer_t *dst_buffer, size_t len);
int hb_buffer_write(hb_buffer_t *buffer, uint8_t *src_buffer, size_t len);
int hb_buffer_write_u8(hb_buffer_t *buffer, uint8_t val);
int hb_buffer_write_be16(hb_buffer_t *buffer, uint16_t val);
int hb_buffer_write_be32(hb_buffer_t *buffer, uint32_t val);
int hb_buffer_write_be64(hb_buffer_t *buffer, uint64_t val);
int hb_buffer_write_buffer(hb_buffer_t *buffer, hb_buffer_t *src_buffer, size_t len);

void hb_buffer_read_reset(hb_buffer_t *buffer);
void hb_buffer_write_reset(hb_buffer_t *buffer);
void hb_buffer_reset(hb_buffer_t *buffer);

size_t hb_buffer_length(hb_buffer_t *buffer);
size_t hb_buffer_read_length(hb_buffer_t *buffer);
int hb_buffer_set_length(hb_buffer_t *buffer, size_t len);
int hb_buffer_add_length(hb_buffer_t *buffer, size_t len);
size_t hb_buffer_remaining(hb_buffer_t *buffer);
size_t hb_buffer_capacity(hb_buffer_t *buffer);

void *hb_buffer_write_ptr(hb_buffer_t *buffer);
void *hb_buffer_read_ptr(hb_buffer_t *buffer);

int hb_buffer_release(hb_buffer_t *buffer);


#endif