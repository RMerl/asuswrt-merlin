/*
 * Unix SMB/CIFS implementation.
 * Support for OneFS system interfaces.
 *
 * Copyright (C) Zack Kirsch, 2009
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
 * along with this program; if not, see <http://www.gnu.org/licenses/>.
 */

#include "includes.h"
#include "onefs.h"

#include <ifs/ifs_syscalls.h>
#include <sys/isi_cifs_brl.h>
#include <isi_ecs/isi_ecs_cbrl.h>

#undef DBGC_CLASS
#define DBGC_CLASS DBGC_LOCKING

extern struct blocking_lock_record *blocking_lock_queue;

static uint64_t onefs_get_new_id(void) {
	static uint64_t id = 0;

	id++;

	return id;
}

enum onefs_cbrl_lock_state {ONEFS_CBRL_NONE, ONEFS_CBRL_ASYNC, ONEFS_CBRL_DONE,
	ONEFS_CBRL_ERROR};

struct onefs_cbrl_blr_state {
	uint64_t id;
	enum onefs_cbrl_lock_state state;
};

static char *onefs_cbrl_blr_state_str(const struct blocking_lock_record *blr)
{
	static fstring result;
	struct onefs_cbrl_blr_state *bs;

	SMB_ASSERT(blr);

	bs = (struct onefs_cbrl_blr_state *)blr->blr_private;

	if (bs == NULL) {
		fstrcpy(result, "NULL CBRL BLR state - Posix lock?");
		return result;
	}

	switch (bs->state) {
	case ONEFS_CBRL_NONE:
		fstr_sprintf(result, "CBRL BLR id=%llu: state=NONE", bs->id);
		break;
	case ONEFS_CBRL_ASYNC:
		fstr_sprintf(result, "CBRL BLR id=%llu: state=ASYNC", bs->id);
		break;
	case ONEFS_CBRL_DONE:
		fstr_sprintf(result, "CBRL BLR id=%llu: state=DONE", bs->id);
		break;
	case ONEFS_CBRL_ERROR:
		fstr_sprintf(result, "CBRL BLR id=%llu: state=ERROR", bs->id);
		break;
	default:
		fstr_sprintf(result, "CBRL BLR id=%llu: unknown state %d",
		    bs->id, bs->state);
		break;
	}

	return result;
}

static void onefs_cbrl_enumerate_blq(const char *fn)
{
	struct blocking_lock_record *blr;

	if (DEBUGLVL(10))
		return;

	DEBUG(10, ("CBRL BLR records (%s):\n", fn));

	for (blr = blocking_lock_queue; blr; blr = blr->next)
		DEBUGADD(10, ("%s\n", onefs_cbrl_blr_state_str(blr)));
}

static struct blocking_lock_record *onefs_cbrl_find_blr(uint64_t id)
{
	struct blocking_lock_record *blr;
	struct onefs_cbrl_blr_state *bs;

	onefs_cbrl_enumerate_blq("onefs_cbrl_find_blr");

	for (blr = blocking_lock_queue; blr; blr = blr->next) {
		bs = (struct onefs_cbrl_blr_state *)blr->blr_private;

		/* We don't control all of the BLRs on the BLQ. */
		if (bs == NULL)
			continue;

		if (bs->id == id) {
			DEBUG(10, ("found %s\n",
			    onefs_cbrl_blr_state_str(blr)));
			break;
		}
	}

	if (blr == NULL) {
		DEBUG(5, ("Could not find CBRL BLR for id %llu\n", id));
		return NULL;
	}

	return blr;
}

static void onefs_cbrl_async_success(uint64_t id)
{
	struct blocking_lock_record *blr;
	struct onefs_cbrl_blr_state *bs;
	uint16 num_locks;

	DEBUG(10, ("CBRL async success!\n"));

	/* Find BLR with id. Its okay not to find one (race with cancel) */
	blr = onefs_cbrl_find_blr(id);
	if (blr == NULL)
		return;

	bs = (struct onefs_cbrl_blr_state *)blr->blr_private;
	SMB_ASSERT(bs);
	SMB_ASSERT(bs->state == ONEFS_CBRL_ASYNC);

	blr->lock_num++;

	num_locks = SVAL(blr->req->vwv+7, 0);

	if (blr->lock_num == num_locks)
		bs->state = ONEFS_CBRL_DONE;
	else
		bs->state = ONEFS_CBRL_NONE;

	/* Self contend our own level 2 oplock. The kernel handles
	 * contention of other opener's level 2 oplocks. */
	contend_level2_oplocks_begin(blr->fsp,
	    LEVEL2_CONTEND_WINDOWS_BRL);

	/* Process the queue, to try the next lock or finish up. */
	process_blocking_lock_queue();
}

static void onefs_cbrl_async_failure(uint64_t id)
{
	struct blocking_lock_record *blr;
	struct onefs_cbrl_blr_state *bs;

	DEBUG(10, ("CBRL async failure!\n"));

	/* Find BLR with id. Its okay not to find one (race with cancel) */
	blr = onefs_cbrl_find_blr(id);
	if (blr == NULL)
		return;

	bs = (struct onefs_cbrl_blr_state *)blr->blr_private;
	SMB_ASSERT(bs);

	SMB_ASSERT(bs->state == ONEFS_CBRL_ASYNC);
	bs->state = ONEFS_CBRL_ERROR;

	/* Process the queue. It will end up trying to retake the same lock,
	 * see the error in onefs_cbrl_lock_windows() and fail. */
	process_blocking_lock_queue();
}

static struct cbrl_event_ops cbrl_ops =
    {.cbrl_async_success = onefs_cbrl_async_success,
     .cbrl_async_failure = onefs_cbrl_async_failure};
 
static void onefs_cbrl_events_handler(struct event_context *ev,
					struct fd_event *fde,
					uint16_t flags,
					void *private_data)
{
	DEBUG(10, ("onefs_cbrl_events_handler\n"));

	if (cbrl_event_dispatcher(&cbrl_ops)) {
		DEBUG(0, ("cbrl_event_dispatcher failed: %s\n",
			strerror(errno)));
	}
}

static void onefs_init_cbrl(void)
{
	static bool init_done = false;
	static int cbrl_event_fd;
	static struct fd_event *cbrl_fde;

	if (init_done)
		return;

	DEBUG(10, ("onefs_init_cbrl\n"));

	/* Register the event channel for CBRL. */
	cbrl_event_fd = cbrl_event_register();
	if (cbrl_event_fd == -1) {
		DEBUG(0, ("cbrl_event_register failed: %s\n",
			strerror(errno)));
		return;
	}

	DEBUG(10, ("cbrl_event_fd = %d\n", cbrl_event_fd));

	/* Register the CBRL event_fd with samba's event system */
	cbrl_fde = event_add_fd(smbd_event_context(),
				     NULL,
				     cbrl_event_fd,
				     EVENT_FD_READ,
				     onefs_cbrl_events_handler,
				     NULL);

	init_done = true;
	return;
}

/**
 * Blocking PID. As far as I can tell, the blocking_pid is only used to tell
 * whether a posix lock or a CIFS lock blocked us. If it was a posix lock,
 * Samba polls every 10 seconds, which we don't want. -zkirsch
 */
#define ONEFS_BLOCKING_PID 0xABCDABCD

/**
 * @param[in]     br_lck        Contains the fsp.
 * @param[in]     plock         Lock request.
 * @param[in]     blocking_lock Only used for figuring out the error.
 * @param[in,out] blr           The BLR for the already-deferred operation.
 */
NTSTATUS onefs_brl_lock_windows(vfs_handle_struct *handle,
				struct byte_range_lock *br_lck,
				struct lock_struct *plock,
				bool blocking_lock,
				struct blocking_lock_record *blr)
{
	int fd = br_lck->fsp->fh->fd;
	uint64_t id = 0;
	enum cbrl_lock_type type;
	bool async = false;
	bool pending = false;
	bool pending_async = false;
	int error;
	struct onefs_cbrl_blr_state *bs;
	NTSTATUS status;

	START_PROFILE(syscall_brl_lock);

	SMB_ASSERT(plock->lock_flav == WINDOWS_LOCK);
	SMB_ASSERT(plock->lock_type != UNLOCK_LOCK);

	onefs_cbrl_enumerate_blq("onefs_brl_lock_windows");

	/* Will only initialize the first time its called. */
	onefs_init_cbrl();

	switch (plock->lock_type) {
		case WRITE_LOCK:
			type = CBRL_LK_EX;
			break;
		case READ_LOCK:
			type = CBRL_LK_SH;
			break;
		case PENDING_WRITE_LOCK:
			/* Called when a blocking lock request is added - do an
			 * async lock. */
			type = CBRL_LK_EX;
			pending = true;
			async = true;
			break;
		case PENDING_READ_LOCK:
			/* Called when a blocking lock request is added - do an
			 * async lock. */
			type = CBRL_LK_SH;
			pending = true;
			async = true;
			break;
		default:
			/* UNLOCK_LOCK: should only be used for a POSIX_LOCK */
			smb_panic("Invalid plock->lock_type passed in to "
			    "onefs_brl_lock_windows");
	}

	/* Figure out if we're actually doing the lock or a no-op. We need to
	 * do a no-op when process_blocking_lock_queue calls back into us.
	 *
	 * We know process_* is calling into us if a blr is passed in and
	 * pending is false. */
	if (!pending && blr) {
		/* Check the BLR state. */
		bs = (struct onefs_cbrl_blr_state *)blr->blr_private;
		SMB_ASSERT(bs);

		/* ASYNC still in progress: The process_* calls will keep
		 * calling even if we haven't gotten the lock. Keep erroring
		 * without calling ifs_cbrl, or getting/setting an id. */
		if (bs->state == ONEFS_CBRL_ASYNC) {
			goto failure;
		}
		else if (bs->state == ONEFS_CBRL_ERROR) {
			END_PROFILE(syscall_brl_lock);
			return NT_STATUS_NO_MEMORY;
		}

		SMB_ASSERT(bs->state == ONEFS_CBRL_NONE);
		async = true;
	}

	if (async) {
		SMB_ASSERT(blocking_lock);
		SMB_ASSERT(blr);
		id = onefs_get_new_id();
	}

	DEBUG(10, ("Calling ifs_cbrl(LOCK)...\n"));
	error = ifs_cbrl(fd, CBRL_OP_LOCK, type, plock->start,
	    plock->size, async, id, plock->context.smbpid, plock->context.tid,
	    plock->fnum);
	if (!error) {
		goto success;
	} else if (errno == EWOULDBLOCK) {
		SMB_ASSERT(!async);
	} else if (errno == EINPROGRESS) {
		SMB_ASSERT(async);

		if (pending) {
			/* Talloc a new BLR private state. */
			blr->blr_private = talloc(blr, struct onefs_cbrl_blr_state);
			pending_async = true;
		}

		/* Store the new id in the BLR private state. */
		bs = (struct onefs_cbrl_blr_state *)blr->blr_private;
		bs->id = id;
		bs->state = ONEFS_CBRL_ASYNC;
	} else {
		DEBUG(0, ("onefs_brl_lock_windows failure: error=%d (%s).\n",
			errno, strerror(errno)));
	}

failure:

	END_PROFILE(syscall_brl_lock);

	/* Failure - error or async. */
	plock->context.smbpid = (uint32) ONEFS_BLOCKING_PID;

	if (pending_async)
		status = NT_STATUS_OK;
	else
		status = brl_lock_failed(br_lck->fsp, plock, blocking_lock);

	DEBUG(10, ("returning %s.\n", nt_errstr(status)));
	return status;

success:
	/* Self contend our own level 2 oplock. The kernel handles
	 * contention of other opener's level 2 oplocks. */
	contend_level2_oplocks_begin(br_lck->fsp,
	    LEVEL2_CONTEND_WINDOWS_BRL);

	END_PROFILE(syscall_brl_lock);

	/* Success. */
	onefs_cbrl_enumerate_blq("onefs_brl_unlock_windows");
	DEBUG(10, ("returning NT_STATUS_OK.\n"));
	return NT_STATUS_OK;
}

bool onefs_brl_unlock_windows(vfs_handle_struct *handle,
			      struct messaging_context *msg_ctx,
			      struct byte_range_lock *br_lck,
			      const struct lock_struct *plock)
{
	int error;
	int fd = br_lck->fsp->fh->fd;

	START_PROFILE(syscall_brl_unlock);

	SMB_ASSERT(plock->lock_flav == WINDOWS_LOCK);
	SMB_ASSERT(plock->lock_type == UNLOCK_LOCK);

	DEBUG(10, ("Calling ifs_cbrl(UNLOCK)...\n"));
	error = ifs_cbrl(fd, CBRL_OP_UNLOCK, CBRL_LK_SH,
	    plock->start, plock->size, false, 0, plock->context.smbpid,
	    plock->context.tid, plock->fnum);

	END_PROFILE(syscall_brl_unlock);

	if (error) {
		DEBUG(10, ("returning false.\n"));
		return false;
	}

	/* For symmetry purposes, end our oplock contention even though its
	 * currently a no-op. */
	contend_level2_oplocks_end(br_lck->fsp, LEVEL2_CONTEND_WINDOWS_BRL);

	DEBUG(10, ("returning true.\n"));
	return true;

	/* Problem with storing things in TDB: I won't know what BRL to unlock in the TDB.
	 *  - I could fake it?
	 *  - I could send Samba a message with which lock is being unlocked?
	 *  - I could *easily* make the "id" something you always pass in to
	 *  lock, unlock or cancel -- it identifies a lock. Makes sense!
	 */
}

/* Default implementation only calls this on PENDING locks. */
bool onefs_brl_cancel_windows(vfs_handle_struct *handle,
			      struct byte_range_lock *br_lck,
			      struct lock_struct *plock,
			      struct blocking_lock_record *blr)
{
	int error;
	int fd = br_lck->fsp->fh->fd;
	struct onefs_cbrl_blr_state *bs;

	START_PROFILE(syscall_brl_cancel);

	SMB_ASSERT(plock);
	SMB_ASSERT(plock->lock_flav == WINDOWS_LOCK);
	SMB_ASSERT(blr);

	onefs_cbrl_enumerate_blq("onefs_brl_cancel_windows");

	bs = ((struct onefs_cbrl_blr_state *)blr->blr_private);
	SMB_ASSERT(bs);

	if (bs->state == ONEFS_CBRL_DONE || bs->state == ONEFS_CBRL_ERROR) {
		/* No-op. */
		DEBUG(10, ("State=%d, returning true\n", bs->state));
		END_PROFILE(syscall_brl_cancel);
		return true;
	}

	SMB_ASSERT(bs->state == ONEFS_CBRL_NONE ||
	    bs->state == ONEFS_CBRL_ASYNC);

	/* A real cancel. */
	DEBUG(10, ("Calling ifs_cbrl(CANCEL)...\n"));
	error = ifs_cbrl(fd, CBRL_OP_CANCEL, CBRL_LK_UNSPEC, plock->start,
	    plock->size, false, bs->id, plock->context.smbpid,
	    plock->context.tid, plock->fnum);

	END_PROFILE(syscall_brl_cancel);

	if (error) {
		DEBUG(10, ("returning false\n"));
		bs->state = ONEFS_CBRL_ERROR;
		return false;
	}

	bs->state = ONEFS_CBRL_DONE;
	onefs_cbrl_enumerate_blq("onefs_brl_cancel_windows");
	DEBUG(10, ("returning true\n"));
	return true;
}

bool onefs_strict_lock(vfs_handle_struct *handle,
			files_struct *fsp,
			struct lock_struct *plock)
{
	int error;

	START_PROFILE(syscall_strict_lock);

	SMB_ASSERT(plock->lock_type == READ_LOCK ||
	    plock->lock_type == WRITE_LOCK);

        if (!lp_locking(handle->conn->params) ||
	    !lp_strict_locking(handle->conn->params)) {
		END_PROFILE(syscall_strict_lock);
                return True;
        }

	if (plock->lock_flav == POSIX_LOCK) {
		END_PROFILE(syscall_strict_lock);
		return SMB_VFS_NEXT_STRICT_LOCK(handle, fsp, plock);
	}

	if (plock->size == 0) {
		END_PROFILE(syscall_strict_lock);
                return True;
	}

	error = ifs_cbrl(fsp->fh->fd, CBRL_OP_LOCK,
	    plock->lock_type == READ_LOCK ? CBRL_LK_RD : CBRL_LK_WR,
	    plock->start, plock->size, 0, 0, plock->context.smbpid,
	    plock->context.tid, plock->fnum);

	END_PROFILE(syscall_strict_lock);

	return (error == 0);
}

void onefs_strict_unlock(vfs_handle_struct *handle,
			files_struct *fsp,
			struct lock_struct *plock)
{
	START_PROFILE(syscall_strict_unlock);

	SMB_ASSERT(plock->lock_type == READ_LOCK ||
	    plock->lock_type == WRITE_LOCK);

        if (!lp_locking(handle->conn->params) ||
	    !lp_strict_locking(handle->conn->params)) {
		END_PROFILE(syscall_strict_unlock);
		return;
        }

	if (plock->lock_flav == POSIX_LOCK) {
		SMB_VFS_NEXT_STRICT_UNLOCK(handle, fsp, plock);
		END_PROFILE(syscall_strict_unlock);
		return;
	}

	if (plock->size == 0) {
		END_PROFILE(syscall_strict_unlock);
		return;
	}

	if (fsp->fh) {
		ifs_cbrl(fsp->fh->fd, CBRL_OP_UNLOCK,
		    plock->lock_type == READ_LOCK ? CBRL_LK_RD : CBRL_LK_WR,
		    plock->start, plock->size, 0, 0, plock->context.smbpid,
		    plock->context.tid, plock->fnum);
	}

	END_PROFILE(syscall_strict_unlock);
}

/* TODO Optimization: Abstract out brl_get_locks() in the Windows case.
 * We'll malloc some memory or whatever (can't return NULL), but not actually
 * touch the TDB. */

/* XXX brl_locktest: CBRL does not support calling this, but its only for
 * strict locking. Add empty VOP? */

/* XXX brl_lockquery: CBRL does not support calling this for WINDOWS LOCKS, but
 * its only called for POSIX LOCKS. Add empty VOP? */

/* XXX brl_close_fnum: CBRL will do this automatically. I think this is a NO-OP
 * for us, we could add an empty VOP. */

