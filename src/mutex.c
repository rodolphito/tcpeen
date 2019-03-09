#include "hb/mutex.h"

#include <assert.h>

#include "aws/common/mutex.h"


// --------------------------------------------------------------------------------------------------------------
int hb_mutex_setup(hb_mutex_t *mtx)
{
	assert(mtx);
	return aws_mutex_init((struct aws_mutex *)mtx);
}

// --------------------------------------------------------------------------------------------------------------
int hb_mutex_lock(hb_mutex_t *mtx)
{
	assert(mtx);
	return aws_mutex_lock((struct aws_mutex *)mtx);
}

// --------------------------------------------------------------------------------------------------------------
int hb_mutex_unlock(hb_mutex_t *mtx)
{
	assert(mtx);
	return aws_mutex_unlock((struct aws_mutex *)mtx);
}

// --------------------------------------------------------------------------------------------------------------
void hb_mutex_cleanup(hb_mutex_t *mtx)
{
	assert(mtx);
	aws_mutex_clean_up((struct aws_mutex *)mtx);
}
