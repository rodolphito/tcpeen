#include "hb/mutex.h"

#include <assert.h>

#include "aws/common/mutex.h"


// --------------------------------------------------------------------------------------------------------------
int tn_mutex_setup(tn_mutex_t *mtx)
{
	assert(mtx);
	return aws_mutex_init((struct aws_mutex *)mtx);
}

// --------------------------------------------------------------------------------------------------------------
int tn_mutex_lock(tn_mutex_t *mtx)
{
	assert(mtx);
	return aws_mutex_lock((struct aws_mutex *)mtx);
}

// --------------------------------------------------------------------------------------------------------------
int tn_mutex_unlock(tn_mutex_t *mtx)
{
	assert(mtx);
	return aws_mutex_unlock((struct aws_mutex *)mtx);
}

// --------------------------------------------------------------------------------------------------------------
void tn_mutex_cleanup(tn_mutex_t *mtx)
{
	assert(mtx);
	aws_mutex_clean_up((struct aws_mutex *)mtx);
}
