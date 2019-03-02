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
	HB_GUARD_CLEANUP(ret = uv_timer_start(priv->uv_accept_timer, on_timer_cb, 100, 100));

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
	HB_GUARD_CLEANUP(ret = uv_listen((uv_stream_t *)priv->tcp_handle, 1024, on_connection_cb));


	// listener is up
	service->state = TCP_SERVICE_STARTED;
	HB_GUARD_CLEANUP(ret = uv_run(priv->uv_loop, UV_RUN_DEFAULT));

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
int tcp_service_start(tcp_service_t *service, const char *ipstr, uint16_t port)
{
	int ret = UV_EINVAL;
	HB_GUARD_NULL(service);

	if (service->state != TCP_SERVICE_NEW && service->state != TCP_SERVICE_STOPPED) {
		return HB_ERROR; // cant start twice
	}
	service->state = TCP_SERVICE_STARTING;

	hb_log_trace("tcp service starting");

	ret = UV_ENOMEM;
	memset(service, 0, sizeof(*service));
	
	HB_GUARD_NULL(service->priv = HB_MEM_ACQUIRE(sizeof(tcp_service_priv_t)));
	memset(service->priv, 0, sizeof(tcp_service_priv_t));

	HB_GUARD_CLEANUP(ret = hb_mutex_setup(&service->mtx_io));

	HB_GUARD_CLEANUP(ret = tcp_channel_list_setup(&service->channel_list, HB_SERVICE_MAX_CLIENTS));

	HB_GUARD_CLEANUP(ret = hb_buffer_pool_setup(&service->pool, HB_SERVICE_MAX_CLIENTS * 10, HB_SERVICE_MAX_READ));
	HB_GUARD_CLEANUP(ret = hb_event_list_setup(&service->events));

	char ipbuf[255];
	tcp_service_priv_t *priv = service->priv;
	HB_GUARD_CLEANUP(ret = hb_endpoint_set(&service->host_listen, ipstr, port));
	HB_GUARD_CLEANUP(ret = hb_endpoint_get_string(&service->host_listen, ipbuf, 255));
	HB_GUARD_CLEANUP(ret = uv_thread_create(&priv->uv_thread, tcp_service_io_thread, service));
	hb_log_info("Listening on %s", ipbuf);

	return HB_SUCCESS;

cleanup:
	hb_mutex_cleanup(&service->mtx_io);

	HB_MEM_RELEASE(service->priv);

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

	hb_mutex_cleanup(&service->mtx_io);

	tcp_channel_list_cleanup(&service->channel_list);

	hb_buffer_pool_cleanup(&service->pool);
	hb_event_list_cleanup(&service->events);

    HB_MEM_RELEASE(priv);
    priv = NULL;

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
int tcp_service_update(tcp_service_t *service, uintptr_t *evt_base, uint32_t *count)
{
	HB_GUARD(evt_base);
	HB_GUARD(count);

	evt_base = NULL;
	*count = 0;

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
