#include "hb/test_harness.h"
#include "hb/event.h"

#include <string.h>

HB_TEST_CASE_BEGIN(test_event_list)
	hb_event_list_t event_list;
	hb_event_base_t *evt;
	hb_event_client_open_t *evt_open;
	hb_event_client_open_t *evt_open2;
	uint64_t max_size = HB_EVENT_MAX_SIZE;

	// test that our sizes are correct (since we pass these to managed)
	ASSERT_TRUE(sizeof(hb_event_base_t) == max_size);
	ASSERT_TRUE(sizeof(hb_event_error_t) <= max_size);
	ASSERT_TRUE(sizeof(hb_event_client_open_t) <= max_size);
	ASSERT_TRUE(sizeof(hb_event_client_close_t) <= max_size);
	ASSERT_TRUE(sizeof(hb_event_client_read_t) <= max_size);

	ASSERT_SUCCESS(hb_event_list_setup(&event_list, 100));
	hb_event_list_cleanup(&event_list);

	ASSERT_SUCCESS(hb_event_list_setup(&event_list, 100));
	ASSERT_SUCCESS(hb_event_list_free_pop_open(&event_list, &evt_open));
	ASSERT_TRUE(evt_open->type == HB_EVENT_CLIENT_OPEN);
	evt_open->client_id = 874;
	strcpy(evt_open->host_local, "0.0.0.0:7777");
	strcpy(evt_open->host_remote, "123.456.789.101:65432");

	ASSERT_TRUE(hb_queue_spsc_count(&event_list.hb_events_free) == event_list.hb_events_free.capacity - 1);
	ASSERT_TRUE(hb_queue_spsc_count(&event_list.hb_events_ready) == 0);

	ASSERT_SUCCESS(hb_event_list_ready_push(&event_list, evt_open));
	ASSERT_TRUE(hb_queue_spsc_count(&event_list.hb_events_free) == event_list.hb_events_free.capacity - 1);
	ASSERT_TRUE(hb_queue_spsc_count(&event_list.hb_events_ready) == 1);

	ASSERT_SUCCESS(hb_event_list_ready_peek(&event_list, &evt));
	ASSERT_TRUE(evt->type == HB_EVENT_CLIENT_OPEN);
	evt_open2 = (hb_event_client_open_t *)evt;
	ASSERT_SUCCESS(strcmp(evt_open2->host_local, "0.0.0.0:7777"));
	ASSERT_SUCCESS(strcmp(evt_open2->host_remote, "123.456.789.101:65432"));

	ASSERT_TRUE(hb_queue_spsc_count(&event_list.hb_events_free) == event_list.hb_events_free.capacity - 1);
	ASSERT_TRUE(hb_queue_spsc_count(&event_list.hb_events_ready) == 1);

	hb_event_list_ready_pop_cached(&event_list);
	ASSERT_TRUE(hb_queue_spsc_count(&event_list.hb_events_free) == event_list.hb_events_free.capacity - 1);
	ASSERT_TRUE(hb_queue_spsc_count(&event_list.hb_events_ready) == 0);

	ASSERT_SUCCESS(hb_event_list_free_push(&event_list, evt));
	ASSERT_TRUE(hb_queue_spsc_count(&event_list.hb_events_free) == event_list.hb_events_free.capacity);
	ASSERT_TRUE(hb_queue_spsc_count(&event_list.hb_events_ready) == 0);

	hb_event_list_cleanup(&event_list);


	ASSERT_SUCCESS(hb_event_list_setup(&event_list, 100));
	uint64_t start = 0, end = event_list.hb_events_free.capacity;
	for (int i = 0; i < event_list.hb_events_free.capacity; i++) {
		ASSERT_SUCCESS(hb_event_list_free_pop_back(&event_list, &evt));
		ASSERT_SUCCESS(hb_event_list_ready_push(&event_list, evt));

		start++;
		end--;
		ASSERT_TRUE(hb_queue_spsc_count(&event_list.hb_events_free) == end);
		ASSERT_TRUE(hb_queue_spsc_count(&event_list.hb_events_ready) == start);
	}

	ASSERT_TRUE(0 != hb_event_list_ready_push(&event_list, evt));
	ASSERT_TRUE(0 != hb_event_list_free_pop_back(&event_list, &evt));

	hb_event_list_cleanup(&event_list);


	return HB_SUCCESS;
}

HB_TEST_CASE(test_events, test_event_list);
