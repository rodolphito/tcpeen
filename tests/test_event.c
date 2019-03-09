#include "hb/test_harness.h"
#include "hb/event.h"

#include <string.h>

HB_TEST_CASE_BEGIN(test_event_list)
	hb_event_list_t event_list;
	hb_event_base_t *evt;
	hb_event_client_open_t *evt_open;
	uint64_t count = 0;

	uint64_t max_size = HB_EVENT_MAX_SIZE;

	// test that our sizes are correct (since we pass these to managed)
	ASSERT_TRUE(sizeof(hb_event_base_t) == max_size);
	ASSERT_TRUE(sizeof(hb_event_error_t) <= max_size);
	ASSERT_TRUE(sizeof(hb_event_client_open_t) <= max_size);
	ASSERT_TRUE(sizeof(hb_event_client_close_t) <= max_size);
	ASSERT_TRUE(sizeof(hb_event_client_read_t) <= max_size);


	ASSERT_SUCCESS(hb_event_list_setup(&event_list));
	ASSERT_SUCCESS(hb_event_list_lock(&event_list));
	ASSERT_SUCCESS(hb_event_list_unlock(&event_list));
	hb_event_list_cleanup(&event_list);


	ASSERT_SUCCESS(hb_event_list_setup(&event_list));

	ASSERT_SUCCESS(hb_event_list_lock(&event_list));
	ASSERT_SUCCESS(hb_event_list_pop_swap(&event_list, (hb_event_base_t **)&evt, &count));
	ASSERT_SUCCESS(hb_event_list_unlock(&event_list));
	ASSERT_TRUE(count == 0);


	ASSERT_SUCCESS(hb_event_list_push_back(&event_list, (hb_event_base_t **)&evt_open));
	evt_open->type = HB_EVENT_CLIENT_OPEN;
	evt_open->client_id = 874;
	strcpy(evt_open->host_local, "0.0.0.0:7777");
	strcpy(evt_open->host_remote, "123.456.789.101:65432");

	ASSERT_SUCCESS(hb_event_list_lock(&event_list));
	ASSERT_SUCCESS(hb_event_list_pop_swap(&event_list, (hb_event_base_t **)&evt, &count));
	ASSERT_SUCCESS(hb_event_list_unlock(&event_list));
	ASSERT_TRUE(count == 1);
	ASSERT_TRUE(evt->type == HB_EVENT_CLIENT_OPEN);

	evt_open = (hb_event_client_open_t *)evt;
	ASSERT_SUCCESS(strcmp(evt_open->host_local, "0.0.0.0:7777"));
	ASSERT_SUCCESS(strcmp(evt_open->host_remote, "123.456.789.101:65432"));

	hb_event_list_cleanup(&event_list);

	return HB_SUCCESS;
}

HB_TEST_CASE(test_events, test_event_list);
