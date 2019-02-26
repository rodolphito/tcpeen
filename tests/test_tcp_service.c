#include "hb/test_harness.h"
#include "hb/thread.h"
#include "hb/tcp_service.h"

int main(void)
{
	ASSERT_SUCCESS(tcp_service_start("0.0.0.0", 3456));

	hb_thread_sleep_s(1);

	ASSERT_SUCCESS(tcp_service_stop());

	return 0;
}