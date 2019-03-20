#include "tn/fslog.h"

#include "uv.h"

#include "tn/error.h"
#include "tn/allocator.h"
#include "tn/log.h"


#ifdef _WIN32
#	include <io.h>
#	ifndef S_IRUSR
#		define S_IRUSR _S_IREAD
#	endif
#	ifndef S_IWUSR
#		define S_IWUSR _S_IWRITE
#	endif
#endif


typedef struct tn_fslog_priv_s {
	uv_loop_t *uv_loop;
	uv_timer_t *uv_timer;
	uv_fs_t uv_fsopen_req;
	uv_fs_t uv_fsread_req;
	uv_fs_t uv_fswrite_req;
	uv_fs_t uv_fsclose_req;
} tn_fslog_priv_t;


// private ------------------------------------------------------------------------------------------------------
void tn_fslog_set_state(tn_fslog_t *fslog, tn_fslog_state_t state)
{
	TN_ASSERT(fslog);
	fslog->state = state;
}

// private ------------------------------------------------------------------------------------------------------
void on_write(uv_fs_t *req)
{
	TN_ASSERT(req && req->data);

	tn_fslog_t *fslog = req->data;
	tn_fslog_priv_t *priv = fslog->priv;
	TN_GUARD_CLEANUP(req != &priv->uv_fswrite_req);

	if (req->result < 0) {
		tn_log_error("write error: %s", uv_strerror((int)req->result));
		tn_fslog_set_state(fslog, TN_FSLOG_STATE_ERROR);
	} else {
		tn_log_trace("wrote %zd bytes", req->result);
	}

cleanup:
	return;
}

// private ------------------------------------------------------------------------------------------------------
void on_fslog_read_cb(uv_fs_t *req)
{
	TN_ASSERT(req && req->data);

	tn_fslog_t *fslog = req->data;
	tn_fslog_priv_t *priv = fslog->priv;
	TN_GUARD_CLEANUP(req != &priv->uv_fsread_req);

	if (req->result < 0) {
		tn_log_error("read error: %s", uv_strerror((int)req->result));
		tn_fslog_set_state(fslog, TN_FSLOG_STATE_ERROR);
	} else if (req->result == 0) {
		tn_log_trace("eof");
		if (uv_fs_close(priv->uv_loop, &priv->uv_fsclose_req, (uv_file)priv->uv_fsopen_req.result, NULL)) {
			tn_log_error("failed to sync close file handle");
			tn_fslog_set_state(fslog, TN_FSLOG_STATE_ERROR);
		}
	} else if (req->result > 0) {
		tn_log_trace("read %zd bytes");
		uv_buf_t iov = uv_buf_init(tn_buffer_write_ptr(fslog->buffer), tn_buffer_capacity(fslog->buffer));
		TN_GUARD_CLEANUP(uv_fs_read(priv->uv_loop, &priv->uv_fsread_req, (uv_file)req->result, &iov, 1, -1, on_fslog_read_cb));
	}

cleanup:
	return;
}

// private ------------------------------------------------------------------------------------------------------
void on_fslog_open_cb(uv_fs_t *req)
{
	TN_ASSERT(req && req->data);

	tn_fslog_t *fslog = req->data;
	tn_fslog_priv_t *priv = fslog->priv;
	TN_GUARD_CLEANUP(req != &priv->uv_fsopen_req);

	if (req->result >= 0) {
		if (fslog->type == TN_FSLOG_TYPE_READ) {
			if (!fslog->buffer) {
				tn_log_trace("aquiring read buffer");
				TN_GUARD_CLEANUP(tn_buffer_pool_pop_back(&fslog->pool, &fslog->buffer));
			}
			tn_log_trace("starting read");
			uv_buf_t iov = uv_buf_init(tn_buffer_write_ptr(fslog->buffer), tn_buffer_capacity(fslog->buffer));
			TN_GUARD_CLEANUP(uv_fs_read(priv->uv_loop, &priv->uv_fsread_req, (uv_file)req->result, &iov, 1, -1, on_fslog_read_cb));
		}
	} else {
		tn_log_error("error opening fslog: %s", uv_strerror((int)req->result));
		tn_fslog_set_state(fslog, TN_FSLOG_STATE_ERROR);
	}

cleanup:
	return;
}

// private ------------------------------------------------------------------------------------------------------
void on_fslog_timer_cb(uv_timer_t *handle)
{
}

// private ------------------------------------------------------------------------------------------------------
void on_fslog_walk_cb(uv_handle_t* handle, void* arg)
{
	if (!uv_is_closing(handle)) {
		tn_log_warning("Manually closing handle: %p -- %s", handle, uv_handle_type_name(uv_handle_get_type(handle)));
		uv_close(handle, NULL);
	}
}

// private thread main ------------------------------------------------------------------------------------------
void tn_fslog_thread(tn_fslog_t *fslog)
{
	TN_ASSERT(fslog);

	int flags, mode;
	int ret = UV_ENOMEM;

	if (fslog->type == TN_FSLOG_TYPE_READ) {
		flags = O_RDONLY;
		mode = 0;
	} else if (fslog->type == TN_FSLOG_TYPE_WRITE) {
		flags = O_WRONLY | O_CREAT;
		mode = S_IWUSR | S_IRUSR;
	} else {
		tn_log_error("invalid fslog type");
	}

	TN_GUARD_NULL_CLEANUP(fslog->priv = TN_MEM_ACQUIRE(sizeof(tn_fslog_priv_t)));
	memset(fslog->priv, 0, sizeof(tn_fslog_priv_t));
	
	tn_fslog_priv_t *priv = fslog->priv;
	priv->uv_fsopen_req.data = fslog;
	priv->uv_fsread_req.data = fslog;
	priv->uv_fswrite_req.data = fslog;
	priv->uv_fsclose_req.data = fslog;

	TN_GUARD_CLEANUP(tn_buffer_pool_setup(&fslog->pool, TN_SERVICE_MAX_CLIENTS, TN_SERVICE_MAX_READ));
	
	priv->uv_loop = NULL;
	TN_GUARD_NULL_CLEANUP(priv->uv_loop = TN_MEM_ACQUIRE(sizeof(*priv->uv_loop)));
	TN_GUARD_CLEANUP(ret = uv_loop_init(priv->uv_loop));

	priv->uv_timer = NULL;
	TN_GUARD_NULL_CLEANUP(priv->uv_timer = TN_MEM_ACQUIRE(sizeof(*priv->uv_timer)));
	TN_GUARD_CLEANUP(ret = uv_timer_init(priv->uv_loop, priv->uv_timer));
	priv->uv_timer->data = fslog;
	TN_GUARD_CLEANUP(ret = uv_timer_start(priv->uv_timer, on_fslog_timer_cb, 100, 100));

	if (tn_fslog_get_state(fslog) == TN_FSLOG_STATE_STARTING) {
		tn_fslog_set_state(fslog, TN_FSLOG_STATE_STARTED);

		priv->uv_fsopen_req.data = fslog;
		TN_GUARD_CLEANUP(uv_fs_open(priv->uv_loop, &priv->uv_fsopen_req, "auditlog.tnl", flags, mode, on_fslog_open_cb));

		ret = uv_run(priv->uv_loop, UV_RUN_DEFAULT);
	}

	if ((ret = uv_loop_close(priv->uv_loop))) {
		tn_log_warning("Walking fslog handles due to unclean IO loop close");
		uv_walk(priv->uv_loop, on_fslog_walk_cb, NULL);
		TN_GUARD_CLEANUP(ret = uv_loop_close(priv->uv_loop));
	}

	ret = TN_SUCCESS;

cleanup:
	if (ret != TN_SUCCESS) {
		tn_log_uv_error(ret);
		tn_fslog_set_state(fslog, TN_FSLOG_STATE_ERROR);
	}

	tn_buffer_pool_cleanup(&fslog->pool);

	TN_MEM_RELEASE(priv->uv_timer);
	TN_MEM_RELEASE(priv->uv_loop);
	TN_MEM_RELEASE(fslog->priv);
}

// --------------------------------------------------------------------------------------------------------------
tn_fslog_state_t tn_fslog_get_state(tn_fslog_t *fslog)
{
	TN_ASSERT(fslog);
	return fslog->state;
}

// --------------------------------------------------------------------------------------------------------------
int tn_fslog_setup(tn_fslog_t *fslog, tn_fslog_type_t type)
{
	TN_ASSERT(fslog);
	memset(fslog, 0, sizeof(*fslog));

	tn_fslog_state_t state = tn_fslog_get_state(fslog);
	TN_GUARD(state != TN_FSLOG_STATE_NEW && state != TN_FSLOG_STATE_STOPPED);

	fslog->type = type;
	fslog->buffer = NULL;
	tn_fslog_set_state(fslog, TN_FSLOG_STATE_STARTING);

	TN_GUARD(tn_thread_launch(&fslog->thread_io, tn_fslog_thread, fslog));

	return TN_SUCCESS;
}

// --------------------------------------------------------------------------------------------------------------
void tn_fslog_cleanup(tn_fslog_t *fslog)
{
	TN_ASSERT(fslog);

	tn_fslog_state_t state = tn_fslog_get_state(fslog);
	TN_GUARD_CLEANUP(state != TN_FSLOG_STATE_STARTING && state != TN_FSLOG_STATE_STARTED);

	tn_log_trace("fslog waiting for thread to stop ...");
	tn_fslog_set_state(fslog, TN_FSLOG_STATE_STOPPING);
	if ((tn_thread_join(&fslog->thread_io))) {
		tn_log_error("error joining fslog IO thread");
	}
	tn_thread_cleanup(&fslog->thread_io);

	tn_fslog_set_state(fslog, TN_FSLOG_STATE_STOPPED);

cleanup:
	return;
}

// --------------------------------------------------------------------------------------------------------------
int tn_fslog_open(tn_fslog_t *fslog)
{
	TN_ASSERT(fslog && fslog->priv);
	tn_fslog_priv_t *priv = fslog->priv;

	return TN_SUCCESS;
}

// --------------------------------------------------------------------------------------------------------------
int tn_fslog_close(tn_fslog_t *fslog)
{
	TN_ASSERT(fslog && fslog->priv);
	tn_fslog_priv_t *priv = fslog->priv;

	return TN_SUCCESS;
}

// --------------------------------------------------------------------------------------------------------------
int tn_fslog_read(tn_fslog_t *fslog)
{
	TN_ASSERT(fslog && fslog->priv && fslog->type == TN_FSLOG_TYPE_READ);
	tn_fslog_priv_t *priv = fslog->priv;

	return TN_SUCCESS;
}

// --------------------------------------------------------------------------------------------------------------
int tn_fslog_write(tn_fslog_t *fslog)
{
	TN_ASSERT(fslog && fslog->priv && fslog->type == TN_FSLOG_TYPE_WRITE);
	tn_fslog_priv_t *priv = fslog->priv;

	return TN_SUCCESS;
}
