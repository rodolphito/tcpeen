#include "tcp_service_internal.h"

#include "uv.h"

#include "hb/error.h"
#include "hb/log.h"
#include "hb/event.h"
#include "hb/tcp_service.h"
#include "hb/tcp_connection.h"


uint64_t send_msgs = 0;
uint64_t send_bytes = 0;
uint64_t recv_msgs = 0;
uint64_t recv_bytes = 0;


typedef struct uv_buffer_work_req_s {
	uv_work_t uv_req;
	hb_buffer_t *hb_buffer;
	uv_stream_t *tcp_handle;
	int ret;
	int close;
} uv_buffer_work_req_t;


// --------------------------------------------------------------------------------------------------------------
void on_sigint_cb(uv_signal_t *handle, int signum)
{
	if (signum != SIGINT) return;

	hb_log_trace("received SIGINT");

	tcp_service_t *service = handle->data;
	assert(service);
	
	if (tcp_service_stop_signal(service)) {
		hb_log_error("failed to stop tcp service");
	}
}

// --------------------------------------------------------------------------------------------------------------
void on_close_handle_cb(uv_handle_t *handle)
{
	HB_MEM_RELEASE(handle);
}

// --------------------------------------------------------------------------------------------------------------
void on_close_channel_cb(uv_handle_t *handle)
{
	tcp_channel_t *channel = handle->data;
	assert(channel);
	channel->state = TCP_CHANNEL_CLOSED;

	if (channel->read_buffer) {
		hb_buffer_pool_push(&channel->service->pool_read, channel->read_buffer);
		channel->read_buffer = NULL;
	}

	hb_event_client_close_t *evt_close;
	HB_GUARD_CLEANUP(hb_event_list_free_pop_close(&channel->service->events, &evt_close));
	evt_close->client_id = channel->id;
	evt_close->error_code = 0;

	if (hb_event_list_ready_push(&channel->service->events, evt_close)) {
		hb_log_error("push failed stranding event on channel: %zu", channel->id);
	}

	if (tcp_channel_list_close(&channel->service->channel_list, channel)) {
		hb_log_error("list failed to close on channel: %zu", channel->id);
	}

cleanup:
	HB_MEM_RELEASE(handle);
}

// --------------------------------------------------------------------------------------------------------------
void on_send_cb(uv_write_t *req, int status)
{
	tcp_service_write_req_t *write_req = (tcp_service_write_req_t *)req;
	assert(write_req);

	if (status) {
		hb_log_uv_error(status);
	} else {
		send_msgs++;
		send_bytes += write_req->uv_buf.len;
	}

	tcp_service_t *service = write_req->channel->service;
	hb_buffer_t *reqbuf = write_req->buffer;
	write_req->buffer = NULL;
	if (hb_buffer_pool_push(&service->pool_write, reqbuf)) {
		hb_log_error("push failed stranding buffer on channel: %zu", write_req->channel->id);
	}

	if (hb_queue_spsc_push(&service->write_reqs_free, write_req)) {
		hb_log_error("push failed stranding write request: %zu on channel: %zu", write_req->id, write_req->channel->id);
	}
}

// --------------------------------------------------------------------------------------------------------------
void on_recv_alloc_cb(uv_handle_t *handle, size_t suggested_size, uv_buf_t *buf)
{
	HB_GUARD_CLEANUP(!handle);
	tcp_channel_t *channel = handle->data;
	assert(channel);

	if (!channel->read_buffer) {
		if (hb_buffer_pool_pop_back(&channel->service->pool_read, &channel->read_buffer)) {
			hb_log_warning("failed to pop read pool, expect UV_ENOBUFS");
			goto cleanup;
		}
	}

	buf->base = hb_buffer_write_ptr(channel->read_buffer);
	buf->len = UV_BUFLEN_CAST(hb_buffer_remaining(channel->read_buffer));

	return;

cleanup:
	if (buf) {
		buf->base = NULL;
		buf->len = 0;
	}
}

//// --------------------------------------------------------------------------------------------------------------
//void on_task_complete_cb(uv_work_t *req, int status)
//{
//	int ret;
//	uv_buffer_work_req_t *work_req = (uv_buffer_work_req_t *)req;
//	assert(work_req);
//
//	if (work_req->ret || work_req->close || status == UV_ECANCELED) {
//		goto cleanup;
//	}
//
//	tcp_service_write_req_t *send_req = req->data;
//	if ((ret = uv_write((uv_write_t *)send_req, (uv_stream_t *)work_req->tcp_handle, &send_req->uv_buf, 1, on_send_cb))) {
//		hb_log_uv_error(ret);
//		work_req->close = 1;
//		goto cleanup;
//	}
//
//cleanup:
//	HB_MEM_RELEASE(req);
//}
//
//// --------------------------------------------------------------------------------------------------------------
//void on_task_process_cb(uv_work_t *req)
//{
//	tcp_service_write_req_t *send_req = NULL;
//
//	uv_buffer_work_req_t *work_req = (uv_buffer_work_req_t *)req;
//	assert(work_req);
//
//	if (!(send_req = HB_MEM_ACQUIRE(sizeof(*send_req)))) {
//		hb_log_uv_error(UV_ENOMEM);
//		goto cleanup;
//	}
//
//	hb_buffer_read_ptr(work_req->hb_buffer);
//	send_req->uv_buf = uv_buf_init(buf, UV_BUFLEN_CAST(len));
//	send_req->buffer = work_req->hb_buffer;
//	req->data = send_req;
//
//	return;
//
//cleanup:
//	work_req->ret = HB_ERROR;
//	work_req->close = 1;
//
//	if (send_req) HB_MEM_RELEASE(send_req);
//}

// --------------------------------------------------------------------------------------------------------------
void on_recv_cb(uv_stream_t *handle, ssize_t nread, const uv_buf_t *buf)
{
	int ret = HB_RECV_ERROR;
	int close_channel = 0;
	uv_buffer_work_req_t *work_req = NULL;
	tcp_channel_t *channel = handle->data;
	assert(channel);

	if (nread < 0) {
		/* nread is an errno when < 0 */
		ret = (int)nread;
		close_channel = 1;
		goto cleanup;
	} else if (nread == 0 || !buf) {
		/* this is technically not an error */
		goto cleanup;
	}

	if (nread >= HB_SERVICE_MAX_READ) {
		hb_log_error("read full %zd buffer on client: %zu", nread, channel->id);
	}

	recv_msgs++;
	recv_bytes += nread;
	ret = HB_RECV_EBUF;
	HB_GUARD_CLEANUP(hb_buffer_add_length(channel->read_buffer, nread));

	hb_event_client_read_t *evt;
	ret = HB_RECV_EVTPOP;
	HB_GUARD_CLEANUP(hb_event_list_free_pop_read(&channel->service->events, &evt));
		
	evt->client_id = channel->id;
	evt->hb_buffer = channel->read_buffer;
	evt->buffer = hb_buffer_read_ptr(channel->read_buffer);
	evt->len = hb_buffer_read_length(channel->read_buffer);

	ret = HB_RECV_EVTPUSH;
	HB_GUARD_CLEANUP(hb_event_list_ready_push(&channel->service->events, evt));

	/* successful recv */

	channel->read_buffer = NULL;

	return;

cleanup:
	channel->error_code = ret;
	if (close_channel) {
		if (!uv_is_closing((uv_handle_t *)handle)) {
			channel->state = TCP_CHANNEL_CLOSING;
			uv_close((uv_handle_t *)handle, on_close_channel_cb);
		}
	}
	
	HB_MEM_RELEASE(work_req);
}

// --------------------------------------------------------------------------------------------------------------
void on_connection_cb(uv_stream_t *server_handle, int status)
{
	int ret = UV_EINVAL;
	int handle_is_init = 0;
	uv_tcp_t *client_handle = NULL;
	tcp_channel_t *channel = NULL;

	if (status < 0) {
		ret = status;
		goto cleanup;
	}

	tcp_service_t *service = server_handle->data;
	assert(service);
	tcp_service_priv_t *priv = service->priv;

	ret = UV_ENOMEM;
	HB_GUARD_NULL_CLEANUP(client_handle = HB_MEM_ACQUIRE(sizeof(*client_handle)));
	HB_GUARD_CLEANUP(ret = uv_tcp_init(priv->uv_loop, client_handle));
	handle_is_init = 1;
	HB_GUARD_CLEANUP(ret = uv_accept(server_handle, (uv_stream_t *)client_handle));

	HB_GUARD_CLEANUP(ret = tcp_channel_list_open(&service->channel_list, &channel));
	client_handle->data = channel;
	channel->priv = client_handle;
	channel->service = service;
	channel->read_state = TCP_CHANNEL_READ_HEADER;
	channel->state = TCP_CHANNEL_OPEN;
	channel->error_code = 0;
	channel->read_buffer = NULL;
	channel->last_msg_id = 0;

	struct sockaddr_storage addr;
	int addrlen = sizeof(addr);
	hb_endpoint_t host_local;
	HB_GUARD_CLEANUP(ret = uv_tcp_getpeername(client_handle, (struct sockaddr *)&addr, &addrlen));
	HB_GUARD_CLEANUP(ret = hb_endpoint_convert_from(&channel->endpoint, &addr));
	HB_GUARD_CLEANUP(ret = uv_tcp_getsockname(client_handle, (struct sockaddr *)&addr, &addrlen));
	HB_GUARD_CLEANUP(ret = hb_endpoint_convert_from(&host_local, &addr));

	hb_event_client_open_t *open_evt;
	HB_GUARD_CLEANUP(ret = hb_event_list_free_pop_open(&service->events, &open_evt));
	open_evt->type = HB_EVENT_CLIENT_OPEN;
	open_evt->client_id = channel->id;
	HB_GUARD_CLEANUP(ret = hb_endpoint_get_string(&host_local, open_evt->host_local, sizeof(open_evt->host_local)));
	HB_GUARD_CLEANUP(ret = hb_endpoint_get_string(&channel->endpoint, open_evt->host_remote, sizeof(open_evt->host_remote)));
	HB_GUARD_CLEANUP(ret = hb_event_list_ready_push(&service->events, open_evt));

	HB_GUARD_CLEANUP(ret = uv_read_start((uv_stream_t *)client_handle, on_recv_alloc_cb, on_recv_cb));
	
	return;

cleanup:
	hb_log_error("connection error: %d -- %s -- %s", ret, uv_err_name(ret), uv_strerror(ret));
	if (client_handle) {
		if (channel) {
			channel->state = TCP_CHANNEL_CLOSING;
			uv_close((uv_handle_t *)client_handle, on_close_channel_cb);
		} else if (handle_is_init) {
			uv_close((uv_handle_t *)client_handle, on_close_handle_cb);
		}
	}
}

// --------------------------------------------------------------------------------------------------------------
void on_timer_cb(uv_timer_t *handle)
{
	tcp_service_t *service = handle->data;
	HB_GUARD_NULL_CLEANUP(service);
	tcp_service_priv_t *priv = service->priv;
	HB_GUARD_CLEANUP(handle != priv->uv_accept_timer);

	if (tcp_service_state(service) != TCP_SERVICE_STARTED) {
		if (!uv_is_closing((uv_handle_t *)priv->tcp_handle)) uv_close((uv_handle_t *)priv->tcp_handle, NULL);
		if (!uv_is_closing((uv_handle_t *)priv->uv_accept_timer)) uv_close((uv_handle_t *)priv->uv_accept_timer, NULL);
		if (!uv_is_closing((uv_handle_t *)priv->uv_prep)) uv_close((uv_handle_t *)priv->uv_prep, NULL);
		if (!uv_is_closing((uv_handle_t *)priv->uv_check)) uv_close((uv_handle_t *)priv->uv_check, NULL);
	}

	return;

cleanup:
	return;
}

// --------------------------------------------------------------------------------------------------------------
void on_prep_cb(uv_prepare_t *handle)
{
	int ret;
	tcp_service_write_req_t *send_req = NULL;
	tcp_service_t *service = handle->data;
	HB_GUARD_NULL_CLEANUP(service);

	while (!hb_queue_spsc_pop_back(&service->write_reqs_ready, &send_req)) {
		assert(send_req);

		if (send_req->channel->state != TCP_CHANNEL_OPEN) {
			hb_buffer_pool_push(&service->pool_write, send_req->buffer);
			hb_queue_spsc_push(&service->write_reqs_free, send_req);
			continue;
		}

		if (ret = uv_write((uv_write_t *)send_req, (uv_stream_t *)send_req->channel->priv, &send_req->uv_buf, 1, on_send_cb)) {
			hb_log_error("IO send failed:  %d -- %s -- %s", ret, uv_err_name(ret), uv_strerror(ret));
			hb_buffer_t *reqbuf = send_req->buffer;
			send_req->buffer = NULL;
			hb_buffer_pool_push(&service->pool_write, reqbuf);
			hb_queue_spsc_push(&service->write_reqs_free, send_req);
		}
	}

	return;

cleanup:
	hb_log_error("handle missing service data");
}

// --------------------------------------------------------------------------------------------------------------
void on_check_cb(uv_check_t *handle)
{
	tcp_service_t *service = handle->data;
	HB_GUARD_NULL_CLEANUP(service);
	tcp_service_priv_t *priv = service->priv;
	return;

cleanup:
	hb_log_error("handle missing service data");
}

// --------------------------------------------------------------------------------------------------------------
void on_async_cb(uv_async_t *handle)
{
	tcp_service_t *service = handle->data;
	HB_GUARD_NULL_CLEANUP(service);
	tcp_service_priv_t *priv = service->priv;
	return;

cleanup:
	hb_log_error("handle missing service data");
}

// --------------------------------------------------------------------------------------------------------------
void on_walk_cb(uv_handle_t* handle, void* arg)
{
	// TODO: take appropriate action based on handle type
	if (!uv_is_closing(handle)) {
		hb_log_warning("Manually closing handle: %p -- %s", handle, uv_handle_type_name(uv_handle_get_type(handle)));
		uv_close(handle, NULL);
	}
}