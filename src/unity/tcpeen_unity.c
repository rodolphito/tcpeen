#include "PluginAPI-2018.3.2f1/IUnityInterface.h"

#include "hb/config.h"
#include "hb/error.h"
#include "hb/allocator.h"

#include "hb/tcp_service.h"

// ----------------------------------------------------------------------------------------------------------------------------------
//void UNITY_INTERFACE_EXPORT UNITY_INTERFACE_API UnityPluginLoad(IUnityInterfaces* unityInterfaces)
//{
//}

// ----------------------------------------------------------------------------------------------------------------------------------
//void UNITY_INTERFACE_EXPORT UNITY_INTERFACE_API UnityPluginUnload(void)
//{
//}


// TODO: pass back service pointer allowing
// multiple services to be created in Managed layer
tcp_service_t tcp_service = {
	.priv = NULL,
};


int32_t UNITY_INTERFACE_EXPORT UNITY_INTERFACE_API tcpeen_service_setup(uint64_t max_clients)
{
	HB_GUARD(tcp_service.priv);
	HB_GUARD(tcp_service_setup(&tcp_service));
	return HB_SUCCESS;
}

int32_t UNITY_INTERFACE_EXPORT UNITY_INTERFACE_API tcpeen_service_cleanup()
{
	HB_GUARD_NULL(tcp_service.priv);
	tcp_service_cleanup(&tcp_service);
	return HB_SUCCESS;
}

int32_t UNITY_INTERFACE_EXPORT UNITY_INTERFACE_API tcpeen_service_start(const char *ipstr, uint16_t port)
{
	HB_GUARD_NULL(tcp_service.priv);
	HB_GUARD(tcp_service_start(&tcp_service, ipstr, port));

	return HB_SUCCESS;
}

int32_t UNITY_INTERFACE_EXPORT UNITY_INTERFACE_API tcpeen_service_stop()
{
	HB_GUARD_NULL(tcp_service.priv);
	HB_GUARD(tcp_service_stop(&tcp_service));
	return HB_SUCCESS;
}

int32_t UNITY_INTERFACE_EXPORT UNITY_INTERFACE_API tcpeen_service_update(hb_event_base_t **out_heap_base, uint64_t *out_block_count, uint8_t *out_state)
{
	HB_GUARD_NULL(tcp_service.priv);
	HB_GUARD_NULL(out_heap_base);
	HB_GUARD_NULL(out_block_count);
	HB_GUARD_NULL(out_state);
	
	HB_GUARD(tcp_service_update(&tcp_service, &out_heap_base, out_block_count));

	return HB_SUCCESS;
}

int32_t UNITY_INTERFACE_EXPORT UNITY_INTERFACE_API tcpeen_service_send(uint64_t client_id, void *buffer, uint64_t length)
{
	HB_GUARD_NULL(tcp_service.priv);
	HB_GUARD_NULL(buffer);
	HB_GUARD_NULL(length);

	//HB_GUARD(tcp_service_send(&tcp_service, client_id, buffer, length));

	return HB_SUCCESS;
}
