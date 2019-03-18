#ifndef TN_EVENT_H
#define TN_EVENT_H

#include <stdint.h>

#include "tn/queue_spsc.h"
#include "tn/endpoint.h"
#include "tn/buffer.h"


#define TN_EVENT_MAX_SIZE 256
#define TN_EVENT_PAD_SIZE (TN_EVENT_MAX_SIZE) - sizeof(uint32_t) - sizeof(uint32_t)

#define TN_EVENT_FIELDS			\
	uint32_t id;				\
	uint32_t type;				\


typedef enum tn_event_type_e {
	TN_EVENT_NONE = 0,
	TN_EVENT_IOERROR,
	TN_EVENT_CLIENT_OPEN,
	TN_EVENT_CLIENT_CLOSE,
	TN_EVENT_CLIENT_READ,
} tn_event_type_t;

// base event
typedef struct tn_event_base_s {
	TN_EVENT_FIELDS
	uint8_t pad[TN_EVENT_PAD_SIZE];
} tn_event_base_t;

// error event
typedef struct tn_event_error_s {
	TN_EVENT_FIELDS
	uint64_t client_id;
	tn_buffer_t *tn_buffer;
	int32_t error_code;
	char error_string[TN_EVENT_PAD_SIZE - sizeof(uint64_t) - sizeof(tn_buffer_t *) - sizeof(int32_t)];
} tn_event_error_t;

// client connected
typedef struct tn_event_client_open_s {
	TN_EVENT_FIELDS
	uint64_t client_id;
	char host_local[64];
	char host_remote[64];
} tn_event_client_open_t;

// client disconnected
typedef struct tn_event_client_close_s {
	TN_EVENT_FIELDS
	uint64_t client_id;
	tn_buffer_t *tn_buffer;
	int32_t error_code;
	char error_string[TN_EVENT_PAD_SIZE - sizeof(uint64_t) - sizeof(tn_buffer_t *) - sizeof(int32_t)];
} tn_event_client_close_t;

// client recv bytes
typedef struct tn_event_client_read_s {
	TN_EVENT_FIELDS
	uint64_t client_id;
	uint8_t *buffer;
	size_t len;
	tn_buffer_t *tn_buffer;
} tn_event_client_read_t;

// IO stats
typedef struct tn_event_io_stats_s {
	TN_EVENT_FIELDS
	uint32_t connections_total;
	uint32_t connections_new;
	uint32_t connections_lost;
	uint32_t recv_msgs;
	uint32_t recv_bytes;
	uint32_t send_msgs;
	uint32_t send_bytes;
} tn_event_io_stats_t;


typedef struct tn_event_list_s {
	tn_event_base_t *tn_events;
	tn_queue_spsc_t tn_events_free;
	tn_queue_spsc_t tn_events_ready;
	uint64_t capacity;
} tn_event_list_t;


int tn_event_list_setup(tn_event_list_t *list, uint64_t capacity);
void tn_event_list_cleanup(tn_event_list_t *list);

int tn_event_list_free_pop_back(tn_event_list_t *list, tn_event_base_t **out_evt);
int tn_event_list_free_pop_error(tn_event_list_t *list, tn_event_error_t **out_evt);
int tn_event_list_free_pop_open(tn_event_list_t *list, tn_event_client_open_t **out_evt);
int tn_event_list_free_pop_close(tn_event_list_t *list, tn_event_client_close_t **out_evt);
int tn_event_list_free_pop_read(tn_event_list_t *list, tn_event_client_read_t **out_evt);
int tn_event_list_ready_push(tn_event_list_t *list, void *out_evt);

int tn_event_list_ready_peek(tn_event_list_t *list, tn_event_base_t **out_evt);
int tn_event_list_ready_pop(tn_event_list_t *list);
void tn_event_list_ready_pop_cached(tn_event_list_t *list);
int tn_event_list_ready_pop_back(tn_event_list_t *list, tn_event_base_t **out_evt);
int tn_event_list_ready_pop_all(tn_event_list_t *list, tn_event_base_t **out_evt, uint64_t *out_count);
int tn_event_list_free_push(tn_event_list_t *list, void *evt);


#endif