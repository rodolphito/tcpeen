#include "PluginAPI-2018.3.2f1/IUnityInterface.h"

#include "tn/config.h"
#include "tn/error.h"
#include "tn/allocator.h"

#include "tn/tcp_service.h"

// ----------------------------------------------------------------------------------------------------------------------------------
void UNITY_INTERFACE_EXPORT UNITY_INTERFACE_API UnityPluginLoad(IUnityInterfaces* unityInterfaces)
{
	return;
}

// ----------------------------------------------------------------------------------------------------------------------------------
void UNITY_INTERFACE_EXPORT UNITY_INTERFACE_API UnityPluginUnload(void)
{
	return;
}


// TODO: pass back service pointer allowing
// multiple services to be created in Managed layer
tcp_service_t tcp_service = {
	.priv = NULL,
};


int32_t UNITY_INTERFACE_EXPORT UNITY_INTERFACE_API tcpeen_service_setup(uint64_t max_clients)
{
	TN_GUARD(tcp_service_setup(&tcp_service));
	return TN_SUCCESS;
}

int32_t UNITY_INTERFACE_EXPORT UNITY_INTERFACE_API tcpeen_service_cleanup(void)
{
	TN_GUARD_NULL(tcp_service.priv);
	tcp_service_cleanup(&tcp_service);
	return TN_SUCCESS;
}

int32_t UNITY_INTERFACE_EXPORT UNITY_INTERFACE_API tcpeen_service_start(const char *ipstr, uint16_t port)
{
	TN_GUARD_NULL(tcp_service.priv);
	TN_GUARD(tcp_service_start(&tcp_service, ipstr, port));

	return TN_SUCCESS;
}

int32_t UNITY_INTERFACE_EXPORT UNITY_INTERFACE_API tcpeen_service_stop(void)
{
	TN_GUARD_NULL(tcp_service.priv);
	TN_GUARD(tcp_service_stop(&tcp_service));
	return TN_SUCCESS;
}

int32_t UNITY_INTERFACE_EXPORT UNITY_INTERFACE_API tcpeen_service_events_acquire(tn_event_base_t **out_evt_ptr_arr[], uint64_t *out_evt_count)
{
	TN_GUARD_NULL(tcp_service.priv);
	TN_GUARD_NULL(out_evt_ptr_arr);
	TN_GUARD_NULL(out_evt_count);
	
	TN_GUARD(tcp_service_events_acquire(&tcp_service, out_evt_ptr_arr, out_evt_count));

	return TN_SUCCESS;
}

int32_t UNITY_INTERFACE_EXPORT UNITY_INTERFACE_API tcpeen_service_events_release(void)
{
	TN_GUARD_NULL(tcp_service.priv);
	TN_GUARD(tcp_service_events_release(&tcp_service));
	return TN_SUCCESS;
}

int32_t UNITY_INTERFACE_EXPORT UNITY_INTERFACE_API tcpeen_service_send(uint64_t client_id, uint8_t *buffer, uint64_t length)
{
	TN_GUARD_NULL(tcp_service.priv);
	TN_GUARD_NULL(buffer);
	TN_GUARD_NULL(length);

	tcp_channel_t *channel;
	TN_GUARD(tcp_channel_list_get(&tcp_service.channel_list, client_id, &channel));
	TN_GUARD(tcp_service_send(&tcp_service, channel, buffer, length));

	return TN_SUCCESS;
}
