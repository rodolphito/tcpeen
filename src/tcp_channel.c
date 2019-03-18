#include "tn/tcp_channel.h"

#include "tn/error.h"
#include "tn/allocator.h"
#include "tn/tcp_service.h"
#include "tn/buffer.h"
#include "tn/buffer_pool.h"


// --------------------------------------------------------------------------------------------------------------
tcp_channel_state_t tcp_channel_state(tcp_channel_t *channel)
{
	assert(channel);
	return channel->state;
}

// --------------------------------------------------------------------------------------------------------------
tcp_channel_read_state_t tcp_channel_read_state(tcp_channel_t *channel)
{
	assert(channel);
	return channel->read_state;
}

// --------------------------------------------------------------------------------------------------------------
int tcp_channel_read_header(tcp_channel_t *channel, uint32_t *out_len)
{
	assert(channel);
	assert(channel->read_buffer);
	TN_GUARD_CLEANUP(tn_buffer_read_be32(channel->read_buffer, out_len));
	channel->next_payload_len = *out_len;
	channel->read_state = TCP_CHANNEL_READ_PAYLOAD;
	return TN_SUCCESS;

cleanup:
	*out_len = 0;
	return TN_ERROR;
}

// --------------------------------------------------------------------------------------------------------------
int tcp_channel_read_payload(tcp_channel_t *channel, tn_buffer_span_t *out_span)
{
	assert(channel && channel->read_buffer);
	assert(out_span);

	out_span->ptr = tn_buffer_read_ptr(channel->read_buffer);
	out_span->len = channel->next_payload_len;
	TN_GUARD_CLEANUP(tn_buffer_read_skip(channel->read_buffer, out_span->len));
	channel->read_state = TCP_CHANNEL_READ_HEADER;
	return TN_SUCCESS;

cleanup:
	out_span->ptr = NULL;
	out_span->len = 0;
	return TN_ERROR;
}

// --------------------------------------------------------------------------------------------------------------
int tcp_channel_buffer_swap(tcp_channel_t *channel)
{
	assert(channel && channel->read_buffer);
	
	tn_buffer_t *prev_buffer = channel->read_buffer;
	TN_GUARD(tn_buffer_pool_pop_back(&channel->service->pool_read, &channel->read_buffer));
	if (tn_buffer_read_length(prev_buffer)) {
		TN_GUARD(tn_buffer_write_buffer(channel->read_buffer, prev_buffer, 0));
	}

	return TN_SUCCESS;
}

// --------------------------------------------------------------------------------------------------------------
int tcp_channel_list_setup(tcp_channel_list_t *list, size_t clients_max)
{
	TN_GUARD_NULL(list);
	memset(list, 0, sizeof(*list));

	TN_GUARD_NULL_CLEANUP(list->client_map = TN_MEM_ACQUIRE(clients_max * sizeof(*list->client_map)));
	memset(list->client_map, 0, clients_max * sizeof(*list->client_map));

	TN_GUARD_CLEANUP(tn_list_ptr_setup(&list->client_list_free, clients_max));
	//TN_GUARD_CLEANUP(tn_list_ptr_setup(&list->client_list_open, clients_max, sizeof(void *)));

	tcp_channel_t *channel;
	for (size_t i = 0; i < clients_max; i++) {
		channel = &list->client_map[i];
		channel->id = i;
		TN_GUARD_CLEANUP(tn_list_ptr_push_back(&list->client_list_free, channel));
	}

	list->clients_max = clients_max;

	return TN_SUCCESS;

cleanup:
	tn_list_ptr_cleanup(&list->client_list_free);
	//tn_list_ptr_cleanup(&list->client_list_open);

	TN_MEM_RELEASE(list->client_map);

	return TN_ERROR;
}

// --------------------------------------------------------------------------------------------------------------
int tcp_channel_list_cleanup(tcp_channel_list_t *list)
{
	TN_GUARD_NULL(list);

	tn_list_ptr_cleanup(&list->client_list_free);
	//tn_list_ptr_cleanup(&list->client_list_open);

	TN_MEM_RELEASE(list->client_map);

	return TN_ERROR;
}

// --------------------------------------------------------------------------------------------------------------
int tcp_channel_list_open(tcp_channel_list_t *list, tcp_channel_t **out_channel)
{
	int ret;
	size_t index = 0;

	TN_GUARD_NULL(list);
	TN_GUARD_NULL(out_channel);

	TN_GUARD(ret = tn_list_ptr_pop_back(&list->client_list_free, (void **)out_channel));
	//TN_GUARD(ret = tn_list_ptr_push_back(&list->client_list_open, out_channel, &index));
	//(*out_channel)->list_id = index;

	return TN_SUCCESS;
}

// --------------------------------------------------------------------------------------------------------------
int tcp_channel_list_close(tcp_channel_list_t *list, tcp_channel_t *channel)
{
	assert(list);
	assert(channel);

	//TN_GUARD(tn_list_ptr_remove(&list->client_list_open, channel->list_id));
	TN_GUARD(tn_list_ptr_push_back(&list->client_list_free, channel));

	return TN_SUCCESS;
}

// --------------------------------------------------------------------------------------------------------------
int tcp_channel_list_reset(tcp_channel_list_t *list)
{
	TN_GUARD_NULL(list);
	memset(list->client_map, 0, list->clients_max * sizeof(*list->client_map));

	tn_list_ptr_clear(&list->client_list_free);
	tn_list_ptr_clear(&list->client_list_open);

	tcp_channel_t *channel;
	for (size_t i = 0; i < list->clients_max; i++) {
		channel = &list->client_map[i];
		channel->id = i;
		TN_GUARD(tn_list_ptr_push_back(&list->client_list_free, channel));
	}

	return TN_SUCCESS;
}

// --------------------------------------------------------------------------------------------------------------
int tcp_channel_list_get(tcp_channel_list_t *list, uint64_t client_id, tcp_channel_t **out_channel)
{
	assert(list);
	assert(out_channel);
	*out_channel = NULL;
	if (client_id >= list->clients_max) return TN_ERROR;
	*out_channel = &list->client_map[client_id];
	return TN_SUCCESS;
}