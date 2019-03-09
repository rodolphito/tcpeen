#include "hb/event.h"

#include "hb/error.h"
#include "hb/allocator.h"
#include "hb/buffer.h"

#define HB_EVENT_LIST_CAPACITY 1048576

// --------------------------------------------------------------------------------------------------------------
int hb_event_list_setup(hb_event_list_t *list, uint64_t capacity)
{
	assert(list);
	assert(capacity > 0);

	HB_GUARD(hb_queue_spsc_setup(&list->hb_events_free, capacity));
	HB_GUARD(hb_queue_spsc_setup(&list->hb_events_ready, capacity));
	list->capacity = hb_queue_spsc_capacity(&list->hb_events_ready);
	HB_GUARD(list->hb_events = HB_MEM_ACQUIRE(list->capacity * sizeof(*list->hb_events)));

	hb_event_base_t *evt_base = NULL;
	for (uint32_t i = 0; i < list->capacity; i++) {
		evt_base = &list->hb_events[i];
		evt_base->id = i;
		HB_GUARD(hb_queue_spsc_push(&list->hb_events_free, evt_base));
	}

	return HB_SUCCESS;
}

// --------------------------------------------------------------------------------------------------------------
void hb_event_list_cleanup(hb_event_list_t *list)
{
	assert(list);
	hb_queue_spsc_cleanup(&list->hb_events_free);
	hb_queue_spsc_cleanup(&list->hb_events_ready);
	HB_MEM_RELEASE(list->hb_events);
}

// --------------------------------------------------------------------------------------------------------------
int hb_event_list_free_pop(hb_event_list_t *list, hb_event_base_t **out_evt)
{
	assert(list);
	assert(out_evt);

	return hb_queue_spsc_pop(&list->hb_events_free, out_evt);
}

// --------------------------------------------------------------------------------------------------------------
int hb_event_list_free_pop_error(hb_event_list_t *list, hb_event_error_t **out_evt)
{
	assert(list);
	assert(out_evt);

	*out_evt = NULL;
	HB_GUARD(hb_queue_spsc_pop(&list->hb_events_free, (void **)out_evt));
	(*out_evt)->type = HB_EVENT_ERROR;
	
	return HB_SUCCESS;
}

// --------------------------------------------------------------------------------------------------------------
int hb_event_list_free_pop_open(hb_event_list_t *list, hb_event_client_open_t **out_evt)
{
	assert(list);
	assert(out_evt);

	*out_evt = NULL;
	HB_GUARD(hb_queue_spsc_pop(&list->hb_events_free, (void **)out_evt));
	(*out_evt)->type = HB_EVENT_CLIENT_OPEN;

	return HB_SUCCESS;
}

// --------------------------------------------------------------------------------------------------------------
int hb_event_list_free_pop_close(hb_event_list_t *list, hb_event_client_close_t **out_evt)
{
	assert(list);
	assert(out_evt);

	*out_evt = NULL;
	HB_GUARD(hb_queue_spsc_pop(&list->hb_events_free, (void **)out_evt));
	(*out_evt)->type = HB_EVENT_CLIENT_CLOSE;

	return HB_SUCCESS;
}

// --------------------------------------------------------------------------------------------------------------
int hb_event_list_free_pop_read(hb_event_list_t *list, hb_event_client_read_t **out_evt)
{
	assert(list);
	assert(out_evt);

	*out_evt = NULL;
	HB_GUARD(hb_queue_spsc_pop(&list->hb_events_free, (void **)out_evt));
	(*out_evt)->type = HB_EVENT_CLIENT_READ;

	return HB_SUCCESS;
}

// --------------------------------------------------------------------------------------------------------------
int hb_event_list_ready_push(hb_event_list_t *list, void *evt)
{
	assert(list);
	assert(evt);

	return hb_queue_spsc_push(&list->hb_events_ready, evt);
}

// --------------------------------------------------------------------------------------------------------------
int hb_event_list_ready_pop(hb_event_list_t *list, hb_event_base_t **out_evt)
{
	return HB_ERROR;
}

// --------------------------------------------------------------------------------------------------------------
int hb_event_list_ready_pop_all(hb_event_list_t *list, hb_event_base_t **out_evt, uint64_t *out_count)
{
	return HB_ERROR;
}

// --------------------------------------------------------------------------------------------------------------
int hb_event_list_free_push(hb_event_list_t *list, void *evt)
{
	assert(list);
	assert(evt);

	return hb_queue_spsc_push(&list->hb_events_free, evt);
}
