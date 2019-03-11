// #define UV_THREAD_HANDLE_DEBUG

// system includes
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <limits.h>

// statically built dependency includes
#include "aws/common/atomics.h"
#include "aws/common/byte_buf.h"
#include "aws/common/array_list.h"
#include "aws/common/clock.h"
#include "uv.h"

// project includes
#include "tcplg/cmdargs.h"
#include "hb/error.h"
#include "hb/allocator.h"
#include "hb/log.h"
#include "hb/tcp_context.h"
#include "hb/tcp_connection.h"


// global app state and statistics
size_t g_appstate;
size_t g_clients;
size_t g_connecting;
size_t g_failed;
size_t g_send_msgs;
size_t g_send_bytes;
size_t g_recv_msgs;
size_t g_recv_bytes;


// libuv globals for loop, terminal
int g_tty_closed = 0;
int g_ttytimer_closed = 0;
uv_tty_t g_tty;
uv_timer_t g_ttytimer;
uv_write_t g_write_req;
int g_width, g_height, g_pos = 0;
uv_loop_t *g_loop = NULL;


// context for connected clients
// typedef struct client_ctx_s
// {
// 	uv_stream_t *t
// } client_ctx_t;
int g_tty_enabled = 0;
int g_max_connections_per_timer = 1000;
int g_num_conns = 0;
tcp_ctx_t *g_tcp_ctx;
tcp_conn_t *g_tcp_conns;
struct sockaddr_storage g_sock_peer;



// globals for operating the main timer callback
uint64_t start_ticks = 0;
uint64_t current_ticks = 0;
uint64_t transmit_ticks = 0;
double elapsed = 0.0;

char tty_data[1024];
const char *space_str = "           ";
const char *pend_str = "Pending Connections: ";
const char *fail_str = "Failed Connections: ";
const char *conn_str = "Successful Connections: ";
const char *smsg_str = "Messages send: ";
const char *rmsg_str = "Messages recv: ";
const char *sbytes_str = "Bandwidth send: ";
const char *rbytes_str = "Bandwidth recv: ";
const char *dropped_str = "Dropped Connections: ";

// main timer callback, where we monitor app state and output data to terminal
// --------------------------------------------------------------------------------------------------------------
void on_tick_cb(uv_timer_t *req)
{
	if (!start_ticks) {
		start_ticks = hb_tstamp();
	}

	current_ticks = hb_tstamp();
	uint64_t tick_diff = (current_ticks - start_ticks);
	if (tick_diff < transmit_ticks) {
		return;
	}
	start_ticks = current_ticks;

	elapsed += (double)tick_diff / (double)AWS_TIMESTAMP_NANOS;
	uv_buf_t buf;
	buf.base = tty_data;
	size_t phase = g_appstate;

	// connecting all the clients
	if (phase == 0) {
		g_appstate++;
		printf("\nEstablishing Connections ======================================\n\n\n");
	} else if (phase == 1) {
		size_t connected = 0, connecting = 0, failed = 0, newcon = 0;
		int conns_made = 0;
		int conns_max = g_num_conns;
		if (conns_max > g_max_connections_per_timer) conns_max = g_max_connections_per_timer;
		for (int i = 0; i < g_num_conns; i++) {
			if (g_tcp_conns[i].state == CS_CONNECTING) {
				connecting++;
				continue;
			}
			if (g_tcp_conns[i].state == CS_CONNECTED) {
				connected++;
				continue;
			}
			if (g_tcp_conns[i].state == CS_DISCONNECTED) {
				failed++;
			}
			if (g_tcp_conns[i].state == CS_DISCONNECTING) {
				failed++;
				continue;
			}

			if (conns_made >= conns_max) continue;
			tcp_connect_begin(&g_tcp_conns[i], cmdline_args.host, cmdline_args.port);
			conns_made++;
		}

		if (connected >= g_num_conns) {
			elapsed = 0.0;
			g_appstate++;
			printf("\n\nTransmitting Data =============================================\n\n\n\n\n");
		} else {

		}

		buf.len = sprintf(tty_data, "\033[2A\033[1000D%s%zu           \033[1B\033[1000D%s%zu           \033[1B\033[1000D%s%zu           ", pend_str, connecting, fail_str, failed, conn_str, connected);
		if (g_tty_enabled) uv_write(&g_write_req, (uv_stream_t*)&g_tty, &buf, 1, NULL);
	} else if (phase == 2) {
		size_t connected = 0, connecting = 0, failed = 0;
		int conns_made = 0;
		int conns_max = g_num_conns;
		if (conns_max > g_max_connections_per_timer) conns_max = g_max_connections_per_timer;
		for (int i = 0; i < g_num_conns; i++) {
			int do_send = 0;
			int do_connect = 0;

			if (g_tcp_conns[i].state == CS_NEW) {
				failed++;
				do_connect = 1;
			} else if (g_tcp_conns[i].state == CS_CONNECTING) {
				failed++;
			} else if (g_tcp_conns[i].state == CS_CONNECTED) {
				connected++;
				do_send = 1;
			} else if (g_tcp_conns[i].state == CS_DISCONNECTING) {
				failed++;
				do_connect = 1;
			} else if (g_tcp_conns[i].state == CS_DISCONNECTED) {
				failed++;
				do_connect = 1;
			}

			if (do_connect && conns_made < conns_max) {
				tcp_connect_begin(&g_tcp_conns[i], cmdline_args.host, cmdline_args.port);
				conns_made++;
			} else if (do_send) {
				tcp_write_begin(g_tcp_conns[i].tcp_handle, cmdline_args.message, cmdline_args.msglen, 0);
			}
		}

		buf.len = sprintf(tty_data, "\033[4A\033[1000D%s%zu%s\033[1B\033[1000D%s%zu%s\033[1B\033[1000D%s%zu%s\033[1B\033[1000D%s%zu%s\033[1B\033[1000D%s%zu%s",
			dropped_str, failed, space_str,
			smsg_str, g_tcp_ctx->send_msgs, space_str,
			rmsg_str, g_tcp_ctx->recv_msgs, space_str,
			sbytes_str, g_tcp_ctx->send_bytes, space_str,
			rbytes_str, g_tcp_ctx->recv_bytes, space_str);
		if (g_tty_enabled) uv_write(&g_write_req, (uv_stream_t*)&g_tty, &buf, 1, NULL);

		if (elapsed > (double)cmdline_args.time) {
			elapsed = 0.0;
			g_appstate++;
			g_failed = 0;
			printf("\n\nWaiting for last messages =====================================\n");
		}
	} else if (phase == 3) {
		if (elapsed > 4.0) {
			for (int i = 0; i < g_num_conns; i++) {
				tcp_conn_disconnect(&g_tcp_conns[i]);
			}

			elapsed = 0.0;
			g_appstate++;

			printf("\n\nCleaning up ===================================================\n");
			printf("Msgs recv: %zu / Msgs send: %zu\n", g_tcp_ctx->recv_msgs, g_tcp_ctx->send_msgs);
			printf("Bytes recv: %zu / Bytes send: %zu\n", g_tcp_ctx->recv_bytes, g_tcp_ctx->send_bytes);
			printf("\n");
			printf("\n");

			uint64_t latency_max = 0;
			uint64_t latency_total = 0;
			uint64_t latency_avg = 0;
			uint64_t msgs_total = 0;
			for (int i = 0; i < g_num_conns; i++) {
				if (g_tcp_conns[i].latency_max > latency_max) {
					latency_max = g_tcp_conns[i].latency_max;
				}
				latency_total += g_tcp_conns[i].latency_total;
				msgs_total += g_tcp_conns[i].recv_msgs;
			}
			latency_avg = latency_total / msgs_total;

			printf("worst observed latency: %zu\n", latency_max);
			printf("average latency: %zu\n", latency_avg);
		}
	} else if (phase == 4) {
		g_appstate++;

		uv_close((uv_handle_t *)&g_tty, NULL);

		// uv_timer_stop(&g_ttytimer);
		// uv_stop(g_loop);
	} else if (phase == 5) {
		g_appstate++;
		uv_close((uv_handle_t *)&g_ttytimer, NULL);

		// uv_timer_stop(&g_ttytimer);
		// uv_stop(g_loop);
	} else {
	}
}

void shutdown_walk_cb(uv_handle_t* handle, void* arg)
{
	if (!uv_is_closing(handle)) {
		printf("Manually closing handle: %p -- %s\n", handle, uv_handle_type_name(uv_handle_get_type(handle)));
		uv_close(handle, on_tcp_close_cb);
	}
}


// application entry point where we:
// - read cmdline args
// - allocate connection contexts
// - start the uv_loop
// - clean up and exit
// --------------------------------------------------------------------------------------------------------------
int main(int argc, char **argv)
{
	int ret;
	int tty_setmode_success = 0;

	g_loop = (uv_loop_t *)HB_MEM_ACQUIRE(sizeof(uv_loop_t));
	if ((ret = uv_loop_init(g_loop))) {
		hb_log_uv_error(ret);
		goto cleanup;
	}

	tcp_ctx_config_t tcp_ctx_config = {
		.uv_loop = g_loop,
	};

	if (!(g_tcp_ctx = tcp_context_new())) {
		hb_log_uv_error(ENOMEM);
		goto cleanup;
	}

	if ((ret = tcp_context_set_config(g_tcp_ctx, &tcp_ctx_config))) {
		hb_log_uv_error(ret);
		goto cleanup;
	}

	if ((ret = uv_tty_init(g_loop, &g_tty, 1, 0))) {
		hb_log_uv_error(ret);
	}

	if ((ret = uv_tty_set_mode(&g_tty, 0))) {
		//hb_log_uv_error(ret);
	}

	tty_setmode_success = 1;
	g_tty_enabled = 0;
	if (uv_tty_get_winsize(&g_tty, &g_width, &g_height)) {
		g_tty_enabled = 0;
	}

#ifndef _DEBUG
	if ((ret = parse_tcploadgen_args(argc, (const char **)argv)) < 0) {
		printf("Error parsing cmdline args\n");
		goto cleanup;
	}
#else
	cmdline_args.clients = 1;
	cmdline_args.rate = 1;
	cmdline_args.message = "I love eating potatoes :D";
	cmdline_args.msglen = (int)strlen(cmdline_args.message);
	cmdline_args.prefix = 32;
	cmdline_args.time = 10;
	//snprintf(cmdline_args.host, sizeof(cmdline_args.host), "%s", "fe80::2c92:d74a:43ba:630e");
	snprintf(cmdline_args.host, sizeof(cmdline_args.host), "%s", "192.168.86.233");
	cmdline_args.port = 7777;
#endif

	printf("server: %s:%d\n", cmdline_args.host, cmdline_args.port);
	printf("clients: %d\n", cmdline_args.clients);
	printf("rate: %d per second\n", cmdline_args.rate);
	printf("message (%d bytes): %s\n", cmdline_args.msglen, cmdline_args.message);
	if (cmdline_args.prefix) {
		printf("message prefix: %d bit integer payload length\n", cmdline_args.prefix);
	} else {
		printf("message prefix: none\n");
	}
	printf("time: %d seconds\n", cmdline_args.time);


	if (cmdline_args.rate == 0) {
		transmit_ticks = 0;
	} else {
		transmit_ticks = AWS_TIMESTAMP_NANOS / cmdline_args.rate;
	}


	g_num_conns = cmdline_args.clients;
	g_tcp_conns = (tcp_conn_t *)HB_MEM_ACQUIRE(sizeof(tcp_conn_t) * g_num_conns);
	if (!g_tcp_conns) {
		hb_log_error("Failed allocating tcp_handles");
		goto cleanup;
	}
	memset(g_tcp_conns, 0, sizeof(tcp_conn_t) * g_num_conns);
	for (int i = 0; i < g_num_conns; i++) {
		if (tcp_conn_init(g_tcp_ctx, &g_tcp_conns[i])) {
			hb_log_uv_error(ret);
			goto cleanup;
		}
	}

	g_appstate = 0;
	g_clients = 0;
	g_connecting = 0;
	g_failed = 0;
	g_send_msgs = 0;
	g_send_bytes = 0;
	g_recv_msgs = 0;
	g_recv_bytes = 0;

	if ((ret = uv_timer_init(g_loop, &g_ttytimer))) {
		hb_log_uv_error(ret);
		goto cleanup;
	}

	if ((ret = uv_timer_start(&g_ttytimer, on_tick_cb, 1, 1))) {
		hb_log_uv_error(ret);
		goto cleanup;
	}

	if ((ret = uv_run(g_loop, UV_RUN_DEFAULT))) {
		hb_log_uv_error(ret);

	}

	if ((ret = uv_loop_close(g_loop))) {
		hb_log_uv_error(ret);

		uv_walk(g_loop, shutdown_walk_cb, NULL);
		if ((ret = uv_loop_close(g_loop)) == 0) {
			printf("Walked loop and shutdown cleanly\n");
		} else {
			goto cleanup;
		}
	}

cleanup:

	if (tty_setmode_success) {
		uv_tty_reset_mode();
	}
	printf("\n");

	HB_MEM_RELEASE(g_tcp_conns);
	tcp_context_delete(&g_tcp_ctx);
	HB_MEM_RELEASE(g_loop);

	return 0;
}