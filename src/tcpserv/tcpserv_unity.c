#include "hb/error.h"
#include "hb/log.h"
#include "hb/event.h"
#include "hb/tcp_service.h"

#include "uv.h"


static int sigint_recvd = 0;

void on_signal_cb(uv_signal_t* handle, int signum)
{

}


int main(void)
{
    int ret;
	tcp_service_t tcp_service = {
		.priv = NULL,
		.state = TCP_SERVICE_NEW,
	};

	HB_GUARD(tcp_service_setup(&tcp_service));

	HB_GUARD(tcp_service_start(&tcp_service, "0.0.0.0", 7777));

	hb_event_base_t *evt_base = NULL;
	uint64_t evt_count = 0;
	uint8_t state = 0;

	int i = 0;
	while (1) {
		// emulate 60 fps tick rate on Unity main thread
		// hb_thread_sleep_ms(16);

		HB_GUARD_CLEANUP(tcp_service_lock(&tcp_service));
		ret = tcp_service_update(&tcp_service, &evt_base, &evt_count, &state);
		HB_GUARD_CLEANUP(tcp_service_unlock(&tcp_service));
        HB_GUARD(ret);

		for (int e = 0; e < evt_count; e++) {
			switch (evt_base->type) {
			case HB_EVENT_CLIENT_OPEN:
				//EventClientOpen *evtOpen = (EventClientOpen *)evtPtr;
				//UnityEngine.Debug.LogFormat("Connected Event -- ID# {0}", evtOpen->client_id);
				//UnityEngine.Debug.LogFormat("Endpoints: {0} -> {1}", Marshal.PtrToStringAnsi((IntPtr)evtOpen->host_remote), Marshal.PtrToStringAnsi((IntPtr)evtOpen->host_local));
				break;
			case HB_EVENT_CLIENT_CLOSE:
				//EventClientClose *evtClose = (EventClientClose *)evtPtr;
				//UnityEngine.Debug.LogFormat("Disconnected Event -- ID# {0}", evtClose->client_id);
				break;
			case HB_EVENT_CLIENT_READ:
				{
					hb_event_client_read_t *evt_read = (hb_event_client_read_t *)evt_base;
                    // evt_read->buffer[evt_read->length] = '\0';
                    // hb_log_warning("%zu bytes -- %s", evt_read->length, (char *)evt_read->buffer + 4);
					HB_GUARD_CLEANUP(tcp_service_lock(&tcp_service));
					ret = tcp_service_send(&tcp_service, evt_read->client_id, evt_read->buffer, evt_read->length);
					HB_GUARD_CLEANUP(tcp_service_unlock(&tcp_service));
                    // HB_GUARD(ret);
				}
			
				break;

			}

			evt_base++;

			continue;

		cleanup:
			hb_log_error("Exiting service loop");
			break;
		}
	}

	HB_GUARD(tcp_service_stop(&tcp_service));

	tcp_service_cleanup(&tcp_service);

	return 0;
}