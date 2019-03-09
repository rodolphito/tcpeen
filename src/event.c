#include "hb/event.h"

#include "hb/error.h"
#include "hb/allocator.h"
#include "hb/buffer.h"

#define HB_EVENT_LIST_CAPACITY 1048576

// --------------------------------------------------------------------------------------------------------------
int hb_event_list_setup(hb_event_list_t *list)
{
	assert(list);

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

	list->count_front = 0;
	list->count_back = 0;
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
	assert(list);

	hb_mutex_cleanup(&list->mtx);
	HB_MEM_RELEASE(list->priv);
}

// --------------------------------------------------------------------------------------------------------------
void hb_event_list_heap(hb_event_list_t *list, hb_event_base_t **out_heap, uint64_t *out_block_count, uint64_t *out_block_size)
{
	assert(list);
	assert(out_heap);
	assert(out_block_count);
	assert(out_block_size);

	*out_heap = list->event[0];
	*out_block_count = list->capacity * 2;
	*out_block_size = sizeof(hb_event_base_t);
}

// --------------------------------------------------------------------------------------------------------------
int hb_event_list_lock(hb_event_list_t *list)
{
	assert(list);
	return hb_mutex_lock(&list->mtx);
}

// --------------------------------------------------------------------------------------------------------------
int hb_event_list_unlock(hb_event_list_t *list)
{
	assert(list);
	return hb_mutex_unlock(&list->mtx);
}

// --------------------------------------------------------------------------------------------------------------
int hb_event_list_pop_swap(hb_event_list_t *list, hb_event_base_t **evt, uint64_t *count)
{
	assert(list);
	assert(evt);
	assert(count);

	hb_event_client_read_t *evt_read = NULL;
	for (int i = 0; i < list->count_front; i++) {
		if (list->event_front[i].type == HB_EVENT_CLIENT_READ) {
			evt_read = (hb_event_client_read_t *)&list->event_front[i];
			if (evt_read->hb_buffer) {
				hb_buffer_release(evt_read->hb_buffer);
				evt_read->hb_buffer = NULL;
			}
		}
	}

	const uint8_t old_swap = list->swap;
	list->swap++;
	list->count_front = list->count_back;
	list->count_back = 0;
	list->event_front = list->event[old_swap % 2];
	list->event_back = list->event[list->swap % 2];

	*count = list->count_front;
	*evt = list->event_front;

	return HB_SUCCESS;
}

// --------------------------------------------------------------------------------------------------------------
int hb_event_list_push_back(hb_event_list_t *list, hb_event_base_t **evt)
{
	assert(list);
	assert(evt);
	HB_GUARD(list->count_back >= list->capacity);

	*evt = &list->event_back[list->count_back++];

	return HB_SUCCESS;
}
