#include "hb/tcp_service.h"

#include <stdlib.h>

#include "hb/error.h"
#include "hb/log.h"
#include "hb/tcp_connection.h"
#include "tcp_service_internal.h"


// private ------------------------------------------------------------------------------------------------------
void tcp_service_run_cleanup()
{
	tcp_service_priv_t *priv = tcp_service.priv;
	if (priv->uv_accept_timer) HB_MEM_RELEASE(priv->uv_accept_timer);
	if (priv->tcp_handle) HB_MEM_RELEASE(priv->tcp_handle);
	if (priv->uv_loop) HB_MEM_RELEASE(priv->uv_loop);
}

// private ------------------------------------------------------------------------------------------------------
void tcp_service_run(void *data)
{
	tcp_service_t *service = data;
	if (!service) return;
	if (!service->priv) return;

	tcp_service_priv_t *priv = service->priv;

	int ret = 0;
	size_t blocks = 10;
	size_t block_size = 512;

	priv->uv_loop = NULL;
	priv->tcp_handle = NULL;
	priv->uv_accept_timer = NULL;

	if (!(priv->uv_loop = HB_MEM_ACQUIRE(sizeof(*priv->uv_loop)))) {
		hb_log_uv_error(UV_ENOMEM);
		goto error;
	}

	if (!(priv->tcp_handle = HB_MEM_ACQUIRE(sizeof(*priv->tcp_handle)))) {
		hb_log_uv_error(UV_ENOMEM);
		goto error;
	}

	if (!(priv->uv_accept_timer = HB_MEM_ACQUIRE(sizeof(*priv->uv_accept_timer)))) {
		hb_log_uv_error(UV_ENOMEM);
		goto error;
	}

	if ((ret = uv_loop_init(priv->uv_loop))) {
		hb_log_uv_error(ret);
		goto error;
	}

	if ((ret = uv_timer_init(priv->uv_loop, priv->uv_accept_timer))) {
		hb_log_uv_error(ret);
		goto error;
	}

	if ((ret = uv_timer_start(priv->uv_accept_timer, on_timer_cb, 500, 500))) {
		hb_log_uv_error(ret);
		goto error;
	}

	if ((ret = uv_tcp_init(priv->uv_loop, priv->tcp_handle))) {
		hb_log_uv_error(ret);
		goto error;
	}

	if ((ret = uv_tcp_nodelay(priv->tcp_handle, 1))) {
		hb_log_uv_error(ret);
		goto error;
	}

	if ((ret = uv_tcp_bind(priv->tcp_handle, (const struct sockaddr *)&tcp_service.host_listen.sockaddr, 0))) {
		hb_log_uv_error(ret);
		goto error;
	}

	if ((ret = uv_listen((uv_stream_t *)priv->tcp_handle, 1024, on_connection_cb))) {
		hb_log_uv_error(ret);
		goto error;
	}

	tcp_service.state = TCP_SERVICE_STARTED;

	if ((ret = uv_run(priv->uv_loop, UV_RUN_DEFAULT))) {
		hb_log_uv_error(ret);
		goto error;
	}

	if ((ret = uv_loop_close(priv->uv_loop))) {
		uv_walk(priv->uv_loop, on_walk_cb, NULL);
		if ((ret = uv_loop_close(priv->uv_loop))) {
			hb_log_uv_error(ret);
			goto error;
		}
		hb_log_error("Walked loop and shutdown cleanly\n");
	}

error:
	tcp_service_run_cleanup();
	return;
}

// --------------------------------------------------------------------------------------------------------------
int tcp_service_start(const char *ipstr, uint16_t port)
{
	if (tcp_service.state == TCP_SERVICE_NEW || tcp_service.state == TCP_SERVICE_STOPPED) {
		memset(&tcp_service, 0, sizeof(tcp_service));
		HB_GUARD_NULL(tcp_service.priv = HB_MEM_ACQUIRE(sizeof(tcp_service_priv_t)));
		memset(tcp_service.priv, 0, sizeof(tcp_service_priv_t));
	} else {
		return HB_ERROR; // cant start twice
	}

	int ret;
	tcp_service.state = TCP_SERVICE_STARTING;
	tcp_service_priv_t *priv = tcp_service.priv;
	priv->uv_closing = 0;
	HB_GUARD(hb_endpoint_set(&tcp_service.host_listen, ipstr, port));

	char ipbuf[255];
	HB_GUARD(hb_endpoint_get_string(&tcp_service.host_listen, ipbuf, 255));
	hb_log_info("Listening on %s\n", ipbuf);

	HB_GUARD(ret = uv_thread_create(&priv->uv_thread, tcp_service_run, &tcp_service));

	return HB_SUCCESS;
}

// --------------------------------------------------------------------------------------------------------------
int tcp_service_stop()
{
	int ret;

	//if (tcp_service.state != TCP_SERVICE_STARTED) return HB_ERROR;
	
	tcp_service.state = TCP_SERVICE_STOPPING;

	tcp_service_priv_t *priv = tcp_service.priv;
	HB_GUARD_NULL(priv);

	if (priv->uv_closing) return HB_ERROR; // already closing
	
	// TODO: check return vals here but ... what to do if they fail?!
	// TODO: fix up server stop mechanism instead of using this async handle
	uv_async_t *async_req = HB_MEM_ACQUIRE(sizeof(*async_req));
	uv_async_init(priv->uv_loop, async_req, on_async_cb);
	uv_async_send(async_req);

	if ((ret = uv_thread_join(&priv->uv_thread))) {
		hb_log_error("Error joining thread: %d", ret);
	}

	tcp_service.state = TCP_SERVICE_STOPPED;

	return HB_SUCCESS;
}

// --------------------------------------------------------------------------------------------------------------
int tcp_service_update(uintptr_t *evt_base, uint32_t *count)
{
	HB_GUARD(evt_base);
	HB_GUARD(count);

	evt_base = NULL;
	*count = 0;

	return HB_SUCCESS;
}
