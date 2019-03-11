#ifndef HB_TCP_CHANNEL_H
#define HB_TCP_CHANNEL_H

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
} tcp_service_client_state_t;

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
	hb_buffer_t *read_buffer;
	hb_endpoint_t endpoint;
	int32_t error_code;
	tcp_service_client_state_t state;
	tcp_channel_read_state_t read_state;
	uint64_t last_msg_id;
} tcp_channel_t;

typedef struct tcp_channel_list_s {
	void *priv;
	tcp_channel_t *client_map;
	size_t clients_max;
	hb_list_ptr_t client_list_free;
	hb_list_ptr_t client_list_open;
} tcp_channel_list_t;


int tcp_channel_list_setup(tcp_channel_list_t *list, size_t clients_max);
int tcp_channel_list_cleanup(tcp_channel_list_t *list);
int tcp_channel_list_open(tcp_channel_list_t *list, tcp_channel_t **out_channel);
int tcp_channel_list_close(tcp_channel_list_t *list, tcp_channel_t *channel);
int tcp_channel_list_reset(tcp_channel_list_t *list);
int tcp_channel_list_get(tcp_channel_list_t *list, uint64_t client_id, tcp_channel_t **out_channel);


#endif