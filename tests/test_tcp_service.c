#include "hb/test_harness.h"
#include "hb/thread.h"
#include "hb/tcp_service.h"

HB_TEST_CASE_BEGIN(test_service)
	tcp_service_t tcp_service = {
		.priv = NULL,
		.state = TCP_SERVICE_NEW,
	};

	ASSERT_SUCCESS(tcp_service_start(&tcp_service, "0.0.0.0", 3456));
	//ASSERT_SUCCESS(tcp_service_lock(&tcp_service));
	hb_thread_sleep_s(1);
	//ASSERT_SUCCESS(tcp_service_unlock(&tcp_service));
	ASSERT_SUCCESS(tcp_service_stop(&tcp_service));

	ASSERT_SUCCESS(tcp_service_start(&tcp_service, "0.0.0.0", 3456));
	//ASSERT_SUCCESS(tcp_service_lock(&tcp_service));
	hb_thread_sleep_s(1);
	//ASSERT_SUCCESS(tcp_service_unlock(&tcp_service));
	ASSERT_SUCCESS(tcp_service_stop(&tcp_service));

	return 0;
}

HB_TEST_CASE(test_tcp_service, test_service)