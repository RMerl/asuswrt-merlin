/*
 * Simulate Posix AIO using pthreads.
 *
 * Based on the aio_fork work from Volker and Volker's pthreadpool library.
 *
 * Copyright (C) Volker Lendecke 2008
 * Copyright (C) Jeremy Allison 2012
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#include "includes.h"
#include "system/filesys.h"
#include "system/shmem.h"
#include "smbd/smbd.h"
#include "lib/pthreadpool/pthreadpool.h"

struct aio_extra;
static struct pthreadpool *pool;
static int aio_pthread_jobid;

struct aio_private_data {
	struct aio_private_data *prev, *next;
	int jobid;
	SMB_STRUCT_AIOCB *aiocb;
	ssize_t ret_size;
	int ret_errno;
	bool cancelled;
	bool write_command;
};

/* List of outstanding requests we have. */
static struct aio_private_data *pd_list;

static void aio_pthread_handle_completion(struct event_context *event_ctx,
				struct fd_event *event,
				uint16 flags,
				void *p);

/************************************************************************
 How many threads to initialize ?
 100 per process seems insane as a default until you realize that
 (a) Threads terminate after 1 second when idle.
 (b) Throttling is done in SMB2 via the crediting algorithm.
 (c) SMB1 clients are limited to max_mux (50) outstanding requests and
     Windows clients don't use this anyway.
 Essentially we want this to be unlimited unless smb.conf says different.
***********************************************************************/

static int aio_get_num_threads(struct vfs_handle_struct *handle)
{
	return lp_parm_int(SNUM(handle->conn),
			   "aio_pthread", "aio num threads", 100);
}

/************************************************************************
 Ensure thread pool is initialized.
***********************************************************************/

static bool init_aio_threadpool(struct vfs_handle_struct *handle)
{
	struct fd_event *sock_event = NULL;
	int ret = 0;
	int num_threads;
	int fd;

	if (pool) {
		return true;
	}

	num_threads = aio_get_num_threads(handle);
	ret = pthreadpool_init(num_threads, &pool);
	if (ret) {
		errno = ret;
		return false;
	}

	fd = pthreadpool_signal_fd(pool);

	set_blocking(fd, false);

	sock_event = tevent_add_fd(server_event_context(),
				NULL,
				fd,
				TEVENT_FD_READ,
				aio_pthread_handle_completion,
				NULL);
	if (sock_event == NULL) {
		pthreadpool_destroy(pool);
		pool = NULL;
		return false;
	}

	DEBUG(10,("init_aio_threadpool: initialized with up to %d threads\n",
			num_threads));

	return true;
}


/************************************************************************
 Worker function - core of the pthread aio engine.
 This is the function that actually does the IO.
***********************************************************************/

static void aio_worker(void *private_data)
{
	struct aio_private_data *pd =
			(struct aio_private_data *)private_data;

	if (pd->write_command) {
		pd->ret_size = sys_pwrite(pd->aiocb->aio_fildes,
				(const void *)pd->aiocb->aio_buf,
				pd->aiocb->aio_nbytes,
				pd->aiocb->aio_offset);
		if (pd->ret_size == -1 && errno == ESPIPE) {
			/* Maintain the fiction that pipes can
			   be seeked (sought?) on. */
			pd->ret_size = sys_write(pd->aiocb->aio_fildes,
					(const void *)pd->aiocb->aio_buf,
					pd->aiocb->aio_nbytes);
		}
	} else {
		pd->ret_size = sys_pread(pd->aiocb->aio_fildes,
				(void *)pd->aiocb->aio_buf,
				pd->aiocb->aio_nbytes,
				pd->aiocb->aio_offset);
		if (pd->ret_size == -1 && errno == ESPIPE) {
			/* Maintain the fiction that pipes can
			   be seeked (sought?) on. */
			pd->ret_size = sys_read(pd->aiocb->aio_fildes,
					(void *)pd->aiocb->aio_buf,
					pd->aiocb->aio_nbytes);
		}
	}
	if (pd->ret_size == -1) {
		pd->ret_errno = errno;
	} else {
		pd->ret_errno = 0;
	}
}

/************************************************************************
 Private data destructor.
***********************************************************************/

static int pd_destructor(struct aio_private_data *pd)
{
	DLIST_REMOVE(pd_list, pd);
	return 0;
}

/************************************************************************
 Create and initialize a private data struct.
***********************************************************************/

static struct aio_private_data *create_private_data(TALLOC_CTX *ctx,
					SMB_STRUCT_AIOCB *aiocb)
{
	struct aio_private_data *pd = talloc_zero(ctx, struct aio_private_data);
	if (!pd) {
		return NULL;
	}
	pd->jobid = aio_pthread_jobid++;
	pd->aiocb = aiocb;
	pd->ret_size = -1;
	pd->ret_errno = EINPROGRESS;
	talloc_set_destructor(pd, pd_destructor);
	DLIST_ADD_END(pd_list, pd, struct aio_private_data *);
	return pd;
}

/************************************************************************
 Spin off a threadpool (if needed) and initiate a pread call.
***********************************************************************/

static int aio_pthread_read(struct vfs_handle_struct *handle,
				struct files_struct *fsp,
				SMB_STRUCT_AIOCB *aiocb)
{
	struct aio_extra *aio_ex = (struct aio_extra *)aiocb->aio_sigevent.sigev_value.sival_ptr;
	struct aio_private_data *pd = NULL;
	int ret;

	if (!init_aio_threadpool(handle)) {
		return -1;
	}

	pd = create_private_data(aio_ex, aiocb);
	if (pd == NULL) {
		DEBUG(10, ("aio_pthread_read: Could not create private data.\n"));
		return -1;
	}

	ret = pthreadpool_add_job(pool, pd->jobid, aio_worker, (void *)pd);
	if (ret) {
		errno = ret;
		return -1;
	}

	DEBUG(10, ("aio_pthread_read: jobid=%d pread requested "
		"of %llu bytes at offset %llu\n",
		pd->jobid,
		(unsigned long long)pd->aiocb->aio_nbytes,
		(unsigned long long)pd->aiocb->aio_offset));

	return 0;
}

/************************************************************************
 Spin off a threadpool (if needed) and initiate a pwrite call.
***********************************************************************/

static int aio_pthread_write(struct vfs_handle_struct *handle,
				struct files_struct *fsp,
				SMB_STRUCT_AIOCB *aiocb)
{
	struct aio_extra *aio_ex = (struct aio_extra *)aiocb->aio_sigevent.sigev_value.sival_ptr;
	struct aio_private_data *pd = NULL;
	int ret;

	if (!init_aio_threadpool(handle)) {
		return -1;
	}

	pd = create_private_data(aio_ex, aiocb);
	if (pd == NULL) {
		DEBUG(10, ("aio_pthread_write: Could not create private data.\n"));
		return -1;
	}

	pd->write_command = true;

	ret = pthreadpool_add_job(pool, pd->jobid, aio_worker, (void *)pd);
	if (ret) {
		errno = ret;
		return -1;
	}

	DEBUG(10, ("aio_pthread_write: jobid=%d pwrite requested "
		"of %llu bytes at offset %llu\n",
		pd->jobid,
		(unsigned long long)pd->aiocb->aio_nbytes,
		(unsigned long long)pd->aiocb->aio_offset));

	return 0;
}

/************************************************************************
 Find the private data by jobid.
***********************************************************************/

static struct aio_private_data *find_private_data_by_jobid(int jobid)
{
	struct aio_private_data *pd;

	for (pd = pd_list; pd != NULL; pd = pd->next) {
		if (pd->jobid == jobid) {
			return pd;
		}
	}

	return NULL;
}

/************************************************************************
 Callback when an IO completes.
***********************************************************************/

static void aio_pthread_handle_completion(struct event_context *event_ctx,
				struct fd_event *event,
				uint16 flags,
				void *p)
{
	struct aio_extra *aio_ex = NULL;
	struct aio_private_data *pd = NULL;
	int jobid = 0;
	int ret;

	DEBUG(10, ("aio_pthread_handle_completion called with flags=%d\n",
			(int)flags));

	if ((flags & EVENT_FD_READ) == 0) {
		return;
	}

	while (true) {
		ret = pthreadpool_finished_job(pool, &jobid);

		if (ret == EINTR || ret == EAGAIN) {
			return;
		}
#ifdef EWOULDBLOCK
		if (ret == EWOULDBLOCK) {
			return;
		}
#endif

		if (ret == ECANCELED) {
			return;
		}

		if (ret) {
			smb_panic("aio_pthread_handle_completion");
			return;
		}

		pd = find_private_data_by_jobid(jobid);
		if (pd == NULL) {
			DEBUG(1, ("aio_pthread_handle_completion cannot find "
				  "jobid %d\n", jobid));
			return;
		}

		aio_ex = (struct aio_extra *)
			pd->aiocb->aio_sigevent.sigev_value.sival_ptr;

		smbd_aio_complete_aio_ex(aio_ex);

		DEBUG(10,("aio_pthread_handle_completion: jobid %d "
			  "completed\n", jobid ));
		TALLOC_FREE(aio_ex);
	}
}

/************************************************************************
 Find the private data by aiocb.
***********************************************************************/

static struct aio_private_data *find_private_data_by_aiocb(SMB_STRUCT_AIOCB *aiocb)
{
	struct aio_private_data *pd;

	for (pd = pd_list; pd != NULL; pd = pd->next) {
		if (pd->aiocb == aiocb) {
			return pd;
		}
	}

	return NULL;
}

/************************************************************************
 Called to return the result of a completed AIO.
 Should only be called if aio_error returns something other than EINPROGRESS.
 Returns:
	Any other value - return from IO operation.
***********************************************************************/

static ssize_t aio_pthread_return_fn(struct vfs_handle_struct *handle,
				struct files_struct *fsp,
				SMB_STRUCT_AIOCB *aiocb)
{
	struct aio_private_data *pd = find_private_data_by_aiocb(aiocb);

	if (pd == NULL) {
		errno = EINVAL;
		DEBUG(0, ("aio_pthread_return_fn: returning EINVAL\n"));
		return -1;
	}

	pd->aiocb = NULL;

	if (pd->ret_size == -1) {
		errno = pd->ret_errno;
	}

	return pd->ret_size;
}

/************************************************************************
 Called to check the result of an AIO.
 Returns:
	EINPROGRESS - still in progress.
	EINVAL - invalid aiocb.
	ECANCELED - request was cancelled.
	0 - request completed successfully.
	Any other value - errno from IO operation.
***********************************************************************/

static int aio_pthread_error_fn(struct vfs_handle_struct *handle,
			     struct files_struct *fsp,
			     SMB_STRUCT_AIOCB *aiocb)
{
	struct aio_private_data *pd = find_private_data_by_aiocb(aiocb);

	if (pd == NULL) {
		return EINVAL;
	}
	if (pd->cancelled) {
		return ECANCELED;
	}
	return pd->ret_errno;
}

/************************************************************************
 Called to request the cancel of an AIO, or all of them on a specific
 fsp if aiocb == NULL.
***********************************************************************/

static int aio_pthread_cancel(struct vfs_handle_struct *handle,
			struct files_struct *fsp,
			SMB_STRUCT_AIOCB *aiocb)
{
	struct aio_private_data *pd = NULL;

	for (pd = pd_list; pd != NULL; pd = pd->next) {
		if (pd->aiocb == NULL) {
			continue;
		}
		if (pd->aiocb->aio_fildes != fsp->fh->fd) {
			continue;
		}
		if ((aiocb != NULL) && (pd->aiocb != aiocb)) {
			continue;
		}

		/*
		 * We let the child do its job, but we discard the result when
		 * it's finished.
		 */

		pd->cancelled = true;
	}

	return AIO_CANCELED;
}

/************************************************************************
 Callback for a previously detected job completion.
***********************************************************************/

static void aio_pthread_handle_immediate(struct tevent_context *ctx,
				struct tevent_immediate *im,
				void *private_data)
{
	struct aio_extra *aio_ex = NULL;
	int *pjobid = (int *)private_data;
	struct aio_private_data *pd = find_private_data_by_jobid(*pjobid);

	if (pd == NULL) {
		DEBUG(1, ("aio_pthread_handle_immediate cannot find jobid %d\n",
			  *pjobid));
		TALLOC_FREE(pjobid);
		return;
	}

	TALLOC_FREE(pjobid);
	aio_ex = (struct aio_extra *)pd->aiocb->aio_sigevent.sigev_value.sival_ptr;
	smbd_aio_complete_aio_ex(aio_ex);
	TALLOC_FREE(aio_ex);
}

/************************************************************************
 Private data struct used in suspend completion code.
***********************************************************************/

struct suspend_private {
	int num_entries;
	int num_finished;
	const SMB_STRUCT_AIOCB * const *aiocb_array;
};

/************************************************************************
 Callback when an IO completes from a suspend call.
***********************************************************************/

static void aio_pthread_handle_suspend_completion(struct event_context *event_ctx,
				struct fd_event *event,
				uint16 flags,
				void *p)
{
	struct suspend_private *sp = (struct suspend_private *)p;
	struct aio_private_data *pd = NULL;
	struct tevent_immediate *im = NULL;
	int *pjobid = NULL;
	int i;

	DEBUG(10, ("aio_pthread_handle_suspend_completion called with flags=%d\n",
			(int)flags));

	if ((flags & EVENT_FD_READ) == 0) {
		return;
	}

	pjobid = talloc_array(NULL, int, 1);
	if (pjobid == NULL) {
		smb_panic("aio_pthread_handle_suspend_completion: no memory.");
	}

	if (pthreadpool_finished_job(pool, pjobid)) {
		smb_panic("aio_pthread_handle_suspend_completion: can't find job.");
		return;
	}

	pd = find_private_data_by_jobid(*pjobid);
	if (pd == NULL) {
		DEBUG(1, ("aio_pthread_handle_completion cannot find jobid %d\n",
			  *pjobid));
		TALLOC_FREE(pjobid);
		return;
	}

	/* Is this a jobid with an aiocb we're interested in ? */
	for (i = 0; i < sp->num_entries; i++) {
		if (sp->aiocb_array[i] == pd->aiocb) {
			sp->num_finished++;
			TALLOC_FREE(pjobid);
			return;
		}
	}

	/* Jobid completed we weren't waiting for.
	   We must reshedule this as an immediate event
	   on the main event context. */
	im = tevent_create_immediate(NULL);
	if (!im) {
		exit_server_cleanly("aio_pthread_handle_suspend_completion: no memory");
	}

	DEBUG(10,("aio_pthread_handle_suspend_completion: "
			"re-scheduling job id %d\n",
			*pjobid));

	tevent_schedule_immediate(im,
			server_event_context(),
			aio_pthread_handle_immediate,
			(void *)pjobid);
}


static void aio_pthread_suspend_timed_out(struct tevent_context *event_ctx,
					struct tevent_timer *te,
					struct timeval now,
					void *private_data)
{
	bool *timed_out = (bool *)private_data;
	/* Remove this timed event handler. */
	TALLOC_FREE(te);
	*timed_out = true;
}

/************************************************************************
 Called to request everything to stop until all IO is completed.
***********************************************************************/

static int aio_pthread_suspend(struct vfs_handle_struct *handle,
			struct files_struct *fsp,
			const SMB_STRUCT_AIOCB * const aiocb_array[],
			int n,
			const struct timespec *timeout)
{
	struct event_context *ev = NULL;
	struct fd_event *sock_event = NULL;
	int ret = -1;
	struct suspend_private sp;
	bool timed_out = false;
	TALLOC_CTX *frame = talloc_stackframe();

	/* This is a blocking call, and has to use a sub-event loop. */
	ev = event_context_init(frame);
	if (ev == NULL) {
		errno = ENOMEM;
		goto out;
	}

	if (timeout) {
		struct timeval tv = convert_timespec_to_timeval(*timeout);
		struct tevent_timer *te = tevent_add_timer(ev,
						frame,
						timeval_current_ofs(tv.tv_sec,
								    tv.tv_usec),
						aio_pthread_suspend_timed_out,
						&timed_out);
		if (!te) {
			errno = ENOMEM;
			goto out;
		}
	}

	ZERO_STRUCT(sp);
	sp.num_entries = n;
	sp.aiocb_array = aiocb_array;
	sp.num_finished = 0;

	sock_event = tevent_add_fd(ev,
				frame,
				pthreadpool_signal_fd(pool),
				TEVENT_FD_READ,
				aio_pthread_handle_suspend_completion,
				(void *)&sp);
	if (sock_event == NULL) {
		pthreadpool_destroy(pool);
		pool = NULL;
		goto out;
	}
	/*
	 * We're going to cheat here. We know that smbd/aio.c
	 * only calls this when it's waiting for every single
	 * outstanding call to finish on a close, so just wait
	 * individually for each IO to complete. We don't care
	 * what order they finish - only that they all do. JRA.
	 */
	while (sp.num_entries != sp.num_finished) {
		if (tevent_loop_once(ev) == -1) {
			goto out;
		}

		if (timed_out) {
			errno = EAGAIN;
			goto out;
		}
	}

	ret = 0;

  out:

	TALLOC_FREE(frame);
	return ret;
}

static struct vfs_fn_pointers vfs_aio_pthread_fns = {
	.aio_read = aio_pthread_read,
	.aio_write = aio_pthread_write,
	.aio_return_fn = aio_pthread_return_fn,
	.aio_cancel = aio_pthread_cancel,
	.aio_error_fn = aio_pthread_error_fn,
	.aio_suspend = aio_pthread_suspend,
};

NTSTATUS vfs_aio_pthread_init(void);
NTSTATUS vfs_aio_pthread_init(void)
{
	return smb_register_vfs(SMB_VFS_INTERFACE_VERSION,
				"aio_pthread", &vfs_aio_pthread_fns);
}
