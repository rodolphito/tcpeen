#ifndef HB_MUTEX_H
#define HB_MUTEX_H

#ifndef _WIN32
#    include <pthread.h>
#endif


typedef struct hb_mutex_s {
#ifdef _WIN32
	void *mutex_handle;
#else
	pthread_mutex_t mutex_handle;
#endif
} hb_mutex_t;


int hb_mutex_setup(hb_mutex_t *mtx);
int hb_mutex_lock(hb_mutex_t *mtx);
int hb_mutex_unlock(hb_mutex_t *mtx);
void hb_mutex_cleanup(hb_mutex_t *mtx);


#endif