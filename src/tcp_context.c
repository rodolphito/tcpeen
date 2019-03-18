#include "tn/tcp_context.h"

#include <string.h>

#include "tn/error.h"
#include "tn/allocator.h"


// --------------------------------------------------------------------------------------------------------------
tcp_ctx_t *tcp_context_new()
{
	tcp_ctx_t *ctx;

	if (!(ctx = TN_MEM_ACQUIRE(sizeof(tcp_ctx_t)))) {
		return NULL;
	}
	memset(ctx, 0, sizeof(tcp_ctx_t));
	if (!(ctx->config = TN_MEM_ACQUIRE(sizeof(tcp_ctx_config_t)))) {
		tcp_context_delete(&ctx);
		return NULL;
	}

	return ctx;
}

// --------------------------------------------------------------------------------------------------------------
void tcp_context_delete(tcp_ctx_t **pctx)
{
	if (!pctx) return;

	tcp_ctx_t *ctx = *pctx;
	if (!ctx) return;

	if (ctx->config) {
		TN_MEM_RELEASE(ctx->config);
	}

	TN_MEM_RELEASE(ctx);
	ctx = NULL;
}

// --------------------------------------------------------------------------------------------------------------
int tcp_context_set_config(tcp_ctx_t *ctx, tcp_ctx_config_t *cfg)
{
	if (!SAFE_TCP_CTX(ctx)) return EINVAL;
	if (!cfg) return EINVAL;

	memcpy(ctx->config, cfg, sizeof(tcp_ctx_config_t));

	return TN_SUCCESS;
}

// --------------------------------------------------------------------------------------------------------------
uv_loop_t *tcp_context_get_loop(tcp_ctx_t *ctx)
{
	if (!SAFE_TCP_CTX(ctx)) return NULL;
	return ctx->config->uv_loop;
}