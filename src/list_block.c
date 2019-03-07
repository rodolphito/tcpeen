#include "hb/list_block.h"

#if (hb_list_block_IMPL == hb_list_block_IMPL_AWS_ARRAY_LIST)

#include "aws/common/array_list.h"

#include "hb/error.h"
#include "hb/allocator.h"


// --------------------------------------------------------------------------------------------------------------
int hb_list_block_setup(hb_list_block_t *list, uint64_t capacity, uint64_t item_size)
{
	HB_GUARD_NULL(list);
	memset(list, 0, sizeof(*list));
	
	const uint64_t priv_size = capacity * sizeof(void *);
	HB_GUARD_NULL_CLEANUP(list->priv_data = HB_MEM_ACQUIRE(priv_size));
	memset(list->priv_data, 0, priv_size);

	HB_GUARD_NULL_CLEANUP(list->priv_impl = HB_MEM_ACQUIRE(sizeof(struct aws_array_list)));
	aws_array_list_init_static(list->priv_impl, list->priv_data, capacity, item_size);
	aws_array_list_clear(list->priv_impl);

	list->capacity = capacity;
	list->item_size = item_size;

	return HB_SUCCESS;
	
cleanup:
	HB_MEM_RELEASE(list->priv_impl);
	HB_MEM_RELEASE(list->priv_data);

	return HB_ERROR;
}

// --------------------------------------------------------------------------------------------------------------
void hb_list_block_cleanup(hb_list_block_t *list)
{
	if (!list) return;

	if (list->priv_impl) {
		aws_array_list_clean_up(list->priv_impl);
	}

	HB_MEM_RELEASE(list->priv_impl);
	HB_MEM_RELEASE(list->priv_data);
}

// --------------------------------------------------------------------------------------------------------------
int hb_list_block_push_back(hb_list_block_t *list, void **item, uint64_t *out_index)
{
	assert(list);
	assert(item);
	assert(*item);

	if (out_index) *out_index = aws_array_list_length(list->priv_impl);
	HB_GUARD(aws_array_list_push_back(list->priv_impl, item));

	return HB_SUCCESS;
}

// --------------------------------------------------------------------------------------------------------------
int hb_list_block_pop_back(hb_list_block_t *list, void **item)
{
	assert(list);
	assert(item);

	HB_GUARD(aws_array_list_back(list->priv_impl, item));
	HB_GUARD(aws_array_list_pop_back(list->priv_impl));
	//assert(*item);
	return HB_SUCCESS;
}

// --------------------------------------------------------------------------------------------------------------
int hb_list_block_count(hb_list_block_t *list, uint64_t *out_count)
{
	HB_GUARD_NULL(list);
	HB_GUARD_NULL(out_count);
	*out_count = aws_array_list_length(list->priv_impl);
	return HB_SUCCESS;
}

// --------------------------------------------------------------------------------------------------------------
int hb_list_block_clear(hb_list_block_t *list)
{
	HB_GUARD_NULL(list);
	aws_array_list_clear(list->priv_impl);
	return HB_SUCCESS;
}

// --------------------------------------------------------------------------------------------------------------
int hb_list_block_get(hb_list_block_t *list, uint64_t index, void **out_item)
{
	HB_GUARD_NULL(list);
	HB_GUARD_NULL(out_item);
	return aws_array_list_get_at(list->priv_impl, out_item, index);
}

// --------------------------------------------------------------------------------------------------------------
int hb_list_block_remove(hb_list_block_t *list, uint64_t index)
{
	HB_GUARD_NULL(list);

	const size_t last_idx = aws_array_list_length(list->priv_impl) - 1;
	aws_array_list_swap(list->priv_impl, index, last_idx);
	return aws_array_list_pop_back(list->priv_impl);
}

#endif