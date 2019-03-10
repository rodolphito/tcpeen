#include "hb/queue_spsc.h"

#include "aws/common/atomics.h"

#include "hb/error.h"
#include "hb/allocator.h"
#include "hb/log.h"
#include "hb/atomic.h"


// --------------------------------------------------------------------------------------------------------------
int hb_queue_spsc_setup(hb_queue_spsc_t *q, uint64_t capacity)
{
	assert(q);
	assert(capacity > 0);

	if (capacity & (capacity - 1)) {
		capacity--;
		capacity |= capacity >> 1;
		capacity |= capacity >> 2;
		capacity |= capacity >> 4;
		capacity |= capacity >> 8;
		capacity |= capacity >> 16;
		capacity |= capacity >> 32;
		capacity++;
	}
	assert(!(capacity & (capacity - 1)));

	int ret = HB_ERROR_NOMEM;
	q->buffer = NULL;
	q->buffer = HB_MEM_ACQUIRE(sizeof(*q->buffer) * (1 + capacity));
	HB_GUARD_NULL(q->buffer);

	hb_atomic_store(&q->head, 0);
	hb_atomic_store(&q->tail, 0);
	q->tail_cache = 0;
	q->capacity = capacity;
	q->mask = capacity - 1;

	return HB_SUCCESS;
}

// --------------------------------------------------------------------------------------------------------------
void hb_queue_spsc_cleanup(hb_queue_spsc_t *q)
{
	assert(q);
	HB_MEM_RELEASE(q->buffer);
}

// --------------------------------------------------------------------------------------------------------------
uint64_t hb_queue_spsc_count(hb_queue_spsc_t *q)
{
	assert(q);
	const uint64_t head = hb_atomic_load_explicit(&q->head, HB_ATOMIC_RELAXED);
	const uint64_t tail = hb_atomic_load(&q->tail);
	return (head - tail);
}

// --------------------------------------------------------------------------------------------------------------
uint64_t hb_queue_spsc_capacity(hb_queue_spsc_t *q)
{
	assert(q);
	return q->capacity;
}

// --------------------------------------------------------------------------------------------------------------
int hb_queue_spsc_empty(hb_queue_spsc_t *q)
{
	return (hb_queue_spsc_count(q) == 0);
}

// --------------------------------------------------------------------------------------------------------------
int hb_queue_spsc_full(hb_queue_spsc_t *q)
{
	return (hb_queue_spsc_count(q) == q->capacity);
}

// --------------------------------------------------------------------------------------------------------------
int hb_queue_spsc_push(hb_queue_spsc_t *q, void *ptr)
{
	assert(q);

	const uint64_t head = hb_atomic_load_explicit(&q->head, HB_ATOMIC_RELAXED);
	const uint64_t tail = hb_atomic_load(&q->tail);
	const uint64_t size = head - tail;

	if (size == q->capacity) return HB_QUEUE_FULL;

	q->buffer[head & q->mask] = (uintptr_t)ptr;
	hb_atomic_store(&q->head, head + 1);

	return HB_SUCCESS;
}

// --------------------------------------------------------------------------------------------------------------
int hb_queue_spsc_peek(hb_queue_spsc_t *q, void **out_ptr)
{
	assert(q);
	assert(out_ptr);

	*out_ptr = NULL;

	const uint64_t tail = hb_atomic_load_explicit(&q->tail, HB_ATOMIC_RELAXED);
	const uint64_t head = hb_atomic_load(&q->head);

	if (head == tail) return HB_QUEUE_EMPTY;

	*out_ptr = (void *)q->buffer[tail & q->mask];
	q->tail_cache = tail;

	return HB_SUCCESS;
}

// --------------------------------------------------------------------------------------------------------------
int hb_queue_spsc_pop(hb_queue_spsc_t *q)
{
	assert(q);

	const uint64_t tail = hb_atomic_load_explicit(&q->tail, HB_ATOMIC_RELAXED);
	const uint64_t head = hb_atomic_load(&q->head);

	if (head == tail) return HB_QUEUE_EMPTY;

	hb_atomic_store(&q->tail, tail + 1);

	return HB_SUCCESS;
}

// --------------------------------------------------------------------------------------------------------------
void hb_queue_spsc_pop_cached(hb_queue_spsc_t *q)
{
	hb_atomic_store(&q->tail, q->tail_cache + 1);
}

// --------------------------------------------------------------------------------------------------------------
int hb_queue_spsc_pop_back(hb_queue_spsc_t *q, void **out_ptr)
{
	assert(q);
	assert(out_ptr);

	*out_ptr = NULL;

	const uint64_t tail = hb_atomic_load_explicit(&q->tail, HB_ATOMIC_RELAXED);
	const uint64_t head = hb_atomic_load(&q->head);

	if (head == tail) return HB_QUEUE_EMPTY;

	*out_ptr = (void *)q->buffer[tail & q->mask];

	hb_atomic_store(&q->tail, tail + 1);

	return HB_SUCCESS;
}

// --------------------------------------------------------------------------------------------------------------
int hb_queue_spsc_pop_all(hb_queue_spsc_t *q, void **out_ptr, uint64_t *out_count)
{
	assert(q);
	assert(out_ptr);
	assert(out_count);
	assert(*out_count > 0);

	const uint64_t mask = q->mask;
	const uint64_t tail = hb_atomic_load_explicit(&q->tail, HB_ATOMIC_RELAXED);
	const uint64_t head = hb_atomic_load(&q->head);
	const uint64_t size = (head - tail);

	if (!size) return HB_QUEUE_EMPTY;

	if (size < *out_count) *out_count = size;
	for (uint64_t off = 0; off < *out_count; off++) {
		out_ptr[off] = (void *)q->buffer[(tail + off) & q->mask];
	}

	hb_atomic_store_explicit(&q->tail, tail + *out_count, HB_ATOMIC_RELAXED);

	return HB_SUCCESS;
}
