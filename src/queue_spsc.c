#include "hb/queue_spsc.h"

#include "aws/common/atomics.h"

#include "hb/error.h"
#include "hb/allocator.h"
#include "hb/log.h"
#include "hb/atomic.h"


// --------------------------------------------------------------------------------------------------------------
int hb_queue_spsc_setup(hb_queue_spsc_t *q, size_t capacity)
{
	assert(q);
	assert(capacity > 0);

	//if (!((capacity != 0) && ((capacity & (~capacity + 1)) == capacity) && (capacity <= (UINT64_MAX >> 3) + 1))) return EINVAL;
	if (!capacity || (capacity & (capacity - 1))) return HB_ERROR_INVAL;

	int ret = HB_ERROR_NOMEM;
	q->buffer = NULL;
	q->buffer = HB_MEM_ACQUIRE(sizeof(*q->buffer) * (1 + capacity));
	HB_GUARD_NULL(q->buffer);

	hb_atomic_store(&q->head, 0);
	q->head_cache = 0;

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
int hb_queue_spsc_push(hb_queue_spsc_t *q, void *ptr)
{
	assert(q);

	const size_t head = hb_atomic_load_explicit(&q->head, HB_ATOMIC_RELAXED);
	const size_t tail = hb_atomic_load(&q->tail);
	const size_t size = head - tail;

	if (size == q->capacity) return HB_QUEUE_FULL;

	q->buffer[head & q->mask] = (uintptr_t)ptr;
	hb_atomic_store(&q->head, head + 1);

	return 0;
}

// --------------------------------------------------------------------------------------------------------------
int hb_queue_spsc_pop(hb_queue_spsc_t *q, void **out_ptr)
{
	assert(q);
	assert(out_ptr);

	*out_ptr = NULL;

	const size_t tail = hb_atomic_load_explicit(&q->tail, HB_ATOMIC_RELAXED);
	const size_t head = hb_atomic_load(&q->head);

	if (head == tail) return HB_QUEUE_EMPTY;

	*out_ptr = (void *)q->buffer[tail & q->mask];

	hb_atomic_store(&q->tail, tail + 1);

	return HB_SUCCESS;
}

// --------------------------------------------------------------------------------------------------------------
int hb_queue_spsc_pop_multi(hb_queue_spsc_t *q, void **out_ptr, uint64_t *out_count)
{
	assert(q);
	assert(out_ptr);
	assert(out_count);

	*out_ptr = NULL;

	uint64_t off;
	const uint64_t mask = q->mask;
	const uint64_t tail = hb_atomic_load_explicit(&q->tail, HB_ATOMIC_RELAXED);
	const uint64_t head = hb_atomic_load(&q->head);
	const uint64_t size = (head - tail);

	if (!size) return HB_QUEUE_EMPTY;

	if (size < *out_count) *out_count = size;
	for (off = 0; off < *out_count; off++) {
		out_ptr[off] = (void *)q->buffer[(tail + off) & q->mask];
	}

	hb_atomic_store_explicit(&q->tail, tail + *out_count, HB_ATOMIC_RELAXED);

	return 0;
}
