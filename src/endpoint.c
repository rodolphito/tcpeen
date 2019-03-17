#include "hb/endpoint.h"

#include "uv.h"

#include "hb/error.h"

// --------------------------------------------------------------------------------------------------------------
int tn_endpoint_get_string(tn_endpoint_t *endpoint, char *buf, int buf_len)
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
		return TN_ERROR;
	}

	sprintf(buf, "%s:%u", ipbuf, port);
	return TN_SUCCESS;
}

// --------------------------------------------------------------------------------------------------------------
int tn_endpoint_set(tn_endpoint_t *endpoint, const char *ip, uint16_t port)
{
	if (tn_endpoint_set_ip6(endpoint, ip, port)) {
		return tn_endpoint_set_ip4(endpoint, ip, port);
	}

	return TN_SUCCESS;
}

// --------------------------------------------------------------------------------------------------------------
int tn_endpoint_set_ip4(tn_endpoint_t *endpoint, const char *ip, uint16_t port)
{
	int ret;

	if (!endpoint) return TN_ERROR;
	if (!ip) return TN_ERROR;

	memset(endpoint, 0, sizeof(*endpoint));
	if ((ret = uv_ip4_addr(ip, port, (struct sockaddr_in *)&endpoint->sockaddr))) return TN_ERROR;
	endpoint->type = TN_ENDPOINT_TYPE_IPV4;

	return TN_SUCCESS;
}

// --------------------------------------------------------------------------------------------------------------
int tn_endpoint_set_ip6(tn_endpoint_t *endpoint, const char *ip, uint16_t port)
{
	int ret;

	if (!endpoint) return TN_ERROR;
	if (!ip) return TN_ERROR;

	memset(endpoint, 0, sizeof(*endpoint));
	if ((ret = uv_ip6_addr(ip, port, (struct sockaddr_in6 *)&endpoint->sockaddr))) return TN_ERROR;
	endpoint->type = TN_ENDPOINT_TYPE_IPV6;

	return TN_SUCCESS;
}

// --------------------------------------------------------------------------------------------------------------
int tn_endpoint_convert_from(tn_endpoint_t *endpoint, void *sockaddr_void)
{
	if (!endpoint) return TN_ERROR;
	if (!sockaddr_void) return TN_ERROR;

	struct sockaddr_storage *sockaddr = sockaddr_void;

	memset(endpoint, 0, sizeof(*endpoint));
	if (sockaddr->ss_family == AF_INET6) {
		memcpy(&endpoint->sockaddr, (struct sockaddr_in6 *)sockaddr, sizeof(struct sockaddr_in6));
		endpoint->type = TN_ENDPOINT_TYPE_IPV6;
	} else if (sockaddr->ss_family == AF_INET) {
		memcpy(&endpoint->sockaddr, (struct sockaddr_in *)sockaddr, sizeof(struct sockaddr_in));
		endpoint->type = TN_ENDPOINT_TYPE_IPV4;
	} else {
		return TN_ERROR;
	}

	return TN_SUCCESS;
}

