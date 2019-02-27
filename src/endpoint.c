#include "hb/endpoint.h"

#include "uv.h"

#include "hb/error.h"

// --------------------------------------------------------------------------------------------------------------
int hb_endpoint_get_string(hb_endpoint_t *endpoint, char *buf, int buf_len)
{
	uint16_t port = 0;
	memset(buf, 0, buf_len);

	char ipbuf[255];
	memset(ipbuf, 0, 255);

	const struct sockaddr_storage *sockaddr = (struct sockaddr_storage *)&endpoint->sockaddr;
	if (sockaddr->ss_family == AF_INET6) {
		struct sockaddr_in6 *sa = (struct sockaddr_in6 *)sockaddr;
		uv_ip6_name(sa, ipbuf, 255);
		port = ntohs(sa->sin6_port);
	} else if (sockaddr->ss_family == AF_INET) {
		struct sockaddr_in *sa = (struct sockaddr_in *)sockaddr;
		uv_ip4_name(sa, ipbuf, 255);
		port = ntohs(sa->sin_port);
	} else {
		return HB_ERROR;
	}

	sprintf(buf, "%s:%u", ipbuf, port);
	return HB_SUCCESS;
}

// --------------------------------------------------------------------------------------------------------------
int hb_endpoint_set(hb_endpoint_t *endpoint, const char *ip, uint16_t port)
{
	if (hb_endpoint_set_ip6(endpoint, ip, port)) {
		return hb_endpoint_set_ip4(endpoint, ip, port);
	}

	return HB_SUCCESS;
}

// --------------------------------------------------------------------------------------------------------------
int hb_endpoint_set_ip4(hb_endpoint_t *endpoint, const char *ip, uint16_t port)
{
	int ret;

	if (!endpoint) return HB_ERROR;
	if (!ip) return HB_ERROR;

	memset(endpoint, 0, sizeof(*endpoint));
	if ((ret = uv_ip4_addr(ip, port, (struct sockaddr_in *)&endpoint->sockaddr))) return HB_ERROR;
	endpoint->type = HB_ENDPOINT_TYPE_IPV4;

	return HB_SUCCESS;
}

// --------------------------------------------------------------------------------------------------------------
int hb_endpoint_set_ip6(hb_endpoint_t *endpoint, const char *ip, uint16_t port)
{
	int ret;

	if (!endpoint) return HB_ERROR;
	if (!ip) return HB_ERROR;

	memset(endpoint, 0, sizeof(*endpoint));
	if ((ret = uv_ip6_addr(ip, port, (struct sockaddr_in6 *)&endpoint->sockaddr))) return HB_ERROR;
	endpoint->type = HB_ENDPOINT_TYPE_IPV6;

	return HB_SUCCESS;
}

// --------------------------------------------------------------------------------------------------------------
int hb_endpoint_convert_from(hb_endpoint_t *endpoint, void *sockaddr_void)
{
	if (!endpoint) return HB_ERROR;
	if (!sockaddr_void) return HB_ERROR;

	struct sockaddr_storage *sockaddr = sockaddr_void;

	memset(endpoint, 0, sizeof(*endpoint));
	if (sockaddr->ss_family == AF_INET6) {
		memcpy(&endpoint->sockaddr, (struct sockaddr_in6 *)sockaddr, sizeof(struct sockaddr_in6));
		endpoint->type = HB_ENDPOINT_TYPE_IPV6;
	} else if (sockaddr->ss_family == AF_INET) {
		memcpy(&endpoint->sockaddr, (struct sockaddr_in *)sockaddr, sizeof(struct sockaddr_in));
		endpoint->type = HB_ENDPOINT_TYPE_IPV4;
	} else {
		return HB_ERROR;
	}

	return HB_SUCCESS;
}

