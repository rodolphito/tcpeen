#include "uv.h"

#include "hb/test_harness.h"
#include "hb/endpoint.h"
#include "hb/error.h"
#include "hb/log.h"

TN_TEST_CASE_BEGIN(test_endpoint)
	char buf[255];
	tn_endpoint_t endpoint;
	struct sockaddr_in6 sa6;
	struct sockaddr_in sa;

	// test that our size if correct (since we pass this to managed)
	ASSERT_INT_EQUALS(sizeof(endpoint), TN_ENDPOINT_MAX_SIZE);

	// test ipv4 conversion
	ASSERT_SUCCESS(uv_ip4_addr("1.2.3.4", 5354, &sa));
	ASSERT_SUCCESS(tn_endpoint_convert_from(&endpoint, &sa));
	ASSERT_SUCCESS(tn_endpoint_get_string(&endpoint, buf, 255));
	ASSERT_SUCCESS(strcmp(buf, "1.2.3.4:5354"));

	// test ipv6 conversion
	ASSERT_SUCCESS(uv_ip6_addr("1123::4567:890a:bcde", 8045, &sa6));
	ASSERT_SUCCESS(tn_endpoint_convert_from(&endpoint, &sa6));
	ASSERT_SUCCESS(tn_endpoint_get_string(&endpoint, buf, 255));
	ASSERT_SUCCESS(strcmp(buf, "1123::4567:890a:bcde:8045"));
	
	// test set ipv4
	ASSERT_SUCCESS(tn_endpoint_set_ip4(&endpoint, "0.0.0.0", 65432));
	ASSERT_SUCCESS(tn_endpoint_get_string(&endpoint, buf, 255));
	ASSERT_SUCCESS(strcmp(buf, "0.0.0.0:65432"));

	// test set ipv6
	ASSERT_SUCCESS(tn_endpoint_set_ip6(&endpoint, "fe80::2c92:d74a:43ba", 12345));
	ASSERT_SUCCESS(tn_endpoint_get_string(&endpoint, buf, 255));
	ASSERT_SUCCESS(strcmp(buf, "fe80::2c92:d74a:43ba:12345"));

	return TN_SUCCESS;
}

TN_TEST_CASE(test_endpoints, test_endpoint)
