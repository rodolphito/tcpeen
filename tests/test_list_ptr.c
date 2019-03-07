#include <string.h>

#include "hb/test_harness.h"
#include "hb/list_ptr.h"

HB_TEST_CASE_BEGIN(list_ptr_create)
	hb_list_ptr_t list;

	ASSERT_SUCCESS(hb_list_ptr_setup(&list, 128));
	ASSERT_TRUE(list.capacity == 128);
	ASSERT_TRUE(list.index == 0);
	ASSERT_TRUE(hb_list_ptr_count(&list) == 0);
	ASSERT_NOT_NULL(list.data);
	hb_list_ptr_cleanup(&list);

	ASSERT_SUCCESS(hb_list_ptr_setup(&list, 8));
	ASSERT_TRUE(list.capacity == 8);
	ASSERT_TRUE(list.index == 0);
	ASSERT_TRUE(hb_list_ptr_count(&list) == 0);
	ASSERT_NOT_NULL(list.data);
	hb_list_ptr_cleanup(&list);

	return HB_SUCCESS;
}

HB_TEST_CASE_BEGIN(list_ptr_ops)
	hb_list_ptr_t list;
	uint64_t data0 = 12983712837;
	uint64_t data1 = 97829736300;
	uint64_t *p0 = &data0;
	uint64_t *p1 = &data1;
	uint64_t *out = NULL;

	ASSERT_SUCCESS(hb_list_ptr_setup(&list, 128));
	ASSERT_TRUE(list.capacity == 128);
	ASSERT_TRUE(list.index == 0);
	ASSERT_TRUE(hb_list_ptr_count(&list) == 0);
	ASSERT_NOT_NULL(list.data);

	ASSERT_SUCCESS(hb_list_ptr_push_back(&list, p0));
	ASSERT_TRUE(hb_list_ptr_count(&list) == 1);

	ASSERT_SUCCESS(hb_list_ptr_push_back(&list, p1));
	ASSERT_TRUE(hb_list_ptr_count(&list) == 2);

	ASSERT_SUCCESS(hb_list_ptr_remove(&list, 0));
	ASSERT_TRUE(hb_list_ptr_count(&list) == 1);

	ASSERT_SUCCESS(hb_list_ptr_pop_back(&list, (void **)&out));
	ASSERT_TRUE(hb_list_ptr_count(&list) == 0);
	ASSERT_TRUE(out == p1);
	ASSERT_TRUE(*out == data1);

	ASSERT_FAILS(hb_list_ptr_pop_back(&list, (void **)&out));

	hb_list_ptr_cleanup(&list);

	return HB_SUCCESS;
}

HB_TEST_CASE(test_list_ptr_create, list_ptr_create);
HB_TEST_CASE(test_list_ptr_ops, list_ptr_ops);
