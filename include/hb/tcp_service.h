#ifndef HB_TCP_SERVICE_H
#define HB_TCP_SERVICE_H

#include <stdint.h>

#include "hb/endpoint.h"

typedef enum tcp_service_state_e {
	TCP_SERVICE_NEW, 
	TCP_SERVICE_STARTING,
	TCP_SERVICE_STARTED,
	TCP_SERVICE_STOPPING,
	TCP_SERVICE_STOPPED,
	TCP_SERVICE_ERROR,
	TCP_SERVICE_INVALID,
} tcp_service_state_t;

typedef struct tcp_service_s {
	void *priv;
	hb_endpoint_t host_listen;
	uint8_t state;
} tcp_service_t;


int tcp_service_start(const char *ipstr, uint16_t port);
int tcp_service_stop();
int tcp_service_update(uintptr_t *evt_base, uint32_t *count);


#endif