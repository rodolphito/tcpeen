#ifndef HB_THREAD_H
#define HB_THREAD_H

#include <stdint.h>

#ifndef _WIN32
#	include <pthread.h>
#endif

#include "hb/time.h"


#define HB_TIME_MS_PER_S 1000
#define HB_TIME_US_PER_S 1000000
#define HB_TIME_NS_PER_S 1000000000

#define HB_TIME_US_PER_MS 1000
#define HB_TIME_NS_PER_MS 1000000

#define HB_TIME_NS_PER_US 1000

#define HB_THREAD_SLEEP_S(s) aws_thread_current_sleep(ms * HB_TIME_NS_PER_S)
#define HB_THREAD_SLEEP_MS(ms) aws_thread_current_sleep(ms * HB_TIME_NS_PER_MS)


enum hb_thread_state {
	HB_THREAD_NEW,
	HB_THREAD_READY,
	HB_THREAD_JOINABLE,
	HB_THREAD_JOINED,
};

typedef struct hb_thread_s {
	struct aws_allocator *allocator;
	enum hb_thread_state detach_state;
#ifdef _WIN32
	void *thread_handle;
	unsigned long thread_id;
#else
	pthread_t thread_id;
#endif
} hb_thread_t;



void hb_thread_sleep(uint64_t ns);
#define hb_thread_sleep_s(tstamp) hb_thread_sleep(hb_tstamp_convert(tstamp, HB_TSTAMP_S, HB_TSTAMP_NS))
#define hb_thread_sleep_ms(tstamp) hb_thread_sleep(hb_tstamp_convert(tstamp, HB_TSTAMP_MS, HB_TSTAMP_NS))

uint64_t hb_thread_id(void);
int hb_thread_setup(hb_thread_t *thread);
int hb_thread_launch(hb_thread_t *thread, void(*func)(void *arg), void *arg);
uint64_t hb_thread_get_id(hb_thread_t *thread);
enum hb_thread_state hb_thread_get_state(hb_thread_t *thread);
int hb_thread_join(hb_thread_t *thread);
void hb_thread_cleanup(hb_thread_t *thread);





#endif
