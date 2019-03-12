#include "hb/buffer.h"

#include "hb/error.h"
#include "hb/allocator.h"
#include "hb/log.h"
#include "hb/buffer_pool.h"


// --------------------------------------------------------------------------------------------------------------
int hb_buffer_setup(hb_buffer_t *buffer, uint8_t *src, size_t capacity)
{
	assert(buffer && src && capacity);

	buffer->pool = NULL;
	buffer->buf = aws_byte_buf_from_empty_array(src, capacity);
	buffer->pos = aws_byte_cursor_from_buf(&buffer->buf);
	return HB_SUCCESS;
}

// --------------------------------------------------------------------------------------------------------------
int hb_buffer_read(hb_buffer_t *buffer, uint8_t *dst_buffer, size_t len)
{
	assert(buffer);
	assert(dst_buffer);
	if (!aws_byte_cursor_read(&buffer->pos, dst_buffer, len)) return HB_ERROR;
	return HB_SUCCESS;
}

// --------------------------------------------------------------------------------------------------------------
int hb_buffer_read_seek(hb_buffer_t *buffer, hb_buffer_span_t *span)
{
	assert(buffer);
	assert(span && span->ptr && span->len);
	assert((span->ptr + span->len) < (buffer->buf.buffer + buffer->buf.capacity));
	buffer->pos.ptr = span->ptr;
	buffer->pos.len = span->len;
	return HB_SUCCESS;
}

// --------------------------------------------------------------------------------------------------------------
int hb_buffer_read_skip(hb_buffer_t *buffer, size_t len)
{
	assert(buffer);
	struct aws_byte_cursor span = aws_byte_cursor_advance(&buffer->pos, len);
	if (span.ptr && span.len) {
		return HB_SUCCESS;
	}
	return HB_ERROR;
}

// --------------------------------------------------------------------------------------------------------------
int hb_buffer_read_u8(hb_buffer_t *buffer, uint8_t *out_val)
{
	assert(buffer);
	assert(out_val);
	if (!aws_byte_cursor_read_u8(&buffer->pos, out_val)) return HB_ERROR;
	return HB_SUCCESS;
}

// --------------------------------------------------------------------------------------------------------------
int hb_buffer_read_be16(hb_buffer_t *buffer, uint16_t *out_val)
{
	assert(buffer);
	assert(out_val);
	if (!aws_byte_cursor_read_be16(&buffer->pos, out_val)) return HB_ERROR;
	return HB_SUCCESS;
}

// --------------------------------------------------------------------------------------------------------------
int hb_buffer_read_be32(hb_buffer_t *buffer, uint32_t *out_val)
{
	assert(buffer);
	assert(out_val);
	if (!aws_byte_cursor_read_be32(&buffer->pos, out_val)) return HB_ERROR;
	return HB_SUCCESS;
}

// --------------------------------------------------------------------------------------------------------------
int hb_buffer_read_be64(hb_buffer_t *buffer, uint64_t *out_val)
{
	assert(buffer);
	assert(out_val);
	if (!aws_byte_cursor_read_be64(&buffer->pos, out_val)) return HB_ERROR;
	return HB_SUCCESS;
}

// --------------------------------------------------------------------------------------------------------------
int hb_buffer_read_buffer(hb_buffer_t *buffer, hb_buffer_t *dst_buffer, size_t len)
{
	assert(buffer && dst_buffer);
	
	if (!len) {
		len = buffer->pos.len;
	} else if (len > buffer->pos.len) {
		return HB_ERROR;
	}

	HB_GUARD(hb_buffer_write(dst_buffer, buffer->pos.ptr, len));
	aws_byte_cursor_advance(&buffer->pos, len);

	return HB_SUCCESS;
}

// --------------------------------------------------------------------------------------------------------------
int hb_buffer_write(hb_buffer_t *buffer, uint8_t *src_buffer, size_t len)
{
	assert(buffer);
	if (!aws_byte_buf_write(&buffer->buf, src_buffer, len)) return HB_ERROR;
	buffer->pos.len += len;
	return HB_SUCCESS;
}

// --------------------------------------------------------------------------------------------------------------
int hb_buffer_write_u8(hb_buffer_t *buffer, uint8_t val)
{
	assert(buffer);
	if (!aws_byte_buf_write_u8(&buffer->buf, val)) return HB_ERROR;
	buffer->pos.len += sizeof(val);
	return HB_SUCCESS;
}

// --------------------------------------------------------------------------------------------------------------
int hb_buffer_write_be16(hb_buffer_t *buffer, uint16_t val)
{
	assert(buffer);
	if (!aws_byte_buf_write_be16(&buffer->buf, val)) return HB_ERROR;
	buffer->pos.len += sizeof(val);
	return HB_SUCCESS;
}

// --------------------------------------------------------------------------------------------------------------
int hb_buffer_write_be32(hb_buffer_t *buffer, uint32_t val)
{
	assert(buffer);
	if (!aws_byte_buf_write_be32(&buffer->buf, val)) return HB_ERROR;
	buffer->pos.len += sizeof(val);
	return HB_SUCCESS;
}

// --------------------------------------------------------------------------------------------------------------
int hb_buffer_write_be64(hb_buffer_t *buffer, uint64_t val)
{
	assert(buffer);
	if (!aws_byte_buf_write_be64(&buffer->buf, val)) return HB_ERROR;
	buffer->pos.len += sizeof(val);
	return HB_SUCCESS;
}

// --------------------------------------------------------------------------------------------------------------
int hb_buffer_write_buffer(hb_buffer_t *buffer, hb_buffer_t *src_buffer, size_t len)
{
	return hb_buffer_read_buffer(src_buffer, buffer, len);
}

// --------------------------------------------------------------------------------------------------------------
void hb_buffer_read_reset(hb_buffer_t *buffer)
{
	assert(buffer);
	buffer->pos.ptr = buffer->buf.buffer;
	buffer->pos.len = buffer->buf.len;
}

// --------------------------------------------------------------------------------------------------------------
void hb_buffer_write_reset(hb_buffer_t *buffer)
{
	assert(buffer);
	buffer->buf.len = 0;
	hb_buffer_read_reset(buffer);
}

// --------------------------------------------------------------------------------------------------------------
void hb_buffer_reset(hb_buffer_t *buffer)
{
	hb_buffer_write_reset(buffer);
}

// --------------------------------------------------------------------------------------------------------------
size_t hb_buffer_length(hb_buffer_t *buffer)
{
	assert(buffer);
	return buffer->buf.len;
}

// --------------------------------------------------------------------------------------------------------------
size_t hb_buffer_read_length(hb_buffer_t *buffer)
{
	assert(buffer);
	return buffer->pos.len;
}

// --------------------------------------------------------------------------------------------------------------
int hb_buffer_set_length(hb_buffer_t *buffer, size_t len)
{
	assert(buffer);
	if (buffer->buf.capacity - buffer->buf.len >= len) {
		buffer->buf.len = len;
		buffer->pos.len = len;
		return HB_SUCCESS;
	}
	return HB_ERROR;
}

// --------------------------------------------------------------------------------------------------------------
int hb_buffer_add_length(hb_buffer_t *buffer, size_t len)
{
	assert(buffer);
	if (buffer->buf.capacity - buffer->buf.len >= len) {
		buffer->buf.len += len;
		buffer->pos.len += len;
		return HB_SUCCESS;
	}
	return HB_ERROR;
}

// --------------------------------------------------------------------------------------------------------------
size_t hb_buffer_remaining(hb_buffer_t *buffer)
{
	assert(buffer);
	return buffer->buf.capacity - buffer->buf.len;
}

// --------------------------------------------------------------------------------------------------------------
size_t hb_buffer_capacity(hb_buffer_t *buffer)
{
	assert(buffer);
	return buffer->pool->block_size;
}

// --------------------------------------------------------------------------------------------------------------
void *hb_buffer_write_ptr(hb_buffer_t *buffer)
{
	assert(buffer);
	return buffer->buf.buffer + buffer->buf.len;
}

// --------------------------------------------------------------------------------------------------------------
void *hb_buffer_read_ptr(hb_buffer_t *buffer)
{
	assert(buffer);
	return buffer->pos.ptr;
}

// --------------------------------------------------------------------------------------------------------------
int hb_buffer_release(hb_buffer_t *buffer)
{
	assert(buffer);
	HB_GUARD_NULL(buffer->pool);

	return hb_buffer_pool_push(buffer->pool, buffer);
}
