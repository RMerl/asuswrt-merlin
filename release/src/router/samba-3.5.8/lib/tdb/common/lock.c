 /* 
   Unix SMB/CIFS implementation.

   trivial database library

   Copyright (C) Andrew Tridgell              1999-2005
   Copyright (C) Paul `Rusty' Russell		   2000
   Copyright (C) Jeremy Allison			   2000-2003
   
     ** NOTE! The following LGPL license applies to the tdb
     ** library. This does NOT imply that all of Samba is released
     ** under the LGPL
   
   This library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Lesser General Public
   License as published by the Free Software Foundation; either
   version 3 of the License, or (at your option) any later version.

   This library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Lesser General Public License for more details.

   You should have received a copy of the GNU Lesser General Public
   License along with this library; if not, see <http://www.gnu.org/licenses/>.
*/

#include "tdb_private.h"

#define TDB_MARK_LOCK 0x80000000

void tdb_setalarm_sigptr(struct tdb_context *tdb, volatile sig_atomic_t *ptr)
{
	tdb->interrupt_sig_ptr = ptr;
}

/* a byte range locking function - return 0 on success
   this functions locks/unlocks 1 byte at the specified offset.

   On error, errno is also set so that errors are passed back properly
   through tdb_open(). 

   note that a len of zero means lock to end of file
*/
int tdb_brlock(struct tdb_context *tdb, tdb_off_t offset, 
	       int rw_type, int lck_type, int probe, size_t len)
{
	struct flock fl;
	int ret;

	if (tdb->flags & TDB_NOLOCK) {
		return 0;
	}

	if ((rw_type == F_WRLCK) && (tdb->read_only || tdb->traverse_read)) {
		tdb->ecode = TDB_ERR_RDONLY;
		return -1;
	}

	fl.l_type = rw_type;
	fl.l_whence = SEEK_SET;
	fl.l_start = offset;
	fl.l_len = len;
	fl.l_pid = 0;

	do {
		ret = fcntl(tdb->fd,lck_type,&fl);

		/* Check for a sigalarm break. */
		if (ret == -1 && errno == EINTR &&
				tdb->interrupt_sig_ptr &&
				*tdb->interrupt_sig_ptr) {
			break;
		}
	} while (ret == -1 && errno == EINTR);

	if (ret == -1) {
		tdb->ecode = TDB_ERR_LOCK;
		/* Generic lock error. errno set by fcntl.
		 * EAGAIN is an expected return from non-blocking
		 * locks. */
		if (!probe && lck_type != F_SETLK) {
			TDB_LOG((tdb, TDB_DEBUG_TRACE,"tdb_brlock failed (fd=%d) at offset %d rw_type=%d lck_type=%d len=%d\n", 
				 tdb->fd, offset, rw_type, lck_type, (int)len));
		}
		return -1;
	}
	return 0;
}


/*
  upgrade a read lock to a write lock. This needs to be handled in a
  special way as some OSes (such as solaris) have too conservative
  deadlock detection and claim a deadlock when progress can be
  made. For those OSes we may loop for a while.  
*/
int tdb_brlock_upgrade(struct tdb_context *tdb, tdb_off_t offset, size_t len)
{
	int count = 1000;
	while (count--) {
		struct timeval tv;
		if (tdb_brlock(tdb, offset, F_WRLCK, F_SETLKW, 1, len) == 0) {
			return 0;
		}
		if (errno != EDEADLK) {
			break;
		}
		/* sleep for as short a time as we can - more portable than usleep() */
		tv.tv_sec = 0;
		tv.tv_usec = 1;
		select(0, NULL, NULL, NULL, &tv);
	}
	TDB_LOG((tdb, TDB_DEBUG_TRACE,"tdb_brlock_upgrade failed at offset %d\n", offset));
	return -1;
}


/* lock a list in the database. list -1 is the alloc list */
static int _tdb_lock(struct tdb_context *tdb, int list, int ltype, int op)
{
	struct tdb_lock_type *new_lck;
	int i;
	bool mark_lock = ((ltype & TDB_MARK_LOCK) == TDB_MARK_LOCK);

	ltype &= ~TDB_MARK_LOCK;

	/* a global lock allows us to avoid per chain locks */
	if (tdb->global_lock.count && 
	    (ltype == tdb->global_lock.ltype || ltype == F_RDLCK)) {
		return 0;
	}

	if (tdb->global_lock.count) {
		tdb->ecode = TDB_ERR_LOCK;
		return -1;
	}

	if (list < -1 || list >= (int)tdb->header.hash_size) {
		tdb->ecode = TDB_ERR_LOCK;
		TDB_LOG((tdb, TDB_DEBUG_ERROR,"tdb_lock: invalid list %d for ltype=%d\n", 
			   list, ltype));
		return -1;
	}
	if (tdb->flags & TDB_NOLOCK)
		return 0;

	for (i=0; i<tdb->num_lockrecs; i++) {
		if (tdb->lockrecs[i].list == list) {
			if (tdb->lockrecs[i].count == 0) {
				/*
				 * Can't happen, see tdb_unlock(). It should
				 * be an assert.
				 */
				TDB_LOG((tdb, TDB_DEBUG_ERROR, "tdb_lock: "
					 "lck->count == 0 for list %d", list));
			}
			/*
			 * Just increment the in-memory struct, posix locks
			 * don't stack.
			 */
			tdb->lockrecs[i].count++;
			return 0;
		}
	}

	new_lck = (struct tdb_lock_type *)realloc(
		tdb->lockrecs,
		sizeof(*tdb->lockrecs) * (tdb->num_lockrecs+1));
	if (new_lck == NULL) {
		errno = ENOMEM;
		return -1;
	}
	tdb->lockrecs = new_lck;

	/* Since fcntl locks don't nest, we do a lock for the first one,
	   and simply bump the count for future ones */
	if (!mark_lock &&
	    tdb->methods->tdb_brlock(tdb,FREELIST_TOP+4*list, ltype, op,
				     0, 1)) {
		return -1;
	}

	tdb->num_locks++;

	tdb->lockrecs[tdb->num_lockrecs].list = list;
	tdb->lockrecs[tdb->num_lockrecs].count = 1;
	tdb->lockrecs[tdb->num_lockrecs].ltype = ltype;
	tdb->num_lockrecs += 1;

	return 0;
}

/* lock a list in the database. list -1 is the alloc list */
int tdb_lock(struct tdb_context *tdb, int list, int ltype)
{
	int ret;
	ret = _tdb_lock(tdb, list, ltype, F_SETLKW);
	if (ret) {
		TDB_LOG((tdb, TDB_DEBUG_ERROR, "tdb_lock failed on list %d "
			 "ltype=%d (%s)\n",  list, ltype, strerror(errno)));
	}
	return ret;
}

/* lock a list in the database. list -1 is the alloc list. non-blocking lock */
int tdb_lock_nonblock(struct tdb_context *tdb, int list, int ltype)
{
	return _tdb_lock(tdb, list, ltype, F_SETLK);
}


/* unlock the database: returns void because it's too late for errors. */
	/* changed to return int it may be interesting to know there
	   has been an error  --simo */
int tdb_unlock(struct tdb_context *tdb, int list, int ltype)
{
	int ret = -1;
	int i;
	struct tdb_lock_type *lck = NULL;
	bool mark_lock = ((ltype & TDB_MARK_LOCK) == TDB_MARK_LOCK);

	ltype &= ~TDB_MARK_LOCK;

	/* a global lock allows us to avoid per chain locks */
	if (tdb->global_lock.count && 
	    (ltype == tdb->global_lock.ltype || ltype == F_RDLCK)) {
		return 0;
	}

	if (tdb->global_lock.count) {
		tdb->ecode = TDB_ERR_LOCK;
		return -1;
	}

	if (tdb->flags & TDB_NOLOCK)
		return 0;

	/* Sanity checks */
	if (list < -1 || list >= (int)tdb->header.hash_size) {
		TDB_LOG((tdb, TDB_DEBUG_ERROR, "tdb_unlock: list %d invalid (%d)\n", list, tdb->header.hash_size));
		return ret;
	}

	for (i=0; i<tdb->num_lockrecs; i++) {
		if (tdb->lockrecs[i].list == list) {
			lck = &tdb->lockrecs[i];
			break;
		}
	}

	if ((lck == NULL) || (lck->count == 0)) {
		TDB_LOG((tdb, TDB_DEBUG_ERROR, "tdb_unlock: count is 0\n"));
		return -1;
	}

	if (lck->count > 1) {
		lck->count--;
		return 0;
	}

	/*
	 * This lock has count==1 left, so we need to unlock it in the
	 * kernel. We don't bother with decrementing the in-memory array
	 * element, we're about to overwrite it with the last array element
	 * anyway.
	 */

	if (mark_lock) {
		ret = 0;
	} else {
		ret = tdb->methods->tdb_brlock(tdb, FREELIST_TOP+4*list, F_UNLCK,
					       F_SETLKW, 0, 1);
	}
	tdb->num_locks--;

	/*
	 * Shrink the array by overwriting the element just unlocked with the
	 * last array element.
	 */

	if (tdb->num_lockrecs > 1) {
		*lck = tdb->lockrecs[tdb->num_lockrecs-1];
	}
	tdb->num_lockrecs -= 1;

	/*
	 * We don't bother with realloc when the array shrinks, but if we have
	 * a completely idle tdb we should get rid of the locked array.
	 */

	if (tdb->num_lockrecs == 0) {
		SAFE_FREE(tdb->lockrecs);
	}

	if (ret)
		TDB_LOG((tdb, TDB_DEBUG_ERROR, "tdb_unlock: An error occurred unlocking!\n")); 
	return ret;
}

/*
  get the transaction lock
 */
int tdb_transaction_lock(struct tdb_context *tdb, int ltype)
{
	if (tdb->global_lock.count) {
		return 0;
	}
	if (tdb->transaction_lock_count > 0) {
		tdb->transaction_lock_count++;
		return 0;
	}

	if (tdb->methods->tdb_brlock(tdb, TRANSACTION_LOCK, ltype, 
				     F_SETLKW, 0, 1) == -1) {
		TDB_LOG((tdb, TDB_DEBUG_ERROR, "tdb_transaction_lock: failed to get transaction lock\n"));
		tdb->ecode = TDB_ERR_LOCK;
		return -1;
	}
	tdb->transaction_lock_count++;
	return 0;
}

/*
  release the transaction lock
 */
int tdb_transaction_unlock(struct tdb_context *tdb)
{
	int ret;
	if (tdb->global_lock.count) {
		return 0;
	}
	if (tdb->transaction_lock_count > 1) {
		tdb->transaction_lock_count--;
		return 0;
	}
	ret = tdb->methods->tdb_brlock(tdb, TRANSACTION_LOCK, F_UNLCK, F_SETLKW, 0, 1);
	if (ret == 0) {
		tdb->transaction_lock_count = 0;
	}
	return ret;
}




/* lock/unlock entire database */
static int _tdb_lockall(struct tdb_context *tdb, int ltype, int op)
{
	bool mark_lock = ((ltype & TDB_MARK_LOCK) == TDB_MARK_LOCK);

	ltype &= ~TDB_MARK_LOCK;

	/* There are no locks on read-only dbs */
	if (tdb->read_only || tdb->traverse_read) {
		tdb->ecode = TDB_ERR_LOCK;
		return -1;
	}

	if (tdb->global_lock.count && tdb->global_lock.ltype == ltype) {
		tdb->global_lock.count++;
		return 0;
	}

	if (tdb->global_lock.count) {
		/* a global lock of a different type exists */
		tdb->ecode = TDB_ERR_LOCK;
		return -1;
	}
	
	if (tdb->num_locks != 0) {
		/* can't combine global and chain locks */
		tdb->ecode = TDB_ERR_LOCK;
		return -1;
	}

	if (!mark_lock &&
	    tdb->methods->tdb_brlock(tdb, FREELIST_TOP, ltype, op,
				     0, 4*tdb->header.hash_size)) {
		if (op == F_SETLKW) {
			TDB_LOG((tdb, TDB_DEBUG_ERROR, "tdb_lockall failed (%s)\n", strerror(errno)));
		}
		return -1;
	}

	tdb->global_lock.count = 1;
	tdb->global_lock.ltype = ltype;

	return 0;
}



/* unlock entire db */
static int _tdb_unlockall(struct tdb_context *tdb, int ltype)
{
	bool mark_lock = ((ltype & TDB_MARK_LOCK) == TDB_MARK_LOCK);

	ltype &= ~TDB_MARK_LOCK;

	/* There are no locks on read-only dbs */
	if (tdb->read_only || tdb->traverse_read) {
		tdb->ecode = TDB_ERR_LOCK;
		return -1;
	}

	if (tdb->global_lock.ltype != ltype || tdb->global_lock.count == 0) {
		tdb->ecode = TDB_ERR_LOCK;
		return -1;
	}

	if (tdb->global_lock.count > 1) {
		tdb->global_lock.count--;
		return 0;
	}

	if (!mark_lock &&
	    tdb->methods->tdb_brlock(tdb, FREELIST_TOP, F_UNLCK, F_SETLKW, 
				     0, 4*tdb->header.hash_size)) {
		TDB_LOG((tdb, TDB_DEBUG_ERROR, "tdb_unlockall failed (%s)\n", strerror(errno)));
		return -1;
	}

	tdb->global_lock.count = 0;
	tdb->global_lock.ltype = 0;

	return 0;
}

/* lock entire database with write lock */
int tdb_lockall(struct tdb_context *tdb)
{
	tdb_trace(tdb, "tdb_lockall");
	return _tdb_lockall(tdb, F_WRLCK, F_SETLKW);
}

/* lock entire database with write lock - mark only */
int tdb_lockall_mark(struct tdb_context *tdb)
{
	tdb_trace(tdb, "tdb_lockall_mark");
	return _tdb_lockall(tdb, F_WRLCK | TDB_MARK_LOCK, F_SETLKW);
}

/* unlock entire database with write lock - unmark only */
int tdb_lockall_unmark(struct tdb_context *tdb)
{
	tdb_trace(tdb, "tdb_lockall_unmark");
	return _tdb_unlockall(tdb, F_WRLCK | TDB_MARK_LOCK);
}

/* lock entire database with write lock - nonblocking varient */
int tdb_lockall_nonblock(struct tdb_context *tdb)
{
	int ret = _tdb_lockall(tdb, F_WRLCK, F_SETLK);
	tdb_trace_ret(tdb, "tdb_lockall_nonblock", ret);
	return ret;
}

/* unlock entire database with write lock */
int tdb_unlockall(struct tdb_context *tdb)
{
	tdb_trace(tdb, "tdb_unlockall");
	return _tdb_unlockall(tdb, F_WRLCK);
}

/* lock entire database with read lock */
int tdb_lockall_read(struct tdb_context *tdb)
{
	tdb_trace(tdb, "tdb_lockall_read");
	return _tdb_lockall(tdb, F_RDLCK, F_SETLKW);
}

/* lock entire database with read lock - nonblock varient */
int tdb_lockall_read_nonblock(struct tdb_context *tdb)
{
	int ret = _tdb_lockall(tdb, F_RDLCK, F_SETLK);
	tdb_trace_ret(tdb, "tdb_lockall_read_nonblock", ret);
	return ret;
}

/* unlock entire database with read lock */
int tdb_unlockall_read(struct tdb_context *tdb)
{
	tdb_trace(tdb, "tdb_unlockall_read");
	return _tdb_unlockall(tdb, F_RDLCK);
}

/* lock/unlock one hash chain. This is meant to be used to reduce
   contention - it cannot guarantee how many records will be locked */
int tdb_chainlock(struct tdb_context *tdb, TDB_DATA key)
{
	int ret = tdb_lock(tdb, BUCKET(tdb->hash_fn(&key)), F_WRLCK);
	tdb_trace_1rec(tdb, "tdb_chainlock", key);
	return ret;
}

/* lock/unlock one hash chain, non-blocking. This is meant to be used
   to reduce contention - it cannot guarantee how many records will be
   locked */
int tdb_chainlock_nonblock(struct tdb_context *tdb, TDB_DATA key)
{
	int ret = tdb_lock_nonblock(tdb, BUCKET(tdb->hash_fn(&key)), F_WRLCK);
	tdb_trace_1rec_ret(tdb, "tdb_chainlock_nonblock", key, ret);
	return ret;
}

/* mark a chain as locked without actually locking it. Warning! use with great caution! */
int tdb_chainlock_mark(struct tdb_context *tdb, TDB_DATA key)
{
	int ret = tdb_lock(tdb, BUCKET(tdb->hash_fn(&key)), F_WRLCK | TDB_MARK_LOCK);
	tdb_trace_1rec(tdb, "tdb_chainlock_mark", key);
	return ret;
}

/* unmark a chain as locked without actually locking it. Warning! use with great caution! */
int tdb_chainlock_unmark(struct tdb_context *tdb, TDB_DATA key)
{
	tdb_trace_1rec(tdb, "tdb_chainlock_unmark", key);
	return tdb_unlock(tdb, BUCKET(tdb->hash_fn(&key)), F_WRLCK | TDB_MARK_LOCK);
}

int tdb_chainunlock(struct tdb_context *tdb, TDB_DATA key)
{
	tdb_trace_1rec(tdb, "tdb_chainunlock", key);
	return tdb_unlock(tdb, BUCKET(tdb->hash_fn(&key)), F_WRLCK);
}

int tdb_chainlock_read(struct tdb_context *tdb, TDB_DATA key)
{
	int ret;
	ret = tdb_lock(tdb, BUCKET(tdb->hash_fn(&key)), F_RDLCK);
	tdb_trace_1rec(tdb, "tdb_chainlock_read", key);
	return ret;
}

int tdb_chainunlock_read(struct tdb_context *tdb, TDB_DATA key)
{
	tdb_trace_1rec(tdb, "tdb_chainunlock_read", key);
	return tdb_unlock(tdb, BUCKET(tdb->hash_fn(&key)), F_RDLCK);
}



/* record lock stops delete underneath */
int tdb_lock_record(struct tdb_context *tdb, tdb_off_t off)
{
	if (tdb->global_lock.count) {
		return 0;
	}
	return off ? tdb->methods->tdb_brlock(tdb, off, F_RDLCK, F_SETLKW, 0, 1) : 0;
}

/*
  Write locks override our own fcntl readlocks, so check it here.
  Note this is meant to be F_SETLK, *not* F_SETLKW, as it's not
  an error to fail to get the lock here.
*/
int tdb_write_lock_record(struct tdb_context *tdb, tdb_off_t off)
{
	struct tdb_traverse_lock *i;
	for (i = &tdb->travlocks; i; i = i->next)
		if (i->off == off)
			return -1;
	return tdb->methods->tdb_brlock(tdb, off, F_WRLCK, F_SETLK, 1, 1);
}

/*
  Note this is meant to be F_SETLK, *not* F_SETLKW, as it's not
  an error to fail to get the lock here.
*/
int tdb_write_unlock_record(struct tdb_context *tdb, tdb_off_t off)
{
	return tdb->methods->tdb_brlock(tdb, off, F_UNLCK, F_SETLK, 0, 1);
}

/* fcntl locks don't stack: avoid unlocking someone else's */
int tdb_unlock_record(struct tdb_context *tdb, tdb_off_t off)
{
	struct tdb_traverse_lock *i;
	uint32_t count = 0;

	if (tdb->global_lock.count) {
		return 0;
	}

	if (off == 0)
		return 0;
	for (i = &tdb->travlocks; i; i = i->next)
		if (i->off == off)
			count++;
	return (count == 1 ? tdb->methods->tdb_brlock(tdb, off, F_UNLCK, F_SETLKW, 0, 1) : 0);
}
