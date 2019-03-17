#include "hb/test_harness.h"
#include "hb/thread.h"
#include "hb/tcp_service.h"

TN_TEST_CASE_BEGIN(test_service)
	tcp_service_t tcp_service = {
		.priv = NULL,
	};

	//tn_event_base_t *evt;
	//uint64_t count;
	//uint8_t state;

	ASSERT_SUCCESS(tcp_service_setup(&tcp_service));
	ASSERT_SUCCESS(tcp_service_start(&tcp_service, "0.0.0.0", 3456));

	//tn_thread_sleep_s(1);
	//ASSERT_SUCCESS(tcp_service_update(&tcp_service, &evt, &count, &state));
	
	ASSERT_SUCCESS(tcp_service_stop(&tcp_service));
	tcp_service_cleanup(&tcp_service);


	ASSERT_SUCCESS(tcp_service_setup(&tcp_service));
	ASSERT_SUCCESS(tcp_service_start(&tcp_service, "0.0.0.0", 3456));

	//tn_thread_sleep_s(1);
	//ASSERT_SUCCESS(tcp_service_update(&tcp_service, &evt, &count, &state));

	ASSERT_SUCCESS(tcp_service_stop(&tcp_service));
	tcp_service_cleanup(&tcp_service);

	return 0;
}

TN_TEST_CASE(test_tcp_service, test_service)
