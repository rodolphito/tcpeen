#ifndef TN_TCP_CHANNEL_H
#define TN_TCP_CHANNEL_H

#include <stdint.h>

#include "hb/list_ptr.h"
#include "hb/endpoint.h"
#include "hb/buffer.h"

// forwards
typedef struct tcp_service_s tcp_service_t;
typedef struct tcp_buffer_s tcp_buffer_t;


typedef enum tcp_channel_state_e {
	TCP_CHANNEL_NEW,
	TCP_CHANNEL_OPEN,
	TCP_CHANNEL_CLOSING,
	TCP_CHANNEL_CLOSED,
	TCP_CHANNEL_ERROR,
	TCP_CHANNEL_INVALID,
} tcp_channel_state_t;

typedef enum tcp_channel_read_state_e {
	TCP_CHANNEL_READ_NONE,
	TCP_CHANNEL_READ_HEADER,
	TCP_CHANNEL_READ_PAYLOAD,
	TCP_CHANNEL_READ_ERROR,
	TCP_CHANNEL_READ_INVALID,
} tcp_channel_read_state_t;

typedef struct tcp_channel_s {
	void *priv;
	uint64_t id;
	uint64_t list_id;
	tcp_service_t *service;
	tn_buffer_t *read_buffer;
	tn_endpoint_t endpoint;
	int32_t error_code;
	tcp_channel_state_t state;
	tcp_channel_read_state_t read_state;
	uint64_t next_payload_len;
	uint64_t last_msg_id;
} tcp_channel_t;

typedef struct tcp_channel_list_s {
	void *priv;
	tcp_channel_t *client_map;
	size_t clients_max;
	tn_list_ptr_t client_list_free;
	tn_list_ptr_t client_list_open;
} tcp_channel_list_t;


tcp_channel_state_t tcp_channel_state(tcp_channel_t *channel);
tcp_channel_read_state_t tcp_channel_read_state(tcp_channel_t *channel);
int tcp_channel_read_header(tcp_channel_t *channel, uint32_t *out_len);
int tcp_channel_read_payload(tcp_channel_t *channel, tn_buffer_span_t *out_span);
int tcp_channel_buffer_swap(tcp_channel_t *channel);

int tcp_channel_list_setup(tcp_channel_list_t *list, size_t clients_max);
int tcp_channel_list_cleanup(tcp_channel_list_t *list);
int tcp_channel_list_open(tcp_channel_list_t *list, tcp_channel_t **out_channel);
int tcp_channel_list_close(tcp_channel_list_t *list, tcp_channel_t *channel);
int tcp_channel_list_reset(tcp_channel_list_t *list);
int tcp_channel_list_get(tcp_channel_list_t *list, uint64_t client_id, tcp_channel_t **out_channel);


#endif