#include <string.h>

#include "hb/test_harness.h"
#include "hb/list_block.h"

HB_TEST_CASE_BEGIN(list_block_create)
	hb_list_block_t list;

	ASSERT_SUCCESS(hb_list_block_setup(&list, 128, sizeof(uintptr_t)));
	ASSERT_TRUE(list.capacity == 128);
	ASSERT_TRUE(list.item_size == sizeof(uintptr_t));
	hb_list_block_cleanup(&list);

	ASSERT_SUCCESS(hb_list_block_setup(&list, 8, 128));
	ASSERT_TRUE(list.capacity == 8);
	ASSERT_TRUE(list.item_size == 128);
	hb_list_block_cleanup(&list);

	return HB_SUCCESS;
}

HB_TEST_CASE_BEGIN(list_block_ops)
	hb_list_block_t list;
	uint64_t data0 = 12983712837;
	uint64_t data1 = 97829736300;
	uint64_t *p0 = &data0;
	uint64_t *p1 = &data1;
	uint64_t *out = NULL;
	size_t count = 0;
	size_t index = 0;

	ASSERT_SUCCESS(hb_list_block_setup(&list, 128, sizeof(uintptr_t)));

	ASSERT_SUCCESS(hb_list_block_count(&list, &count));
	ASSERT_TRUE(count == 0);

	ASSERT_SUCCESS(hb_list_block_push_back(&list, &p0, &index));
	ASSERT_SUCCESS(hb_list_block_count(&list, &count));
	ASSERT_TRUE(count == 1);

	ASSERT_SUCCESS(hb_list_block_push_back(&list, &p1, &index));
	ASSERT_SUCCESS(hb_list_block_count(&list, &count));
	ASSERT_TRUE(count == 2);

	ASSERT_SUCCESS(hb_list_block_remove(&list, 0));
	ASSERT_SUCCESS(hb_list_block_count(&list, &count));
	ASSERT_TRUE(count == 1);

	ASSERT_SUCCESS(hb_list_block_pop_back(&list, &out));
	ASSERT_SUCCESS(hb_list_block_count(&list, &count));
	ASSERT_TRUE(count == 0);
	ASSERT_TRUE(out == p1);

	hb_list_block_cleanup(&list);

	return HB_SUCCESS;
}

HB_TEST_CASE(test_list_block_create, list_block_create);
HB_TEST_CASE(test_list_block_ops, list_block_ops);
