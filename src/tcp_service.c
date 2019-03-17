#include "hb/tcp_service.h"

#include <stdlib.h>

#include "hb/error.h"
#include "hb/log.h"
#include "hb/tcp_connection.h"
#include "tcp_service_internal.h"

// private ------------------------------------------------------------------------------------------------------
void tcp_service_set_state(tcp_service_t *service, tcp_service_state_t state)
{
	tn_atomic_store(&service->state, state);
}

// private ------------------------------------------------------------------------------------------------------
void tcp_service_io_thread(void *data)
{
	tcp_service_t *service = data;
	if (!service) return;

	tcp_service_priv_t *priv = service->priv;
	if (!priv) return;

	int ret;
	priv->uv_loop = NULL;
	priv->tcp_handle = NULL;
	priv->uv_accept_timer = NULL;
	priv->uv_prep = NULL;
	priv->uv_check = NULL;

	ret = UV_ENOMEM;
	TN_GUARD_NULL_CLEANUP(priv->uv_loop = TN_MEM_ACQUIRE(sizeof(*priv->uv_loop)));
	TN_GUARD_NULL_CLEANUP(priv->tcp_handle = TN_MEM_ACQUIRE(sizeof(*priv->tcp_handle)));
	TN_GUARD_NULL_CLEANUP(priv->uv_accept_timer = TN_MEM_ACQUIRE(sizeof(*priv->uv_accept_timer)));
	TN_GUARD_NULL_CLEANUP(priv->uv_prep = TN_MEM_ACQUIRE(sizeof(*priv->uv_prep)));
	TN_GUARD_NULL_CLEANUP(priv->uv_check = TN_MEM_ACQUIRE(sizeof(*priv->uv_check)));
	TN_GUARD_NULL_CLEANUP(priv->uv_signal = TN_MEM_ACQUIRE(sizeof(*priv->uv_signal)));

	TN_GUARD_CLEANUP(ret = uv_loop_init(priv->uv_loop));

	TN_GUARD_CLEANUP(ret = uv_timer_init(priv->uv_loop, priv->uv_accept_timer));
	priv->uv_accept_timer->data = service;
	TN_GUARD_CLEANUP(ret = uv_timer_start(priv->uv_accept_timer, on_timer_cb, 100, 100));

	TN_GUARD_CLEANUP(ret = uv_prepare_init(priv->uv_loop, priv->uv_prep));
	priv->uv_prep->data = service;
	TN_GUARD_CLEANUP(ret = uv_prepare_start(priv->uv_prep, on_prep_cb));

	TN_GUARD_CLEANUP(ret = uv_check_init(priv->uv_loop, priv->uv_check));
	priv->uv_check->data = service;
	TN_GUARD_CLEANUP(ret = uv_check_start(priv->uv_check, on_check_cb));

	//TN_GUARD_CLEANUP(ret = uv_signal_init(priv->uv_loop, priv->uv_signal));
	//priv->uv_signal->data = service;
	//TN_GUARD_CLEANUP(ret = uv_signal_start_oneshot(priv->uv_signal, on_sigint_cb, SIGINT));

	// tcp listener
	TN_GUARD_CLEANUP(ret = uv_tcp_init(priv->uv_loop, priv->tcp_handle));
	priv->tcp_handle->data = service;
	TN_GUARD_CLEANUP(ret = uv_tcp_nodelay(priv->tcp_handle, 1));
	TN_GUARD_CLEANUP(ret = uv_tcp_bind(priv->tcp_handle, (const struct sockaddr *)&service->host_listen.sockaddr, 0));
	TN_GUARD_CLEANUP(ret = uv_listen((uv_stream_t *)priv->tcp_handle, TN_SERVICE_MAX_CLIENTS, on_connection_cb));


	// listener is up
	if (tcp_service_state(service) == TCP_SERVICE_STARTING) {
		tcp_service_set_state(service, TCP_SERVICE_STARTED);
		ret = uv_run(priv->uv_loop, UV_RUN_DEFAULT);
	}

	if ((ret = uv_loop_close(priv->uv_loop))) {
		tn_log_warning("Walking handles due to unclean IO loop close");
		uv_walk(priv->uv_loop, on_walk_cb, NULL);
		TN_GUARD_CLEANUP(ret = uv_loop_close(priv->uv_loop));
	}

	ret = TN_SUCCESS;

cleanup:
	if (ret != TN_SUCCESS) tn_log_uv_error(ret);

	if (priv->uv_check) TN_MEM_RELEASE(priv->uv_check);
	if (priv->uv_prep) TN_MEM_RELEASE(priv->uv_prep);
	if (priv->uv_accept_timer) TN_MEM_RELEASE(priv->uv_accept_timer);
	if (priv->tcp_handle) TN_MEM_RELEASE(priv->tcp_handle);
	if (priv->uv_loop) TN_MEM_RELEASE(priv->uv_loop);

	return;
}

// --------------------------------------------------------------------------------------------------------------
int tcp_service_setup(tcp_service_t *service)
{
	int ret = UV_EINVAL;
	TN_GUARD_NULL(service);
	memset(service, 0, sizeof(*service));

	tcp_service_set_state(service, TCP_SERVICE_NEW);

	TN_GUARD_NULL(service->priv = TN_MEM_ACQUIRE(sizeof(tcp_service_priv_t)));
	memset(service->priv, 0, sizeof(tcp_service_priv_t));

	TN_GUARD_CLEANUP(ret = tcp_channel_list_setup(&service->channel_list, TN_SERVICE_MAX_CLIENTS));

	service->buffer_count = TN_SERVICE_MAX_CLIENTS * 4;
	TN_GUARD_CLEANUP(tn_buffer_pool_setup(&service->pool_read, service->buffer_count, TN_SERVICE_MAX_READ));
	TN_GUARD_CLEANUP(tn_buffer_pool_setup(&service->pool_write, service->buffer_count, TN_SERVICE_MAX_READ));
	TN_GUARD_CLEANUP(tn_event_list_setup(&service->events, service->buffer_count));
	TN_GUARD_NULL_CLEANUP(service->event_updates = TN_MEM_ACQUIRE(service->buffer_count * sizeof(tn_event_base_t *)));
	TN_GUARD_NULL_CLEANUP(service->write_reqs = TN_MEM_ACQUIRE(service->buffer_count * sizeof(*service->write_reqs)));
	TN_GUARD_CLEANUP(tn_queue_spsc_setup(&service->write_reqs_free, service->buffer_count));
	TN_GUARD_CLEANUP(tn_queue_spsc_setup(&service->write_reqs_ready, service->buffer_count));

	tcp_service_write_req_t *write_req;
	for (uint64_t i = 0; i < service->buffer_count; i++) {
		write_req = &service->write_reqs[i];
		write_req->id = i;
		write_req->buffer = NULL;
		TN_GUARD_CLEANUP(tn_queue_spsc_push(&service->write_reqs_free, write_req));

		service->event_updates[i] = NULL;
	}

	return TN_SUCCESS;

cleanup:
	TN_MEM_RELEASE(service->event_updates);
	TN_MEM_RELEASE(service->write_reqs);
	TN_MEM_RELEASE(service->priv);

	return TN_ERROR;
}

// --------------------------------------------------------------------------------------------------------------
void tcp_service_cleanup(tcp_service_t *service)
{
	if (!service) return;
	if (!service->priv) return;

	tcp_channel_list_cleanup(&service->channel_list);
	tn_buffer_pool_cleanup(&service->pool_read);
	tn_buffer_pool_cleanup(&service->pool_write);
	tn_event_list_cleanup(&service->events);
	tn_queue_spsc_cleanup(&service->write_reqs_free);
	tn_queue_spsc_cleanup(&service->write_reqs_ready);

	TN_MEM_RELEASE(service->event_updates);
	TN_MEM_RELEASE(service->write_reqs);
	TN_MEM_RELEASE(service->priv);
}

// --------------------------------------------------------------------------------------------------------------
int tcp_service_start(tcp_service_t *service, const char *ipstr, uint16_t port)
{
	tcp_service_state_t state = tcp_service_state(service);
	TN_GUARD_CLEANUP(state != TCP_SERVICE_NEW && state != TCP_SERVICE_STOPPED);
	tcp_service_set_state(service, TCP_SERVICE_STARTING);

	char ipbuf[255];
	TN_GUARD_CLEANUP(tn_endpoint_set(&service->host_listen, ipstr, port));
	TN_GUARD_CLEANUP(tn_endpoint_get_string(&service->host_listen, ipbuf, 255));
	TN_GUARD_CLEANUP(tn_thread_launch(&service->thread_io, tcp_service_io_thread, service));
	tn_log_trace("Listening on %s", ipbuf);

	return TN_SUCCESS;

cleanup:
	return TN_ERROR;
}

// --------------------------------------------------------------------------------------------------------------
int tcp_service_stop(tcp_service_t *service)
{
	tcp_service_state_t state = tcp_service_state(service);
	TN_GUARD(state == TCP_SERVICE_STOPPING || state == TCP_SERVICE_STOPPED);
	tcp_service_set_state(service, TCP_SERVICE_STOPPING);

	tn_log_trace("tcp service stopping");
	if ((tn_thread_join(&service->thread_io))) {
		tn_log_error("error joining IO thread");
	}

	tcp_service_set_state(service, TCP_SERVICE_STOPPED);

	return TN_SUCCESS;
}

// --------------------------------------------------------------------------------------------------------------
int tcp_service_stop_signal(tcp_service_t *service)
{
	assert(service);
	tcp_service_state_t state = tcp_service_state(service);
	TN_GUARD(state == TCP_SERVICE_STOPPING || state == TCP_SERVICE_STOPPED);
	tcp_service_set_state(service, TCP_SERVICE_STOPPING);
	return TN_SUCCESS;
}

// --------------------------------------------------------------------------------------------------------------
tcp_service_state_t tcp_service_state(tcp_service_t *service)
{
	return tn_atomic_load(&service->state);
}

// --------------------------------------------------------------------------------------------------------------
int tcp_service_events_acquire(tcp_service_t *service, tn_event_base_t ***out_evt_base, uint64_t *out_count)
{
	assert(service);
	assert(out_evt_base);
	assert(out_count);

	*out_evt_base = NULL;
	*out_count = service->event_updates_count = 0;
	uint64_t count = service->buffer_count;
	TN_GUARD(tn_event_list_ready_pop_all(&service->events, service->event_updates, &count));

	*out_evt_base = service->event_updates;
	*out_count = service->event_updates_count = count;

	return TN_SUCCESS;
}

// --------------------------------------------------------------------------------------------------------------
int tcp_service_events_release(tcp_service_t *service)
{
	assert(service);
	
	tn_event_client_read_t *evt_read;
	for (size_t i = 0; i < service->event_updates_count; i++) {
		switch (service->event_updates[i]->type) {
		case TN_EVENT_CLIENT_OPEN:
			break;
		case TN_EVENT_CLIENT_CLOSE:
			break;
		case TN_EVENT_CLIENT_READ:
			evt_read = (tn_event_client_read_t *)service->event_updates[i];
			tn_buffer_pool_push(&service->pool_read, evt_read->tn_buffer);
			break;
		default:
			break;
		}
		tn_event_list_free_push(&service->events, service->event_updates[i]);
	}

	return TN_SUCCESS;
}

// --------------------------------------------------------------------------------------------------------------
int tcp_service_send(tcp_service_t *service, tcp_channel_t *channel, uint8_t *sndbuf, size_t sndlen)
{
	assert(service);
	assert(channel);
	assert(sndbuf && sndlen);

	int ret;
	tcp_service_write_req_t *send_req;

	/* send req will now be in our hands, but we need to pop it or drop it before return */
	ret = TN_SEND_NOREQ;
	TN_GUARD_CLEANUP(tn_queue_spsc_peek(&service->write_reqs_free, (void **)&send_req));
	
	ret = TN_SEND_NOBUF;
	TN_GUARD_CLEANUP(tn_buffer_pool_peek(&service->pool_write, &send_req->buffer));
	TN_GUARD_CLEANUP(tn_buffer_write(send_req->buffer, sndbuf, sndlen));

	send_req->channel = channel;
	send_req->uv_buf.base = tn_buffer_read_ptr(send_req->buffer);
	send_req->uv_buf.len = UV_BUFLEN_CAST(tn_buffer_length(send_req->buffer));

	/* push the request into the ready queue *before* popping anything from free queues */
	ret = TN_SEND_PUSH;
	TN_GUARD(tn_queue_spsc_push(&service->write_reqs_ready, send_req));
	tn_buffer_pool_pop_cached(&service->pool_write);
	tn_queue_spsc_pop_cached(&service->write_reqs_free);

	return TN_SUCCESS;

cleanup:
	tn_log_error("failed to send write request: %d", ret);
	return ret;
}

// --------------------------------------------------------------------------------------------------------------
int tcp_service_stats_clear(tcp_service_t *service)
{
	return TN_SUCCESS;
}

// --------------------------------------------------------------------------------------------------------------
int tcp_service_stats_get(tcp_service_t *service, tcp_service_stats_t *stats)
{
	return TN_SUCCESS;
}
