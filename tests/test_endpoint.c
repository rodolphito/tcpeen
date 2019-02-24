#include <assert.h>

#include "uv.h"

#include "hb/endpoint.h"
#include "hb/error.h"
#include "hb/log.h"

int main(void)
{
	char buf[255];
	hb_endpoint_t endpoint;
	struct sockaddr_in6 sa6;
	struct sockaddr_in sa;

	// test that our size if correct (since we pass this to managed)
	assert(sizeof(endpoint) == HB_ENDPOINT_MAX_SIZE);

	// test ipv4 conversion
	assert(0 == uv_ip4_addr("1.2.3.4", 5354, &sa));
	assert(0 == hb_endpoint_convert_from(&endpoint, &sa));
	assert(0 == hb_endpoint_get_string(&endpoint, buf, 255));
	assert(0 == strcmp(buf, "1.2.3.4:5354"));

	// test ipv6 conversion
	assert(0 == uv_ip6_addr("1123::4567:890a:bcde", 8045, &sa6));
	assert(0 == hb_endpoint_convert_from(&endpoint, &sa6));
	assert(0 == hb_endpoint_get_string(&endpoint, buf, 255));
	assert(0 == strcmp(buf, "1123::4567:890a:bcde:8045"));
	
	// test set ipv4
	assert(HB_SUCCESS == hb_endpoint_set_ip4(&endpoint, "0.0.0.0", 65432));
	assert(HB_SUCCESS == hb_endpoint_get_string(&endpoint, buf, 255));
	assert(strcmp(buf, "0.0.0.0:65432") == 0);

	// test set ipv6
	assert(HB_SUCCESS == hb_endpoint_set_ip6(&endpoint, "fe80::2c92:d74a:43ba", 12345));
	assert(HB_SUCCESS == hb_endpoint_get_string(&endpoint, buf, 255));
	assert(!strcmp(buf, "fe80::2c92:d74a:43ba:12345"));

	return 0;
}
