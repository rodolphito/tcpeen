#include "hb/test_harness.h"
#include "hb/buffer.h"

HB_TEST_CASE_BEGIN(test_buffer)
	hb_buffer_pool_t pool;
	hb_buffer_t *buffer1 = NULL;
	hb_buffer_t *buffer2 = NULL;

	ASSERT_SUCCESS(hb_buffer_pool_setup(&pool, 128, 8));
	ASSERT_SUCCESS(hb_buffer_pool_lock(&pool));
	ASSERT_SUCCESS(hb_buffer_pool_unlock(&pool));
	hb_buffer_pool_cleanup(&pool);


	ASSERT_SUCCESS(hb_buffer_pool_setup(&pool, 128, 8));
	ASSERT_SUCCESS(hb_buffer_pool_lock(&pool));

	ASSERT_SUCCESS(hb_buffer_pool_acquire(&pool, &buffer1));
	ASSERT_TRUE(buffer1->meta->state == HB_BUFFER_INUSE);
	ASSERT_TRUE(buffer1->meta->capacity == 8);
	ASSERT_TRUE(buffer1->meta->length == 0);
	ASSERT_TRUE(buffer1->meta->id == 0);
	ASSERT_TRUE(pool.blocks_inuse == 1);
	ASSERT_TRUE(pool.bytes_inuse == 8);

	ASSERT_SUCCESS(hb_buffer_pool_acquire(&pool, &buffer2));
	ASSERT_TRUE(buffer2->meta->state == HB_BUFFER_INUSE);
	ASSERT_TRUE(buffer2->meta->capacity == 8);
	ASSERT_TRUE(buffer2->meta->length == 0);
	ASSERT_TRUE(buffer2->meta->id == 1);
	ASSERT_TRUE(pool.blocks_inuse == 2);
	ASSERT_TRUE(pool.bytes_inuse == 16);

	hb_buffer_pool_release(&pool, buffer1);
	ASSERT_TRUE(buffer1->meta->state == HB_BUFFER_FREE);
	ASSERT_TRUE(pool.meta_list[0].state == HB_BUFFER_FREE);
	ASSERT_TRUE(pool.blocks_inuse == 1);
	ASSERT_TRUE(pool.bytes_inuse == 8);

	ASSERT_SUCCESS(hb_buffer_pool_unlock(&pool));
	hb_buffer_pool_cleanup(&pool);

	return HB_SUCCESS;
}

HB_TEST_CASE(test_buffer_pool, test_buffer)