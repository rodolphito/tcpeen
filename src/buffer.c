#include "hb/buffer.h"

#include "hb/error.h"
#include "hb/allocator.h"

#define HB_POOL_NEXT_VALID() if ()

// --------------------------------------------------------------------------------------------------------------
int hb_buffer_pool_setup(hb_buffer_pool_t *pool, uint64_t block_count, uint64_t block_size)
{
	HB_GUARD_NULL(pool);

	memset(pool, 0, sizeof(*pool));

	HB_GUARD(hb_mutex_setup(&pool->mtx));

	HB_GUARD_NULL(pool->allocation = HB_MEM_ACQUIRE(block_count * block_size));
	HB_GUARD_NULL(pool->buffer_list = HB_MEM_ACQUIRE(block_count * sizeof(*pool->buffer_list)));
	HB_GUARD_NULL(pool->meta_list = HB_MEM_ACQUIRE(block_count * sizeof(*pool->meta_list)));

	for (uint64_t i = 0; i < block_count; i++) {
		uint8_t *buf = pool->allocation + (i * block_size);

		pool->meta_list[i].id = i;
		pool->meta_list[i].length = 0;
		pool->meta_list[i].capacity = block_size;
		pool->meta_list[i].state = HB_BUFFER_FREE;

		pool->buffer_list[i].buf = buf;
		pool->buffer_list[i].meta = &pool->meta_list[i];
	}

	pool->block_count = block_count;
	pool->block_size = block_size;
	pool->next = 0;

	return HB_SUCCESS;
}

// --------------------------------------------------------------------------------------------------------------
void hb_buffer_pool_cleanup(hb_buffer_pool_t *pool)
{
	if (!pool) return;

	hb_mutex_cleanup(&pool->mtx);

	HB_MEM_RELEASE(pool->meta_list);
	HB_MEM_RELEASE(pool->buffer_list);
	HB_MEM_RELEASE(pool->allocation);
}

// --------------------------------------------------------------------------------------------------------------
int hb_buffer_pool_lock(hb_buffer_pool_t *pool)
{
	HB_GUARD_NULL(pool);
	return hb_mutex_lock(&pool->mtx);
}

// --------------------------------------------------------------------------------------------------------------
int hb_buffer_pool_unlock(hb_buffer_pool_t *pool)
{
	HB_GUARD_NULL(pool);
	return hb_mutex_unlock(&pool->mtx);
}

// private ------------------------------------------------------------------------------------------------------
void hb_buffer_pool_safe_next(hb_buffer_pool_t *pool)
{
	if (pool->next >= pool->block_count) pool->next = 0;
}

// private ------------------------------------------------------------------------------------------------------
int hb_buffer_pool_find_next(hb_buffer_pool_t *pool)
{
	hb_buffer_pool_safe_next(pool);
	for (uint64_t i = 0; i < pool->block_count; i++) {
		if (pool->meta_list[i].state == HB_BUFFER_FREE) {
			pool->next = i;
			return HB_SUCCESS;
		}
	}

	return HB_ERROR;
}

// --------------------------------------------------------------------------------------------------------------
int hb_buffer_pool_acquire(hb_buffer_pool_t *pool, hb_buffer_t **out_buffer)
{
	HB_GUARD_NULL(pool);
	HB_GUARD_NULL(out_buffer);

	*out_buffer = NULL;

	hb_buffer_pool_safe_next(pool);
	if (pool->meta_list[pool->next].state != HB_BUFFER_FREE) {
		HB_GUARD(hb_buffer_pool_find_next(pool));
	}

	*out_buffer = &pool->buffer_list[pool->next];
	pool->meta_list[pool->next].state = HB_BUFFER_INUSE;
	pool->blocks_inuse++;
	pool->bytes_inuse += pool->block_size;
	
	return HB_SUCCESS;
}

// --------------------------------------------------------------------------------------------------------------
void hb_buffer_pool_release(hb_buffer_pool_t *pool, hb_buffer_t *buffer)
{
	if (pool && buffer) {
		buffer->meta->state = HB_BUFFER_FREE;
		pool->blocks_inuse--;
		pool->bytes_inuse -= pool->block_size;
	}
}
