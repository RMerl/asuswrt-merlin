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
   version 2 of the License, or (at your option) any later version.

   This library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Lesser General Public License for more details.

   You should have received a copy of the GNU Lesser General Public
   License along with this library; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
*/

#include "tdb_private.h"

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
		/* Generic lock error. errno set by fcntl.
		 * EAGAIN is an expected return from non-blocking
		 * locks. */
		if (!probe && lck_type != F_SETLK) {
			/* Ensure error code is set for log fun to examine. */
			tdb->ecode = TDB_ERR_LOCK;
			TDB_LOG((tdb, TDB_DEBUG_TRACE,"tdb_brlock failed (fd=%d) at offset %d rw_type=%d lck_type=%d len=%d\n", 
				 tdb->fd, offset, rw_type, lck_type, (int)len));
		}
		return TDB_ERRCODE(TDB_ERR_LOCK, -1);
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
int tdb_lock(struct tdb_context *tdb, int list, int ltype)
{
	struct tdb_lock_type *new_lck;
	int i;

	/* a global lock allows us to avoid per chain locks */
	if (tdb->global_lock.count && 
	    (ltype == tdb->global_lock.ltype || ltype == F_RDLCK)) {
		return 0;
	}

	if (tdb->global_lock.count) {
		return TDB_ERRCODE(TDB_ERR_LOCK, -1);
	}

	if (list < -1 || list >= (int)tdb->header.hash_size) {
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
	if (tdb->methods->tdb_brlock(tdb,FREELIST_TOP+4*list,ltype,F_SETLKW,
				     0, 1)) {
		TDB_LOG((tdb, TDB_DEBUG_ERROR, "tdb_lock failed on list %d "
			 "ltype=%d (%s)\n",  list, ltype, strerror(errno)));
		return -1;
	}

	tdb->num_locks++;

	tdb->lockrecs[tdb->num_lockrecs].list = list;
	tdb->lockrecs[tdb->num_lockrecs].count = 1;
	tdb->lockrecs[tdb->num_lockrecs].ltype = ltype;
	tdb->num_lockrecs += 1;

	return 0;
}

/* unlock the database: returns void because it's too late for errors. */
	/* changed to return int it may be interesting to know there
	   has been an error  --simo */
int tdb_unlock(struct tdb_context *tdb, int list, int ltype)
{
	int ret = -1;
	int i;
	struct tdb_lock_type *lck = NULL;

	/* a global lock allows us to avoid per chain locks */
	if (tdb->global_lock.count && 
	    (ltype == tdb->global_lock.ltype || ltype == F_RDLCK)) {
		return 0;
	}

	if (tdb->global_lock.count) {
		return TDB_ERRCODE(TDB_ERR_LOCK, -1);
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

	ret = tdb->methods->tdb_brlock(tdb, FREELIST_TOP+4*list, F_UNLCK,
				       F_SETLKW, 0, 1);
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



/* lock/unlock entire database */
static int _tdb_lockall(struct tdb_context *tdb, int ltype)
{
	/* There are no locks on read-only dbs */
	if (tdb->read_only || tdb->traverse_read)
		return TDB_ERRCODE(TDB_ERR_LOCK, -1);

	if (tdb->global_lock.count && tdb->global_lock.ltype == ltype) {
		tdb->global_lock.count++;
		return 0;
	}

	if (tdb->global_lock.count) {
		/* a global lock of a different type exists */
		return TDB_ERRCODE(TDB_ERR_LOCK, -1);
	}
	
	if (tdb->num_locks != 0) {
		/* can't combine global and chain locks */
		return TDB_ERRCODE(TDB_ERR_LOCK, -1);
	}

	if (tdb->methods->tdb_brlock(tdb, FREELIST_TOP, ltype, F_SETLKW, 
				     0, 4*tdb->header.hash_size)) {
		TDB_LOG((tdb, TDB_DEBUG_ERROR, "tdb_lockall failed (%s)\n", strerror(errno)));
		return -1;
	}

	tdb->global_lock.count = 1;
	tdb->global_lock.ltype = ltype;

	return 0;
}

/* unlock entire db */
static int _tdb_unlockall(struct tdb_context *tdb, int ltype)
{
	/* There are no locks on read-only dbs */
	if (tdb->read_only || tdb->traverse_read) {
		return TDB_ERRCODE(TDB_ERR_LOCK, -1);
	}

	if (tdb->global_lock.ltype != ltype || tdb->global_lock.count == 0) {
		return TDB_ERRCODE(TDB_ERR_LOCK, -1);
	}

	if (tdb->global_lock.count > 1) {
		tdb->global_lock.count--;
		return 0;
	}

	if (tdb->methods->tdb_brlock(tdb, FREELIST_TOP, F_UNLCK, F_SETLKW, 
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
	return _tdb_lockall(tdb, F_WRLCK);
}

/* unlock entire database with write lock */
int tdb_unlockall(struct tdb_context *tdb)
{
	return _tdb_unlockall(tdb, F_WRLCK);
}

/* lock entire database with read lock */
int tdb_lockall_read(struct tdb_context *tdb)
{
	return _tdb_lockall(tdb, F_RDLCK);
}

/* unlock entire database with read lock */
int tdb_unlockall_read(struct tdb_context *tdb)
{
	return _tdb_unlockall(tdb, F_RDLCK);
}

/* lock/unlock one hash chain. This is meant to be used to reduce
   contention - it cannot guarantee how many records will be locked */
int tdb_chainlock(struct tdb_context *tdb, TDB_DATA key)
{
	return tdb_lock(tdb, BUCKET(tdb->hash_fn(&key)), F_WRLCK);
}

int tdb_chainunlock(struct tdb_context *tdb, TDB_DATA key)
{
	return tdb_unlock(tdb, BUCKET(tdb->hash_fn(&key)), F_WRLCK);
}

int tdb_chainlock_read(struct tdb_context *tdb, TDB_DATA key)
{
	return tdb_lock(tdb, BUCKET(tdb->hash_fn(&key)), F_RDLCK);
}

int tdb_chainunlock_read(struct tdb_context *tdb, TDB_DATA key)
{
	return tdb_unlock(tdb, BUCKET(tdb->hash_fn(&key)), F_RDLCK);
}



/* record lock stops delete underneath */
int tdb_lock_record(struct tdb_context *tdb, tdb_off_t off)
{
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
	u32 count = 0;

	if (off == 0)
		return 0;
	for (i = &tdb->travlocks; i; i = i->next)
		if (i->off == off)
			count++;
	return (count == 1 ? tdb->methods->tdb_brlock(tdb, off, F_UNLCK, F_SETLKW, 0, 1) : 0);
}
