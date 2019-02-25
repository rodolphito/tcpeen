#include <assert.h>
#include <string.h>

#include "hb/event.h"

int main(void)
{
	hb_event_list_t event_list;
	hb_event_base_t *evt;
	hb_event_client_open_t *evt_open;
	uint32_t count = 0;
	uint32_t id;

	// test that our sizes are correct (since we pass these to managed)
	assert(sizeof(hb_event_base_t) == HB_EVENT_MAX_SIZE);
	assert(sizeof(hb_event_error_t) <= HB_EVENT_MAX_SIZE);
	assert(sizeof(hb_event_client_open_t) <= HB_EVENT_MAX_SIZE);
	assert(sizeof(hb_event_client_close_t) <= HB_EVENT_MAX_SIZE);
	assert(sizeof(hb_event_client_read_t) <= HB_EVENT_MAX_SIZE);


	assert(0 == hb_event_list_setup(&event_list));
	assert(0 == hb_event_list_lock(&event_list));
	assert(0 == hb_event_list_unlock(&event_list));
	hb_event_list_cleanup(&event_list);


	assert(0 == hb_event_list_setup(&event_list));

	assert(0 == hb_event_list_lock(&event_list));
	assert(0 == hb_event_list_pop_swap(&event_list, &evt, &count));
	assert(0 == hb_event_list_unlock(&event_list));
	assert(count == 0);


	assert(0 == hb_event_list_push_back(&event_list, &evt_open));
	id = evt_open->id;
	evt_open->type = HB_EVENT_CLIENT_OPEN;
	evt_open->client_id = 874;
	strcpy(evt_open->host_local, "0.0.0.0:7777");
	strcpy(evt_open->host_remote, "123.456.789.101:65432");

	assert(0 == hb_event_list_lock(&event_list));
	assert(0 == hb_event_list_pop_swap(&event_list, &evt, &count));
	assert(0 == hb_event_list_unlock(&event_list));
	assert(count == 1);
	assert(evt->type == HB_EVENT_CLIENT_OPEN);

	evt_open = (hb_event_client_open_t *)evt;
	assert(0 == strcmp(evt_open->host_local, "0.0.0.0:7777"));
	assert(0 == strcmp(evt_open->host_remote, "123.456.789.101:65432"));

	hb_event_list_cleanup(&event_list);

	return 0;
}
