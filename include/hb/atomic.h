#ifndef HB_ATOMIC_H
#define HB_ATOMIC_H


#include <stdint.h>


typedef struct hb_atomic_s {
	void *value;
} hb_atomic_t;


// atomic memory barriers
#define HB_ATOMIC_RELAXED 0
#define HB_ATOMIC_ACQUIRE 2
#define HB_ATOMIC_RELEASE 3
#define HB_ATOMIC_ACQ_REL 4
#define HB_ATOMIC_SEQ_CST 5


uint64_t hb_atomic_load(const volatile hb_atomic_t *a);
uint64_t hb_atomic_load_explicit(const volatile hb_atomic_t *a, int mem_order);

void *hb_atomic_load_ptr(const volatile hb_atomic_t *a);
void *hb_atomic_load_ptr_explicit(const volatile hb_atomic_t *a, int mem_order);

void hb_atomic_store(volatile hb_atomic_t *a, uint64_t val);
void hb_atomic_store_explicit(volatile hb_atomic_t *a, uint64_t val, int mem_order);

void hb_atomic_store_ptr(volatile hb_atomic_t *a, void *val);
void hb_atomic_store_ptr_explicit(volatile hb_atomic_t *a, void *val, int mem_order);

uint64_t hb_atomic_fetch_add(volatile hb_atomic_t *a, uint64_t val);
uint64_t hb_atomic_store_fetch_add(volatile hb_atomic_t *a, uint64_t val, int mem_order);

#endif