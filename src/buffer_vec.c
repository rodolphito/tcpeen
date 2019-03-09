#include "hb/buffer.h"

#include "hb/error.h"
#include "hb/allocator.h"


// --------------------------------------------------------------------------------------------------------------
int hb_buffer_setup(hb_buffer_t *buffer)
{
	assert(buffer);
    buffer->buf.len = 0;
    buffer->pos.ptr = buffer->buf.buffer;
    buffer->pos.len = 0;
    return HB_SUCCESS;
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
int hb_buffer_remaining_length(hb_buffer_t *buffer, size_t *out_remaining)
{
	assert(buffer);
    *out_remaining = buffer->buf.capacity - buffer->buf.len;
    return HB_SUCCESS;
}

// --------------------------------------------------------------------------------------------------------------
int hb_buffer_write_ptr(hb_buffer_t *buffer, uint8_t **out_write_ptr, size_t *out_write_len)
{
	assert(buffer);
    *out_write_ptr = buffer->buf.buffer + buffer->buf.len;
    *out_write_len = buffer->buf.capacity - buffer->buf.len;
    return HB_SUCCESS;
}

// --------------------------------------------------------------------------------------------------------------
int hb_buffer_read_ptr(hb_buffer_t *buffer, uint8_t **out_read_ptr, size_t *out_read_len)
{
	assert(buffer);
    *out_read_ptr = buffer->pos.ptr;
    *out_read_len = buffer->pos.len;
    return HB_SUCCESS;
}

// --------------------------------------------------------------------------------------------------------------
int hb_buffer_release(hb_buffer_t *buffer)
{
	assert(buffer);
	assert(buffer->pool);
	return hb_buffer_pool_release(buffer->pool, buffer);
}


// --------------------------------------------------------------------------------------------------------------
int hb_buffer_pool_setup(hb_buffer_pool_t *pool, uint64_t block_count, uint64_t block_size)
{
	int ret = EINVAL;
	assert(pool);
	memset(pool, 0, sizeof(*pool));

	HB_GUARD(hb_mutex_setup(&pool->mtx));

	ret = ENOMEM;
	HB_GUARD_NULL_CLEANUP(pool->allocation = HB_MEM_ACQUIRE(block_count * block_size));
	memset(pool->allocation, 0, block_count * block_size);

	HB_GUARD_NULL_CLEANUP(pool->buffer_array = HB_MEM_ACQUIRE(block_count * sizeof(*pool->buffer_array)));
	memset(pool->buffer_array, 0, block_count * sizeof(*pool->buffer_array));

	HB_GUARD_CLEANUP(ret = hb_list_ptr_setup(&pool->buffer_list_free, block_count));

	hb_buffer_t *hb_buffer;
	for (uint64_t i = 0; i < block_count; i++) {
		hb_buffer = &pool->buffer_array[i];

		uint8_t *buf = pool->allocation + (i * block_size);

		hb_buffer->pool = pool;
		hb_buffer->id = i;
		hb_buffer->buf = aws_byte_buf_from_empty_array(buf, block_size);
		hb_buffer->pos = aws_byte_cursor_from_buf(&hb_buffer->buf);

		assert(hb_buffer);
		HB_GUARD_CLEANUP(ret = hb_list_ptr_push_back(&pool->buffer_list_free, hb_buffer));
	}

	pool->block_count = block_count;
	pool->block_size = block_size;
	pool->blocks_inuse = 0;
	pool->bytes_inuse = 0;

	return HB_SUCCESS;

cleanup:
	hb_mutex_cleanup(&pool->mtx);
	hb_list_ptr_cleanup(&pool->buffer_list_free);
	HB_MEM_RELEASE(pool->buffer_array);
	HB_MEM_RELEASE(pool->allocation);

	return ret;
}

// --------------------------------------------------------------------------------------------------------------
void hb_buffer_pool_cleanup(hb_buffer_pool_t *pool)
{
	if (!pool) return;

	pool->blocks_inuse = 0;
	pool->bytes_inuse = 0;

	hb_mutex_cleanup(&pool->mtx);
	hb_list_ptr_cleanup(&pool->buffer_list_free);
	HB_MEM_RELEASE(pool->buffer_array);
	HB_MEM_RELEASE(pool->allocation);
}

// --------------------------------------------------------------------------------------------------------------
void hb_buffer_pool_heap(hb_buffer_pool_t *pool, void **out_heap, uint64_t *out_block_count, uint64_t *out_block_size)
{
	*out_heap = pool->allocation;
	*out_block_count = pool->block_count;
	*out_block_size = pool->block_size;
}

// --------------------------------------------------------------------------------------------------------------
int hb_buffer_pool_lock(hb_buffer_pool_t *pool)
{
	assert(pool);
	return hb_mutex_lock(&pool->mtx);
}

// --------------------------------------------------------------------------------------------------------------
int hb_buffer_pool_unlock(hb_buffer_pool_t *pool)
{
	assert(pool);
	return hb_mutex_unlock(&pool->mtx);
}

// --------------------------------------------------------------------------------------------------------------
int hb_buffer_pool_acquire(hb_buffer_pool_t *pool, hb_buffer_t **out_buffer)
{
	int ret;
	assert(pool);
	assert(out_buffer);

	HB_GUARD(ret = hb_list_ptr_pop_back(&pool->buffer_list_free, out_buffer));
	assert(*out_buffer);
	pool->blocks_inuse++;
	pool->bytes_inuse += pool->block_size;

	return HB_SUCCESS;
}

// --------------------------------------------------------------------------------------------------------------
int hb_buffer_pool_release(hb_buffer_pool_t *pool, hb_buffer_t *buffer)
{
	assert(pool);
	assert(buffer);
	assert(buffer->id);
	HB_GUARD(hb_list_ptr_push_back(&pool->buffer_list_free, buffer));

	pool->blocks_inuse--;
	pool->bytes_inuse -= pool->block_size;

	return HB_SUCCESS;
}
