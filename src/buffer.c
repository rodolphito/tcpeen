#include "hb/buffer.h"

#include "hb/error.h"
#include "hb/allocator.h"
#include "hb/log.h"


// --------------------------------------------------------------------------------------------------------------
int hb_buffer_read(hb_buffer_t *buffer, uint8_t *out_buffer, size_t len)
{
	assert(buffer);
	assert(out_buffer);
	if (!aws_byte_cursor_read(&buffer->pos, out_buffer, len)) return HB_ERROR;
	return HB_SUCCESS;
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
int hb_buffer_write(hb_buffer_t *buffer, uint8_t *out_buffer, size_t len)
{
	assert(buffer);
	if (!aws_byte_buf_write(&buffer->buf, out_buffer, len)) return HB_ERROR;
	return HB_SUCCESS;
}

// --------------------------------------------------------------------------------------------------------------
int hb_buffer_write_u8(hb_buffer_t *buffer, uint8_t val)
{
	assert(buffer);
	if (!aws_byte_buf_write_u8(&buffer->buf, val)) return HB_ERROR;
	return HB_SUCCESS;
}

// --------------------------------------------------------------------------------------------------------------
int hb_buffer_write_be16(hb_buffer_t *buffer, uint16_t val)
{
	assert(buffer);
	if (!aws_byte_buf_write_be16(&buffer->buf, val)) return HB_ERROR;
	return HB_SUCCESS;
}

// --------------------------------------------------------------------------------------------------------------
int hb_buffer_write_be32(hb_buffer_t *buffer, uint32_t val)
{
	assert(buffer);
	if (!aws_byte_buf_write_be32(&buffer->buf, val)) return HB_ERROR;
	return HB_SUCCESS;
}

// --------------------------------------------------------------------------------------------------------------
int hb_buffer_write_be64(hb_buffer_t *buffer, uint64_t val)
{
	assert(buffer);
	if (!aws_byte_buf_write_be64(&buffer->buf, val)) return HB_ERROR;
	return HB_SUCCESS;
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
int hb_buffer_set_length(hb_buffer_t *buffer, size_t len)
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
size_t hb_buffer_remaining_length(hb_buffer_t *buffer)
{
	assert(buffer);
	return buffer->buf.capacity - buffer->buf.len;
}

// --------------------------------------------------------------------------------------------------------------
void hb_buffer_write_ptr(hb_buffer_t *buffer, uint8_t **out_write_ptr, size_t *out_write_len)
{
	assert(buffer);
	assert(out_write_ptr);
	assert(out_write_len);
	*out_write_ptr = buffer->buf.buffer + buffer->buf.len;
	*out_write_len = buffer->buf.capacity - buffer->buf.len;
}

// --------------------------------------------------------------------------------------------------------------
void hb_buffer_read_ptr(hb_buffer_t *buffer, uint8_t **out_read_ptr, size_t *out_read_len)
{
	assert(buffer);
	assert(out_read_ptr);
	assert(out_read_len);
	*out_read_ptr = buffer->pos.ptr;
	*out_read_len = buffer->buf.len;
}

// --------------------------------------------------------------------------------------------------------------
int hb_buffer_release(hb_buffer_t *buffer)
{
	assert(buffer);
	assert(buffer->pool);
	return hb_buffer_pool_release(buffer->pool, buffer);
}
