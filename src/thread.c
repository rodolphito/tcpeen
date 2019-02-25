#include "hb/thread.h"

#include "aws/common/thread.h"
#include "aws/common/clock.h"
#include "aws/common/mutex.h"

#include "hb/error.h"
#include "hb/log.h"
#include "hb/allocator.h"

// --------------------------------------------------------------------------------------------------------------
uint64_t hb_tstamp_convert(uint64_t tstamp, hb_tstamp_unit_t from, hb_tstamp_unit_t to, uint64_t *remainder)
{
	return aws_timestamp_convert(tstamp, from, to, remainder);
}

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
int hb_mutex_setup(hb_mutex_t *mtx)
{
	mtx->mtx_impl = HB_MEM_ACQUIRE(sizeof(struct aws_mutex));
	return aws_mutex_init(mtx->mtx_impl);
}

// --------------------------------------------------------------------------------------------------------------
int hb_mutex_lock(hb_mutex_t *mtx)
{
	return aws_mutex_lock(mtx->mtx_impl);
}

// --------------------------------------------------------------------------------------------------------------
int hb_mutex_unlock(hb_mutex_t *mtx)
{
	return aws_mutex_unlock(mtx->mtx_impl);
}

// --------------------------------------------------------------------------------------------------------------
void hb_mutex_cleanup(hb_mutex_t *mtx)
{
	aws_mutex_clean_up(mtx->mtx_impl);
	HB_MEM_RELEASE(mtx->mtx_impl);
}