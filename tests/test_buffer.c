#include <assert.h>

#include "hb/buffer.h"

int main(void)
{
	hb_buffer_pool_t pool;
	hb_buffer_t *buffer1 = NULL;
	hb_buffer_t *buffer2 = NULL;

	assert(0 == hb_buffer_pool_setup(&pool, 128, 8));
	assert(0 == hb_buffer_pool_lock(&pool));
	assert(0 == hb_buffer_pool_unlock(&pool));
	hb_buffer_pool_cleanup(&pool);


	assert(0 == hb_buffer_pool_setup(&pool, 128, 8));
	assert(0 == hb_buffer_pool_lock(&pool));

	assert(0 == hb_buffer_pool_acquire(&pool, &buffer1));
	assert(buffer1->meta->state == HB_BUFFER_INUSE);
	assert(buffer1->meta->capacity == 8);
	assert(buffer1->meta->length == 0);
	assert(buffer1->meta->id == 0);
	assert(pool.blocks_inuse == 1);
	assert(pool.bytes_inuse == 8);

	assert(0 == hb_buffer_pool_acquire(&pool, &buffer2));
	assert(buffer2->meta->state == HB_BUFFER_INUSE);
	assert(buffer2->meta->capacity == 8);
	assert(buffer2->meta->length == 0);
	assert(buffer2->meta->id == 1);
	assert(pool.blocks_inuse == 2);
	assert(pool.bytes_inuse == 16);

	hb_buffer_pool_release(&pool, buffer1);
	assert(buffer1->meta->state == HB_BUFFER_FREE);
	assert(pool.meta_list[0].state == HB_BUFFER_FREE);
	assert(pool.blocks_inuse == 1);
	assert(pool.bytes_inuse == 8);

	assert(0 == hb_buffer_pool_unlock(&pool));
	hb_buffer_pool_cleanup(&pool);

	return 0;
}