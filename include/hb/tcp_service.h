#ifndef HB_TCP_SERVICE_H
#define HB_TCP_SERVICE_H

#include <stdint.h>

#include "hb/tcp_channel.h"
#include "hb/endpoint.h"
#include "hb/buffer.h"
#include "hb/event.h"
#include "hb/list.h"

#define HB_SERVICE_MAX_CLIENTS 10000
#define HB_SERVICE_MAX_READ 65535


typedef enum tcp_service_state_e {
	TCP_SERVICE_NEW, 
	TCP_SERVICE_STARTING,
	TCP_SERVICE_STARTED,
	TCP_SERVICE_STOPPING,
	TCP_SERVICE_STOPPED,
	TCP_SERVICE_ERROR,
	TCP_SERVICE_INVALID,
} tcp_service_state_t;

typedef struct tcp_service_s {
	void *priv;
	hb_endpoint_t host_listen;
	tcp_channel_list_t channel_list;
	hb_buffer_pool_t pool;
	hb_event_list_t events;
	hb_mutex_t mtx_io;
	uint8_t state;
} tcp_service_t;


int tcp_service_start(tcp_service_t *service, const char *ipstr, uint16_t port);
int tcp_service_stop(tcp_service_t *service);
int tcp_service_lock(tcp_service_t *service);
int tcp_service_unlock(tcp_service_t *service);
int tcp_service_update(tcp_service_t *service, uintptr_t *evt_base, uint32_t *count);


#endif