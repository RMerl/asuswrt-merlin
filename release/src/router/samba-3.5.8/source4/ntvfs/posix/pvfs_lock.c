/* 
   Unix SMB/CIFS implementation.

   POSIX NTVFS backend - locking

   Copyright (C) Andrew Tridgell 2004

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 3 of the License, or
   (at your option) any later version.
   
   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.
   
   You should have received a copy of the GNU General Public License
   along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#include "includes.h"
#include "vfs_posix.h"
#include "system/time.h"
#include "../lib/util/dlinklist.h"
#include "messaging/messaging.h"


/*
  check if we can perform IO on a range that might be locked
*/
NTSTATUS pvfs_check_lock(struct pvfs_state *pvfs,
			 struct pvfs_file *f,
			 uint32_t smbpid,
			 uint64_t offset, uint64_t count,
			 enum brl_type rw)
{
	if (!(pvfs->flags & PVFS_FLAG_STRICT_LOCKING)) {
		return NT_STATUS_OK;
	}

	return brl_locktest(pvfs->brl_context,
			    f->brl_handle,
			    smbpid,
			    offset, count, rw);
}

/* this state structure holds information about a lock we are waiting on */
struct pvfs_pending_lock {
	struct pvfs_pending_lock *next, *prev;
	struct pvfs_state *pvfs;
	union smb_lock *lck;
	struct pvfs_file *f;
	struct ntvfs_request *req;
	int pending_lock;
	struct pvfs_wait *wait_handle;
	struct timeval end_time;
};

/*
  a secondary attempt to setup a lock has failed - back out
  the locks we did get and send an error
*/
static void pvfs_lock_async_failed(struct pvfs_state *pvfs,
				   struct ntvfs_request *req,
				   struct pvfs_file *f,
				   struct smb_lock_entry *locks,
				   int i,
				   NTSTATUS status)
{
	/* undo the locks we just did */
	for (i--;i>=0;i--) {
		brl_unlock(pvfs->brl_context,
			   f->brl_handle,
			   locks[i].pid,
			   locks[i].offset,
			   locks[i].count);
		f->lock_count--;
	}
	req->async_states->status = status;
	req->async_states->send_fn(req);
}


/*
  called when we receive a pending lock notification. It means that
  either our lock timed out or someone else has unlocked a overlapping
  range, so we should try the lock again. Note that on timeout we
  do retry the lock, giving it a last chance.
*/
static void pvfs_pending_lock_continue(void *private_data, enum pvfs_wait_notice reason)
{
	struct pvfs_pending_lock *pending = talloc_get_type(private_data,
					    struct pvfs_pending_lock);
	struct pvfs_state *pvfs = pending->pvfs;
	struct pvfs_file *f = pending->f;
	struct ntvfs_request *req = pending->req;
	union smb_lock *lck = pending->lck;
	struct smb_lock_entry *locks;
	enum brl_type rw;
	NTSTATUS status;
	int i;
	bool timed_out;

	timed_out = (reason != PVFS_WAIT_EVENT);

	locks = lck->lockx.in.locks + lck->lockx.in.ulock_cnt;

	if (lck->lockx.in.mode & LOCKING_ANDX_SHARED_LOCK) {
		rw = READ_LOCK;
	} else {
		rw = WRITE_LOCK;
	}

	DLIST_REMOVE(f->pending_list, pending);

	/* we don't retry on a cancel */
	if (reason == PVFS_WAIT_CANCEL) {
		status = NT_STATUS_FILE_LOCK_CONFLICT;
	} else {
		/* 
		 * here it's important to pass the pending pointer
		 * because with this we'll get the correct error code
		 * FILE_LOCK_CONFLICT in the error case
		 */
		status = brl_lock(pvfs->brl_context,
				  f->brl_handle,
				  locks[pending->pending_lock].pid,
				  locks[pending->pending_lock].offset,
				  locks[pending->pending_lock].count,
				  rw, pending);
	}
	if (NT_STATUS_IS_OK(status)) {
		f->lock_count++;
		timed_out = false;
	}

	/* if we have failed and timed out, or succeeded, then we
	   don't need the pending lock any more */
	if (NT_STATUS_IS_OK(status) || timed_out) {
		NTSTATUS status2;
		status2 = brl_remove_pending(pvfs->brl_context, 
					     f->brl_handle, pending);
		if (!NT_STATUS_IS_OK(status2)) {
			DEBUG(0,("pvfs_lock: failed to remove pending lock - %s\n", nt_errstr(status2)));
		}
		talloc_free(pending->wait_handle);
	}

	if (!NT_STATUS_IS_OK(status)) {
		if (timed_out) {
			/* no more chances */
			pvfs_lock_async_failed(pvfs, req, f, locks, pending->pending_lock, status);
			talloc_free(pending);
		} else {
			/* we can try again */
			DLIST_ADD(f->pending_list, pending);
		}
		return;
	}

	/* if we haven't timed out yet, then we can do more pending locks */
	if (rw == READ_LOCK) {
		rw = PENDING_READ_LOCK;
	} else {
		rw = PENDING_WRITE_LOCK;
	}

	/* we've now got the pending lock. try and get the rest, which might
	   lead to more pending locks */
	for (i=pending->pending_lock+1;i<lck->lockx.in.lock_cnt;i++) {		
		if (pending) {
			pending->pending_lock = i;
		}

		status = brl_lock(pvfs->brl_context,
				  f->brl_handle,
				  locks[i].pid,
				  locks[i].offset,
				  locks[i].count,
				  rw, pending);
		if (!NT_STATUS_IS_OK(status)) {
			if (pending) {
				/* a timed lock failed - setup a wait message to handle
				   the pending lock notification or a timeout */
				pending->wait_handle = pvfs_wait_message(pvfs, req, MSG_BRL_RETRY, 
									 pending->end_time,
									 pvfs_pending_lock_continue,
									 pending);
				if (pending->wait_handle == NULL) {
					pvfs_lock_async_failed(pvfs, req, f, locks, i, NT_STATUS_NO_MEMORY);
					talloc_free(pending);
				} else {
					talloc_steal(pending, pending->wait_handle);
					DLIST_ADD(f->pending_list, pending);
				}
				return;
			}
			pvfs_lock_async_failed(pvfs, req, f, locks, i, status);
			talloc_free(pending);
			return;
		}

		f->lock_count++;
	}

	/* we've managed to get all the locks. Tell the client */
	req->async_states->status = NT_STATUS_OK;
	req->async_states->send_fn(req);
	talloc_free(pending);
}


/*
  called when we close a file that might have locks
*/
void pvfs_lock_close(struct pvfs_state *pvfs, struct pvfs_file *f)
{
	struct pvfs_pending_lock *p, *next;

	if (f->lock_count || f->pending_list) {
		DEBUG(5,("pvfs_lock: removing %.0f locks on close\n", 
			 (double)f->lock_count));
		brl_close(f->pvfs->brl_context, f->brl_handle);
		f->lock_count = 0;
	}

	/* reply to all the pending lock requests, telling them the 
	   lock failed */
	for (p=f->pending_list;p;p=next) {
		next = p->next;
		DLIST_REMOVE(f->pending_list, p);
		p->req->async_states->status = NT_STATUS_RANGE_NOT_LOCKED;
		p->req->async_states->send_fn(p->req);
	}
}


/*
  cancel a set of locks
*/
static NTSTATUS pvfs_lock_cancel(struct pvfs_state *pvfs, struct ntvfs_request *req, union smb_lock *lck,
				 struct pvfs_file *f)
{
	struct pvfs_pending_lock *p;

	for (p=f->pending_list;p;p=p->next) {
		/* check if the lock request matches exactly - you can only cancel with exact matches */
		if (p->lck->lockx.in.ulock_cnt == lck->lockx.in.ulock_cnt &&
		    p->lck->lockx.in.lock_cnt  == lck->lockx.in.lock_cnt &&
		    p->lck->lockx.in.file.ntvfs== lck->lockx.in.file.ntvfs &&
		    p->lck->lockx.in.mode      == (lck->lockx.in.mode & ~LOCKING_ANDX_CANCEL_LOCK)) {
			int i;

			for (i=0;i<lck->lockx.in.ulock_cnt + lck->lockx.in.lock_cnt;i++) {
				if (p->lck->lockx.in.locks[i].pid != lck->lockx.in.locks[i].pid ||
				    p->lck->lockx.in.locks[i].offset != lck->lockx.in.locks[i].offset ||
				    p->lck->lockx.in.locks[i].count != lck->lockx.in.locks[i].count) {
					break;
				}
			}
			if (i < lck->lockx.in.ulock_cnt) continue;

			/* an exact match! we can cancel it, which is equivalent
			   to triggering the timeout early */
			pvfs_pending_lock_continue(p, PVFS_WAIT_TIMEOUT);
			return NT_STATUS_OK;
		}
	}

	return NT_STATUS_DOS(ERRDOS, ERRcancelviolation);
}


/*
  lock or unlock a byte range
*/
NTSTATUS pvfs_lock(struct ntvfs_module_context *ntvfs,
		   struct ntvfs_request *req, union smb_lock *lck)
{
	struct pvfs_state *pvfs = talloc_get_type(ntvfs->private_data,
				  struct pvfs_state);
	struct pvfs_file *f;
	struct smb_lock_entry *locks;
	int i;
	enum brl_type rw;
	struct pvfs_pending_lock *pending = NULL;
	NTSTATUS status;

	if (lck->generic.level != RAW_LOCK_GENERIC) {
		return ntvfs_map_lock(ntvfs, req, lck);
	}

	if (lck->lockx.in.mode & LOCKING_ANDX_OPLOCK_RELEASE) {
		return pvfs_oplock_release(ntvfs, req, lck);
	}

	f = pvfs_find_fd(pvfs, req, lck->lockx.in.file.ntvfs);
	if (!f) {
		return NT_STATUS_INVALID_HANDLE;
	}

	if (f->handle->fd == -1) {
		return NT_STATUS_FILE_IS_A_DIRECTORY;
	}

	status = pvfs_break_level2_oplocks(f);
	NT_STATUS_NOT_OK_RETURN(status);

	if (lck->lockx.in.timeout != 0 && 
	    (req->async_states->state & NTVFS_ASYNC_STATE_MAY_ASYNC)) {
		pending = talloc(f, struct pvfs_pending_lock);
		if (pending == NULL) {
			return NT_STATUS_NO_MEMORY;
		}

		pending->pvfs = pvfs;
		pending->lck = lck;
		pending->f = f;
		pending->req = req;

		pending->end_time = 
			timeval_current_ofs(lck->lockx.in.timeout/1000,
					    1000*(lck->lockx.in.timeout%1000));
	}

	if (lck->lockx.in.mode & LOCKING_ANDX_SHARED_LOCK) {
		rw = pending? PENDING_READ_LOCK : READ_LOCK;
	} else {
		rw = pending? PENDING_WRITE_LOCK : WRITE_LOCK;
	}

	if (lck->lockx.in.mode & LOCKING_ANDX_CANCEL_LOCK) {
		talloc_free(pending);
		return pvfs_lock_cancel(pvfs, req, lck, f);
	}

	if (lck->lockx.in.mode & LOCKING_ANDX_CHANGE_LOCKTYPE) {
		/* this seems to not be supported by any windows server,
		   or used by any clients */
		talloc_free(pending);
		return NT_STATUS_DOS(ERRDOS, ERRnoatomiclocks);
	}

	/* the unlocks happen first */
	locks = lck->lockx.in.locks;

	for (i=0;i<lck->lockx.in.ulock_cnt;i++) {
		status = brl_unlock(pvfs->brl_context,
				    f->brl_handle,
				    locks[i].pid,
				    locks[i].offset,
				    locks[i].count);
		if (!NT_STATUS_IS_OK(status)) {
			talloc_free(pending);
			return status;
		}
		f->lock_count--;
	}

	locks += i;

	for (i=0;i<lck->lockx.in.lock_cnt;i++) {
		if (pending) {
			pending->pending_lock = i;
		}

		status = brl_lock(pvfs->brl_context,
				  f->brl_handle,
				  locks[i].pid,
				  locks[i].offset,
				  locks[i].count,
				  rw, pending);
		if (!NT_STATUS_IS_OK(status)) {
			if (pending) {
				/* a timed lock failed - setup a wait message to handle
				   the pending lock notification or a timeout */
				pending->wait_handle = pvfs_wait_message(pvfs, req, MSG_BRL_RETRY, 
									 pending->end_time,
									 pvfs_pending_lock_continue,
									 pending);
				if (pending->wait_handle == NULL) {
					talloc_free(pending);
					return NT_STATUS_NO_MEMORY;
				}
				talloc_steal(pending, pending->wait_handle);
				DLIST_ADD(f->pending_list, pending);
				return NT_STATUS_OK;
			}

			/* undo the locks we just did */
			for (i--;i>=0;i--) {
				brl_unlock(pvfs->brl_context,
					   f->brl_handle,
					   locks[i].pid,
					   locks[i].offset,
					   locks[i].count);
				f->lock_count--;
			}
			talloc_free(pending);
			return status;
		}
		f->lock_count++;
	}

	talloc_free(pending);
	return NT_STATUS_OK;
}
