#include "hb/list_ptr.h"

#include "hb/error.h"
#include "hb/allocator.h"


// --------------------------------------------------------------------------------------------------------------
int hb_list_ptr_setup(hb_list_ptr_t *list, size_t capacity)
{
	HB_GUARD_NULL(list);
	memset(list, 0, sizeof(*list));

	list->capacity = capacity;
	HB_GUARD_NULL(list->data = HB_MEM_ACQUIRE(list->capacity * sizeof(*list->data)));
	
	return HB_SUCCESS;
}

// --------------------------------------------------------------------------------------------------------------
void hb_list_ptr_cleanup(hb_list_ptr_t *list)
{
	if (!list) return;
	HB_MEM_RELEASE(list->data);
}

// --------------------------------------------------------------------------------------------------------------
int hb_list_ptr_push_back(hb_list_ptr_t *list, void *item)
{
	assert(list);
	assert(list->data);
	assert(item);
	assert(list->index < list->capacity);

	HB_GUARD(list->index >= list->capacity);
	list->data[list->index++] = (uintptr_t)item;

	return HB_SUCCESS;
}

// --------------------------------------------------------------------------------------------------------------
int hb_list_ptr_pop_back(hb_list_ptr_t *list, void **item)
{
	assert(list);
	assert(list->data);
	assert(item);
	assert(list->index > 0);

	HB_GUARD(list->index == 0);
	*item = (void *)list->data[--list->index];

	return HB_SUCCESS;
}

// --------------------------------------------------------------------------------------------------------------
uint64_t hb_list_ptr_count(hb_list_ptr_t *list)
{
	assert(list);
	return list->index;
}

// --------------------------------------------------------------------------------------------------------------
void hb_list_ptr_clear(hb_list_ptr_t *list)
{
	assert(list);
	list->index = 0;
}

// --------------------------------------------------------------------------------------------------------------
void *hb_list_ptr_get(hb_list_ptr_t *list, uint64_t index)
{
	assert(list);
	return (void *)list->data[index];
}

// --------------------------------------------------------------------------------------------------------------
void hb_list_ptr_swap(hb_list_ptr_t *list, uint64_t index1, uint64_t index2)
{
	assert(list);

	uintptr_t tmp = list->data[index1];
	list->data[index1] = list->data[index2];
	list->data[index2] = tmp;
}

// --------------------------------------------------------------------------------------------------------------
int hb_list_ptr_remove(hb_list_ptr_t *list, size_t index)
{
	assert(list);

	HB_GUARD(list->index == 0);
	HB_GUARD(index >= list->capacity);

	list->data[index] = list->data[--list->index];

	return HB_SUCCESS;
}
