#include "hb/tcp_service.h"

#include <stdlib.h>

#include "hb/error.h"
#include "hb/log.h"
#include "hb/tcp_connection.h"
#include "tcp_service_internal.h"


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

	// if allocations fail it's because we are out of memory
	ret = UV_ENOMEM;
	HB_GUARD_NULL_CLEANUP(priv->uv_loop = HB_MEM_ACQUIRE(sizeof(*priv->uv_loop)));
	HB_GUARD_NULL_CLEANUP(priv->tcp_handle = HB_MEM_ACQUIRE(sizeof(*priv->tcp_handle)));
	HB_GUARD_NULL_CLEANUP(priv->uv_accept_timer = HB_MEM_ACQUIRE(sizeof(*priv->uv_accept_timer)));
	HB_GUARD_NULL_CLEANUP(priv->uv_prep = HB_MEM_ACQUIRE(sizeof(*priv->uv_prep)));
	HB_GUARD_NULL_CLEANUP(priv->uv_check = HB_MEM_ACQUIRE(sizeof(*priv->uv_check)));


	// loop / timers init
	HB_GUARD_CLEANUP(ret = uv_loop_init(priv->uv_loop));

	//HB_GUARD_CLEANUP(ret = uv_timer_init(priv->uv_loop, priv->uv_accept_timer));
	//priv->uv_accept_timer->data = service;
	//HB_GUARD_CLEANUP(ret = uv_timer_start(priv->uv_accept_timer, on_timer_cb, 100, 100));

	//HB_GUARD_CLEANUP(ret = uv_prepare_init(priv->uv_loop, priv->uv_prep));
	//priv->uv_prep->data = service;
	//HB_GUARD_CLEANUP(ret = uv_prepare_start(priv->uv_prep, on_prep_cb));

	//HB_GUARD_CLEANUP(ret = uv_check_init(priv->uv_loop, priv->uv_check));
	//priv->uv_check->data = service;
	//HB_GUARD_CLEANUP(ret = uv_check_start(priv->uv_check, on_check_cb));


	// tcp listener
	HB_GUARD_CLEANUP(ret = uv_tcp_init(priv->uv_loop, priv->tcp_handle));
	priv->tcp_handle->data = service;
	HB_GUARD_CLEANUP(ret = uv_tcp_nodelay(priv->tcp_handle, 1));
	HB_GUARD_CLEANUP(ret = uv_tcp_bind(priv->tcp_handle, (const struct sockaddr *)&service->host_listen.sockaddr, 0));
	HB_GUARD_CLEANUP(ret = uv_listen((uv_stream_t *)priv->tcp_handle, HB_SERVICE_MAX_CLIENTS, on_connection_cb));


	// listener is up
	service->state = TCP_SERVICE_STARTED;
	ret = uv_run(priv->uv_loop, UV_RUN_DEFAULT);

	if ((ret = uv_loop_close(priv->uv_loop))) {
		hb_log_warning("Walking handles due to unclean IO loop close");
		uv_walk(priv->uv_loop, on_walk_cb, NULL);
		HB_GUARD_CLEANUP(ret = uv_loop_close(priv->uv_loop));
	}

	ret = HB_SUCCESS;

cleanup:
	if (ret != HB_SUCCESS) hb_log_uv_error(ret);

	if (priv->uv_check) HB_MEM_RELEASE(priv->uv_check);
	if (priv->uv_prep) HB_MEM_RELEASE(priv->uv_prep);
	if (priv->uv_accept_timer) HB_MEM_RELEASE(priv->uv_accept_timer);
	if (priv->tcp_handle) HB_MEM_RELEASE(priv->tcp_handle);
	if (priv->uv_loop) HB_MEM_RELEASE(priv->uv_loop);

	return;
}

// --------------------------------------------------------------------------------------------------------------
int tcp_service_setup(tcp_service_t *service)
{
	int ret = UV_EINVAL;
	HB_GUARD_NULL(service);

	memset(service, 0, sizeof(*service));

	HB_GUARD_NULL(service->priv = HB_MEM_ACQUIRE(sizeof(tcp_service_priv_t)));
	memset(service->priv, 0, sizeof(tcp_service_priv_t));

	HB_GUARD_CLEANUP(ret = tcp_channel_list_setup(&service->channel_list, HB_SERVICE_MAX_CLIENTS));

	const uint64_t bufcount = HB_SERVICE_MAX_CLIENTS * 4;
	HB_GUARD_CLEANUP(hb_buffer_pool_setup(&service->pool, bufcount, HB_SERVICE_MAX_READ));
	HB_GUARD_CLEANUP(hb_event_list_setup(&service->events, bufcount));

	HB_GUARD_NULL_CLEANUP(service->write_reqs = HB_MEM_ACQUIRE(bufcount * sizeof(*service->write_reqs)));
	HB_GUARD_CLEANUP(hb_queue_spsc_setup(&service->write_reqs_free, bufcount));

	tcp_service_write_req_t *write_req;
	for (uint64_t i = 0; i < bufcount; i++) {
		write_req = &service->write_reqs[i];
		write_req->id = i;
		write_req->buffer = NULL;
		HB_GUARD_CLEANUP(hb_queue_spsc_push(&service->write_reqs_free, write_req));
	}

	return HB_SUCCESS;

cleanup:
	HB_MEM_RELEASE(service->write_reqs);
	HB_MEM_RELEASE(service->priv);

	return HB_ERROR;
}

// --------------------------------------------------------------------------------------------------------------
void tcp_service_cleanup(tcp_service_t *service)
{
	if (!service) return;
	if (!service->priv) return;

	tcp_channel_list_cleanup(&service->channel_list);
	hb_buffer_pool_cleanup(&service->pool);
	hb_event_list_cleanup(&service->events);
	hb_queue_spsc_cleanup(&service->write_reqs_free);

	HB_MEM_RELEASE(service->write_reqs);
	HB_MEM_RELEASE(service->priv);
}

// --------------------------------------------------------------------------------------------------------------
int tcp_service_start(tcp_service_t *service, const char *ipstr, uint16_t port)
{
	if (service->state != TCP_SERVICE_NEW && service->state != TCP_SERVICE_STOPPED) {
		return HB_ERROR;  // can't start from here
	}
	service->state = TCP_SERVICE_STARTING;

	char ipbuf[255];
	HB_GUARD_CLEANUP(hb_endpoint_set(&service->host_listen, ipstr, port));
	HB_GUARD_CLEANUP(hb_endpoint_get_string(&service->host_listen, ipbuf, 255));
	HB_GUARD_CLEANUP(hb_thread_launch(&service->thread_io, tcp_service_io_thread, service));
	hb_log_info("Listening on %s", ipbuf);

	return HB_SUCCESS;

cleanup:
	return HB_ERROR;
}

// --------------------------------------------------------------------------------------------------------------
int tcp_service_stop(tcp_service_t *service)
{
	HB_GUARD(service->state == TCP_SERVICE_STOPPING || service->state == TCP_SERVICE_STOPPED);

	service->state = TCP_SERVICE_STOPPING;

	hb_log_trace("tcp service stopping");
	if ((hb_thread_join(&service->thread_io))) {
		hb_log_error("error joining IO thread");
	}

	service->state = TCP_SERVICE_STOPPED;

	return HB_SUCCESS;
}

// --------------------------------------------------------------------------------------------------------------
int tcp_service_update(tcp_service_t *service, hb_event_base_t **out_evt_base, uint64_t *out_count, uint8_t *out_state)
{
	assert(service);
	assert(out_evt_base);
	assert(out_count);
	assert(out_state);

	*out_evt_base = NULL;
	*out_count = 0;
	*out_state = service->state;

	if (service->state == TCP_SERVICE_STARTED) {
		HB_GUARD(hb_event_list_pop_swap(&service->events, out_evt_base, out_count));
	}

	return HB_SUCCESS;
}

// --------------------------------------------------------------------------------------------------------------
int tcp_service_send(tcp_service_t *service, uint64_t client_id, void *buffer_base, uint64_t length)
{
	int ret;
	tcp_service_write_req_t *send_req = NULL;

	assert(service);
	assert(buffer_base);

	tcp_service_priv_t *priv = service->priv;
	assert(priv);

	tcp_channel_t *channel;
	HB_GUARD(tcp_channel_list_get(&service->channel_list, client_id, &channel));
	assert(channel);

	HB_GUARD(channel->state != TCP_CHANNEL_OPEN);
	HB_GUARD(uv_is_closing((uv_handle_t *)channel->priv));

	HB_GUARD(tcp_service_write_req_acquire(service, &send_req));
	HB_GUARD_NULL(send_req);

	send_req->channel = channel;
	send_req->buffer = NULL;
	HB_GUARD_CLEANUP(hb_buffer_pool_acquire(&channel->service->pool, &send_req->buffer));
	HB_GUARD_NULL_CLEANUP(send_req->buffer);
	hb_buffer_reset(send_req->buffer);

	uint8_t *bufbase = NULL;
	size_t buflen = 0;
	HB_GUARD_CLEANUP(hb_buffer_write(send_req->buffer, buffer_base, length));
	hb_buffer_read_ptr(send_req->buffer, &bufbase, &buflen);
	send_req->uv_buf.base = bufbase;
	send_req->uv_buf.len = UV_BUFLEN_CAST(length);

	if (ret = uv_write((uv_write_t *)send_req, (uv_stream_t *)channel->priv, &send_req->uv_buf, 1, on_send_cb)) {
		hb_log_uv_error(ret);
		goto cleanup;
	}

	return HB_SUCCESS;

cleanup:
	if (send_req->buffer && hb_buffer_release(send_req->buffer)) {
		hb_log_error("Error releasing send req buffer");
	}
	send_req->buffer = NULL;

	if (send_req && tcp_service_write_req_release(service, send_req)) {
		hb_log_error("Error releasing send req");
	}

	return HB_ERROR;
}

// --------------------------------------------------------------------------------------------------------------
int tcp_service_write_req_acquire(tcp_service_t *service, tcp_service_write_req_t **out_write_req)
{
	assert(service);
	assert(out_write_req);

	HB_GUARD(hb_queue_spsc_pop(&service->write_reqs_free, (void **)out_write_req));

	return HB_SUCCESS;
}

// --------------------------------------------------------------------------------------------------------------
int tcp_service_write_req_release(tcp_service_t *service, tcp_service_write_req_t *write_req)
{
	assert(service);
	assert(write_req);

	HB_GUARD(hb_queue_spsc_push(&service->write_reqs_free, write_req));

	return HB_SUCCESS;
}

// --------------------------------------------------------------------------------------------------------------
int tcp_service_stats_clear(tcp_service_t *service)
{
	return HB_SUCCESS;
}

// --------------------------------------------------------------------------------------------------------------
int tcp_service_stats_get(tcp_service_t *service, tcp_service_stats_t *stats)
{
	return HB_SUCCESS;
}
