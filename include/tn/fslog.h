#ifndef TN_FSLOG_H
#define TN_FSLOG_H

#include "tn/thread.h"
#include "tn/buffer.h"
#include "tn/buffer_pool.h"
#include "tn/tcp_service.h"

typedef enum tn_fslog_state_e {
	TN_FSLOG_STATE_NEW,
	TN_FSLOG_STATE_STARTING,
	TN_FSLOG_STATE_STARTED,
	TN_FSLOG_STATE_STOPPING,
	TN_FSLOG_STATE_STOPPED,
	TN_FSLOG_STATE_ERROR,
	TN_FSLOG_STATE_INVALID,
} tn_fslog_state_t;

typedef enum tn_fslog_type_e {
	TN_FSLOG_TYPE_NONE,
	TN_FSLOG_TYPE_READ,
	TN_FSLOG_TYPE_WRITE,
	TN_FSLOG_TYPE_INVALID,
} tn_fslog_type_t;

typedef struct tn_fslog_s {
	void *priv;
	tn_thread_t thread_io;
	tn_buffer_pool_t pool;
	tn_buffer_t *buffer;
	tn_fslog_state_t state;
	tn_fslog_type_t type;
} tn_fslog_t;

tn_fslog_state_t tn_fslog_get_state(tn_fslog_t *fslog);

int tn_fslog_setup(tn_fslog_t *fslog, tn_fslog_type_t type);
void tn_fslog_cleanup(tn_fslog_t *fslog);

int tn_fslog_open(tn_fslog_t *fslog);
int tn_fslog_close(tn_fslog_t *fslog);
int tn_fslog_read(tn_fslog_t *fslog);
int tn_fslog_write(tn_fslog_t *fslog);


#endif
