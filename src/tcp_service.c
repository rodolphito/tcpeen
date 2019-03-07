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

	HB_GUARD_CLEANUP(ret = uv_timer_init(priv->uv_loop, priv->uv_accept_timer));
	priv->uv_accept_timer->data = service;
	HB_GUARD_CLEANUP(ret = uv_timer_start(priv->uv_accept_timer, on_timer_cb, 1, 1));

	HB_GUARD_CLEANUP(ret = uv_prepare_init(priv->uv_loop, priv->uv_prep));
	priv->uv_prep->data = service;
	HB_GUARD_CLEANUP(ret = uv_prepare_start(priv->uv_prep, on_prep_cb));

	HB_GUARD_CLEANUP(ret = uv_check_init(priv->uv_loop, priv->uv_check));
	priv->uv_check->data = service;
	HB_GUARD_CLEANUP(ret = uv_check_start(priv->uv_check, on_check_cb));


	// tcp listener
	HB_GUARD_CLEANUP(ret = uv_tcp_init(priv->uv_loop, priv->tcp_handle));
	priv->tcp_handle->data = service;
	HB_GUARD_CLEANUP(ret = uv_tcp_nodelay(priv->tcp_handle, 1));
	HB_GUARD_CLEANUP(ret = uv_tcp_bind(priv->tcp_handle, (const struct sockaddr *)&service->host_listen.sockaddr, 0));
	HB_GUARD_CLEANUP(ret = uv_listen((uv_stream_t *)priv->tcp_handle, HB_SERVICE_MAX_CLIENTS, on_connection_cb));


	// listener is up
	service->state = TCP_SERVICE_STARTED;
	while (ret = uv_run(priv->uv_loop, UV_RUN_NOWAIT));

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

	HB_GUARD_CLEANUP(ret = hb_mutex_setup(&service->mtx_io));

	HB_GUARD_CLEANUP(ret = tcp_channel_list_setup(&service->channel_list, HB_SERVICE_MAX_CLIENTS));

	HB_GUARD_CLEANUP(ret = hb_buffer_pool_setup(&service->pool, HB_SERVICE_MAX_CLIENTS * 4, HB_SERVICE_MAX_READ));
	HB_GUARD_CLEANUP(ret = hb_event_list_setup(&service->events));

	HB_GUARD_NULL_CLEANUP(service->write_req = HB_MEM_ACQUIRE(HB_SERVICE_MAX_CLIENTS * sizeof(*service->write_req)));
	HB_GUARD_CLEANUP(hb_list_ptr_setup(&service->write_req_free, HB_SERVICE_MAX_CLIENTS * 2));
	HB_GUARD_CLEANUP(hb_list_ptr_setup(&service->write_req_ready, HB_SERVICE_MAX_CLIENTS * 2));

	tcp_service_write_req_t *write_req;
	for (uint64_t i = 0; i < HB_SERVICE_MAX_CLIENTS; i++) {
		write_req = &service->write_req[i];
		HB_GUARD_CLEANUP(ret = hb_list_ptr_push_back(&service->write_req_free, write_req));
	}

	return HB_SUCCESS;

cleanup:
	HB_MEM_RELEASE(service->write_req);
	HB_MEM_RELEASE(service->priv);

	return HB_ERROR;
}

// --------------------------------------------------------------------------------------------------------------
void tcp_service_cleanup(tcp_service_t *service)
{
	if (!service) return;
	if (!service->priv) return;

	hb_mutex_cleanup(&service->mtx_io);

	tcp_channel_list_cleanup(&service->channel_list);

	hb_buffer_pool_cleanup(&service->pool);
	hb_event_list_cleanup(&service->events);

	hb_list_ptr_cleanup(&service->write_req_ready);
	hb_list_ptr_cleanup(&service->write_req_free);

	HB_MEM_RELEASE(service->write_req);
	HB_MEM_RELEASE(service->priv);
	service->priv = NULL;
}

// --------------------------------------------------------------------------------------------------------------
int tcp_service_start(tcp_service_t *service, const char *ipstr, uint16_t port)
{
	int ret;
	if (service->state != TCP_SERVICE_NEW && service->state != TCP_SERVICE_STOPPED) {
		return HB_ERROR;  // can't start from here
	}
	service->state = TCP_SERVICE_STARTING;

	char ipbuf[255];
	tcp_service_priv_t *priv = service->priv;
	HB_GUARD_CLEANUP(ret = hb_endpoint_set(&service->host_listen, ipstr, port));
	HB_GUARD_CLEANUP(ret = hb_endpoint_get_string(&service->host_listen, ipbuf, 255));
	HB_GUARD_CLEANUP(ret = uv_thread_create(&priv->uv_thread, tcp_service_io_thread, service));
	hb_log_info("Listening on %s", ipbuf);

	return HB_SUCCESS;

cleanup:
	return HB_ERROR;
}

// --------------------------------------------------------------------------------------------------------------
int tcp_service_stop(tcp_service_t *service)
{
	int ret;
	if (service->state == TCP_SERVICE_STOPPING || service->state == TCP_SERVICE_STOPPED) {
		return HB_ERROR;  // already closing
	}
	service->state = TCP_SERVICE_STOPPING;

	hb_log_trace("tcp service stopping");

	tcp_service_priv_t *priv = service->priv;
	HB_GUARD_NULL(priv);

	if ((ret = uv_thread_join(&priv->uv_thread))) {
		hb_log_error("Error joining thread: %d", ret);
	}

	service->state = TCP_SERVICE_STOPPED;

	return HB_SUCCESS;
}

// --------------------------------------------------------------------------------------------------------------
int tcp_service_lock(tcp_service_t *service)
{
	HB_GUARD_NULL(service);
	HB_GUARD(hb_mutex_lock(&service->mtx_io));
	return HB_SUCCESS;
}

// --------------------------------------------------------------------------------------------------------------
int tcp_service_unlock(tcp_service_t *service)
{
	HB_GUARD_NULL(service);
	HB_GUARD(hb_mutex_unlock(&service->mtx_io));
	return HB_SUCCESS;
}

// --------------------------------------------------------------------------------------------------------------
int tcp_service_update(tcp_service_t *service, void **out_evt_base, uint64_t *out_count, uint8_t *out_state)
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
	tcp_service_write_req_t *send_req = NULL;

	assert(service);
	assert(buffer_base);

	tcp_service_priv_t *priv = service->priv;
	assert(priv);

	tcp_channel_t *channel;
	HB_GUARD(tcp_channel_list_get(&service->channel_list, client_id, &channel));
	assert(channel);

	HB_GUARD(channel->state != TCP_CHANNEL_OPEN);

	HB_GUARD(tcp_service_write_req_acquire(service, &send_req));
	HB_GUARD_NULL(send_req);

	send_req->channel = channel;
	send_req->buffer = NULL;
	HB_GUARD_CLEANUP(hb_buffer_pool_acquire(&channel->service->pool, &send_req->buffer));
	HB_GUARD_NULL_CLEANUP(send_req->buffer);
	HB_GUARD_CLEANUP(hb_buffer_setup(send_req->buffer));

	uint8_t *bufbase = NULL;
	size_t buflen = 0;
	HB_GUARD_CLEANUP(hb_buffer_write_ptr(send_req->buffer, &bufbase, &buflen));

	HB_GUARD_CLEANUP(length >= buflen);

	memcpy(bufbase, buffer_base, length);
	hb_buffer_add_length(send_req->buffer, length);
	send_req->uv_buf = uv_buf_init(bufbase, UV_BUFLEN_CAST(length));

	return HB_SUCCESS;

cleanup:
	if (send_req->buffer && hb_buffer_release(send_req->buffer)) {
		hb_log_error("Error releasing send req buffer");
	}

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

	HB_GUARD(hb_list_ptr_pop_back(&service->write_req_free, out_write_req));
	//memset(*out_write_req, 0, sizeof(**out_write_req));
	HB_GUARD(hb_list_ptr_push_back(&service->write_req_ready, *out_write_req));

	return HB_SUCCESS;
}

// --------------------------------------------------------------------------------------------------------------
int tcp_service_write_req_next(tcp_service_t *service, tcp_service_write_req_t **out_write_req)
{
	assert(service);
	assert(out_write_req);

	HB_GUARD(hb_list_ptr_pop_back(&service->write_req_ready, out_write_req));

	return HB_SUCCESS;
}

// --------------------------------------------------------------------------------------------------------------
int tcp_service_write_req_release(tcp_service_t *service, tcp_service_write_req_t *write_req)
{
	assert(service);
	assert(write_req);

	HB_GUARD(hb_list_ptr_push_back(&service->write_req_free, write_req));

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
