#include "hb/test_harness.h"
#include "hb/buffer.h"

HB_TEST_CASE_BEGIN(test_buffer)
	hb_buffer_pool_t pool;
	hb_buffer_t *buffer1 = NULL;
	hb_buffer_t *buffer2 = NULL;
	size_t blocks = 128;

	ASSERT_SUCCESS(hb_buffer_pool_setup(&pool, blocks, 8));
	ASSERT_SUCCESS(hb_buffer_pool_lock(&pool));
	ASSERT_SUCCESS(hb_buffer_pool_unlock(&pool));
	hb_buffer_pool_cleanup(&pool);

	ASSERT_SUCCESS(hb_buffer_pool_setup(&pool, blocks, 8));
	ASSERT_SUCCESS(hb_buffer_pool_lock(&pool));

	for (int i = 0; i < 1000000; i++) {
		ASSERT_SUCCESS(hb_buffer_pool_acquire(&pool, &buffer1));
		ASSERT_SUCCESS(hb_buffer_pool_acquire(&pool, &buffer2));

		ASSERT_SUCCESS(hb_buffer_pool_release(&pool, buffer1));
		ASSERT_SUCCESS(hb_buffer_release(buffer2));

		buffer1 = NULL;
		buffer2 = NULL;

		ASSERT_SUCCESS(hb_buffer_pool_acquire(&pool, &buffer1));
		ASSERT_SUCCESS(hb_buffer_pool_acquire(&pool, &buffer2));

		ASSERT_SUCCESS(hb_buffer_pool_release(&pool, buffer1));
		ASSERT_SUCCESS(hb_buffer_release(buffer2));
	}

	ASSERT_SUCCESS(hb_buffer_pool_unlock(&pool));
	hb_buffer_pool_cleanup(&pool);

	return HB_SUCCESS;
}

HB_TEST_CASE(test_buffer_pool, test_buffer)