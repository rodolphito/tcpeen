#include <string.h>

#include "hb/test_harness.h"
#include "hb/list.h"

HB_TEST_CASE_BEGIN(test_list_create)
	hb_list_t list;

	ASSERT_SUCCESS(hb_list_setup(&list, 128, sizeof(uintptr_t)));
	ASSERT_TRUE(list.capacity == 128);
	ASSERT_TRUE(list.item_size == sizeof(uintptr_t));
	hb_list_cleanup(&list);

	ASSERT_SUCCESS(hb_list_setup(&list, 8, 128));
	ASSERT_TRUE(list.capacity == 8);
	ASSERT_TRUE(list.item_size == 128);
	hb_list_cleanup(&list);

	return HB_SUCCESS;
}

HB_TEST_CASE_BEGIN(test_list_ops)
	hb_list_t list;
	uintptr_t p0 = 12983712837;
	uintptr_t p1 = 97829736300;
	uintptr_t out = 0;
	uintptr_t *pout = &out;
	size_t count = 0;
	size_t index = 0;

	ASSERT_SUCCESS(hb_list_setup(&list, 128, sizeof(uintptr_t)));

	ASSERT_SUCCESS(hb_list_count(&list, &count));
	ASSERT_TRUE(count == 0);

	ASSERT_SUCCESS(hb_list_push_back(&list, &p0, &index));
	ASSERT_SUCCESS(hb_list_count(&list, &count));
	ASSERT_TRUE(count == 1);

	ASSERT_SUCCESS(hb_list_push_back(&list, &p1, &index));
	ASSERT_SUCCESS(hb_list_count(&list, &count));
	ASSERT_TRUE(count == 2);

	ASSERT_SUCCESS(hb_list_remove(&list, 0));
	ASSERT_SUCCESS(hb_list_count(&list, &count));
	ASSERT_TRUE(count == 1);

	ASSERT_SUCCESS(hb_list_pop_back(&list, (void **)&pout));
	ASSERT_SUCCESS(hb_list_count(&list, &count));
	ASSERT_TRUE(count == 0);
	ASSERT_TRUE(*pout == p1);

	hb_list_cleanup(&list);

	return HB_SUCCESS;
}

HB_TEST_CASE(test_event_list_create, test_list_create);
HB_TEST_CASE(test_event_list_ops, test_list_ops);
