#include "hb/test_harness.h"
#include "hb/buffer.h"

#include "aws/common/array_list.h"

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

	ASSERT_SUCCESS(hb_buffer_pool_acquire(&pool, &buffer1));
	ASSERT_SUCCESS(hb_buffer_pool_acquire(&pool, &buffer2));

	ASSERT_TRUE(((struct aws_array_list *)pool.buffer_list_free.priv_impl)->length == (blocks - 2));

	hb_buffer_pool_release(&pool, &buffer1);
	ASSERT_TRUE(((struct aws_array_list *)pool.buffer_list_free.priv_impl)->length == (blocks - 1));

	hb_buffer_release(buffer2);
	ASSERT_TRUE(((struct aws_array_list *)pool.buffer_list_free.priv_impl)->length == blocks);

	ASSERT_SUCCESS(hb_buffer_pool_unlock(&pool));
	hb_buffer_pool_cleanup(&pool);

	return HB_SUCCESS;
}

HB_TEST_CASE(test_buffer_pool, test_buffer)