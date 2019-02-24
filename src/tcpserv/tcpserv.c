#include "tcpserv/tcpserv.h"

#include <stdlib.h>

#include "hb/error.h"
#include "hb/log.h"
#include "hb/tcp_connection.h"


uint64_t send_msgs = 0;
uint64_t send_bytes = 0;
uint64_t recv_msgs = 0;
uint64_t recv_bytes = 0;

uvu_thread_private_t *uvu_thread_priv = NULL;
uv_thread_t uvu_thread = (uv_thread_t)NULL;
uv_loop_t *uvu_loop = NULL;
uv_tcp_t *uvu_tcp_server = NULL;
static uv_timer_t *uvu_accept_timer = NULL;
int uvu_closing = 0;


void free_write_req(uv_write_t *req)
{
	tcp_write_req_t *send_req = (tcp_write_req_t *)req;
	HB_MEM_RELEASE(send_req->buf.base);
	HB_MEM_RELEASE(send_req);
}

void on_recv_alloc_cb(uv_handle_t *handle, size_t suggested_size, uv_buf_t *buf)
{
	buf->base = (char*)HB_MEM_ACQUIRE(suggested_size);
	buf->len = UV_BUFLEN_CAST(suggested_size);
}

void on_close_cb(uv_handle_t* handle)
{
	HB_MEM_RELEASE(handle);
}

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
	if (!uv_is_closing((uv_handle_t *)handle)) uv_close((uv_handle_t *)handle, on_close_cb);

cleanup:
	HB_MEM_RELEASE(buf->base);
	HB_MEM_RELEASE(send_req);
}




void on_connection_cb(uv_stream_t *server, int status)
{
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

	uv_tcp_init(uvu_loop, client);
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
		uv_close((uv_handle_t *)client, on_close_cb);
	}
}


int uvu_server_start(const char *ipstr, uint16_t port)
{
	uvu_closing = 0;

	int uvret;
	if (uvu_thread_priv) {
		return UV_UNKNOWN;
	}

	uvu_thread_priv = (uvu_thread_private_t *)HB_MEM_ACQUIRE(sizeof(uvu_thread_private_t));
	if (!uvu_thread_priv) {
		return UV_ENOMEM;
	}
	memset(uvu_thread_priv, 0, sizeof(uvu_thread_priv));

	if ((uvret = uv_ip6_addr(ipstr, port, (struct sockaddr_in6 *)&uvu_thread_priv->listen_addr))) {
		if ((uvret = uv_ip4_addr(ipstr, port, (struct sockaddr_in *)&uvu_thread_priv->listen_addr))) {
			return uvret;
		}
	}

	char ipbuf[255];
	uint16_t ipport = 0;
	memset(&ipbuf, 0, sizeof(ipbuf));
	if (uvu_thread_priv->listen_addr.ss_family == AF_INET6) {
		struct sockaddr_in6 *sockaddr = (struct sockaddr_in6 *)&uvu_thread_priv->listen_addr;
		uv_ip6_name(sockaddr, ipbuf, sizeof(ipbuf));
		ipport = sockaddr->sin6_port;
	} else if (uvu_thread_priv->listen_addr.ss_family == AF_INET) {
		struct sockaddr_in *sockaddr = (struct sockaddr_in *)&uvu_thread_priv->listen_addr;
		uv_ip4_name(sockaddr, ipbuf, sizeof(ipbuf));
		ipport = sockaddr->sin_port;
	} else {
		return 1;
	}
	hb_log_info("Listening on %s:%d\n", ipbuf, ntohs(ipport));

	if ((uvret = uv_thread_create(&uvu_thread, uvu_server_run, (void *)uvu_thread_priv))) {
		return uvret;
	}

	return 0;
}


static void close_cb(uv_handle_t *handle)
{
	// if (!handle) return;

	// HB_MEM_RELEASE(handle);
	// handle = NULL;
}

static void timer_cb(uv_timer_t *handle)
{
	if (handle != uvu_accept_timer)
		return;

	if (uvu_closing) {
		if (!uv_is_closing((uv_handle_t *)uvu_tcp_server)) uv_close((uv_handle_t *)uvu_tcp_server, close_cb);
		if (!uv_is_closing((uv_handle_t *)uvu_accept_timer)) uv_close((uv_handle_t *)uvu_accept_timer, close_cb);
	}
}

void walk_cb(uv_handle_t *handle, void *arg)
{
	if (uv_is_closing(handle)) return;

	uv_close(handle, close_cb);
}

void async_cb(uv_async_t *handle)
{
	//uv_close((uv_handle_t *)uvu_accept_timer, close_cb);
	//uv_close((uv_handle_t *)uvu_udp_server, close_cb);
	uv_close((uv_handle_t *)handle, close_cb);
	uvu_closing = 1;
	//uv_stop(uvu_loop);
}

int uvu_server_stop()
{
	if (uvu_closing) return -1;

	int uvret;

	uv_async_t *async = HB_MEM_ACQUIRE(sizeof(async));
	uv_async_init(uvu_loop, async, async_cb);
	uv_async_send(async);

	uvret = uv_thread_join(&uvu_thread);
	if (uvret) {
		hb_log_uv_error(uvret);
	}

	if (uvu_thread_priv) {
		HB_MEM_RELEASE(uvu_thread_priv);
	}

	return 0;
}

void shutdown_walk_cb(uv_handle_t* handle, void* arg)
{
	if (!uv_is_closing(handle)) {
		printf("Manually closing handle: %p -- %s\n", handle, uv_handle_type_name(uv_handle_get_type(handle)));
		uv_close(handle, on_close_cb);
	}
}


void uvu_server_run_cleanup()
{
	if (uvu_accept_timer) HB_MEM_RELEASE(uvu_accept_timer);
	if (uvu_tcp_server) HB_MEM_RELEASE(uvu_tcp_server);
	if (uvu_loop) HB_MEM_RELEASE(uvu_loop);
}


// TODO : CLEANUP ON ERROR
void uvu_server_run(void *priv_data)
{
	int uvret = 0;
	size_t blocks = 10;
	size_t block_size = 512;
	uvu_thread_private_t *thread_priv = (uvu_thread_private_t *)priv_data;

	uvu_loop = NULL;
	uvu_tcp_server = NULL;
	uvu_accept_timer = NULL;


	if (!(uvu_loop = HB_MEM_ACQUIRE(sizeof(uv_loop_t)))) {
		hb_log_uv_error(UV_ENOMEM);
		goto error;
	}

	if (!(uvu_tcp_server = HB_MEM_ACQUIRE(sizeof(uv_udp_t)))) {
		hb_log_uv_error(UV_ENOMEM);
		goto error;
	}

	if (!(uvu_accept_timer = HB_MEM_ACQUIRE(sizeof(uv_timer_t)))) {
		hb_log_uv_error(UV_ENOMEM);
		goto error;
	}

	if ((uvret = uv_loop_init(uvu_loop))) {
		hb_log_uv_error(uvret);
		goto error;
	}

	if ((uvret = uv_timer_init(uvu_loop, uvu_accept_timer))) {
		hb_log_uv_error(uvret);
		goto error;
	}

	if ((uvret = uv_timer_start(uvu_accept_timer, timer_cb, 500, 500))) {
		hb_log_uv_error(uvret);
		goto error;
	}

	if ((uvret = uv_tcp_init(uvu_loop, uvu_tcp_server))) {
		hb_log_uv_error(uvret);
		goto error;
	}

	if ((uvret = uv_tcp_nodelay(uvu_tcp_server, 1))) {
		hb_log_uv_error(uvret);
		goto error;
	}

	if ((uvret = uv_tcp_bind(uvu_tcp_server, (const struct sockaddr *)&thread_priv->listen_addr, UV_UDP_PARTIAL))) {
		hb_log_uv_error(uvret);
		goto error;
	}

	if ((uvret = uv_listen((uv_stream_t *)uvu_tcp_server, 1024, on_connection_cb))) {
		hb_log_uv_error(uvret);
		goto error;
	}

	if ((uvret = uv_run(uvu_loop, UV_RUN_DEFAULT))) {
		hb_log_uv_error(uvret);
		goto error;
	}

	if ((uvret = uv_loop_close(uvu_loop))) {
		uv_walk(uvu_loop, shutdown_walk_cb, NULL);
		if ((uvret = uv_loop_close(uvu_loop))) {
			hb_log_uv_error(uvret);
			goto error;
		}
		hb_log_error("Walked loop and shutdown cleanly\n");
	}

	hb_log_info("Send: %zu -- %zu\n\n", send_msgs, send_bytes);
	hb_log_info("Recv: %zu -- %zu\n\n", recv_msgs, recv_bytes);

	uvu_server_run_cleanup();
	return;

error:
	uvu_server_run_cleanup();
	return;
}

int main(void)
{
	uvu_server_start("0.0.0.0", 7777);

	getchar();

	uvu_server_stop();

	return 0;
}