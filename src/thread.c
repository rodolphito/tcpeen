#include "tn/thread.h"

#include "aws/common/thread.h"

#include "tn/error.h"
#include "tn/log.h"
#include "tn/allocator.h"


// --------------------------------------------------------------------------------------------------------------
void tn_thread_sleep(uint64_t ns)
{
	aws_thread_current_sleep(ns);
}

// --------------------------------------------------------------------------------------------------------------
uint64_t tn_thread_id(void)
{
	return aws_thread_current_thread_id();
}

// --------------------------------------------------------------------------------------------------------------
int tn_thread_setup(tn_thread_t *thread)
{
	assert(thread);
	return aws_thread_init((struct aws_thread *)thread, aws_default_allocator());
}

// --------------------------------------------------------------------------------------------------------------
int tn_thread_launch(tn_thread_t *thread, void(*func)(void *arg), void *arg)
{
	assert(thread);
	tn_thread_setup(thread);
	return aws_thread_launch((struct aws_thread *)thread, func, arg, aws_default_thread_options());
}

// --------------------------------------------------------------------------------------------------------------
uint64_t tn_thread_get_id(tn_thread_t *thread)
{
	assert(thread);
	return aws_thread_get_id((struct aws_thread *)thread);
}


enum tn_thread_state tn_thread_get_state(tn_thread_t *thread)
{
	assert(thread);
	return (enum tn_thread_state)aws_thread_get_detach_state((struct aws_thread *)thread);
}

// --------------------------------------------------------------------------------------------------------------
int tn_thread_join(tn_thread_t *thread)
{
	assert(thread);
	return aws_thread_join((struct aws_thread *)thread);
}

// --------------------------------------------------------------------------------------------------------------
void tn_thread_cleanup(tn_thread_t *thread)
{
	assert(thread);
	aws_thread_clean_up((struct aws_thread *)thread);
}
