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

tcp_service_t tcp_service = {
	.priv = NULL,
	.state = TCP_SERVICE_NEW,
};


// --------------------------------------------------------------------------------------------------------------
void on_close_release_cb(uv_handle_t* handle)
{
	HB_MEM_RELEASE(handle);
}

// --------------------------------------------------------------------------------------------------------------
void on_close_cb(uv_handle_t *handle)
{
}

// --------------------------------------------------------------------------------------------------------------
void free_write_req(uv_write_t *req)
{
	tcp_write_req_t *send_req = (tcp_write_req_t *)req;
	HB_MEM_RELEASE(send_req->buf.base);
	HB_MEM_RELEASE(send_req);
}

// --------------------------------------------------------------------------------------------------------------
void on_send_cb(uv_write_t *req, int status)
{
	if (status) {
		hb_log_uv_error(status);
	} else {
		tcp_write_req_t *send_req = (tcp_write_req_t *)req;
		send_msgs++;
		send_bytes += send_req->buf.len;
	}
	free_write_req(req);
}

// --------------------------------------------------------------------------------------------------------------
void on_recv_alloc_cb(uv_handle_t *handle, size_t suggested_size, uv_buf_t *buf)
{
	buf->base = HB_MEM_ACQUIRE(suggested_size);
	buf->len = UV_BUFLEN_CAST(suggested_size);
}

// --------------------------------------------------------------------------------------------------------------
void on_recv_cb(uv_stream_t *handle, ssize_t nread, const uv_buf_t *buf)
{
	int ret;
	tcp_write_req_t *send_req = NULL;

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
	} else if (nread == 0) {
		// this isn't an error, its an empty packet or other non error event with nothing to read
		goto cleanup;
	}

	recv_msgs++;
	recv_bytes += nread;

	if (!(send_req = HB_MEM_ACQUIRE(sizeof(*send_req)))) {
		hb_log_uv_error(ENOMEM);
		goto close;
	}

	send_req->buf = uv_buf_init(buf->base, UV_BUFLEN_CAST(nread));
	if ((ret = uv_write((uv_write_t *)send_req, (uv_stream_t *)handle, &send_req->buf, 1, on_send_cb))) {
		hb_log_uv_error((int)nread);
		goto close;
	}

	return;

close:
	if (!uv_is_closing((uv_handle_t *)handle)) uv_close((uv_handle_t *)handle, on_close_release_cb);

cleanup:
	HB_MEM_RELEASE(buf->base);
	HB_MEM_RELEASE(send_req);
}

// --------------------------------------------------------------------------------------------------------------
void on_connection_cb(uv_stream_t *server, int status)
{
	tcp_service_priv_t *priv = tcp_service.priv;

	int ret;
	uv_tcp_t *client = NULL;

	if (status < 0) {
		hb_log_uv_error(status);
		return;
	}

	if (!(client = malloc(sizeof(*client)))) {
		hb_log_uv_error(ENOMEM);
		goto cleanup;
	}

	uv_tcp_init(priv->uv_loop, client);
	if ((ret = uv_accept(server, (uv_stream_t *)client))) {
		hb_log_uv_error(ret);
		goto cleanup;
	}

	if ((ret = uv_read_start((uv_stream_t *)client, on_recv_alloc_cb, on_recv_cb))) {
		hb_log_uv_error(ret);
		goto cleanup;
	}

	// TODO: new connection event

	return;

cleanup:
	if (client && !uv_is_closing((uv_handle_t *)client)) {
		uv_close((uv_handle_t *)client, on_close_release_cb);
	}
}

// --------------------------------------------------------------------------------------------------------------
void on_timer_cb(uv_timer_t *handle)
{
	tcp_service_priv_t *priv = tcp_service.priv;
	if (handle != priv->uv_accept_timer) {
		return;
	}

	if (priv->uv_closing) {
		if (!uv_is_closing((uv_handle_t *)priv->tcp_handle)) uv_close((uv_handle_t *)priv->tcp_handle, on_close_cb);
		if (!uv_is_closing((uv_handle_t *)priv->uv_accept_timer)) uv_close((uv_handle_t *)priv->uv_accept_timer, on_close_cb);
	}
}

// --------------------------------------------------------------------------------------------------------------
void on_async_cb(uv_async_t *handle)
{
	tcp_service_priv_t *priv = tcp_service.priv;

	//uv_close((uv_handle_t *)uvu_accept_timer, close_cb);
	//uv_close((uv_handle_t *)uvu_udp_server, close_cb);
	uv_close((uv_handle_t *)handle, on_close_release_cb);

	priv->uv_closing = 1;
	//uv_stop(uvu_loop);
}

// --------------------------------------------------------------------------------------------------------------
void on_walk_cb(uv_handle_t* handle, void* arg)
{
	if (!uv_is_closing(handle)) {
		hb_log_warning("Manually closing handle: %p -- %s\n", handle, uv_handle_type_name(uv_handle_get_type(handle)));
		uv_close(handle, on_close_cb);
	}
}