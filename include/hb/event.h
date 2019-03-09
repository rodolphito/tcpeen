#ifndef HB_EVENT_H
#define HB_EVENT_H

#include <stdint.h>

#include "hb/thread.h"
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
	int32_t error_code;
	char error_string[HB_EVENT_PAD_SIZE - sizeof(uint64_t) - sizeof(int32_t)];
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
	int32_t error_code;
	char error_string[HB_EVENT_PAD_SIZE - sizeof(uint64_t) - sizeof(int32_t)];
} hb_event_client_close_t;

// client recv bytes
typedef struct hb_event_client_read_s {
	HB_EVENT_FIELDS
	uint64_t client_id;
	hb_buffer_t *hb_buffer;
	uint8_t *buffer;
	uint64_t length;
} hb_event_client_read_t;


typedef struct hb_event_list_s {
	void *priv;
	uint64_t count_front;
	uint64_t count_back;
	uint64_t capacity;
	hb_event_base_t *event_front;
	hb_event_base_t *event_back;
	hb_event_base_t *event[2];
	hb_mutex_t mtx;
	uint8_t swap;
} hb_event_list_t;


int hb_event_list_setup(hb_event_list_t *list);
void hb_event_list_cleanup(hb_event_list_t *list);
void hb_event_list_heap(hb_event_list_t *list, hb_event_base_t **out_heap, uint64_t *out_block_count, uint64_t *out_block_size);

int hb_event_list_lock(hb_event_list_t *list);
int hb_event_list_unlock(hb_event_list_t *list);

// everything below requires the list to be locked
int hb_event_list_pop_swap(hb_event_list_t *list, hb_event_base_t **evt, uint64_t *count);
int hb_event_list_push_back(hb_event_list_t *list, hb_event_base_t **evt);

#endif