#ifndef HB_TCP_SERVICE_H
#define HB_TCP_SERVICE_H

#include <stdint.h>

#include "hb/thread.h"
#include "hb/tcp_channel.h"
#include "hb/endpoint.h"
#include "hb/buffer_pool.h"
#include "hb/event.h"
#include "hb/queue_spsc.h"


#define HB_SERVICE_MAX_CLIENTS 1000
#define HB_SERVICE_MAX_READ 65535


// forwards
typedef struct tcp_service_write_req_s tcp_service_write_req_t;


typedef enum tcp_service_state_e {
	TCP_SERVICE_NEW, 
	TCP_SERVICE_STARTING,
	TCP_SERVICE_STARTED,
	TCP_SERVICE_STOPPING,
	TCP_SERVICE_STOPPED,
	TCP_SERVICE_ERROR,
	TCP_SERVICE_INVALID,
} tcp_service_state_t;

typedef struct tcp_service_stats_s {
	uint64_t recv_count;
	uint64_t recv_bytes;
	uint64_t send_count;
	uint64_t send_bytes;
} tcp_service_stats_t;

typedef struct tcp_service_s {
	void *priv;
	hb_thread_t thread_io;
	hb_endpoint_t host_listen;
	tcp_channel_list_t channel_list;
	hb_buffer_pool_t pool;
	hb_event_list_t events;
	hb_event_base_t **event_updates;
	uint64_t buffer_count;
	tcp_service_write_req_t *write_reqs;
	hb_queue_spsc_t write_reqs_free;
	tcp_service_stats_t stats;
	tcp_service_state_t state;
} tcp_service_t;


int tcp_service_setup(tcp_service_t *service);
void tcp_service_cleanup(tcp_service_t *service);
int tcp_service_start(tcp_service_t *service, const char *ipstr, uint16_t port);
int tcp_service_stop(tcp_service_t *service);
tcp_service_state_t tcp_service_state(tcp_service_t *service);
int tcp_service_update(tcp_service_t *service, hb_event_base_t **out_evt_base, uint64_t *out_count, uint8_t *out_state);
int tcp_service_send(tcp_service_t *service, uint64_t client_id, void *buffer_base, uint64_t length);

int tcp_service_write_req_acquire(tcp_service_t *service, tcp_service_write_req_t **out_write_req);
int tcp_service_write_req_next(tcp_service_t *service, tcp_service_write_req_t **out_write_req);
uint64_t tcp_service_write_req_count(tcp_service_t *service);
int tcp_service_write_req_release(tcp_service_t *service, tcp_service_write_req_t *write_req);

int tcp_service_stats_clear(tcp_service_t *service);
int tcp_service_stats_get(tcp_service_t *service, tcp_service_stats_t *stats);

#endif