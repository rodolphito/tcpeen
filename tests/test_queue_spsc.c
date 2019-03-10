#include "hb/test_harness.h"

#include "hb/error.h"
#include "hb/allocator.h"
#include "hb/log.h"
#include "hb/atomic.h"
#include "hb/thread.h"
#include "hb/queue_spsc.h"


static uint64_t queue_capacity = (1 << 25);// (1 << 10);
static uint64_t msgs_limit = 10000000;
static uint64_t err_read, err_write;
static uint64_t msgs_processed;
static int threads_quit = 0;
static hb_atomic_t test_ready;
static hb_atomic_t threads_ready;

void consume_spsc_thread_run(void *priv)
{
	uintptr_t prev;
	uint64_t i, count;
	hb_queue_spsc_t *queue = priv;

	const uint64_t capacity = 100000;
	const uint16_t block_size = sizeof(void *);
	const uint64_t capacity_bytes = block_size * capacity;

	uint64_t *num = HB_MEM_ACQUIRE(capacity_bytes);

	uint64_t ready = false;
	hb_atomic_fetch_add(&threads_ready, 1);
	while (!ready) {
		ready = hb_atomic_load(&test_ready);
	}

	err_read = prev = 0;
	while (ready) {
		count = capacity;
		if (hb_queue_spsc_pop_all(queue, (void **)num, &count)) {
			hb_thread_sleep_ms(16);
			err_read++;
			continue;
		}

		for (i = 0; i < count; i++) {
			if (num[i] != (prev + 1)) {
				hb_log_error("returned: %zu but %zu was expected", num[i], (prev + 1));
				ready = 0;
				break;
			}
			prev++;
			if (prev >= msgs_limit) {
				ready = 0;
				break;
			}
		}
	}

	HB_MEM_RELEASE(num);
}


void produce_spsc_thread_run(void *priv)
{
	uintptr_t num = 1;
	hb_queue_spsc_t *queue = priv;

	uint64_t ready = false;
	hb_atomic_fetch_add(&threads_ready, 1);
	while (!ready) {
		ready = hb_atomic_load(&test_ready);
	}

	err_write = 0;
	while (ready) {
		if (hb_queue_spsc_push(queue, (void *)num)) {
			err_write++;
			continue;
		}

		if (num >= msgs_limit) break;
		num++;
	}
}


HB_TEST_CASE_BEGIN(queue_spsc_stress)
	hb_queue_spsc_t queue;
	hb_thread_t consume_thread, produce_thread;
	static uint64_t tstamp_start, tstamp_end;

	assert(((sizeof(queue) % HB_CACHE_LINE_SIZE) == 0));

	msgs_processed = 0;
	err_read = 0;
	err_write = 0;
	threads_quit = 0;

	hb_atomic_store(&test_ready, 0);
	hb_atomic_store(&threads_ready, 0);

	HB_GUARD_CLEANUP(hb_queue_spsc_setup(&queue, queue_capacity));
	HB_GUARD_CLEANUP(hb_thread_launch(&produce_thread, produce_spsc_thread_run, &queue));
	HB_GUARD_CLEANUP(hb_thread_launch(&consume_thread, consume_spsc_thread_run, &queue));

	while (hb_atomic_load(&threads_ready) < 2) {};
	hb_atomic_store(&test_ready, 1);

	tstamp_start = hb_tstamp();

	HB_GUARD_CLEANUP(hb_thread_join(&produce_thread));
	HB_GUARD_CLEANUP(hb_thread_join(&consume_thread));

	tstamp_end = hb_tstamp();

	hb_queue_spsc_cleanup(&queue);

	uint64_t elapsed_ns = tstamp_end - tstamp_start;
	double elapsed_s = (double)elapsed_ns / HB_TIME_NS_PER_S;
	double msgs_per_second = (double)msgs_limit / elapsed_s;

	hb_log_info("hb_queue_spsc test results ---------------------------------------");
	hb_log_info("%zu intptrs transferred in %0.2f seconds: %0.2f per second", msgs_limit, elapsed_s, msgs_per_second);
	hb_log_info("%zu read errors / %zu write errors", err_read, err_write);

	return HB_SUCCESS;

cleanup:
	return HB_ERROR;
}

HB_TEST_CASE_BEGIN(queue_spsc_empty)
	uintptr_t intptr;
	uintptr_t intptr2;
	hb_queue_spsc_t queue;
	ASSERT_SUCCESS(hb_queue_spsc_setup(&queue, 8));
	ASSERT_TRUE(HB_QUEUE_EMPTY == hb_queue_spsc_pop(&queue, (void **)&intptr));
	ASSERT_NULL(intptr);

	intptr = 235235;
	ASSERT_SUCCESS(hb_queue_spsc_push(&queue, (void *)intptr));

	ASSERT_SUCCESS(hb_queue_spsc_pop(&queue, (void **)&intptr2));
	ASSERT_TRUE(intptr2 == 235235);

	ASSERT_TRUE(HB_QUEUE_EMPTY == hb_queue_spsc_pop(&queue, (void **)&intptr2));
	ASSERT_NULL(intptr2);

	return HB_SUCCESS;
}

HB_TEST_CASE_BEGIN(queue_spsc_full)
	uintptr_t intptr = 97234;
	uintptr_t intptr2 = 83214;
	hb_queue_spsc_t queue;
	ASSERT_SUCCESS(hb_queue_spsc_setup(&queue, 8));

	for (int i = 0; i < 8; i++) {
		ASSERT_SUCCESS(hb_queue_spsc_push(&queue, (void *)intptr));
	}
	ASSERT_TRUE(HB_QUEUE_FULL == hb_queue_spsc_push(&queue, (void *)intptr));

	ASSERT_SUCCESS(hb_queue_spsc_pop(&queue, (void **)&intptr2));
	ASSERT_TRUE(intptr2 == 97234);

	ASSERT_SUCCESS(hb_queue_spsc_push(&queue, (void *)intptr));
	ASSERT_TRUE(HB_QUEUE_FULL == hb_queue_spsc_push(&queue, (void *)intptr));

	return HB_SUCCESS;
}

HB_TEST_CASE_BEGIN(queue_spsc_npot)
	hb_queue_spsc_t queue;
	ASSERT_TRUE(HB_ERROR_INVAL == hb_queue_spsc_setup(&queue, 11));
	return HB_SUCCESS;
}

HB_TEST_CASE(test_queue_spsc_stress, queue_spsc_stress)
HB_TEST_CASE(test_queue_spsc_empty, queue_spsc_empty)
HB_TEST_CASE(test_queue_spsc_full, queue_spsc_full)
HB_TEST_CASE(test_queue_spsc_npot, queue_spsc_npot)
