#ifndef HB_TCP_CONTEXT_H
#define HB_TCP_CONTEXT_H

#include <stdint.h>

#include "uv.h"

#define SAFE_TCP_CTX(ctx) (ctx && ctx->config)


typedef struct tcp_ctx_config_s {
	uv_loop_t *uv_loop;
} tcp_ctx_config_t;

typedef struct tcp_ctx_s {
	uv_loop_t *uv_loop;
	tcp_ctx_config_t *config;
	uint64_t recv_msgs;
	uint64_t recv_bytes;
	uint64_t send_msgs;
	uint64_t send_bytes;
	uint64_t latency_total;
	uint64_t latency_max;
	uint64_t tstamp_first_msg;
	uint64_t tstamp_last_msg;
	uint64_t current_msg_id;
	uint64_t last_recv_msg_id;
} tcp_ctx_t;

tcp_ctx_t *tcp_context_new();
void tcp_context_delete(tcp_ctx_t **ctx);
int tcp_context_set_config(tcp_ctx_t *ctx, tcp_ctx_config_t *cfg);
uv_loop_t *tcp_context_get_loop(tcp_ctx_t *ctx);

#endif