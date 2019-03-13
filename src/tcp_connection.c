#include "hb/tcp_connection.h"

#include "aws/common/byte_buf.h"
#include "aws/common/clock.h"

#include "hb/error.h"
#include "hb/allocator.h"
#include "hb/log.h"


// lib uv async callbacks
// all callbacks take the form on_****_cb
// (on_ prefix and _cb suffix)
// none of these should ever be called directly

// this callback is registered by uv_close
// --------------------------------------------------------------------------------------------------------------
void on_tcp_close_cb(uv_handle_t *handle)
{
	tcp_conn_t *conn = (tcp_conn_t *)handle->data;
	if (conn) {
		conn->state = CS_DISCONNECTED;
		conn->tcp_handle = NULL;
	}
	//HB_MEM_RELEASE(handle);
}

// this callback is registered by uv_start_read
// flow: tcp_connect_begin:uv_tcp_connect -> on_connect_cb:uv_start_read -> on_read_cb
// --------------------------------------------------------------------------------------------------------------
static void on_tcp_recv_alloc_cb(uv_handle_t *handle, size_t suggested_size, uv_buf_t *buf)
{
	buf->base = HB_MEM_ACQUIRE(suggested_size);
	if (!buf->base) {
		return;
	}
	buf->len = UV_BUFLEN_CAST(suggested_size);
}

// this callback is registered by uv_start_read
// flow: tcp_connect_begin:uv_tcp_connect -> on_connect_cb:uv_start_read -> on_read_cb
// --------------------------------------------------------------------------------------------------------------
void on_tcp_recv_cb(uv_stream_t *stream, ssize_t nread, const uv_buf_t *buf)
{
	int should_close = 0;
	uint64_t cur_ticks = hb_tstamp();
	uint64_t latency = 0;
	uint64_t msg_id = 0;
	uint32_t msg_len = 0;
	uint32_t msg_remaining = 0;

	tcp_conn_t *conn = (tcp_conn_t *)stream->data;


	if (nread < 0) {
		conn->read_err = (int)nread;
		should_close = 1;
		goto cleanup;
	} else if (nread == 0) {
		goto cleanup;
	}

	HB_GUARD_CLEANUP(hb_buffer_write(conn->next_buffer, buf->base, nread));
	int processing = 1;
	while (processing) {
		if (conn->read_state == TCP_CHANNEL_READ_HEADER) {
			if (hb_buffer_read_length(conn->next_buffer) < sizeof(uint32_t)) {
				processing = 0;
			} else {
				HB_GUARD_CLEANUP(hb_buffer_read_be32(conn->next_buffer, &conn->next_payload_len));
				//hb_log_trace("next payload: %u", conn->next_payload_len);
				conn->read_state = TCP_CHANNEL_READ_PAYLOAD;
			}
		} else if (conn->read_state == TCP_CHANNEL_READ_PAYLOAD) {
			if (hb_buffer_read_length(conn->next_buffer) < conn->next_payload_len) {
				processing = 0;
			} else {
				HB_GUARD_CLEANUP(hb_buffer_read_be64(conn->next_buffer, &msg_id));
				HB_GUARD_CLEANUP(hb_buffer_read_be64(conn->next_buffer, &latency));

				msg_remaining = conn->next_payload_len - 16;
				HB_GUARD_CLEANUP(hb_buffer_read_skip(conn->next_buffer, msg_remaining));

				latency = aws_timestamp_convert(cur_ticks - latency, AWS_TIMESTAMP_NANOS, AWS_TIMESTAMP_MILLIS, 0);

				//hb_log_debug("len: %u, id: %zu, latency: %zu", msg_len, msg_id, latency);
				const uint64_t expected = (conn->last_recv_msg_id + 1);
				if (msg_id != expected) {
					hb_log_warning("total len: %zd -- span len: %u -- expected id: %zu but recvd id: %zu", nread, conn->next_payload_len, expected, msg_id);
				} else {
					conn->last_recv_msg_id = expected;
					//hb_log_debug("total len: %zd -- span len: %u -- id match: %zu", nread, conn->next_payload_len, msg_id);
				}

				conn->read_state = TCP_CHANNEL_READ_HEADER;
			}
		}
	}

	if (!hb_buffer_read_length(conn->next_buffer)) {
		hb_buffer_reset(conn->next_buffer);
	}

	conn->latency_total += latency;
	if (latency > conn->latency_max) {
		conn->latency_max = latency;
	}

	conn->tstamp_last_msg = cur_ticks;
	conn->recv_msgs++;
	conn->recv_bytes += nread;
	conn->ctx->recv_msgs++;
	conn->ctx->recv_bytes += nread;

cleanup:
	HB_MEM_RELEASE(buf->base);

	if (should_close) {
		tcp_conn_disconnect(conn);
	}
}

// this callback is registered by uv_tcp_connect (for client -> server connections)
// flow: tcp_connect_begin:uv_tcp_connect -> on_connect_cb
// --------------------------------------------------------------------------------------------------------------
void on_tcp_connected_cb(uv_connect_t *connection, int status)
{
	int ret;
	tcp_conn_t *conn = (tcp_conn_t *)connection->data;

	if (status < 0) {
		hb_log_uv_error(status);
		goto fail;
	}

	conn->state = CS_CONNECTED;
	conn->tcp_handle = (uv_tcp_t *)connection->handle;
	conn->tcp_handle->data = (void *)conn;

	if ((ret = uv_read_start((uv_stream_t *)conn->tcp_handle, on_tcp_recv_alloc_cb, on_tcp_recv_cb))) {
		hb_log_uv_error(ret);
		goto fail;
	}

	goto cleanup;

fail:
	tcp_conn_disconnect(conn);

cleanup:
	HB_MEM_RELEASE(connection);

	return;
}

void on_tcp_write_frag_cb(uv_write_t *req, int status)
{
	int should_close = 0;
	uint64_t cur_ticks = hb_tstamp();

	tcp_write_req_t *write_req = (tcp_write_req_t *)req;
	tcp_conn_t *conn = req->data;
	uv_buf_t *wbuf = &write_req->buf;

	if (status) {
		//hb_log_uv_error(status);
		should_close = 1;
		conn->write_err = status;
	} else if (!wbuf->len) {
	} else {
		if (!conn->tstamp_first_msg) {
			conn->tstamp_first_msg = cur_ticks;
		}
		conn->send_msgs++;
		conn->send_bytes += wbuf->len;
		conn->ctx->send_msgs++;
		conn->ctx->send_bytes += wbuf->len;
	}

	HB_MEM_RELEASE(req);

	if (conn->state != CS_CONNECTED) should_close = 1;
	if (should_close) tcp_conn_disconnect(conn);
}

// this callback is registered by uv_write
// flow: tcp_write_begin:uv_write -> on_write_cb
// --------------------------------------------------------------------------------------------------------------
void on_tcp_write_cb(uv_write_t *req, int status)
{
	int should_close = 0;
	uint64_t cur_ticks = hb_tstamp();

	tcp_write_req_t *write_req = (tcp_write_req_t *)req;
	tcp_conn_t *conn = req->data;
	uv_buf_t *wbuf = &write_req->buf;

	if (status) {
		//hb_log_uv_error(status);
		should_close = 1;
		conn->write_err = status;
	} else if (!wbuf->len) {
	} else {
		if (!conn->tstamp_first_msg) {
			conn->tstamp_first_msg = cur_ticks;
		}
		conn->send_msgs++;
		conn->send_bytes += wbuf->len;
		conn->ctx->send_msgs++;
		conn->ctx->send_bytes += wbuf->len;
	}

	HB_MEM_RELEASE(wbuf->base);
	HB_MEM_RELEASE(req);

	if (conn->state != CS_CONNECTED) should_close = 1;
	if (should_close) tcp_conn_disconnect(conn);
}


// async initiator methods 
// all initiators take the form ****_begin (suffixed with _begin)
// any method named like this will initiate an async operation
// and install the appropriate callbacks
// TODO: return int status code
// TODO: cleanup memory on error
// --------------------------------------------------------------------------------------------------------------
void tcp_write_begin(uv_tcp_t *tcp_handle, char *data, int len, unsigned flags)
{
	int ret;
	uint64_t cur_ticks = hb_tstamp();

	if (!tcp_handle) return;
	if (!tcp_handle->data) return;
	if (!data) return;
	if (!len) return;

	tcp_conn_t *conn = (tcp_conn_t *)tcp_handle->data;
	if (conn->state == CS_CONNECTING) {
		conn->state = CS_CONNECTED;
	} else if (conn->state != CS_CONNECTED) {
		return;
	}
	conn->current_msg_id++;

	struct aws_byte_buf bb_data;
	if (aws_byte_buf_init(&bb_data, &hb_aws_default_allocator, len + sizeof(uint32_t) + sizeof(uint64_t) + sizeof(uint64_t) + 1) != AWS_OP_SUCCESS) {
		hb_log_error("aws_byte_buf_init failed");
		return;
	}

	/* hard code BE 32 bit int prefix for protocol message length */
	if (!aws_byte_buf_write_be32(&bb_data, (uint32_t)(len + sizeof(uint64_t) + sizeof(uint64_t) + 1))) {
		hb_log_error("aws_byte_buf_write_be32 failed");
		return;
	}

	/* msg id */
	if (!aws_byte_buf_write_be64(&bb_data, conn->current_msg_id)) {
		hb_log_error("aws_byte_buf_write_be64 failed");
		return;
	}

	/* timestamp */
	if (!aws_byte_buf_write_be64(&bb_data, cur_ticks)) {
		hb_log_error("aws_byte_buf_write_be64 failed");
		return;
	}

	/* payload */
	if (!aws_byte_buf_write(&bb_data, (uint8_t *)data, len)) {
		hb_log_error("aws_byte_buf_write failed");
	}

	/* null byte */
	if (!aws_byte_buf_write_u8(&bb_data, '\0')) {
		hb_log_error("aws_byte_buf_write failed");
	}

	//hb_log_trace("sending: %d -> %zu -- %zu -- %zu", len, bb_data.len, conn->current_msg_id, cur_ticks);

	int fake_frag = 0;

	tcp_write_req_t *write_req = HB_MEM_ACQUIRE(sizeof(*write_req));
	if (!write_req) {
		hb_log_uv_error(ENOMEM);
		return;
	}
	write_req->req.data = conn;
	write_req->buf = uv_buf_init(bb_data.buffer, UV_BUFLEN_CAST(bb_data.len - fake_frag));

	if ((ret = uv_write((uv_write_t *)write_req, (uv_stream_t *)tcp_handle, &write_req->buf, 1, on_tcp_write_cb))) {
		hb_log_uv_error(ret);

		HB_MEM_RELEASE(write_req->buf.base);
		HB_MEM_RELEASE(write_req);

		tcp_conn_disconnect(conn);
		return;
	}

	if (fake_frag) {
		//write_req = HB_MEM_ACQUIRE(sizeof(*write_req));
		//if (!write_req) {
		//	hb_log_uv_error(ENOMEM);
		//	return;
		//}
		//write_req->req.data = conn;
		//write_req->buf = uv_buf_init(bb_data.buffer + (bb_data.len - fake_frag), UV_BUFLEN_CAST(fake_frag));

		//if ((ret = uv_write((uv_write_t *)write_req, (uv_stream_t *)tcp_handle, &write_req->buf, 1, on_tcp_write_frag_cb))) {
		//	hb_log_uv_error(ret);

		//	HB_MEM_RELEASE(write_req->buf.base);
		//	HB_MEM_RELEASE(write_req);

		//	tcp_conn_disconnect(conn);
		//	return;
		//}
	}
}


// --------------------------------------------------------------------------------------------------------------
int tcp_connect_begin(tcp_conn_t *conn, const char *host, int port)
{
	int ret;
	uv_loop_t *loop;

	// TODO: holy mother of god clean this up into guard macros or something
	if (!conn) return EINVAL;
	if (conn->state != CS_NEW && conn->state != CS_DISCONNECTED) return EINVAL;
	if (!conn->ctx) return EINVAL;
	if (!(loop = tcp_context_get_loop(conn->ctx))) return EINVAL;

	conn->tcp_handle = NULL;
	uv_connect_t *connection = NULL;

	conn->tcp_handle = HB_MEM_ACQUIRE(sizeof(*conn->tcp_handle));
	if (!conn->tcp_handle) {
		hb_log_uv_error(ENOMEM);
		return ENOMEM;
	}

	if ((ret = uv_tcp_init(loop, conn->tcp_handle))) {
		hb_log_uv_error(ret);
		goto cleanup;
	}
	conn->tcp_handle->data = conn;

	if ((ret = hb_endpoint_set(&conn->host_remote, host, port))) {
		hb_log_uv_error(ret);
		goto cleanup;
	}

	if ((ret = hb_endpoint_set(&conn->host_local, "0.0.0.0", 0))) {
		hb_log_uv_error(ret);
		goto cleanup;
	}

	if ((ret = uv_tcp_nodelay(conn->tcp_handle, 1))) {
		hb_log_uv_error(ret);
		goto cleanup;
	}

	if ((ret = uv_tcp_bind(conn->tcp_handle, (const struct sockaddr *)&conn->host_local.sockaddr, 0))) {
		hb_log_uv_error(ret);
		goto cleanup;
	}

	if (!(connection = HB_MEM_ACQUIRE(sizeof(uv_connect_t)))) {
		hb_log_uv_error(ENOMEM);
		goto cleanup;
	}

	connection->data = (void *)conn;
	conn->state = CS_CONNECTING;
	if ((ret = uv_tcp_connect(connection, conn->tcp_handle, (const struct sockaddr *)&conn->host_remote.sockaddr, on_tcp_connected_cb))) {
		// hb_log_uv_error(ret);
		goto cleanup;
	}

	return HB_SUCCESS;

cleanup:
	if (connection) HB_MEM_RELEASE(connection);
	tcp_conn_disconnect(conn);

	return HB_ERROR;
}

// --------------------------------------------------------------------------------------------------------------
tcp_conn_t *tcp_conn_new(tcp_ctx_t *ctx)
{
	tcp_conn_t *conn;

	if (!ctx) return NULL;

	if (!(conn = HB_MEM_ACQUIRE(sizeof(tcp_conn_t)))) {
		return NULL;
	}

	memset(conn, 0, sizeof(tcp_conn_t));

	conn->ctx = ctx;

	return conn;
}

// --------------------------------------------------------------------------------------------------------------
void tcp_conn_delete(tcp_conn_t **pconn)
{
	if (!pconn) return;

	tcp_conn_t *conn = *pconn;
	if (!conn) return;

	HB_MEM_RELEASE(conn);
	conn = NULL;
}

// --------------------------------------------------------------------------------------------------------------
int tcp_conn_init(tcp_ctx_t *ctx, tcp_conn_t *conn)
{
	assert(conn && ctx);
	
	memset(conn, 0, sizeof(tcp_conn_t));
	conn->ctx = ctx;
	conn->read_state = TCP_CHANNEL_READ_HEADER;

	HB_GUARD_NULL(conn->next_buf = HB_MEM_ACQUIRE(65535 * 2));
	HB_GUARD_NULL(conn->next_buffer = HB_MEM_ACQUIRE(sizeof(*conn->next_buffer)));
	HB_GUARD(hb_buffer_setup(conn->next_buffer, conn->next_buf, 65535 * 2));

	return HB_SUCCESS;
}

// --------------------------------------------------------------------------------------------------------------
void tcp_conn_disconnect(tcp_conn_t *conn)
{
	if (!conn) return;
	if (conn->state == CS_DISCONNECTED || conn->state == CS_DISCONNECTING) return;
	if (!conn->tcp_handle) return;

	if (!uv_is_closing((uv_handle_t *)conn->tcp_handle)) {
		conn->state = CS_DISCONNECTING;
		uv_close((uv_handle_t *)conn->tcp_handle, on_tcp_close_cb);
	}
}

// --------------------------------------------------------------------------------------------------------------
void tcp_conn_close(tcp_conn_t *conn)
{
	tcp_conn_disconnect(conn);
}

// --------------------------------------------------------------------------------------------------------------
void tcp_get_conns(tcp_conn_t *conns, int count)
{
	tcp_ctx_t *ctx = tcp_context_new();
	for (int i = 0; i < count; i++) {
		tcp_conn_init(ctx, &conns[i]);
		//conns[i].stream = NULL;
		conns[i].read_err = 1;
		conns[i].write_err = 2;
		conns[i].state = CS_DISCONNECTED;
	}
}