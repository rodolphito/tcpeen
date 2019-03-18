#ifndef TN_TCP_SERVICE_H
#define TN_TCP_SERVICE_H

#include <stdint.h>

#include "hb/thread.h"
#include "hb/tcp_channel.h"
#include "hb/endpoint.h"
#include "hb/buffer_pool.h"
#include "hb/event.h"
#include "hb/queue_spsc.h"


#define TN_SERVICE_MAX_CLIENTS 10000
#define TN_SERVICE_MAX_READ 65535


// forwards
struct tcp_service_write_req_s;


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
	uint64_t tstamp_last;
} tcp_service_stats_t;

typedef struct tcp_service_s {
	/* impl specific struct pointer */
	void *priv;

	/* IO thread (calls uv_run, handles read/write, etc) */
	tn_thread_t thread_io;

	/* server's bind address */
	tn_endpoint_t host_listen;

	/* tcp channel context (eg. connections) */
	tcp_channel_list_t channel_list;

	/* IO thread's buffer pool for recv */
	tn_buffer_pool_t pool_read;

	/* main thread's buffer pool for send */
	tn_buffer_pool_t pool_write;

	/* context for the IO thread to queue events for the main thread */
	tn_event_list_t events;

	/* pointer array of events to pull from the event queue */
	tn_event_base_t **event_updates;
	uint64_t event_updates_count;

	/* to how many buffers, event updates, are we bounded */
	uint64_t buffer_count;

	/* write request pool for main thread */
	struct tcp_service_write_req_s *write_reqs;

	/* write request free list for IO thread --> main thread */
	tn_queue_spsc_t write_reqs_free;

	/* write request ready list for main thread --> IO thread */
	tn_queue_spsc_t write_reqs_ready;

	/* overall connection statistics - these will be sent in the event queue - don't touch */
	tn_event_io_stats_t stats;

	/* server state (connecting, connecting, etc) */
	tn_atomic_t state;
} tcp_service_t;


int tcp_service_setup(tcp_service_t *service);
void tcp_service_cleanup(tcp_service_t *service);
int tcp_service_start(tcp_service_t *service, const char *ipstr, uint16_t port);
int tcp_service_stop(tcp_service_t *service);
int tcp_service_stop_signal(tcp_service_t *service);
tcp_service_state_t tcp_service_state(tcp_service_t *service);
int tcp_service_events_acquire(tcp_service_t *service, tn_event_base_t ***out_evt_base, uint64_t *out_count);
int tcp_service_events_release(tcp_service_t *service);
int tcp_service_send(tcp_service_t *service, tcp_channel_t *channel, uint8_t *sndbuf, size_t sndlen);

int tcp_service_stats_clear(tcp_service_t *service);
int tcp_service_stats_get(tcp_service_t *service, tcp_service_stats_t *stats);

#endif