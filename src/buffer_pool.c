#include "hb/buffer_pool.h"

#include "hb/error.h"
#include "hb/allocator.h"
#include "hb/log.h"
#include "hb/buffer.h"


// --------------------------------------------------------------------------------------------------------------
int hb_buffer_pool_setup(hb_buffer_pool_t *pool, uint64_t block_count, uint64_t block_size)
{
	assert(pool);
	assert(block_count > 0);
	assert(block_size > 0);

	pool->block_count = block_count;
	pool->block_size = block_size;
	pool->blocks_inuse = 0;
	pool->bytes_inuse = 0;
	pool->hb_buffers = NULL;
	pool->allocation = NULL;

	HB_GUARD_NULL_CLEANUP(pool->allocation = HB_MEM_ACQUIRE(block_count * block_size));
	HB_GUARD_NULL_CLEANUP(pool->hb_buffers = HB_MEM_ACQUIRE(block_count * sizeof(*pool->hb_buffers)));
	HB_GUARD_CLEANUP(hb_queue_spsc_setup(&pool->hb_buffers_free, block_count));

	hb_buffer_t *hb_buffer;
	for (uint64_t i = 0; i < block_count; i++) {
		hb_buffer = &pool->hb_buffers[i];

		uint8_t *buf = pool->allocation + ((block_count - 1 - i) * block_size);

		hb_buffer->pool = pool;
		hb_buffer->buf = aws_byte_buf_from_empty_array(buf, block_size);
		hb_buffer->pos = aws_byte_cursor_from_buf(&hb_buffer->buf);

		HB_GUARD_CLEANUP(hb_queue_spsc_push(&pool->hb_buffers_free, hb_buffer));
	}

	return HB_SUCCESS;

cleanup:
	HB_MEM_RELEASE(pool->hb_buffers);
	HB_MEM_RELEASE(pool->allocation);

	return HB_ERROR;
}

// --------------------------------------------------------------------------------------------------------------
void hb_buffer_pool_cleanup(hb_buffer_pool_t *pool)
{
	if (!pool) return;

	pool->blocks_inuse = 0;
	pool->bytes_inuse = 0;

	hb_queue_spsc_cleanup(&pool->hb_buffers_free);
	HB_MEM_RELEASE(pool->hb_buffers);
	HB_MEM_RELEASE(pool->allocation);
}

// --------------------------------------------------------------------------------------------------------------
int hb_buffer_pool_acquire(hb_buffer_pool_t *pool, hb_buffer_t **out_buffer)
{
	int ret;
	assert(pool);
	assert(out_buffer);

	*out_buffer = NULL;

	if (hb_queue_spsc_pop(&pool->hb_buffers_free, (void **)out_buffer) == HB_QUEUE_EMPTY) {
		hb_log_error("hb_buffer_pool_acquire failed due to empty queue");
		return HB_ERROR;
	}
	assert(*out_buffer);

	pool->blocks_inuse++;
	pool->bytes_inuse += pool->block_size;

	hb_buffer_reset(*out_buffer);

	return HB_SUCCESS;
}

// --------------------------------------------------------------------------------------------------------------
int hb_buffer_pool_release(hb_buffer_pool_t *pool, hb_buffer_t *buffer)
{
	assert(pool);
	assert(buffer);

	if (hb_queue_spsc_push(&pool->hb_buffers_free, buffer) == HB_QUEUE_FULL) {
		hb_log_error("hb_buffer_pool_acquire failed due to empty queue");
		return HB_ERROR;
	}

	pool->blocks_inuse--;
	pool->bytes_inuse -= pool->block_size;

	return HB_SUCCESS;
}
