/* 
   Unix SMB/CIFS implementation.
   byte range locking code
   Updated to handle range splits/merges.

   Copyright (C) Andrew Tridgell 1992-2000
   Copyright (C) Jeremy Allison 1992-2000
   
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

/* This module implements a tdb based byte range locking service,
   replacing the fcntl() based byte range locking previously
   used. This allows us to provide the same semantics as NT */

#include "includes.h"
#include "system/filesys.h"
#include "locking/proto.h"
#include "smbd/globals.h"
#include "dbwrap.h"
#include "serverid.h"
#include "messages.h"

#undef DBGC_CLASS
#define DBGC_CLASS DBGC_LOCKING

#define ZERO_ZERO 0

/* The open brlock.tdb database. */

static struct db_context *brlock_db;

/****************************************************************************
 Debug info at level 10 for lock struct.
****************************************************************************/

static void print_lock_struct(unsigned int i, struct lock_struct *pls)
{
	DEBUG(10,("[%u]: smblctx = %llu, tid = %u, pid = %s, ",
			i,
			(unsigned long long)pls->context.smblctx,
			(unsigned int)pls->context.tid,
			procid_str(talloc_tos(), &pls->context.pid) ));
	
	DEBUG(10,("start = %.0f, size = %.0f, fnum = %d, %s %s\n",
		(double)pls->start,
		(double)pls->size,
		pls->fnum,
		lock_type_name(pls->lock_type),
		lock_flav_name(pls->lock_flav) ));
}

/****************************************************************************
 See if two locking contexts are equal.
****************************************************************************/

bool brl_same_context(const struct lock_context *ctx1, 
			     const struct lock_context *ctx2)
{
	return (procid_equal(&ctx1->pid, &ctx2->pid) &&
		(ctx1->smblctx == ctx2->smblctx) &&
		(ctx1->tid == ctx2->tid));
}

/****************************************************************************
 See if lck1 and lck2 overlap.
****************************************************************************/

static bool brl_overlap(const struct lock_struct *lck1,
                        const struct lock_struct *lck2)
{
	/* XXX Remove for Win7 compatibility. */
	/* this extra check is not redundent - it copes with locks
	   that go beyond the end of 64 bit file space */
	if (lck1->size != 0 &&
	    lck1->start == lck2->start &&
	    lck1->size == lck2->size) {
		return True;
	}

	if (lck1->start >= (lck2->start+lck2->size) ||
	    lck2->start >= (lck1->start+lck1->size)) {
		return False;
	}
	return True;
}

/****************************************************************************
 See if lock2 can be added when lock1 is in place.
****************************************************************************/

static bool brl_conflict(const struct lock_struct *lck1, 
			 const struct lock_struct *lck2)
{
	/* Ignore PENDING locks. */
	if (IS_PENDING_LOCK(lck1->lock_type) || IS_PENDING_LOCK(lck2->lock_type))
		return False;

	/* Read locks never conflict. */
	if (lck1->lock_type == READ_LOCK && lck2->lock_type == READ_LOCK) {
		return False;
	}

	/* A READ lock can stack on top of a WRITE lock if they have the same
	 * context & fnum. */
	if (lck1->lock_type == WRITE_LOCK && lck2->lock_type == READ_LOCK &&
	    brl_same_context(&lck1->context, &lck2->context) &&
	    lck1->fnum == lck2->fnum) {
		return False;
	}

	return brl_overlap(lck1, lck2);
} 

/****************************************************************************
 See if lock2 can be added when lock1 is in place - when both locks are POSIX
 flavour. POSIX locks ignore fnum - they only care about dev/ino which we
 know already match.
****************************************************************************/

static bool brl_conflict_posix(const struct lock_struct *lck1, 
			 	const struct lock_struct *lck2)
{
#if defined(DEVELOPER)
	SMB_ASSERT(lck1->lock_flav == POSIX_LOCK);
	SMB_ASSERT(lck2->lock_flav == POSIX_LOCK);
#endif

	/* Ignore PENDING locks. */
	if (IS_PENDING_LOCK(lck1->lock_type) || IS_PENDING_LOCK(lck2->lock_type))
		return False;

	/* Read locks never conflict. */
	if (lck1->lock_type == READ_LOCK && lck2->lock_type == READ_LOCK) {
		return False;
	}

	/* Locks on the same context con't conflict. Ignore fnum. */
	if (brl_same_context(&lck1->context, &lck2->context)) {
		return False;
	}

	/* One is read, the other write, or the context is different,
	   do they overlap ? */
	return brl_overlap(lck1, lck2);
} 

#if ZERO_ZERO
static bool brl_conflict1(const struct lock_struct *lck1, 
			 const struct lock_struct *lck2)
{
	if (IS_PENDING_LOCK(lck1->lock_type) || IS_PENDING_LOCK(lck2->lock_type))
		return False;

	if (lck1->lock_type == READ_LOCK && lck2->lock_type == READ_LOCK) {
		return False;
	}

	if (brl_same_context(&lck1->context, &lck2->context) &&
	    lck2->lock_type == READ_LOCK && lck1->fnum == lck2->fnum) {
		return False;
	}

	if (lck2->start == 0 && lck2->size == 0 && lck1->size != 0) {
		return True;
	}

	if (lck1->start >= (lck2->start + lck2->size) ||
	    lck2->start >= (lck1->start + lck1->size)) {
		return False;
	}
	    
	return True;
} 
#endif

/****************************************************************************
 Check to see if this lock conflicts, but ignore our own locks on the
 same fnum only. This is the read/write lock check code path.
 This is never used in the POSIX lock case.
****************************************************************************/

static bool brl_conflict_other(const struct lock_struct *lck1, const struct lock_struct *lck2)
{
	if (IS_PENDING_LOCK(lck1->lock_type) || IS_PENDING_LOCK(lck2->lock_type))
		return False;

	if (lck1->lock_type == READ_LOCK && lck2->lock_type == READ_LOCK) 
		return False;

	/* POSIX flavour locks never conflict here - this is only called
	   in the read/write path. */

	if (lck1->lock_flav == POSIX_LOCK && lck2->lock_flav == POSIX_LOCK)
		return False;

	/*
	 * Incoming WRITE locks conflict with existing READ locks even
	 * if the context is the same. JRA. See LOCKTEST7 in smbtorture.
	 */

	if (!(lck2->lock_type == WRITE_LOCK && lck1->lock_type == READ_LOCK)) {
		if (brl_same_context(&lck1->context, &lck2->context) &&
					lck1->fnum == lck2->fnum)
			return False;
	}

	return brl_overlap(lck1, lck2);
} 

/****************************************************************************
 Check if an unlock overlaps a pending lock.
****************************************************************************/

static bool brl_pending_overlap(const struct lock_struct *lock, const struct lock_struct *pend_lock)
{
	if ((lock->start <= pend_lock->start) && (lock->start + lock->size > pend_lock->start))
		return True;
	if ((lock->start >= pend_lock->start) && (lock->start <= pend_lock->start + pend_lock->size))
		return True;
	return False;
}

/****************************************************************************
 Amazingly enough, w2k3 "remembers" whether the last lock failure on a fnum
 is the same as this one and changes its error code. I wonder if any
 app depends on this ?
****************************************************************************/

NTSTATUS brl_lock_failed(files_struct *fsp, const struct lock_struct *lock, bool blocking_lock)
{
	if (lock->start >= 0xEF000000 && (lock->start >> 63) == 0) {
		/* amazing the little things you learn with a test
		   suite. Locks beyond this offset (as a 64 bit
		   number!) always generate the conflict error code,
		   unless the top bit is set */
		if (!blocking_lock) {
			fsp->last_lock_failure = *lock;
		}
		return NT_STATUS_FILE_LOCK_CONFLICT;
	}

	if (procid_equal(&lock->context.pid, &fsp->last_lock_failure.context.pid) &&
			lock->context.tid == fsp->last_lock_failure.context.tid &&
			lock->fnum == fsp->last_lock_failure.fnum &&
			lock->start == fsp->last_lock_failure.start) {
		return NT_STATUS_FILE_LOCK_CONFLICT;
	}

	if (!blocking_lock) {
		fsp->last_lock_failure = *lock;
	}
	return NT_STATUS_LOCK_NOT_GRANTED;
}

/****************************************************************************
 Open up the brlock.tdb database.
****************************************************************************/

void brl_init(bool read_only)
{
	int tdb_flags;

	if (brlock_db) {
		return;
	}

	tdb_flags = TDB_DEFAULT|TDB_VOLATILE|TDB_CLEAR_IF_FIRST|TDB_INCOMPATIBLE_HASH;

	if (!lp_clustering()) {
		/*
		 * We can't use the SEQNUM trick to cache brlock
		 * entries in the clustering case because ctdb seqnum
		 * propagation has a delay.
		 */
		tdb_flags |= TDB_SEQNUM;
	}

	brlock_db = db_open(NULL, lock_path("brlock.tdb"),
			    lp_open_files_db_hash_size(), tdb_flags,
			    read_only?O_RDONLY:(O_RDWR|O_CREAT), 0644 );
	if (!brlock_db) {
		DEBUG(0,("Failed to open byte range locking database %s\n",
			lock_path("brlock.tdb")));
		return;
	}
}

/****************************************************************************
 Close down the brlock.tdb database.
****************************************************************************/

void brl_shutdown(void)
{
	TALLOC_FREE(brlock_db);
}

#if ZERO_ZERO
/****************************************************************************
 Compare two locks for sorting.
****************************************************************************/

static int lock_compare(const struct lock_struct *lck1, 
			 const struct lock_struct *lck2)
{
	if (lck1->start != lck2->start) {
		return (lck1->start - lck2->start);
	}
	if (lck2->size != lck1->size) {
		return ((int)lck1->size - (int)lck2->size);
	}
	return 0;
}
#endif

/****************************************************************************
 Lock a range of bytes - Windows lock semantics.
****************************************************************************/

NTSTATUS brl_lock_windows_default(struct byte_range_lock *br_lck,
    struct lock_struct *plock, bool blocking_lock)
{
	unsigned int i;
	files_struct *fsp = br_lck->fsp;
	struct lock_struct *locks = br_lck->lock_data;
	NTSTATUS status;

	SMB_ASSERT(plock->lock_type != UNLOCK_LOCK);

	if ((plock->start + plock->size - 1 < plock->start) &&
			plock->size != 0) {
		return NT_STATUS_INVALID_LOCK_RANGE;
	}

	for (i=0; i < br_lck->num_locks; i++) {
		/* Do any Windows or POSIX locks conflict ? */
		if (brl_conflict(&locks[i], plock)) {
			/* Remember who blocked us. */
			plock->context.smblctx = locks[i].context.smblctx;
			return brl_lock_failed(fsp,plock,blocking_lock);
		}
#if ZERO_ZERO
		if (plock->start == 0 && plock->size == 0 && 
				locks[i].size == 0) {
			break;
		}
#endif
	}

	if (!IS_PENDING_LOCK(plock->lock_type)) {
		contend_level2_oplocks_begin(fsp, LEVEL2_CONTEND_WINDOWS_BRL);
	}

	/* We can get the Windows lock, now see if it needs to
	   be mapped into a lower level POSIX one, and if so can
	   we get it ? */

	if (!IS_PENDING_LOCK(plock->lock_type) && lp_posix_locking(fsp->conn->params)) {
		int errno_ret;
		if (!set_posix_lock_windows_flavour(fsp,
				plock->start,
				plock->size,
				plock->lock_type,
				&plock->context,
				locks,
				br_lck->num_locks,
				&errno_ret)) {

			/* We don't know who blocked us. */
			plock->context.smblctx = 0xFFFFFFFFFFFFFFFFLL;

			if (errno_ret == EACCES || errno_ret == EAGAIN) {
				status = NT_STATUS_FILE_LOCK_CONFLICT;
				goto fail;
			} else {
				status = map_nt_error_from_unix(errno);
				goto fail;
			}
		}
	}

	/* no conflicts - add it to the list of locks */
	locks = (struct lock_struct *)SMB_REALLOC(locks, (br_lck->num_locks + 1) * sizeof(*locks));
	if (!locks) {
		status = NT_STATUS_NO_MEMORY;
		goto fail;
	}

	memcpy(&locks[br_lck->num_locks], plock, sizeof(struct lock_struct));
	br_lck->num_locks += 1;
	br_lck->lock_data = locks;
	br_lck->modified = True;

	return NT_STATUS_OK;
 fail:
	if (!IS_PENDING_LOCK(plock->lock_type)) {
		contend_level2_oplocks_end(fsp, LEVEL2_CONTEND_WINDOWS_BRL);
	}
	return status;
}

/****************************************************************************
 Cope with POSIX range splits and merges.
****************************************************************************/

static unsigned int brlock_posix_split_merge(struct lock_struct *lck_arr,	/* Output array. */
						struct lock_struct *ex,		/* existing lock. */
						struct lock_struct *plock)	/* proposed lock. */
{
	bool lock_types_differ = (ex->lock_type != plock->lock_type);

	/* We can't merge non-conflicting locks on different context - ignore fnum. */

	if (!brl_same_context(&ex->context, &plock->context)) {
		/* Just copy. */
		memcpy(&lck_arr[0], ex, sizeof(struct lock_struct));
		return 1;
	}

	/* We now know we have the same context. */

	/* Did we overlap ? */

/*********************************************
                                        +---------+
                                        | ex      |
                                        +---------+
                         +-------+
                         | plock |
                         +-------+
OR....
        +---------+
        |  ex     |
        +---------+
**********************************************/

	if ( (ex->start > (plock->start + plock->size)) ||
		(plock->start > (ex->start + ex->size))) {

		/* No overlap with this lock - copy existing. */

		memcpy(&lck_arr[0], ex, sizeof(struct lock_struct));
		return 1;
	}

/*********************************************
        +---------------------------+
        |          ex               |
        +---------------------------+
        +---------------------------+
        |       plock               | -> replace with plock.
        +---------------------------+
OR
             +---------------+
             |       ex      |
             +---------------+
        +---------------------------+
        |       plock               | -> replace with plock.
        +---------------------------+

**********************************************/

	if ( (ex->start >= plock->start) &&
		(ex->start + ex->size <= plock->start + plock->size) ) {

		/* Replace - discard existing lock. */

		return 0;
	}

/*********************************************
Adjacent after.
                        +-------+
                        |  ex   |
                        +-------+
        +---------------+
        |   plock       |
        +---------------+

BECOMES....
        +---------------+-------+
        |   plock       | ex    | - different lock types.
        +---------------+-------+
OR.... (merge)
        +-----------------------+
        |   plock               | - same lock type.
        +-----------------------+
**********************************************/

	if (plock->start + plock->size == ex->start) {

		/* If the lock types are the same, we merge, if different, we
		   add the remainder of the old lock. */

		if (lock_types_differ) {
			/* Add existing. */
			memcpy(&lck_arr[0], ex, sizeof(struct lock_struct));
			return 1;
		} else {
			/* Merge - adjust incoming lock as we may have more
			 * merging to come. */
			plock->size += ex->size;
			return 0;
		}
	}

/*********************************************
Adjacent before.
        +-------+
        |  ex   |
        +-------+
                +---------------+
                |   plock       |
                +---------------+
BECOMES....
        +-------+---------------+
        | ex    |   plock       | - different lock types
        +-------+---------------+

OR.... (merge)
        +-----------------------+
        |      plock            | - same lock type.
        +-----------------------+

**********************************************/

	if (ex->start + ex->size == plock->start) {

		/* If the lock types are the same, we merge, if different, we
		   add the existing lock. */

		if (lock_types_differ) {
			memcpy(&lck_arr[0], ex, sizeof(struct lock_struct));
			return 1;
		} else {
			/* Merge - adjust incoming lock as we may have more
			 * merging to come. */
			plock->start = ex->start;
			plock->size += ex->size;
			return 0;
		}
	}

/*********************************************
Overlap after.
        +-----------------------+
        |          ex           |
        +-----------------------+
        +---------------+
        |   plock       |
        +---------------+
OR
               +----------------+
               |       ex       |
               +----------------+
        +---------------+
        |   plock       |
        +---------------+

BECOMES....
        +---------------+-------+
        |   plock       | ex    | - different lock types.
        +---------------+-------+
OR.... (merge)
        +-----------------------+
        |   plock               | - same lock type.
        +-----------------------+
**********************************************/

	if ( (ex->start >= plock->start) &&
		(ex->start <= plock->start + plock->size) &&
		(ex->start + ex->size > plock->start + plock->size) ) {

		/* If the lock types are the same, we merge, if different, we
		   add the remainder of the old lock. */

		if (lock_types_differ) {
			/* Add remaining existing. */
			memcpy(&lck_arr[0], ex, sizeof(struct lock_struct));
			/* Adjust existing start and size. */
			lck_arr[0].start = plock->start + plock->size;
			lck_arr[0].size = (ex->start + ex->size) - (plock->start + plock->size);
			return 1;
		} else {
			/* Merge - adjust incoming lock as we may have more
			 * merging to come. */
			plock->size += (ex->start + ex->size) - (plock->start + plock->size);
			return 0;
		}
	}

/*********************************************
Overlap before.
        +-----------------------+
        |  ex                   |
        +-----------------------+
                +---------------+
                |   plock       |
                +---------------+
OR
        +-------------+
        |  ex         |
        +-------------+
                +---------------+
                |   plock       |
                +---------------+

BECOMES....
        +-------+---------------+
        | ex    |   plock       | - different lock types
        +-------+---------------+

OR.... (merge)
        +-----------------------+
        |      plock            | - same lock type.
        +-----------------------+

**********************************************/

	if ( (ex->start < plock->start) &&
			(ex->start + ex->size >= plock->start) &&
			(ex->start + ex->size <= plock->start + plock->size) ) {

		/* If the lock types are the same, we merge, if different, we
		   add the truncated old lock. */

		if (lock_types_differ) {
			memcpy(&lck_arr[0], ex, sizeof(struct lock_struct));
			/* Adjust existing size. */
			lck_arr[0].size = plock->start - ex->start;
			return 1;
		} else {
			/* Merge - adjust incoming lock as we may have more
			 * merging to come. MUST ADJUST plock SIZE FIRST ! */
			plock->size += (plock->start - ex->start);
			plock->start = ex->start;
			return 0;
		}
	}

/*********************************************
Complete overlap.
        +---------------------------+
        |        ex                 |
        +---------------------------+
                +---------+
                |  plock  |
                +---------+
BECOMES.....
        +-------+---------+---------+
        | ex    |  plock  | ex      | - different lock types.
        +-------+---------+---------+
OR
        +---------------------------+
        |        plock              | - same lock type.
        +---------------------------+
**********************************************/

	if ( (ex->start < plock->start) && (ex->start + ex->size > plock->start + plock->size) ) {

		if (lock_types_differ) {

			/* We have to split ex into two locks here. */

			memcpy(&lck_arr[0], ex, sizeof(struct lock_struct));
			memcpy(&lck_arr[1], ex, sizeof(struct lock_struct));

			/* Adjust first existing size. */
			lck_arr[0].size = plock->start - ex->start;

			/* Adjust second existing start and size. */
			lck_arr[1].start = plock->start + plock->size;
			lck_arr[1].size = (ex->start + ex->size) - (plock->start + plock->size);
			return 2;
		} else {
			/* Just eat the existing locks, merge them into plock. */
			plock->start = ex->start;
			plock->size = ex->size;
			return 0;
		}
	}

	/* Never get here. */
	smb_panic("brlock_posix_split_merge");
	/* Notreached. */

	/* Keep some compilers happy. */
	return 0;
}

/****************************************************************************
 Lock a range of bytes - POSIX lock semantics.
 We must cope with range splits and merges.
****************************************************************************/

static NTSTATUS brl_lock_posix(struct messaging_context *msg_ctx,
			       struct byte_range_lock *br_lck,
			       struct lock_struct *plock)
{
	unsigned int i, count, posix_count;
	struct lock_struct *locks = br_lck->lock_data;
	struct lock_struct *tp;
	bool signal_pending_read = False;
	bool break_oplocks = false;
	NTSTATUS status;

	/* No zero-zero locks for POSIX. */
	if (plock->start == 0 && plock->size == 0) {
		return NT_STATUS_INVALID_PARAMETER;
	}

	/* Don't allow 64-bit lock wrap. */
	if (plock->start + plock->size - 1 < plock->start) {
		return NT_STATUS_INVALID_PARAMETER;
	}

	/* The worst case scenario here is we have to split an
	   existing POSIX lock range into two, and add our lock,
	   so we need at most 2 more entries. */

	tp = SMB_MALLOC_ARRAY(struct lock_struct, (br_lck->num_locks + 2));
	if (!tp) {
		return NT_STATUS_NO_MEMORY;
	}

	count = posix_count = 0;

	for (i=0; i < br_lck->num_locks; i++) {
		struct lock_struct *curr_lock = &locks[i];

		/* If we have a pending read lock, a lock downgrade should
		   trigger a lock re-evaluation. */
		if (curr_lock->lock_type == PENDING_READ_LOCK &&
				brl_pending_overlap(plock, curr_lock)) {
			signal_pending_read = True;
		}

		if (curr_lock->lock_flav == WINDOWS_LOCK) {
			/* Do any Windows flavour locks conflict ? */
			if (brl_conflict(curr_lock, plock)) {
				/* No games with error messages. */
				SAFE_FREE(tp);
				/* Remember who blocked us. */
				plock->context.smblctx = curr_lock->context.smblctx;
				return NT_STATUS_FILE_LOCK_CONFLICT;
			}
			/* Just copy the Windows lock into the new array. */
			memcpy(&tp[count], curr_lock, sizeof(struct lock_struct));
			count++;
		} else {
			unsigned int tmp_count = 0;

			/* POSIX conflict semantics are different. */
			if (brl_conflict_posix(curr_lock, plock)) {
				/* Can't block ourselves with POSIX locks. */
				/* No games with error messages. */
				SAFE_FREE(tp);
				/* Remember who blocked us. */
				plock->context.smblctx = curr_lock->context.smblctx;
				return NT_STATUS_FILE_LOCK_CONFLICT;
			}

			/* Work out overlaps. */
			tmp_count += brlock_posix_split_merge(&tp[count], curr_lock, plock);
			posix_count += tmp_count;
			count += tmp_count;
		}
	}

	/*
	 * Break oplocks while we hold a brl. Since lock() and unlock() calls
	 * are not symetric with POSIX semantics, we cannot guarantee our
	 * contend_level2_oplocks_begin/end calls will be acquired and
	 * released one-for-one as with Windows semantics. Therefore we only
	 * call contend_level2_oplocks_begin if this is the first POSIX brl on
	 * the file.
	 */
	break_oplocks = (!IS_PENDING_LOCK(plock->lock_type) &&
			 posix_count == 0);
	if (break_oplocks) {
		contend_level2_oplocks_begin(br_lck->fsp,
					     LEVEL2_CONTEND_POSIX_BRL);
	}

	/* Try and add the lock in order, sorted by lock start. */
	for (i=0; i < count; i++) {
		struct lock_struct *curr_lock = &tp[i];

		if (curr_lock->start <= plock->start) {
			continue;
		}
	}

	if (i < count) {
		memmove(&tp[i+1], &tp[i],
			(count - i)*sizeof(struct lock_struct));
	}
	memcpy(&tp[i], plock, sizeof(struct lock_struct));
	count++;

	/* We can get the POSIX lock, now see if it needs to
	   be mapped into a lower level POSIX one, and if so can
	   we get it ? */

	if (!IS_PENDING_LOCK(plock->lock_type) && lp_posix_locking(br_lck->fsp->conn->params)) {
		int errno_ret;

		/* The lower layer just needs to attempt to
		   get the system POSIX lock. We've weeded out
		   any conflicts above. */

		if (!set_posix_lock_posix_flavour(br_lck->fsp,
				plock->start,
				plock->size,
				plock->lock_type,
				&errno_ret)) {

			/* We don't know who blocked us. */
			plock->context.smblctx = 0xFFFFFFFFFFFFFFFFLL;

			if (errno_ret == EACCES || errno_ret == EAGAIN) {
				SAFE_FREE(tp);
				status = NT_STATUS_FILE_LOCK_CONFLICT;
				goto fail;
			} else {
				SAFE_FREE(tp);
				status = map_nt_error_from_unix(errno);
				goto fail;
			}
		}
	}

	/* If we didn't use all the allocated size,
	 * Realloc so we don't leak entries per lock call. */
	if (count < br_lck->num_locks + 2) {
		tp = (struct lock_struct *)SMB_REALLOC(tp, count * sizeof(*locks));
		if (!tp) {
			status = NT_STATUS_NO_MEMORY;
			goto fail;
		}
	}

	br_lck->num_locks = count;
	SAFE_FREE(br_lck->lock_data);
	br_lck->lock_data = tp;
	locks = tp;
	br_lck->modified = True;

	/* A successful downgrade from write to read lock can trigger a lock
	   re-evalutation where waiting readers can now proceed. */

	if (signal_pending_read) {
		/* Send unlock messages to any pending read waiters that overlap. */
		for (i=0; i < br_lck->num_locks; i++) {
			struct lock_struct *pend_lock = &locks[i];

			/* Ignore non-pending locks. */
			if (!IS_PENDING_LOCK(pend_lock->lock_type)) {
				continue;
			}

			if (pend_lock->lock_type == PENDING_READ_LOCK &&
					brl_pending_overlap(plock, pend_lock)) {
				DEBUG(10,("brl_lock_posix: sending unlock message to pid %s\n",
					procid_str_static(&pend_lock->context.pid )));

				messaging_send(msg_ctx, pend_lock->context.pid,
					       MSG_SMB_UNLOCK, &data_blob_null);
			}
		}
	}

	return NT_STATUS_OK;
 fail:
	if (break_oplocks) {
		contend_level2_oplocks_end(br_lck->fsp,
					   LEVEL2_CONTEND_POSIX_BRL);
	}
	return status;
}

NTSTATUS smb_vfs_call_brl_lock_windows(struct vfs_handle_struct *handle,
				       struct byte_range_lock *br_lck,
				       struct lock_struct *plock,
				       bool blocking_lock,
				       struct blocking_lock_record *blr)
{
	VFS_FIND(brl_lock_windows);
	return handle->fns->brl_lock_windows(handle, br_lck, plock,
					     blocking_lock, blr);
}

/****************************************************************************
 Lock a range of bytes.
****************************************************************************/

NTSTATUS brl_lock(struct messaging_context *msg_ctx,
		struct byte_range_lock *br_lck,
		uint64_t smblctx,
		struct server_id pid,
		br_off start,
		br_off size, 
		enum brl_type lock_type,
		enum brl_flavour lock_flav,
		bool blocking_lock,
		uint64_t *psmblctx,
		struct blocking_lock_record *blr)
{
	NTSTATUS ret;
	struct lock_struct lock;

#if !ZERO_ZERO
	if (start == 0 && size == 0) {
		DEBUG(0,("client sent 0/0 lock - please report this\n"));
	}
#endif

#ifdef DEVELOPER
	/* Quieten valgrind on test. */
	memset(&lock, '\0', sizeof(lock));
#endif

	lock.context.smblctx = smblctx;
	lock.context.pid = pid;
	lock.context.tid = br_lck->fsp->conn->cnum;
	lock.start = start;
	lock.size = size;
	lock.fnum = br_lck->fsp->fnum;
	lock.lock_type = lock_type;
	lock.lock_flav = lock_flav;

	if (lock_flav == WINDOWS_LOCK) {
		ret = SMB_VFS_BRL_LOCK_WINDOWS(br_lck->fsp->conn, br_lck,
		    &lock, blocking_lock, blr);
	} else {
		ret = brl_lock_posix(msg_ctx, br_lck, &lock);
	}

#if ZERO_ZERO
	/* sort the lock list */
	TYPESAFE_QSORT(br_lck->lock_data, (size_t)br_lck->num_locks, lock_compare);
#endif

	/* If we're returning an error, return who blocked us. */
	if (!NT_STATUS_IS_OK(ret) && psmblctx) {
		*psmblctx = lock.context.smblctx;
	}
	return ret;
}

/****************************************************************************
 Unlock a range of bytes - Windows semantics.
****************************************************************************/

bool brl_unlock_windows_default(struct messaging_context *msg_ctx,
			       struct byte_range_lock *br_lck,
			       const struct lock_struct *plock)
{
	unsigned int i, j;
	struct lock_struct *locks = br_lck->lock_data;
	enum brl_type deleted_lock_type = READ_LOCK; /* shut the compiler up.... */

	SMB_ASSERT(plock->lock_type == UNLOCK_LOCK);

#if ZERO_ZERO
	/* Delete write locks by preference... The lock list
	   is sorted in the zero zero case. */

	for (i = 0; i < br_lck->num_locks; i++) {
		struct lock_struct *lock = &locks[i];

		if (lock->lock_type == WRITE_LOCK &&
		    brl_same_context(&lock->context, &plock->context) &&
		    lock->fnum == plock->fnum &&
		    lock->lock_flav == WINDOWS_LOCK &&
		    lock->start == plock->start &&
		    lock->size == plock->size) {

			/* found it - delete it */
			deleted_lock_type = lock->lock_type;
			break;
		}
	}

	if (i != br_lck->num_locks) {
		/* We found it - don't search again. */
		goto unlock_continue;
	}
#endif

	for (i = 0; i < br_lck->num_locks; i++) {
		struct lock_struct *lock = &locks[i];

		if (IS_PENDING_LOCK(lock->lock_type)) {
			continue;
		}

		/* Only remove our own locks that match in start, size, and flavour. */
		if (brl_same_context(&lock->context, &plock->context) &&
					lock->fnum == plock->fnum &&
					lock->lock_flav == WINDOWS_LOCK &&
					lock->start == plock->start &&
					lock->size == plock->size ) {
			deleted_lock_type = lock->lock_type;
			break;
		}
	}

	if (i == br_lck->num_locks) {
		/* we didn't find it */
		return False;
	}

#if ZERO_ZERO
  unlock_continue:
#endif

	/* Actually delete the lock. */
	if (i < br_lck->num_locks - 1) {
		memmove(&locks[i], &locks[i+1], 
			sizeof(*locks)*((br_lck->num_locks-1) - i));
	}

	br_lck->num_locks -= 1;
	br_lck->modified = True;

	/* Unlock the underlying POSIX regions. */
	if(lp_posix_locking(br_lck->fsp->conn->params)) {
		release_posix_lock_windows_flavour(br_lck->fsp,
				plock->start,
				plock->size,
				deleted_lock_type,
				&plock->context,
				locks,
				br_lck->num_locks);
	}

	/* Send unlock messages to any pending waiters that overlap. */
	for (j=0; j < br_lck->num_locks; j++) {
		struct lock_struct *pend_lock = &locks[j];

		/* Ignore non-pending locks. */
		if (!IS_PENDING_LOCK(pend_lock->lock_type)) {
			continue;
		}

		/* We could send specific lock info here... */
		if (brl_pending_overlap(plock, pend_lock)) {
			DEBUG(10,("brl_unlock: sending unlock message to pid %s\n",
				procid_str_static(&pend_lock->context.pid )));

			messaging_send(msg_ctx, pend_lock->context.pid,
				       MSG_SMB_UNLOCK, &data_blob_null);
		}
	}

	contend_level2_oplocks_end(br_lck->fsp, LEVEL2_CONTEND_WINDOWS_BRL);
	return True;
}

/****************************************************************************
 Unlock a range of bytes - POSIX semantics.
****************************************************************************/

static bool brl_unlock_posix(struct messaging_context *msg_ctx,
			     struct byte_range_lock *br_lck,
			     struct lock_struct *plock)
{
	unsigned int i, j, count;
	struct lock_struct *tp;
	struct lock_struct *locks = br_lck->lock_data;
	bool overlap_found = False;

	/* No zero-zero locks for POSIX. */
	if (plock->start == 0 && plock->size == 0) {
		return False;
	}

	/* Don't allow 64-bit lock wrap. */
	if (plock->start + plock->size < plock->start ||
			plock->start + plock->size < plock->size) {
		DEBUG(10,("brl_unlock_posix: lock wrap\n"));
		return False;
	}

	/* The worst case scenario here is we have to split an
	   existing POSIX lock range into two, so we need at most
	   1 more entry. */

	tp = SMB_MALLOC_ARRAY(struct lock_struct, (br_lck->num_locks + 1));
	if (!tp) {
		DEBUG(10,("brl_unlock_posix: malloc fail\n"));
		return False;
	}

	count = 0;
	for (i = 0; i < br_lck->num_locks; i++) {
		struct lock_struct *lock = &locks[i];
		unsigned int tmp_count;

		/* Only remove our own locks - ignore fnum. */
		if (IS_PENDING_LOCK(lock->lock_type) ||
				!brl_same_context(&lock->context, &plock->context)) {
			memcpy(&tp[count], lock, sizeof(struct lock_struct));
			count++;
			continue;
		}

		if (lock->lock_flav == WINDOWS_LOCK) {
			/* Do any Windows flavour locks conflict ? */
			if (brl_conflict(lock, plock)) {
				SAFE_FREE(tp);
				return false;
			}
			/* Just copy the Windows lock into the new array. */
			memcpy(&tp[count], lock, sizeof(struct lock_struct));
			count++;
			continue;
		}

		/* Work out overlaps. */
		tmp_count = brlock_posix_split_merge(&tp[count], lock, plock);

		if (tmp_count == 0) {
			/* plock overlapped the existing lock completely,
			   or replaced it. Don't copy the existing lock. */
			overlap_found = true;
		} else if (tmp_count == 1) {
			/* Either no overlap, (simple copy of existing lock) or
			 * an overlap of an existing lock. */
			/* If the lock changed size, we had an overlap. */
			if (tp[count].size != lock->size) {
				overlap_found = true;
			}
			count += tmp_count;
		} else if (tmp_count == 2) {
			/* We split a lock range in two. */
			overlap_found = true;
			count += tmp_count;

			/* Optimisation... */
			/* We know we're finished here as we can't overlap any
			   more POSIX locks. Copy the rest of the lock array. */

			if (i < br_lck->num_locks - 1) {
				memcpy(&tp[count], &locks[i+1],
					sizeof(*locks)*((br_lck->num_locks-1) - i));
				count += ((br_lck->num_locks-1) - i);
			}
			break;
		}

	}

	if (!overlap_found) {
		/* Just ignore - no change. */
		SAFE_FREE(tp);
		DEBUG(10,("brl_unlock_posix: No overlap - unlocked.\n"));
		return True;
	}

	/* Unlock any POSIX regions. */
	if(lp_posix_locking(br_lck->fsp->conn->params)) {
		release_posix_lock_posix_flavour(br_lck->fsp,
						plock->start,
						plock->size,
						&plock->context,
						tp,
						count);
	}

	/* Realloc so we don't leak entries per unlock call. */
	if (count) {
		tp = (struct lock_struct *)SMB_REALLOC(tp, count * sizeof(*locks));
		if (!tp) {
			DEBUG(10,("brl_unlock_posix: realloc fail\n"));
			return False;
		}
	} else {
		/* We deleted the last lock. */
		SAFE_FREE(tp);
		tp = NULL;
	}

	contend_level2_oplocks_end(br_lck->fsp,
				   LEVEL2_CONTEND_POSIX_BRL);

	br_lck->num_locks = count;
	SAFE_FREE(br_lck->lock_data);
	locks = tp;
	br_lck->lock_data = tp;
	br_lck->modified = True;

	/* Send unlock messages to any pending waiters that overlap. */

	for (j=0; j < br_lck->num_locks; j++) {
		struct lock_struct *pend_lock = &locks[j];

		/* Ignore non-pending locks. */
		if (!IS_PENDING_LOCK(pend_lock->lock_type)) {
			continue;
		}

		/* We could send specific lock info here... */
		if (brl_pending_overlap(plock, pend_lock)) {
			DEBUG(10,("brl_unlock: sending unlock message to pid %s\n",
				procid_str_static(&pend_lock->context.pid )));

			messaging_send(msg_ctx, pend_lock->context.pid,
				       MSG_SMB_UNLOCK, &data_blob_null);
		}
	}

	return True;
}

bool smb_vfs_call_brl_unlock_windows(struct vfs_handle_struct *handle,
				     struct messaging_context *msg_ctx,
				     struct byte_range_lock *br_lck,
				     const struct lock_struct *plock)
{
	VFS_FIND(brl_unlock_windows);
	return handle->fns->brl_unlock_windows(handle, msg_ctx, br_lck, plock);
}

/****************************************************************************
 Unlock a range of bytes.
****************************************************************************/

bool brl_unlock(struct messaging_context *msg_ctx,
		struct byte_range_lock *br_lck,
		uint64_t smblctx,
		struct server_id pid,
		br_off start,
		br_off size,
		enum brl_flavour lock_flav)
{
	struct lock_struct lock;

	lock.context.smblctx = smblctx;
	lock.context.pid = pid;
	lock.context.tid = br_lck->fsp->conn->cnum;
	lock.start = start;
	lock.size = size;
	lock.fnum = br_lck->fsp->fnum;
	lock.lock_type = UNLOCK_LOCK;
	lock.lock_flav = lock_flav;

	if (lock_flav == WINDOWS_LOCK) {
		return SMB_VFS_BRL_UNLOCK_WINDOWS(br_lck->fsp->conn, msg_ctx,
		    br_lck, &lock);
	} else {
		return brl_unlock_posix(msg_ctx, br_lck, &lock);
	}
}

/****************************************************************************
 Test if we could add a lock if we wanted to.
 Returns True if the region required is currently unlocked, False if locked.
****************************************************************************/

bool brl_locktest(struct byte_range_lock *br_lck,
		uint64_t smblctx,
		struct server_id pid,
		br_off start,
		br_off size, 
		enum brl_type lock_type,
		enum brl_flavour lock_flav)
{
	bool ret = True;
	unsigned int i;
	struct lock_struct lock;
	const struct lock_struct *locks = br_lck->lock_data;
	files_struct *fsp = br_lck->fsp;

	lock.context.smblctx = smblctx;
	lock.context.pid = pid;
	lock.context.tid = br_lck->fsp->conn->cnum;
	lock.start = start;
	lock.size = size;
	lock.fnum = fsp->fnum;
	lock.lock_type = lock_type;
	lock.lock_flav = lock_flav;

	/* Make sure existing locks don't conflict */
	for (i=0; i < br_lck->num_locks; i++) {
		/*
		 * Our own locks don't conflict.
		 */
		if (brl_conflict_other(&locks[i], &lock)) {
			return False;
		}
	}

	/*
	 * There is no lock held by an SMB daemon, check to
	 * see if there is a POSIX lock from a UNIX or NFS process.
	 * This only conflicts with Windows locks, not POSIX locks.
	 */

	if(lp_posix_locking(fsp->conn->params) && (lock_flav == WINDOWS_LOCK)) {
		ret = is_posix_locked(fsp, &start, &size, &lock_type, WINDOWS_LOCK);

		DEBUG(10,("brl_locktest: posix start=%.0f len=%.0f %s for fnum %d file %s\n",
			(double)start, (double)size, ret ? "locked" : "unlocked",
			fsp->fnum, fsp_str_dbg(fsp)));

		/* We need to return the inverse of is_posix_locked. */
		ret = !ret;
        }

	/* no conflicts - we could have added it */
	return ret;
}

/****************************************************************************
 Query for existing locks.
****************************************************************************/

NTSTATUS brl_lockquery(struct byte_range_lock *br_lck,
		uint64_t *psmblctx,
		struct server_id pid,
		br_off *pstart,
		br_off *psize, 
		enum brl_type *plock_type,
		enum brl_flavour lock_flav)
{
	unsigned int i;
	struct lock_struct lock;
	const struct lock_struct *locks = br_lck->lock_data;
	files_struct *fsp = br_lck->fsp;

	lock.context.smblctx = *psmblctx;
	lock.context.pid = pid;
	lock.context.tid = br_lck->fsp->conn->cnum;
	lock.start = *pstart;
	lock.size = *psize;
	lock.fnum = fsp->fnum;
	lock.lock_type = *plock_type;
	lock.lock_flav = lock_flav;

	/* Make sure existing locks don't conflict */
	for (i=0; i < br_lck->num_locks; i++) {
		const struct lock_struct *exlock = &locks[i];
		bool conflict = False;

		if (exlock->lock_flav == WINDOWS_LOCK) {
			conflict = brl_conflict(exlock, &lock);
		} else {	
			conflict = brl_conflict_posix(exlock, &lock);
		}

		if (conflict) {
			*psmblctx = exlock->context.smblctx;
        		*pstart = exlock->start;
		        *psize = exlock->size;
        		*plock_type = exlock->lock_type;
			return NT_STATUS_LOCK_NOT_GRANTED;
		}
	}

	/*
	 * There is no lock held by an SMB daemon, check to
	 * see if there is a POSIX lock from a UNIX or NFS process.
	 */

	if(lp_posix_locking(fsp->conn->params)) {
		bool ret = is_posix_locked(fsp, pstart, psize, plock_type, POSIX_LOCK);

		DEBUG(10,("brl_lockquery: posix start=%.0f len=%.0f %s for fnum %d file %s\n",
			(double)*pstart, (double)*psize, ret ? "locked" : "unlocked",
			fsp->fnum, fsp_str_dbg(fsp)));

		if (ret) {
			/* Hmmm. No clue what to set smblctx to - use -1. */
			*psmblctx = 0xFFFFFFFFFFFFFFFFLL;
			return NT_STATUS_LOCK_NOT_GRANTED;
		}
        }

	return NT_STATUS_OK;
}


bool smb_vfs_call_brl_cancel_windows(struct vfs_handle_struct *handle,
				     struct byte_range_lock *br_lck,
				     struct lock_struct *plock,
				     struct blocking_lock_record *blr)
{
	VFS_FIND(brl_cancel_windows);
	return handle->fns->brl_cancel_windows(handle, br_lck, plock, blr);
}

/****************************************************************************
 Remove a particular pending lock.
****************************************************************************/
bool brl_lock_cancel(struct byte_range_lock *br_lck,
		uint64_t smblctx,
		struct server_id pid,
		br_off start,
		br_off size,
		enum brl_flavour lock_flav,
		struct blocking_lock_record *blr)
{
	bool ret;
	struct lock_struct lock;

	lock.context.smblctx = smblctx;
	lock.context.pid = pid;
	lock.context.tid = br_lck->fsp->conn->cnum;
	lock.start = start;
	lock.size = size;
	lock.fnum = br_lck->fsp->fnum;
	lock.lock_flav = lock_flav;
	/* lock.lock_type doesn't matter */

	if (lock_flav == WINDOWS_LOCK) {
		ret = SMB_VFS_BRL_CANCEL_WINDOWS(br_lck->fsp->conn, br_lck,
		    &lock, blr);
	} else {
		ret = brl_lock_cancel_default(br_lck, &lock);
	}

	return ret;
}

bool brl_lock_cancel_default(struct byte_range_lock *br_lck,
		struct lock_struct *plock)
{
	unsigned int i;
	struct lock_struct *locks = br_lck->lock_data;

	SMB_ASSERT(plock);

	for (i = 0; i < br_lck->num_locks; i++) {
		struct lock_struct *lock = &locks[i];

		/* For pending locks we *always* care about the fnum. */
		if (brl_same_context(&lock->context, &plock->context) &&
				lock->fnum == plock->fnum &&
				IS_PENDING_LOCK(lock->lock_type) &&
				lock->lock_flav == plock->lock_flav &&
				lock->start == plock->start &&
				lock->size == plock->size) {
			break;
		}
	}

	if (i == br_lck->num_locks) {
		/* Didn't find it. */
		return False;
	}

	if (i < br_lck->num_locks - 1) {
		/* Found this particular pending lock - delete it */
		memmove(&locks[i], &locks[i+1], 
			sizeof(*locks)*((br_lck->num_locks-1) - i));
	}

	br_lck->num_locks -= 1;
	br_lck->modified = True;
	return True;
}

/****************************************************************************
 Remove any locks associated with a open file.
 We return True if this process owns any other Windows locks on this
 fd and so we should not immediately close the fd.
****************************************************************************/

void brl_close_fnum(struct messaging_context *msg_ctx,
		    struct byte_range_lock *br_lck)
{
	files_struct *fsp = br_lck->fsp;
	uint16 tid = fsp->conn->cnum;
	int fnum = fsp->fnum;
	unsigned int i;
	struct lock_struct *locks = br_lck->lock_data;
	struct server_id pid = sconn_server_id(fsp->conn->sconn);
	struct lock_struct *locks_copy;
	unsigned int num_locks_copy;

	/* Copy the current lock array. */
	if (br_lck->num_locks) {
		locks_copy = (struct lock_struct *)TALLOC_MEMDUP(br_lck, locks, br_lck->num_locks * sizeof(struct lock_struct));
		if (!locks_copy) {
			smb_panic("brl_close_fnum: talloc failed");
			}
	} else {
		locks_copy = NULL;
	}

	num_locks_copy = br_lck->num_locks;

	for (i=0; i < num_locks_copy; i++) {
		struct lock_struct *lock = &locks_copy[i];

		if (lock->context.tid == tid && procid_equal(&lock->context.pid, &pid) &&
				(lock->fnum == fnum)) {
			brl_unlock(msg_ctx,
				br_lck,
				lock->context.smblctx,
				pid,
				lock->start,
				lock->size,
				lock->lock_flav);
		}
	}
}

/****************************************************************************
 Ensure this set of lock entries is valid.
****************************************************************************/
static bool validate_lock_entries(unsigned int *pnum_entries, struct lock_struct **pplocks)
{
	unsigned int i;
	unsigned int num_valid_entries = 0;
	struct lock_struct *locks = *pplocks;

	for (i = 0; i < *pnum_entries; i++) {
		struct lock_struct *lock_data = &locks[i];
		if (!serverid_exists(&lock_data->context.pid)) {
			/* This process no longer exists - mark this
			   entry as invalid by zeroing it. */
			ZERO_STRUCTP(lock_data);
		} else {
			num_valid_entries++;
		}
	}

	if (num_valid_entries != *pnum_entries) {
		struct lock_struct *new_lock_data = NULL;

		if (num_valid_entries) {
			new_lock_data = SMB_MALLOC_ARRAY(struct lock_struct, num_valid_entries);
			if (!new_lock_data) {
				DEBUG(3, ("malloc fail\n"));
				return False;
			}

			num_valid_entries = 0;
			for (i = 0; i < *pnum_entries; i++) {
				struct lock_struct *lock_data = &locks[i];
				if (lock_data->context.smblctx &&
						lock_data->context.tid) {
					/* Valid (nonzero) entry - copy it. */
					memcpy(&new_lock_data[num_valid_entries],
						lock_data, sizeof(struct lock_struct));
					num_valid_entries++;
				}
			}
		}

		SAFE_FREE(*pplocks);
		*pplocks = new_lock_data;
		*pnum_entries = num_valid_entries;
	}

	return True;
}

struct brl_forall_cb {
	void (*fn)(struct file_id id, struct server_id pid,
		   enum brl_type lock_type,
		   enum brl_flavour lock_flav,
		   br_off start, br_off size,
		   void *private_data);
	void *private_data;
};

/****************************************************************************
 Traverse the whole database with this function, calling traverse_callback
 on each lock.
****************************************************************************/

static int traverse_fn(struct db_record *rec, void *state)
{
	struct brl_forall_cb *cb = (struct brl_forall_cb *)state;
	struct lock_struct *locks;
	struct file_id *key;
	unsigned int i;
	unsigned int num_locks = 0;
	unsigned int orig_num_locks = 0;

	/* In a traverse function we must make a copy of
	   dbuf before modifying it. */

	locks = (struct lock_struct *)memdup(rec->value.dptr,
					     rec->value.dsize);
	if (!locks) {
		return -1; /* Terminate traversal. */
	}

	key = (struct file_id *)rec->key.dptr;
	orig_num_locks = num_locks = rec->value.dsize/sizeof(*locks);

	/* Ensure the lock db is clean of entries from invalid processes. */

	if (!validate_lock_entries(&num_locks, &locks)) {
		SAFE_FREE(locks);
		return -1; /* Terminate traversal */
	}

	if (orig_num_locks != num_locks) {
		if (num_locks) {
			TDB_DATA data;
			data.dptr = (uint8_t *)locks;
			data.dsize = num_locks*sizeof(struct lock_struct);
			rec->store(rec, data, TDB_REPLACE);
		} else {
			rec->delete_rec(rec);
		}
	}

	if (cb->fn) {
		for ( i=0; i<num_locks; i++) {
			cb->fn(*key,
				locks[i].context.pid,
				locks[i].lock_type,
				locks[i].lock_flav,
				locks[i].start,
				locks[i].size,
				cb->private_data);
		}
	}

	SAFE_FREE(locks);
	return 0;
}

/*******************************************************************
 Call the specified function on each lock in the database.
********************************************************************/

int brl_forall(void (*fn)(struct file_id id, struct server_id pid,
			  enum brl_type lock_type,
			  enum brl_flavour lock_flav,
			  br_off start, br_off size,
			  void *private_data),
	       void *private_data)
{
	struct brl_forall_cb cb;

	if (!brlock_db) {
		return 0;
	}
	cb.fn = fn;
	cb.private_data = private_data;
	return brlock_db->traverse(brlock_db, traverse_fn, &cb);
}

/*******************************************************************
 Store a potentially modified set of byte range lock data back into
 the database.
 Unlock the record.
********************************************************************/

static void byte_range_lock_flush(struct byte_range_lock *br_lck)
{
	if (br_lck->read_only) {
		SMB_ASSERT(!br_lck->modified);
	}

	if (!br_lck->modified) {
		goto done;
	}

	if (br_lck->num_locks == 0) {
		/* No locks - delete this entry. */
		NTSTATUS status = br_lck->record->delete_rec(br_lck->record);
		if (!NT_STATUS_IS_OK(status)) {
			DEBUG(0, ("delete_rec returned %s\n",
				  nt_errstr(status)));
			smb_panic("Could not delete byte range lock entry");
		}
	} else {
		TDB_DATA data;
		NTSTATUS status;

		data.dptr = (uint8 *)br_lck->lock_data;
		data.dsize = br_lck->num_locks * sizeof(struct lock_struct);

		status = br_lck->record->store(br_lck->record, data,
					       TDB_REPLACE);
		if (!NT_STATUS_IS_OK(status)) {
			DEBUG(0, ("store returned %s\n", nt_errstr(status)));
			smb_panic("Could not store byte range mode entry");
		}
	}

 done:

	br_lck->read_only = true;
	br_lck->modified = false;

	TALLOC_FREE(br_lck->record);
}

static int byte_range_lock_destructor(struct byte_range_lock *br_lck)
{
	byte_range_lock_flush(br_lck);
	SAFE_FREE(br_lck->lock_data);
	return 0;
}

/*******************************************************************
 Fetch a set of byte range lock data from the database.
 Leave the record locked.
 TALLOC_FREE(brl) will release the lock in the destructor.
********************************************************************/

static struct byte_range_lock *brl_get_locks_internal(TALLOC_CTX *mem_ctx,
					files_struct *fsp, bool read_only)
{
	TDB_DATA key, data;
	struct byte_range_lock *br_lck = TALLOC_P(mem_ctx, struct byte_range_lock);
	bool do_read_only = read_only;

	if (br_lck == NULL) {
		return NULL;
	}

	br_lck->fsp = fsp;
	br_lck->num_locks = 0;
	br_lck->modified = False;
	br_lck->key = fsp->file_id;

	key.dptr = (uint8 *)&br_lck->key;
	key.dsize = sizeof(struct file_id);

	if (!fsp->lockdb_clean) {
		/* We must be read/write to clean
		   the dead entries. */
		do_read_only = false;
	}

	if (do_read_only) {
		if (brlock_db->fetch(brlock_db, br_lck, key, &data) == -1) {
			DEBUG(3, ("Could not fetch byte range lock record\n"));
			TALLOC_FREE(br_lck);
			return NULL;
		}
		br_lck->record = NULL;
	} else {
		br_lck->record = brlock_db->fetch_locked(brlock_db, br_lck, key);

		if (br_lck->record == NULL) {
			DEBUG(3, ("Could not lock byte range lock entry\n"));
			TALLOC_FREE(br_lck);
			return NULL;
		}

		data = br_lck->record->value;
	}

	br_lck->read_only = do_read_only;
	br_lck->lock_data = NULL;

	talloc_set_destructor(br_lck, byte_range_lock_destructor);

	br_lck->num_locks = data.dsize / sizeof(struct lock_struct);

	if (br_lck->num_locks != 0) {
		br_lck->lock_data = SMB_MALLOC_ARRAY(struct lock_struct,
						     br_lck->num_locks);
		if (br_lck->lock_data == NULL) {
			DEBUG(0, ("malloc failed\n"));
			TALLOC_FREE(br_lck);
			return NULL;
		}

		memcpy(br_lck->lock_data, data.dptr, data.dsize);
	}
	
	if (!fsp->lockdb_clean) {
		int orig_num_locks = br_lck->num_locks;

		/* This is the first time we've accessed this. */
		/* Go through and ensure all entries exist - remove any that don't. */
		/* Makes the lockdb self cleaning at low cost. */

		if (!validate_lock_entries(&br_lck->num_locks,
					   &br_lck->lock_data)) {
			SAFE_FREE(br_lck->lock_data);
			TALLOC_FREE(br_lck);
			return NULL;
		}

		/* Ensure invalid locks are cleaned up in the destructor. */
		if (orig_num_locks != br_lck->num_locks) {
			br_lck->modified = True;
		}

		/* Mark the lockdb as "clean" as seen from this open file. */
		fsp->lockdb_clean = True;
	}

	if (DEBUGLEVEL >= 10) {
		unsigned int i;
		struct lock_struct *locks = br_lck->lock_data;
		DEBUG(10,("brl_get_locks_internal: %u current locks on file_id %s\n",
			br_lck->num_locks,
			  file_id_string_tos(&fsp->file_id)));
		for( i = 0; i < br_lck->num_locks; i++) {
			print_lock_struct(i, &locks[i]);
		}
	}

	if (do_read_only != read_only) {
		/*
		 * this stores the record and gets rid of
		 * the write lock that is needed for a cleanup
		 */
		byte_range_lock_flush(br_lck);
	}

	return br_lck;
}

struct byte_range_lock *brl_get_locks(TALLOC_CTX *mem_ctx,
					files_struct *fsp)
{
	return brl_get_locks_internal(mem_ctx, fsp, False);
}

struct byte_range_lock *brl_get_locks_readonly(files_struct *fsp)
{
	struct byte_range_lock *br_lock;

	if (lp_clustering()) {
		return brl_get_locks_internal(talloc_tos(), fsp, true);
	}

	if ((fsp->brlock_rec != NULL)
	    && (brlock_db->get_seqnum(brlock_db) == fsp->brlock_seqnum)) {
		return fsp->brlock_rec;
	}

	TALLOC_FREE(fsp->brlock_rec);

	br_lock = brl_get_locks_internal(talloc_tos(), fsp, true);
	if (br_lock == NULL) {
		return NULL;
	}
	fsp->brlock_seqnum = brlock_db->get_seqnum(brlock_db);

	fsp->brlock_rec = talloc_move(fsp, &br_lock);

	return fsp->brlock_rec;
}

struct brl_revalidate_state {
	ssize_t array_size;
	uint32 num_pids;
	struct server_id *pids;
};

/*
 * Collect PIDs of all processes with pending entries
 */

static void brl_revalidate_collect(struct file_id id, struct server_id pid,
				   enum brl_type lock_type,
				   enum brl_flavour lock_flav,
				   br_off start, br_off size,
				   void *private_data)
{
	struct brl_revalidate_state *state =
		(struct brl_revalidate_state *)private_data;

	if (!IS_PENDING_LOCK(lock_type)) {
		return;
	}

	add_to_large_array(state, sizeof(pid), (void *)&pid,
			   &state->pids, &state->num_pids,
			   &state->array_size);
}

/*
 * qsort callback to sort the processes
 */

static int compare_procids(const void *p1, const void *p2)
{
	const struct server_id *i1 = (struct server_id *)p1;
	const struct server_id *i2 = (struct server_id *)p2;

	if (i1->pid < i2->pid) return -1;
	if (i2->pid > i2->pid) return 1;
	return 0;
}

/*
 * Send a MSG_SMB_UNLOCK message to all processes with pending byte range
 * locks so that they retry. Mainly used in the cluster code after a node has
 * died.
 *
 * Done in two steps to avoid double-sends: First we collect all entries in an
 * array, then qsort that array and only send to non-dupes.
 */

static void brl_revalidate(struct messaging_context *msg_ctx,
			   void *private_data,
			   uint32_t msg_type,
			   struct server_id server_id,
			   DATA_BLOB *data)
{
	struct brl_revalidate_state *state;
	uint32 i;
	struct server_id last_pid;

	if (!(state = TALLOC_ZERO_P(NULL, struct brl_revalidate_state))) {
		DEBUG(0, ("talloc failed\n"));
		return;
	}

	brl_forall(brl_revalidate_collect, state);

	if (state->array_size == -1) {
		DEBUG(0, ("talloc failed\n"));
		goto done;
	}

	if (state->num_pids == 0) {
		goto done;
	}

	TYPESAFE_QSORT(state->pids, state->num_pids, compare_procids);

	ZERO_STRUCT(last_pid);

	for (i=0; i<state->num_pids; i++) {
		if (procid_equal(&last_pid, &state->pids[i])) {
			/*
			 * We've seen that one already
			 */
			continue;
		}

		messaging_send(msg_ctx, state->pids[i], MSG_SMB_UNLOCK,
			       &data_blob_null);
		last_pid = state->pids[i];
	}

 done:
	TALLOC_FREE(state);
	return;
}

void brl_register_msgs(struct messaging_context *msg_ctx)
{
	messaging_register(msg_ctx, NULL, MSG_SMB_BRL_VALIDATE,
			   brl_revalidate);
}
