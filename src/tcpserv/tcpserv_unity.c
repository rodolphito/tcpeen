#include "hb/error.h"
#include "hb/log.h"
#include "hb/event.h"
#include "hb/tcp_service.h"
#include "hb/queue_spsc.h"

#include "uv.h"


int handle_client_read(tcp_service_t *service, hb_event_client_read_t *evt_read)
{
	int ret;
	tcp_channel_t *channel;

	ret = HB_EVENT_NOCHAN;
	HB_GUARD_CLEANUP(tcp_channel_list_get(&service->channel_list, evt_read->client_id, &channel));
	assert(channel);

	uint64_t msg_id;
	uint64_t tstamp;
	for (int s = 0; s < HB_EVENT_MAX_SPANS_PER_READ; s++) {
		if (!evt_read->span[s].ptr) {
			//hb_log_debug("end of spans");
			break;
		}
		ret = HB_EVENT_SPANREAD;

		HB_GUARD_CLEANUP(hb_buffer_read_seek(evt_read->hb_buffer, &evt_read->span[s]));
		HB_GUARD_CLEANUP(hb_buffer_read_be64(evt_read->hb_buffer, &msg_id));
		HB_GUARD_CLEANUP(hb_buffer_read_be64(evt_read->hb_buffer, &tstamp));
		//hb_log_debug("connection: %zu -- span: %d -- msg_id: %zu -- tstamp: %zu", evt_read->client_id, s, msg_id, tstamp);

		const uint64_t expected_id = channel->last_msg_id + 1;
		if (msg_id != expected_id) {
			hb_log_warning("connection: %zu -- msg len: %zu -- id mismatch: %zu - %zu", evt_read->client_id, hb_buffer_length(evt_read->hb_buffer), channel->last_msg_id + 1, msg_id);
		} else {
			//hb_log_debug("connection: %zu -- msg len: %zu -- id match: %zu", evt_read->client_id, hb_buffer_length(evt_read->hb_buffer), msg_id);
			channel->last_msg_id = expected_id;
		}

		//HB_GUARD_CLEANUP(ret = tcp_service_send(service, channel, evt_read->span[s].ptr, evt_read->span[s].len));

		ret = HB_SUCCESS;
	}

	hb_buffer_read_reset(evt_read->hb_buffer);
	HB_GUARD_CLEANUP(ret = tcp_service_send(service, channel, hb_buffer_read_ptr(evt_read->hb_buffer), hb_buffer_read_length(evt_read->hb_buffer)));

	hb_buffer_t *evtbuf = evt_read->hb_buffer;
	evt_read->hb_buffer = NULL;
	hb_buffer_pool_push(&service->pool_read, evtbuf);
	hb_event_list_free_push(&service->events, evt_read);

	return HB_SUCCESS;

cleanup:
	if (ret) hb_log_error("failed to process read event: %d", ret);
	return ret;
}


int main(void)
{
	int ret;
	tcp_service_t tcp_service = {
		.priv = NULL,
	};

	HB_GUARD(tcp_service_setup(&tcp_service));

	HB_GUARD(tcp_service_start(&tcp_service, "0.0.0.0", 7777));

	while (tcp_service_state(&tcp_service) != TCP_SERVICE_STOPPING) {
		// emulate 60 fps tick rate on Unity main thread
		hb_thread_sleep_ms(16);

		hb_event_base_t **evt_base;
		uint64_t evt_count = 0;
		ret = tcp_service_update(&tcp_service, &evt_base, &evt_count);

		for (int eidx = 0; eidx < evt_count; eidx++) {
			switch (evt_base[eidx]->type) {
			case HB_EVENT_CLIENT_OPEN:
				break;
			case HB_EVENT_CLIENT_CLOSE:
				break;
			case HB_EVENT_CLIENT_READ:
				ret = handle_client_read(&tcp_service, (hb_event_client_read_t *)evt_base[eidx]);
				break;
			default:
				break;
			}
		}
	}

	HB_GUARD(tcp_service_stop(&tcp_service));

	tcp_service_cleanup(&tcp_service);

	return 0;
}