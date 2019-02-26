#include <assert.h>

#include "hb/thread.h"
#include "hb/tcp_service.h"

int main(void)
{
	assert(0 == tcp_service_start("0.0.0.0", 3456));

	hb_thread_sleep_s(100);

	assert(0 == tcp_service_stop());

	return 0;
}