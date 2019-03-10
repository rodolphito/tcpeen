#ifndef HB_QUEUE_SPSC_H
#define HB_QUEUE_SPSC_H

#include <stdint.h>

#include "hb/atomic.h"


#define HB_QUEUE_EMPTY -100
#define HB_QUEUE_FULL -101
#define HB_QUEUE_AGAIN -102

#define HB_CACHE_LINE_SIZE 64


typedef struct hb_queue_spsc_s {
	uint64_t capacity;
	uint64_t mask;
	uintptr_t *buffer;
	uint8_t cache_line_pad0[HB_CACHE_LINE_SIZE - sizeof(uint64_t) - sizeof(uint64_t) - sizeof(uintptr_t *)];

	hb_atomic_t head;
	uint64_t tail_cache;
	uint8_t cache_line_pad1[HB_CACHE_LINE_SIZE - sizeof(hb_atomic_t) - sizeof(uint64_t)];

	hb_atomic_t tail;
	uint64_t head_cache;
	uint8_t cache_line_pad2[HB_CACHE_LINE_SIZE - sizeof(hb_atomic_t) - sizeof(uint64_t)];
} hb_queue_spsc_t;


int hb_queue_spsc_setup(hb_queue_spsc_t *q, uint64_t capacity);
void hb_queue_spsc_cleanup(hb_queue_spsc_t *q);
uint64_t hb_queue_spsc_capacity(hb_queue_spsc_t *q);
int hb_queue_spsc_push(hb_queue_spsc_t *q, void *ptr);
int hb_queue_spsc_pop(hb_queue_spsc_t *q, void **out_ptr);
int hb_queue_spsc_pop_all(hb_queue_spsc_t *q, void **out_ptr, uint64_t *out_count);


#endif