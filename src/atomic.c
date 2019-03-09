#include "hb/atomic.h"

#include "aws/common/atomics.h"


uint64_t hb_atomic_load(const volatile hb_atomic_t *a) { return aws_atomic_load_int_explicit((const volatile struct aws_atomic_var *)a, HB_ATOMIC_ACQUIRE); }
uint64_t hb_atomic_load_explicit(const volatile hb_atomic_t *a, int mem_order) { return aws_atomic_load_int_explicit((const volatile struct aws_atomic_var *)a, mem_order); }

void *hb_atomic_load_ptr(const volatile hb_atomic_t *a) { return aws_atomic_load_ptr_explicit((const volatile struct aws_atomic_var *)a, HB_ATOMIC_ACQUIRE); }
void *hb_atomic_load_ptr_explicit(const volatile hb_atomic_t *a, int mem_order) { return aws_atomic_load_ptr_explicit((const volatile struct aws_atomic_var *)a, mem_order); }

void hb_atomic_store(volatile hb_atomic_t *a, uint64_t val) { aws_atomic_store_int_explicit((volatile struct aws_atomic_var *)a, val, HB_ATOMIC_RELEASE); }
void hb_atomic_store_explicit(volatile hb_atomic_t *a, uint64_t val, int mem_order) { aws_atomic_store_int_explicit((volatile struct aws_atomic_var *)a, val, mem_order); }

void hb_atomic_store_ptr(volatile hb_atomic_t *a, void *val) { aws_atomic_store_ptr_explicit((volatile struct aws_atomic_var *)a, val, HB_ATOMIC_RELEASE); }
void hb_atomic_store_ptr_explicit(volatile hb_atomic_t *a, void *val, int mem_order) { aws_atomic_store_ptr_explicit((volatile struct aws_atomic_var *)a, val, mem_order); }

uint64_t hb_atomic_fetch_add(volatile hb_atomic_t *a, uint64_t val) { return aws_atomic_fetch_add_explicit((volatile struct aws_atomic_var *)a, val, HB_ATOMIC_ACQ_REL); }
uint64_t hb_atomic_store_fetch_add(volatile hb_atomic_t *a, uint64_t val, int mem_order) { return aws_atomic_fetch_add_explicit((volatile struct aws_atomic_var *)a, val, HB_ATOMIC_ACQ_REL); }
