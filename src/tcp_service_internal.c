#include "tcp_service_internal.h"

#include "uv.h"

#include "hb/error.h"
#include "hb/log.h"
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
void on_close_release_cb(uv_handle_t* handle)
{
	tcp_channel_t *channel = handle->data;
	assert(channel);

	if (channel->read_buffer) {
		hb_buffer_release(channel->read_buffer);
		channel->read_buffer = NULL;
	}

	HB_MEM_RELEASE(handle);
}

// --------------------------------------------------------------------------------------------------------------
void on_send_cb(uv_write_t *req, int status)
{
	tcp_service_write_req_t *write_req = (tcp_service_write_req_t *)req;
	assert(write_req);

	hb_buffer_t *buffer = write_req->buffer;
	assert(buffer);

	if (status) {
		hb_log_uv_error(status);
	} else {
		send_msgs++;
		send_bytes += write_req->uv_buf.len;
	}

	hb_buffer_release(buffer);

	HB_MEM_RELEASE(write_req);
}

// --------------------------------------------------------------------------------------------------------------
void on_recv_alloc_cb(uv_handle_t *handle, size_t suggested_size, uv_buf_t *buf)
{
	int ret;
	tcp_channel_t *channel = handle->data;
	if (!channel->read_buffer) {
		ret = UV_ENOBUFS;
		HB_GUARD_CLEANUP(hb_buffer_pool_acquire(&channel->service->pool, &channel->read_buffer));
        HB_GUARD_CLEANUP(hb_buffer_setup(channel->read_buffer));
	}

	uint8_t *bufbase = NULL;
	size_t buflen = 0;
	ret = UV_E2BIG;
	HB_GUARD_CLEANUP(hb_buffer_write_ptr(channel->read_buffer, &bufbase, &buflen));

	buf->base = bufbase;
	buf->len = UV_BUFLEN_CAST(buflen);

	return;

cleanup:
    hb_log_uv_error(ret);
	if (channel->read_buffer) {
		hb_buffer_release(channel->read_buffer);
		channel->read_buffer = NULL;
	}
	buf->base = NULL;
	buf->len = 0;
}

// --------------------------------------------------------------------------------------------------------------
void on_task_complete_cb(uv_work_t *req, int status)
{
	int ret;
	uv_buffer_work_req_t *work_req = (uv_buffer_work_req_t *)req;
	assert(work_req);

	if (work_req->ret || work_req->close || status == UV_ECANCELED) {
		goto cleanup;
	}

	tcp_service_write_req_t *send_req = req->data;
	if ((ret = uv_write((uv_write_t *)send_req, (uv_stream_t *)work_req->tcp_handle, &send_req->uv_buf, 1, on_send_cb))) {
		hb_log_uv_error(ret);
		work_req->close = 1;
		goto cleanup;
	}

cleanup:
	HB_MEM_RELEASE(req);
}

// --------------------------------------------------------------------------------------------------------------
void on_task_process_cb(uv_work_t *req)
{
	tcp_service_write_req_t *send_req = NULL;

	uv_buffer_work_req_t *work_req = (uv_buffer_work_req_t *)req;
	assert(work_req);

	if (!(send_req = HB_MEM_ACQUIRE(sizeof(*send_req)))) {
		hb_log_uv_error(UV_ENOMEM);
		goto cleanup;
	}

	uint8_t *buf = NULL;
	size_t len = 0;
	HB_GUARD_CLEANUP(hb_buffer_read_ptr(work_req->hb_buffer, &buf, &len));
	send_req->uv_buf = uv_buf_init(buf, UV_BUFLEN_CAST(len));
	send_req->buffer = work_req->hb_buffer;
	req->data = send_req;

	return;

cleanup:
	work_req->ret = HB_ERROR;
	work_req->close = 1;

	if (send_req) HB_MEM_RELEASE(send_req);
}

// --------------------------------------------------------------------------------------------------------------
void on_recv_cb(uv_stream_t *handle, ssize_t nread, const uv_buf_t *buf)
{
	int ret = UV_EINVAL;
	uv_buffer_work_req_t *work_req = NULL;
	
	tcp_channel_t *channel = handle->data;
	HB_GUARD_NULL_CLEANUP(channel);

	if (nread < 0) { // nread an error when < 0
		switch (nread) {
		case UV_ECONNRESET: // connection reset by peer
		case UV_EOF: // EOF in TCP means the peer disconnected gracefully
			break;
		default:
			hb_log_uv_error((int)nread);
			break;
		}

		goto close;
	} else if (nread == 0 || !buf) {
		// this isn't an error, its an empty packet or other non error event with nothing to read
		goto cleanup;
	}

	recv_msgs++;
	recv_bytes += nread;
	HB_GUARD_CLEANUP(hb_buffer_add_length(channel->read_buffer, nread));

	if (!(work_req = HB_MEM_ACQUIRE(sizeof(*work_req)))) {
		hb_log_uv_error(UV_ENOMEM);
		goto close;
	}

	work_req->uv_req.data = NULL;
	work_req->tcp_handle = handle;
	work_req->hb_buffer = channel->read_buffer;
	work_req->close = 0;
	work_req->ret = HB_SUCCESS;

	channel->read_buffer = NULL;

	tcp_service_priv_t *service_priv = (tcp_service_priv_t *)channel->service->priv;
	if ((ret = uv_queue_work(service_priv->uv_loop, (uv_work_t *)work_req, on_task_process_cb, on_task_complete_cb))) {
		hb_log_uv_error(ret);
		goto close;
	}

	return;

close:
	if (!uv_is_closing((uv_handle_t *)handle)) uv_close((uv_handle_t *)handle, on_close_release_cb);

cleanup:
	if (work_req) HB_MEM_RELEASE(work_req);
}

// --------------------------------------------------------------------------------------------------------------
void on_connection_cb(uv_stream_t *server_handle, int status)
{
	int ret = UV_EINVAL;
	uv_tcp_t *client_handle = NULL;

	tcp_service_t *service = server_handle->data;
	HB_GUARD_NULL_CLEANUP(service);
	tcp_service_priv_t *priv = service->priv;

	if (status < 0) {
		ret = status;
		goto cleanup;
	}

	ret = UV_ENOMEM;
	HB_GUARD_NULL_CLEANUP(client_handle = HB_MEM_ACQUIRE(sizeof(*client_handle)));
	HB_GUARD_CLEANUP(ret = uv_tcp_init(priv->uv_loop, client_handle));
	HB_GUARD_CLEANUP(ret = uv_accept(server_handle, (uv_stream_t *)client_handle));

	// TODO: new connection event
	tcp_channel_t *channel;
	HB_GUARD_CLEANUP(ret = tcp_channel_list_open(&service->channel_list, &channel));

	client_handle->data = channel;
	channel->priv = client_handle;
	channel->service = service;
	channel->read_state = TCP_CHANNEL_READ_HEADER;
	channel->state = TCP_CHANNEL_OPEN;
	channel->error_code = 0;
	channel->read_buffer = NULL;

	HB_GUARD_CLEANUP(ret = uv_read_start((uv_stream_t *)client_handle, on_recv_alloc_cb, on_recv_cb));

	char peerstr[255];
	int recvbuf = 0, sendbuff = 0;
	struct sockaddr_storage peeraddr;
	int addrlen = sizeof(peeraddr);
	HB_GUARD_CLEANUP(ret = uv_tcp_getpeername(client_handle, (struct sockaddr *)&peeraddr, &addrlen));
	HB_GUARD_CLEANUP(ret = hb_endpoint_convert_from(&channel->endpoint, &peeraddr));
	HB_GUARD_CLEANUP(ret = hb_endpoint_get_string(&channel->endpoint, peerstr, 255));

	HB_GUARD_CLEANUP(ret = uv_recv_buffer_size((uv_handle_t *)client_handle, &recvbuf));
	
	return;

cleanup:
	hb_log_error("connection error");
	hb_log_uv_error(ret);

	if (client_handle && !uv_is_closing((uv_handle_t *)client_handle)) {
		uv_close((uv_handle_t *)client_handle, on_close_release_cb);
	}
}

// --------------------------------------------------------------------------------------------------------------
void on_timer_cb(uv_timer_t *handle)
{
	int ret = UV_EINVAL;
	tcp_service_t *service = handle->data;
	HB_GUARD_NULL_CLEANUP(service);
	tcp_service_priv_t *priv = service->priv;

	if (handle != priv->uv_accept_timer) {
		return;
	}

	if (service->state != TCP_SERVICE_STARTED) {
		if (!uv_is_closing((uv_handle_t *)priv->tcp_handle)) uv_close((uv_handle_t *)priv->tcp_handle, NULL);
		if (!uv_is_closing((uv_handle_t *)priv->uv_accept_timer)) uv_close((uv_handle_t *)priv->uv_accept_timer, NULL);
		if (!uv_is_closing((uv_handle_t *)priv->uv_prep)) uv_close((uv_handle_t *)priv->uv_prep, NULL);
		if (!uv_is_closing((uv_handle_t *)priv->uv_check)) uv_close((uv_handle_t *)priv->uv_check, NULL);
	}

	return;

cleanup:
	hb_log_uv_error(ret);
}

// --------------------------------------------------------------------------------------------------------------
void on_prep_cb(uv_prepare_t *handle)
{
	int ret = UV_EINVAL;
	tcp_service_t *service = handle->data;
	HB_GUARD_NULL_CLEANUP(service);

	HB_GUARD_CLEANUP(ret = tcp_service_lock(service));
	return;

cleanup:
	hb_log_error("IO lock failed");
	hb_log_uv_error(ret);
}

// --------------------------------------------------------------------------------------------------------------
void on_check_cb(uv_check_t *handle)
{
	int ret = UV_EINVAL;
	tcp_service_t *service = handle->data;
	HB_GUARD_NULL_CLEANUP(service);

	HB_GUARD_CLEANUP(ret = tcp_service_unlock(service));
	return;

cleanup:
	hb_log_error("IO unlock failed");
	hb_log_uv_error(ret);
}

// --------------------------------------------------------------------------------------------------------------
void on_async_cb(uv_async_t *handle)
{
	int ret = UV_EINVAL;
	tcp_service_t *service = handle->data;
	HB_GUARD_NULL_CLEANUP(service);
	tcp_service_priv_t *priv = service->priv;

	return;

cleanup:
	hb_log_uv_error(ret);
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