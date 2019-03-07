#include "hb/map.h"

#include "aws/common/hash_table.h"

#include "hb/error.h"
#include "hb/allocator.h"

// private ------------------------------------------------------------------------------------------------------
uint64_t hb_map_key_hash_fn(const void *key)
{
	return (uint64_t)key;
}

// private ------------------------------------------------------------------------------------------------------
bool hb_map_key_cmp_fn(const void *a, const void *b)
{
	return (a == b);
}

// --------------------------------------------------------------------------------------------------------------
int hb_map_setup(hb_map_t *map, uint64_t capacity)
{
	assert(map);
	memset(map, 0, sizeof(*map));

	map->capacity = capacity;

	struct aws_hash_table *priv = HB_MEM_ACQUIRE(sizeof(*priv));
	HB_GUARD_NULL(priv);

	HB_GUARD_NULL_CLEANUP(aws_hash_table_init(priv, &hb_aws_default_allocator, capacity, hb_map_key_hash_fn, hb_map_key_cmp_fn, NULL, NULL));

	map->priv = priv;
cleanup:
	HB_MEM_RELEASE(priv);
	return HB_ERROR;
}

// --------------------------------------------------------------------------------------------------------------
void hb_map_cleanup(hb_map_t *map)
{
	assert(map);
	aws_hash_table_clean_up((struct aws_hash_table *)map->priv);
	HB_MEM_RELEASE(map->priv);
}

// --------------------------------------------------------------------------------------------------------------
int hb_map_get(hb_map_t *map, void *key, void **out_value)
{
	assert(map);
	assert(map->priv);
	assert(out_value);

	*out_value = NULL;
	struct aws_hash_element *elem = NULL;
	HB_GUARD(aws_hash_table_find((struct aws_hash_table *)map->priv, key, &elem));
	
	HB_GUARD_NULL(elem);
	*out_value = elem->value;

	return HB_SUCCESS;
}

// --------------------------------------------------------------------------------------------------------------
int hb_map_set(hb_map_t *map, void *key, void *value)
{
	assert(map);
	assert(map->priv);
	assert(value);

	int created = 0;
	HB_GUARD(aws_hash_table_put((struct aws_hash_table *)map->priv, key, value, &created));
	HB_GUARD(created == 0);

	return HB_SUCCESS;
}

// --------------------------------------------------------------------------------------------------------------
int hb_map_remove(hb_map_t *map, void *key)
{
	assert(map);
	assert(map->priv);

	int removed = 0;
	HB_GUARD(aws_hash_table_remove((struct aws_hash_table *)map->priv, key, NULL, &removed));
	HB_GUARD(removed == 0);

	return HB_SUCCESS;
}

// --------------------------------------------------------------------------------------------------------------
void hb_map_clear(hb_map_t *map)
{
	assert(map);
	assert(map->priv);
	aws_hash_table_clear((struct aws_hash_table *)map->priv);
}