#ifndef HB_TCP_SERVICE_INTERNAL_H
#define HB_TCP_SERVICE_INTERNAL_H

#include <stdint.h>

#include "uv.h"

#include "hb/tcp_service.h"


// priv tcp service impl
typedef struct tcp_service_priv_s {
	uv_loop_t *uv_loop;
	uv_tcp_t *tcp_handle;
	uv_timer_t *uv_accept_timer;
	uv_prepare_t *uv_prep;
	uv_check_t *uv_check;
} tcp_service_priv_t;

typedef struct tcp_service_write_req_s {
	uv_write_t uv_req;
	uv_buf_t uv_buf;
	tcp_channel_t *channel;
	hb_buffer_t *buffer;
	uint64_t id;
} tcp_service_write_req_t;

void on_close_release_cb(uv_handle_t* handle);
void on_close_cb(uv_handle_t *handle);
void on_send_cb(uv_write_t *req, int status);
void on_recv_alloc_cb(uv_handle_t *handle, size_t suggested_size, uv_buf_t *buf);
void on_recv_cb(uv_stream_t *handle, ssize_t nread, const uv_buf_t *buf);
void on_connection_cb(uv_stream_t *server, int status);
void on_timer_cb(uv_timer_t *handle);
void on_prep_cb(uv_prepare_t *handle);
void on_check_cb(uv_check_t *handle);
void on_async_cb(uv_async_t *handle);
void on_walk_cb(uv_handle_t* handle, void* arg);


#endif