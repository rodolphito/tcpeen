#ifndef HB_EVENT_H
#define HB_EVENT_H

#include <stdint.h>

#include "hb/queue_spsc.h"
#include "hb/endpoint.h"


// forwards
typedef struct hb_buffer_s hb_buffer_t;


#define HB_EVENT_MAX_SIZE 256
#define HB_EVENT_PAD_SIZE (HB_EVENT_MAX_SIZE) - sizeof(uint32_t) - sizeof(uint32_t)

#define HB_EVENT_FIELDS			\
	uint32_t id;				\
	uint32_t type;				\


typedef enum hb_event_type_e {
	HB_EVENT_NONE = 0,
	HB_EVENT_ERROR,
	HB_EVENT_CLIENT_OPEN,
	HB_EVENT_CLIENT_CLOSE,
	HB_EVENT_CLIENT_READ,
} hb_event_type_t;

// base event
typedef struct hb_event_base_s {
	HB_EVENT_FIELDS
	uint8_t pad[HB_EVENT_PAD_SIZE];
} hb_event_base_t;

// error event
typedef struct hb_event_error_s {
	HB_EVENT_FIELDS
	uint64_t client_id;
	hb_buffer_t *hb_buffer;
	int32_t error_code;
	char error_string[HB_EVENT_PAD_SIZE - sizeof(uint64_t) - sizeof(hb_buffer_t *) - sizeof(int32_t)];
} hb_event_error_t;

// client connected
typedef struct hb_event_client_open_s {
	HB_EVENT_FIELDS
	uint64_t client_id;
	char host_local[64];
	char host_remote[64];
} hb_event_client_open_t;

// client disconnected
typedef struct hb_event_client_close_s {
	HB_EVENT_FIELDS
	uint64_t client_id;
	hb_buffer_t *hb_buffer;
	int32_t error_code;
	char error_string[HB_EVENT_PAD_SIZE - sizeof(uint64_t) - sizeof(hb_buffer_t *) - sizeof(int32_t)];
} hb_event_client_close_t;

// client recv bytes
typedef struct hb_event_client_read_s {
	HB_EVENT_FIELDS
	uint64_t client_id;
	hb_buffer_t *hb_buffer;
	uint8_t *msg_ptr[10];
} hb_event_client_read_t;


typedef struct hb_event_list_s {
	hb_event_base_t *hb_events;
	hb_queue_spsc_t hb_events_free;
	hb_queue_spsc_t hb_events_ready;
	uint64_t capacity;
} hb_event_list_t;


int hb_event_list_setup(hb_event_list_t *list, uint64_t capacity);
void hb_event_list_cleanup(hb_event_list_t *list);

int hb_event_list_free_pop_back(hb_event_list_t *list, hb_event_base_t **out_evt);
int hb_event_list_free_pop_error(hb_event_list_t *list, hb_event_error_t **out_evt);
int hb_event_list_free_pop_open(hb_event_list_t *list, hb_event_client_open_t **out_evt);
int hb_event_list_free_pop_close(hb_event_list_t *list, hb_event_client_close_t **out_evt);
int hb_event_list_free_pop_read(hb_event_list_t *list, hb_event_client_read_t **out_evt);
int hb_event_list_ready_push(hb_event_list_t *list, void *out_evt);

int hb_event_list_ready_peek(hb_event_list_t *list, hb_event_base_t **out_evt);
int hb_event_list_ready_pop(hb_event_list_t *list);
void hb_event_list_ready_pop_cached(hb_event_list_t *list);
int hb_event_list_ready_pop_back(hb_event_list_t *list, hb_event_base_t **out_evt);
int hb_event_list_ready_pop_all(hb_event_list_t *list, hb_event_base_t **out_evt, uint64_t *out_count);
int hb_event_list_free_push(hb_event_list_t *list, void *evt);

#endif