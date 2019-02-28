#include "tcp_service_internal.h"

#include "uv.h"

#include "hb/error.h"
#include "hb/log.h"
#include "hb/tcp_service.h"
#include "hb/tcp_connection.h"


#define HB_SERVICE_USE_BUFFER_POOL


uint64_t send_msgs = 0;
uint64_t send_bytes = 0;
uint64_t recv_msgs = 0;
uint64_t recv_bytes = 0;



// --------------------------------------------------------------------------------------------------------------
void on_close_release_cb(uv_handle_t* handle)
{
	tcp_channel_t *channel = handle->data;
	HB_GUARD_NULL_CLEANUP(channel);

#ifdef HB_SERVICE_USE_BUFFER_POOL
	if (channel->read_buffer) {
		hb_buffer_pool_release(&channel->service->pool, &channel->read_buffer);
		channel->read_buffer = NULL;
	}
#endif

cleanup:
	HB_MEM_RELEASE(handle);
}

// --------------------------------------------------------------------------------------------------------------
void on_close_cb(uv_handle_t *handle)
{
}

// --------------------------------------------------------------------------------------------------------------
void on_send_cb(uv_write_t *req, int status)
{
	tcp_service_write_req_t *write_req = (tcp_service_write_req_t *)req;
	assert(write_req);

#ifdef HB_SERVICE_USE_BUFFER_POOL
	tcp_service_t *service = write_req->service;
	assert(service);

	hb_buffer_t *buffer = write_req->buffer;
	assert(buffer);
#endif

	if (status) {
		hb_log_uv_error(status);
	} else {
		send_msgs++;
		send_bytes += write_req->uv_buf.len;
	}

#ifdef HB_SERVICE_USE_BUFFER_POOL
	hb_buffer_pool_release(&service->pool, &buffer);
#else
	HB_MEM_RELEASE(write_req->uv_buf.base);
#endif

	HB_MEM_RELEASE(write_req);
}

// --------------------------------------------------------------------------------------------------------------
void on_recv_alloc_cb(uv_handle_t *handle, size_t suggested_size, uv_buf_t *buf)
{
#ifdef HB_SERVICE_USE_BUFFER_POOL
	int ret;
	tcp_channel_t *channel = handle->data;
	if (!channel->read_buffer) {
		HB_GUARD_CLEANUP(ret = hb_buffer_pool_acquire(&channel->service->pool, &channel->read_buffer));
        HB_GUARD_CLEANUP(ret = hb_buffer_setup(channel->read_buffer));
	}

	uint8_t *bufbase = NULL;
	size_t buflen = 0;
	HB_GUARD_CLEANUP(hb_buffer_write_ptr(channel->read_buffer, &bufbase, &buflen));

	buf->base = bufbase;
	buf->len = UV_BUFLEN_CAST(buflen);

	return;

cleanup:
    hb_log_uv_error(ret);
	if (channel->read_buffer) {
		hb_buffer_pool_release(&channel->service->pool, &channel->read_buffer);
		channel->read_buffer = NULL;
	}
	buf->base = NULL;
	buf->len = 0;
#else
	if ((buf->base = HB_MEM_ACQUIRE(suggested_size))) {
		buf->len = UV_BUFLEN_CAST(suggested_size);
		return;
	}
	buf->base = NULL;
	buf->len = 0;
#endif
}

// --------------------------------------------------------------------------------------------------------------
void on_recv_cb(uv_stream_t *handle, ssize_t nread, const uv_buf_t *buf)
{
	int ret = UV_EINVAL;
	tcp_service_write_req_t *send_req = NULL;
	
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

#ifdef HB_SERVICE_USE_BUFFER_POOL
	hb_buffer_add_length(channel->read_buffer, nread);
#endif

	recv_msgs++;
	recv_bytes += nread;

	if (!(send_req = HB_MEM_ACQUIRE(sizeof(*send_req)))) {
		hb_log_uv_error(ENOMEM);
		goto close;
	}

	send_req->uv_req.data = channel->read_buffer;
	send_req->uv_buf = uv_buf_init(buf->base, UV_BUFLEN_CAST(nread));

#ifdef HB_SERVICE_USE_BUFFER_POOL
	send_req->service = channel->service;
	send_req->buffer = channel->read_buffer;
#endif
	if ((ret = uv_write((uv_write_t *)send_req, (uv_stream_t *)handle, &send_req->uv_buf, 1, on_send_cb))) {
		hb_log_uv_error((int)nread);
		goto close;
	}

#ifdef HB_SERVICE_USE_BUFFER_POOL
	channel->read_buffer = NULL;
#endif

	return;

close:
	if (!uv_is_closing((uv_handle_t *)handle)) uv_close((uv_handle_t *)handle, on_close_release_cb);

cleanup:
	if (send_req) HB_MEM_RELEASE(send_req);

#ifndef HB_SERVICE_USE_BUFFER_POOL
	if (buf && buf->base) HB_MEM_RELEASE(buf->base);
#endif
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
		if (!uv_is_closing((uv_handle_t *)priv->tcp_handle)) uv_close((uv_handle_t *)priv->tcp_handle, on_close_cb);
		if (!uv_is_closing((uv_handle_t *)priv->uv_accept_timer)) uv_close((uv_handle_t *)priv->uv_accept_timer, on_close_cb);
		if (!uv_is_closing((uv_handle_t *)priv->uv_prep)) uv_close((uv_handle_t *)priv->uv_prep, on_close_cb);
		if (!uv_is_closing((uv_handle_t *)priv->uv_check)) uv_close((uv_handle_t *)priv->uv_check, on_close_cb);
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
		uv_close(handle, on_close_cb);
	}
}