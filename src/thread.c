#include "hb/thread.h"

#include "aws/common/thread.h"

#include "hb/error.h"
#include "hb/log.h"
#include "hb/allocator.h"


// --------------------------------------------------------------------------------------------------------------
void hb_thread_sleep(uint64_t ns)
{
	aws_thread_current_sleep(ns);
}

// --------------------------------------------------------------------------------------------------------------
uint64_t hb_thread_id(void)
{
	return aws_thread_current_thread_id();
}

// --------------------------------------------------------------------------------------------------------------
int hb_thread_setup(hb_thread_t *thread)
{
	assert(thread);
	return aws_thread_init((struct aws_thread *)thread, aws_default_allocator());
}

// --------------------------------------------------------------------------------------------------------------
int hb_thread_launch(hb_thread_t *thread, void(*func)(void *arg), void *arg)
{
	assert(thread);
	hb_thread_setup(thread);
	return aws_thread_launch((struct aws_thread *)thread, func, arg, aws_default_thread_options());
}

// --------------------------------------------------------------------------------------------------------------
uint64_t hb_thread_get_id(hb_thread_t *thread)
{
	assert(thread);
	return aws_thread_get_id((struct aws_thread *)thread);
}


enum hb_thread_state hb_thread_get_state(hb_thread_t *thread)
{
	assert(thread);
	return (enum hb_thread_state)aws_thread_get_detach_state((struct aws_thread *)thread);
}

// --------------------------------------------------------------------------------------------------------------
int hb_thread_join(hb_thread_t *thread)
{
	assert(thread);
	return aws_thread_join((struct aws_thread *)thread);
}

// --------------------------------------------------------------------------------------------------------------
void hb_thread_cleanup(hb_thread_t *thread)
{
	assert(thread);
	aws_thread_clean_up((struct aws_thread *)thread);
}
