#include "hb/error.h"
#include "hb/log.h"
#include "hb/event.h"
#include "hb/tcp_service.h"
#include "hb/queue_spsc.h"

#include "uv.h"


int main(void)
{
	int ret;
	tcp_service_t tcp_service = {
		.priv = NULL,
	};

	HB_GUARD(tcp_service_setup(&tcp_service));

	HB_GUARD(tcp_service_start(&tcp_service, "0.0.0.0", 7777));

	hb_event_base_t **evt_base = NULL;
	uint64_t evt_count = 0;
	uint8_t state = 0;

	int i = 0;
	while (1) {
		// emulate 60 fps tick rate on Unity main thread
		 hb_thread_sleep_ms(16);

		ret = tcp_service_update(&tcp_service, &evt_base, &evt_count);

		for (int eidx = 0; eidx < evt_count; eidx++) {
			switch (evt_base[eidx]->type) {
			case HB_EVENT_CLIENT_OPEN:
				break;
			case HB_EVENT_CLIENT_CLOSE:
				break;
			case HB_EVENT_CLIENT_READ:
			{
				hb_event_client_read_t *evt_read = (hb_event_client_read_t *)evt_base[eidx];
				
				uint32_t len;
				uint64_t msg_id;
				uint64_t tstamp;
				hb_buffer_read_be32(evt_read->hb_buffer, &len);
				hb_buffer_read_be64(evt_read->hb_buffer, &msg_id);
				hb_buffer_read_be64(evt_read->hb_buffer, &tstamp);

				tcp_channel_t *channel = NULL;
				if (tcp_channel_list_get(&tcp_service.channel_list, evt_read->client_id, &channel)) break;
				assert(channel);

				if (msg_id != channel->last_msg_id + 1) {
					//hb_log_warning("connection: %zu -- msg len: %zu -- id mismatch: %zu - %zu", evt_read->client_id, hb_buffer_length(evt_read->hb_buffer), channel->last_msg_id + 1, msg_id);
				} else {
					//hb_log_debug("connection: %zu -- msg len: %zu -- id match: %zu - %zu", evt_read->client_id, hb_buffer_length(evt_read->hb_buffer), channel->last_msg_id + 1, msg_id);
					channel->last_msg_id++;
				}

				const uint64_t buflen = hb_buffer_length(evt_read->hb_buffer);
				//evt_read->hb_buffer->pos.ptr[len] = '\0';
				//hb_log_warning("%u vs %zu bytes -- %s", len, buflen, (char *)evt_read->hb_buffer->pos.ptr);
				tcp_service_send(&tcp_service, evt_read->client_id, evt_read->hb_buffer->buf.buffer, buflen);

				hb_buffer_t *evtbuf = evt_read->hb_buffer;
				evt_read->hb_buffer = NULL;
				hb_buffer_pool_push(&tcp_service.pool_read, evtbuf);
				hb_event_list_free_push(&tcp_service.events, evt_read);

				break;
			}
			default:
				break;
			}
		}
	}

	HB_GUARD(tcp_service_stop(&tcp_service));

	tcp_service_cleanup(&tcp_service);

	return 0;
}