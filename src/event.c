#include "hb/event.h"

#include "hb/error.h"
#include "hb/allocator.h"

#define HB_EVENT_LIST_CAPACITY 1048576

// --------------------------------------------------------------------------------------------------------------
int hb_event_list_setup(hb_event_list_t *list)
{
	HB_GUARD_NULL(list);

	memset(list, 0, sizeof(*list));

	HB_GUARD(hb_mutex_setup(&list->mtx));

	list->capacity = 1048576;
	const size_t halfbytes = list->capacity * sizeof(hb_event_base_t);
	HB_GUARD_NULL(list->priv = HB_MEM_ACQUIRE(halfbytes * 2));
	
	memset(list->priv, 0, halfbytes * 2);
	hb_event_base_t *evt_base = list->priv;
	for (uint32_t i = 0; i < list->capacity; i++) {
		evt_base[0].id = i;
	}

	list->count = 0;
	list->event[0] = &evt_base[0];
	list->event[1] = &evt_base[list->capacity];

	list->swap = 1;
	list->event_front = list->event[0];
	list->event_back = list->event[1];

	return HB_SUCCESS;
}

// --------------------------------------------------------------------------------------------------------------
void hb_event_list_cleanup(hb_event_list_t *list)
{
	if (!list) return;

	hb_mutex_cleanup(&list->mtx);
	HB_MEM_RELEASE(list->priv);
}

// --------------------------------------------------------------------------------------------------------------
int hb_event_list_lock(hb_event_list_t *list)
{
	HB_GUARD_NULL(list);
	return hb_mutex_lock(&list->mtx);
}

// --------------------------------------------------------------------------------------------------------------
int hb_event_list_unlock(hb_event_list_t *list)
{
	HB_GUARD_NULL(list);
	return hb_mutex_unlock(&list->mtx);
}

// --------------------------------------------------------------------------------------------------------------
int hb_event_list_pop_swap(hb_event_list_t *list, void **evt, uint32_t *count)
{
	HB_GUARD_NULL(list);

	const uint8_t old_swap = list->swap;
	const size_t bytes = sizeof(hb_event_base_t) * list->capacity;

	*count = list->count;

	list->swap++;
	list->count = 0;
	list->event_front = list->event[old_swap % 2];
	list->event_back = list->event[list->swap % 2];

	// make old event data unreadable
	memset(list->event_back, 0, bytes);

	*evt = list->event_front;

	return HB_SUCCESS;
}

// --------------------------------------------------------------------------------------------------------------
int hb_event_list_push_back(hb_event_list_t *list, void **evt)
{
	HB_GUARD_NULL(list);
	HB_GUARD_NULL(evt);
	HB_GUARD(list->count >= list->capacity);

	*evt = &list->event_back[list->count++];

	return HB_SUCCESS;
}

