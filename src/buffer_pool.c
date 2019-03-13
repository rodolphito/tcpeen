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

		HB_GUARD_CLEANUP(hb_buffer_setup(hb_buffer, buf, block_size));
		hb_buffer->pool = pool;

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
int hb_buffer_pool_push(hb_buffer_pool_t *pool, hb_buffer_t *buffer)
{
	assert(pool);
	assert(buffer);

	HB_GUARD(hb_queue_spsc_push(&pool->hb_buffers_free, buffer));

	pool->blocks_inuse--;
	pool->bytes_inuse -= pool->block_size;

	return HB_SUCCESS;
}

// --------------------------------------------------------------------------------------------------------------
int hb_buffer_pool_peek(hb_buffer_pool_t *pool, hb_buffer_t **out_buffer)
{
	assert(pool);
	assert(out_buffer);

	*out_buffer = NULL;

	HB_GUARD(hb_queue_spsc_peek(&pool->hb_buffers_free, (void **)out_buffer));
	assert(*out_buffer);

	hb_buffer_reset(*out_buffer);

	return HB_SUCCESS;
}

// --------------------------------------------------------------------------------------------------------------
int hb_buffer_pool_pop(hb_buffer_pool_t *pool)
{
	assert(pool);

	HB_GUARD(hb_queue_spsc_pop(&pool->hb_buffers_free));

	pool->blocks_inuse++;
	pool->bytes_inuse += pool->block_size;

	return HB_SUCCESS;
}

// --------------------------------------------------------------------------------------------------------------
void hb_buffer_pool_pop_cached(hb_buffer_pool_t *pool)
{
	assert(pool);

	hb_queue_spsc_pop_cached(&pool->hb_buffers_free);

	pool->blocks_inuse++;
	pool->bytes_inuse += pool->block_size;
}

// --------------------------------------------------------------------------------------------------------------
int hb_buffer_pool_pop_back(hb_buffer_pool_t *pool, hb_buffer_t **out_buffer)
{
	assert(pool);
	assert(out_buffer);

	*out_buffer = NULL;

	HB_GUARD(hb_queue_spsc_pop_back(&pool->hb_buffers_free, (void **)out_buffer));
	assert(*out_buffer);

	pool->blocks_inuse++;
	pool->bytes_inuse += pool->block_size;

	hb_buffer_reset(*out_buffer);

	return HB_SUCCESS;
}