#include "hb/error.h"
#include "hb/log.h"
#include "hb/tcp_service.h"

#include "uv.h"


static int sigint_recvd = 0;

void on_signal_cb(uv_signal_t* handle, int signum)
{

}


int main(void)
{
	tcp_service_t tcp_service = {
		.priv = NULL,
		.state = TCP_SERVICE_NEW,
	};

	tcp_service_start(&tcp_service, "0.0.0.0", 7777);

	while (1) {
		// emulate 60 fps tick rate on Unity main thread
		hb_thread_sleep_ms(16);
	}

	tcp_service_stop(&tcp_service);

	return 0;
}