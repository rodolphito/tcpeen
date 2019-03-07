#include "hb/tcp_channel.h"

#include "hb/error.h"
#include "hb/allocator.h"

// --------------------------------------------------------------------------------------------------------------
int tcp_channel_list_setup(tcp_channel_list_t *list, size_t clients_max)
{
	HB_GUARD_NULL(list);
	memset(list, 0, sizeof(*list));

	HB_GUARD_NULL_CLEANUP(list->client_map = HB_MEM_ACQUIRE(clients_max * sizeof(*list->client_map)));
	memset(list->client_map, 0, clients_max * sizeof(*list->client_map));

	HB_GUARD_CLEANUP(hb_list_block_setup(&list->client_list_free, clients_max, sizeof(void *)));
	//HB_GUARD_CLEANUP(hb_list_block_setup(&list->client_list_open, clients_max, sizeof(void *)));

	tcp_channel_t *channel;
	for (size_t i = 0; i < clients_max; i++) {
		channel = &list->client_map[i];
		channel->id = i;
		HB_GUARD_CLEANUP(hb_list_block_push_back(&list->client_list_free, &channel, NULL));
	}

	list->clients_max = clients_max;

	return HB_SUCCESS;

cleanup:
	hb_list_block_cleanup(&list->client_list_free);
	//hb_list_block_cleanup(&list->client_list_open);

	HB_MEM_RELEASE(list->client_map);

	return HB_ERROR;
}

// --------------------------------------------------------------------------------------------------------------
int tcp_channel_list_cleanup(tcp_channel_list_t *list)
{
	HB_GUARD_NULL(list);

	hb_list_block_cleanup(&list->client_list_free);
	//hb_list_block_cleanup(&list->client_list_open);

	HB_MEM_RELEASE(list->client_map);

	return HB_ERROR;
}

// --------------------------------------------------------------------------------------------------------------
int tcp_channel_list_open(tcp_channel_list_t *list, tcp_channel_t **out_channel)
{
	int ret;
	size_t index = 0;

	HB_GUARD_NULL(list);
	HB_GUARD_NULL(out_channel);

	HB_GUARD(ret = hb_list_block_pop_back(&list->client_list_free, (void **)out_channel));
	//HB_GUARD(ret = hb_list_block_push_back(&list->client_list_open, out_channel, &index));
	//(*out_channel)->list_id = index;

	return HB_SUCCESS;
}

// --------------------------------------------------------------------------------------------------------------
int tcp_channel_list_close(tcp_channel_list_t *list, tcp_channel_t **channel_ptr)
{
	assert(list);
	assert(channel_ptr);

	tcp_channel_t *channel = *channel_ptr;
	assert(channel);

	int ret;
	//HB_GUARD(ret = hb_list_block_remove(&list->client_list_open, channel->list_id));
	HB_GUARD(ret = hb_list_block_push_back(&list->client_list_free, channel_ptr, NULL));

	return HB_SUCCESS;
}

// --------------------------------------------------------------------------------------------------------------
int tcp_channel_list_reset(tcp_channel_list_t *list)
{
	HB_GUARD_NULL(list);
	memset(list->client_map, 0, list->clients_max * sizeof(*list->client_map));

	HB_GUARD(hb_list_block_clear(&list->client_list_free));
	//HB_GUARD(hb_list_block_clear(&list->client_list_open));

	tcp_channel_t *channel;
	for (size_t i = 0; i < list->clients_max; i++) {
		channel = &list->client_map[i];
		channel->id = i;
		HB_GUARD(hb_list_block_push_back(&list->client_list_free, &channel, NULL));
	}

	return HB_SUCCESS;
}

// --------------------------------------------------------------------------------------------------------------
int tcp_channel_list_get(tcp_channel_list_t *list, uint64_t client_id, tcp_channel_t **out_channel)
{
	assert(list);
	if (client_id >= list->clients_max) return HB_ERROR;
	*out_channel = &list->client_map[client_id];
	return HB_SUCCESS;
}