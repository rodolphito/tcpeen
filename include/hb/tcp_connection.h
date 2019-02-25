#ifndef HB_TCP_CONNECTION_H
#define HB_TCP_CONNECTION_H

#include <stdint.h>

#include "uv.h"

#include "hb/allocator.h"
#include "hb/endpoint.h"
#include "hb/tcp_context.h"

#if _MSC_VER
#	define UV_BUFLEN_CAST(x) (ULONG)x
#else
#	define UV_BUFLEN_CAST(x) x
#endif


typedef enum connection_state_e {
	CS_NEW = 0,
	CS_CONNECTING,
	CS_CONNECTED,
	CS_DISCONNECTING,
	CS_DISCONNECTED,
} connection_state_t;


typedef struct tcp_conn_s {
	tcp_ctx_t *ctx;
	uv_tcp_t *tcp_handle;
	hb_endpoint_t host_local;
	hb_endpoint_t host_remote;
	int32_t read_err;
	int32_t write_err;
	int32_t state;
} tcp_conn_t;


/* this allows us to attach the buffer to the write req, which we will want to reference in the write cb */
typedef struct {
	uv_write_t req;
	uv_buf_t buf;
} tcp_write_req_t;


/* uv callbacks */
void on_tcp_close_cb(uv_handle_t *handle);
static void on_tcp_recv_alloc_cb(uv_handle_t *handle, size_t suggested_size, uv_buf_t *buf);
void on_tcp_recv_cb(uv_stream_t *stream, ssize_t nread, const uv_buf_t *buf);
void on_tcp_connected_cb(uv_connect_t *connection, int status);
void on_tcp_write_cb(uv_write_t *req, int status);


/* uv interface functions */
void tcp_write_begin(uv_tcp_t *tcp, char *data, int len, unsigned flags);
int tcp_connect_begin(tcp_conn_t *conn, const char *host, int port);


/* connection and state management */
tcp_conn_t *tcp_conn_new(tcp_ctx_t *ctx);
void tcp_conn_delete(tcp_conn_t **conn);
int tcp_conn_init(tcp_ctx_t *ctx, tcp_conn_t *conn);
void tcp_conn_disconnect(tcp_conn_t *conn);
void tcp_conn_close(tcp_conn_t *conn);
void tcp_get_conns(tcp_conn_t *conns, int count);


#endif