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

/* all contexts, to ensure no double-opens (fcntl locks don't nest!) */
static struct tdb_context *tdbs = NULL;

/* We use two hashes to double-check they're using the right hash function. */
void tdb_header_hash(struct tdb_context *tdb,
		     uint32_t *magic1_hash, uint32_t *magic2_hash)
{
	TDB_DATA hash_key;
	uint32_t tdb_magic = TDB_MAGIC;

	hash_key.dptr = discard_const_p(unsigned char, TDB_MAGIC_FOOD);
	hash_key.dsize = sizeof(TDB_MAGIC_FOOD);
	*magic1_hash = tdb->hash_fn(&hash_key);

	hash_key.dptr = (unsigned char *)CONVERT(tdb_magic);
	hash_key.dsize = sizeof(tdb_magic);
	*magic2_hash = tdb->hash_fn(&hash_key);

	/* Make sure at least one hash is non-zero! */
	if (*magic1_hash == 0 && *magic2_hash == 0)
		*magic1_hash = 1;
}

/* initialise a new database with a specified hash size */
static int tdb_new_database(struct tdb_context *tdb, int hash_size)
{
	struct tdb_header *newdb;
	size_t size;
	int ret = -1;

	/* We make it up in memory, then write it out if not internal */
	size = sizeof(struct tdb_header) + (hash_size+1)*sizeof(tdb_off_t);
	if (!(newdb = (struct tdb_header *)calloc(size, 1))) {
		tdb->ecode = TDB_ERR_OOM;
		return -1;
	}

	/* Fill in the header */
	newdb->version = TDB_VERSION;
	newdb->hash_size = hash_size;

	tdb_header_hash(tdb, &newdb->magic1_hash, &newdb->magic2_hash);

	/* Make sure older tdbs (which don't check the magic hash fields)
	 * will refuse to open this TDB. */
	if (tdb->flags & TDB_INCOMPATIBLE_HASH)
		newdb->rwlocks = TDB_HASH_RWLOCK_MAGIC;

	if (tdb->flags & TDB_INTERNAL) {
		tdb->map_size = size;
		tdb->map_ptr = (char *)newdb;
		memcpy(&tdb->header, newdb, sizeof(tdb->header));
		/* Convert the `ondisk' version if asked. */
		CONVERT(*newdb);
		return 0;
	}
	if (lseek(tdb->fd, 0, SEEK_SET) == -1)
		goto fail;

	if (ftruncate(tdb->fd, 0) == -1)
		goto fail;

	/* This creates an endian-converted header, as if read from disk */
	CONVERT(*newdb);
	memcpy(&tdb->header, newdb, sizeof(tdb->header));
	/* Don't endian-convert the magic food! */
	memcpy(newdb->magic_food, TDB_MAGIC_FOOD, strlen(TDB_MAGIC_FOOD)+1);
	/* we still have "ret == -1" here */
	if (tdb_write_all(tdb->fd, newdb, size))
		ret = 0;

  fail:
	SAFE_FREE(newdb);
	return ret;
}



static int tdb_already_open(dev_t device,
			    ino_t ino)
{
	struct tdb_context *i;

	for (i = tdbs; i; i = i->next) {
		if (i->device == device && i->inode == ino) {
			return 1;
		}
	}

	return 0;
}

/* open the database, creating it if necessary 

   The open_flags and mode are passed straight to the open call on the
   database file. A flags value of O_WRONLY is invalid. The hash size
   is advisory, use zero for a default value.

   Return is NULL on error, in which case errno is also set.  Don't 
   try to call tdb_error or tdb_errname, just do strerror(errno).

   @param name may be NULL for internal databases. */
_PUBLIC_ struct tdb_context *tdb_open(const char *name, int hash_size, int tdb_flags,
		      int open_flags, mode_t mode)
{
	return tdb_open_ex(name, hash_size, tdb_flags, open_flags, mode, NULL, NULL);
}

/* a default logging function */
static void null_log_fn(struct tdb_context *tdb, enum tdb_debug_level level, const char *fmt, ...) PRINTF_ATTRIBUTE(3, 4);
static void null_log_fn(struct tdb_context *tdb, enum tdb_debug_level level, const char *fmt, ...)
{
}

static bool check_header_hash(struct tdb_context *tdb,
			      bool default_hash, uint32_t *m1, uint32_t *m2)
{
	tdb_header_hash(tdb, m1, m2);
	if (tdb->header.magic1_hash == *m1 &&
	    tdb->header.magic2_hash == *m2) {
		return true;
	}

	/* If they explicitly set a hash, always respect it. */
	if (!default_hash)
		return false;

	/* Otherwise, try the other inbuilt hash. */
	if (tdb->hash_fn == tdb_old_hash)
		tdb->hash_fn = tdb_jenkins_hash;
	else
		tdb->hash_fn = tdb_old_hash;
	return check_header_hash(tdb, false, m1, m2);
}

_PUBLIC_ struct tdb_context *tdb_open_ex(const char *name, int hash_size, int tdb_flags,
				int open_flags, mode_t mode,
				const struct tdb_logging_context *log_ctx,
				tdb_hash_func hash_fn)
{
	struct tdb_context *tdb;
	struct stat st;
	int rev = 0, locked = 0;
	unsigned char *vp;
	uint32_t vertest;
	unsigned v;
	const char *hash_alg;
	uint32_t magic1, magic2;

	if (!(tdb = (struct tdb_context *)calloc(1, sizeof *tdb))) {
		/* Can't log this */
		errno = ENOMEM;
		goto fail;
	}
	tdb_io_init(tdb);
	tdb->fd = -1;
#ifdef TDB_TRACE
	tdb->tracefd = -1;
#endif
	tdb->name = NULL;
	tdb->map_ptr = NULL;
	tdb->flags = tdb_flags;
	tdb->open_flags = open_flags;
	if (log_ctx) {
		tdb->log = *log_ctx;
	} else {
		tdb->log.log_fn = null_log_fn;
		tdb->log.log_private = NULL;
	}

	if (name == NULL && (tdb_flags & TDB_INTERNAL)) {
		name = "__TDB_INTERNAL__";
	}

	if (name == NULL) {
		tdb->name = discard_const_p(char, "__NULL__");
		TDB_LOG((tdb, TDB_DEBUG_FATAL, "tdb_open_ex: called with name == NULL\n"));
		tdb->name = NULL;
		errno = EINVAL;
		goto fail;
	}

	/* now make a copy of the name, as the caller memory might went away */
	if (!(tdb->name = (char *)strdup(name))) {
		/*
		 * set the name as the given string, so that tdb_name() will
		 * work in case of an error.
		 */
		tdb->name = discard_const_p(char, name);
		TDB_LOG((tdb, TDB_DEBUG_ERROR, "tdb_open_ex: can't strdup(%s)\n",
			 name));
		tdb->name = NULL;
		errno = ENOMEM;
		goto fail;
	}

	if (hash_fn) {
		tdb->hash_fn = hash_fn;
		hash_alg = "the user defined";
	} else {
		/* This controls what we use when creating a tdb. */
		if (tdb->flags & TDB_INCOMPATIBLE_HASH) {
			tdb->hash_fn = tdb_jenkins_hash;
		} else {
			tdb->hash_fn = tdb_old_hash;
		}
		hash_alg = "either default";
	}

	/* cache the page size */
	tdb->page_size = getpagesize();
	if (tdb->page_size <= 0) {
		tdb->page_size = 0x2000;
	}

	tdb->max_dead_records = (tdb_flags & TDB_VOLATILE) ? 5 : 0;

	if ((open_flags & O_ACCMODE) == O_WRONLY) {
		TDB_LOG((tdb, TDB_DEBUG_ERROR, "tdb_open_ex: can't open tdb %s write-only\n",
			 name));
		errno = EINVAL;
		goto fail;
	}

	if (hash_size == 0)
		hash_size = DEFAULT_HASH_SIZE;
	if ((open_flags & O_ACCMODE) == O_RDONLY) {
		tdb->read_only = 1;
		/* read only databases don't do locking or clear if first */
		tdb->flags |= TDB_NOLOCK;
		tdb->flags &= ~TDB_CLEAR_IF_FIRST;
	}

	if ((tdb->flags & TDB_ALLOW_NESTING) &&
	    (tdb->flags & TDB_DISALLOW_NESTING)) {
		tdb->ecode = TDB_ERR_NESTING;
		TDB_LOG((tdb, TDB_DEBUG_FATAL, "tdb_open_ex: "
			"allow_nesting and disallow_nesting are not allowed together!"));
		errno = EINVAL;
		goto fail;
	}

	if (getenv("TDB_NO_FSYNC")) {
		tdb->flags |= TDB_NOSYNC;
	}

	/*
	 * TDB_ALLOW_NESTING is the default behavior.
	 * Note: this may change in future versions!
	 */
	if (!(tdb->flags & TDB_DISALLOW_NESTING)) {
		tdb->flags |= TDB_ALLOW_NESTING;
	}

	/* internal databases don't mmap or lock, and start off cleared */
	if (tdb->flags & TDB_INTERNAL) {
		tdb->flags |= (TDB_NOLOCK | TDB_NOMMAP);
		tdb->flags &= ~TDB_CLEAR_IF_FIRST;
		if (tdb_new_database(tdb, hash_size) != 0) {
			TDB_LOG((tdb, TDB_DEBUG_ERROR, "tdb_open_ex: tdb_new_database failed!"));
			goto fail;
		}
		goto internal;
	}

	if ((tdb->fd = open(name, open_flags, mode)) == -1) {
		TDB_LOG((tdb, TDB_DEBUG_WARNING, "tdb_open_ex: could not open file %s: %s\n",
			 name, strerror(errno)));
		goto fail;	/* errno set by open(2) */
	}

	/* on exec, don't inherit the fd */
	v = fcntl(tdb->fd, F_GETFD, 0);
        fcntl(tdb->fd, F_SETFD, v | FD_CLOEXEC);

	/* ensure there is only one process initialising at once */
	if (tdb_nest_lock(tdb, OPEN_LOCK, F_WRLCK, TDB_LOCK_WAIT) == -1) {
		TDB_LOG((tdb, TDB_DEBUG_ERROR, "tdb_open_ex: failed to get open lock on %s: %s\n",
			 name, strerror(errno)));
		goto fail;	/* errno set by tdb_brlock */
	}

	/* we need to zero database if we are the only one with it open */
	if ((tdb_flags & TDB_CLEAR_IF_FIRST) &&
	    (!tdb->read_only) &&
	    (locked = (tdb_nest_lock(tdb, ACTIVE_LOCK, F_WRLCK, TDB_LOCK_NOWAIT|TDB_LOCK_PROBE) == 0))) {
		int ret;
		ret = tdb_brlock(tdb, F_WRLCK, FREELIST_TOP, 0,
				TDB_LOCK_WAIT);
		if (ret == -1) {
			TDB_LOG((tdb, TDB_DEBUG_FATAL, "tdb_open_ex: "
				"tdb_brlock failed for %s: %s\n",
				name, strerror(errno)));
			goto fail;
		}
		ret = tdb_new_database(tdb, hash_size);
		if (ret == -1) {
			TDB_LOG((tdb, TDB_DEBUG_FATAL, "tdb_open_ex: "
				"tdb_new_database failed for %s: %s\n",
				name, strerror(errno)));
			tdb_unlockall(tdb);
			goto fail;
		}
		ret = tdb_brunlock(tdb, F_WRLCK, FREELIST_TOP, 0);
		if (ret == -1) {
			TDB_LOG((tdb, TDB_DEBUG_FATAL, "tdb_open_ex: "
				"tdb_unlockall failed for %s: %s\n",
				name, strerror(errno)));
			goto fail;
		}
		ret = lseek(tdb->fd, 0, SEEK_SET);
		if (ret == -1) {
			TDB_LOG((tdb, TDB_DEBUG_FATAL, "tdb_open_ex: "
				"lseek failed for %s: %s\n",
				name, strerror(errno)));
			goto fail;
		}
	}

	errno = 0;
	if (read(tdb->fd, &tdb->header, sizeof(tdb->header)) != sizeof(tdb->header)
	    || strcmp(tdb->header.magic_food, TDB_MAGIC_FOOD) != 0) {
		if (!(open_flags & O_CREAT) || tdb_new_database(tdb, hash_size) == -1) {
			if (errno == 0) {
				errno = EIO; /* ie bad format or something */
			}
			goto fail;
		}
		rev = (tdb->flags & TDB_CONVERT);
	} else if (tdb->header.version != TDB_VERSION
		   && !(rev = (tdb->header.version==TDB_BYTEREV(TDB_VERSION)))) {
		/* wrong version */
		errno = EIO;
		goto fail;
	}
	vp = (unsigned char *)&tdb->header.version;
	vertest = (((uint32_t)vp[0]) << 24) | (((uint32_t)vp[1]) << 16) |
		  (((uint32_t)vp[2]) << 8) | (uint32_t)vp[3];
	tdb->flags |= (vertest==TDB_VERSION) ? TDB_BIGENDIAN : 0;
	if (!rev)
		tdb->flags &= ~TDB_CONVERT;
	else {
		tdb->flags |= TDB_CONVERT;
		tdb_convert(&tdb->header, sizeof(tdb->header));
	}
	if (fstat(tdb->fd, &st) == -1)
		goto fail;

	if (tdb->header.rwlocks != 0 &&
	    tdb->header.rwlocks != TDB_HASH_RWLOCK_MAGIC) {
		TDB_LOG((tdb, TDB_DEBUG_ERROR, "tdb_open_ex: spinlocks no longer supported\n"));
		goto fail;
	}

	if ((tdb->header.magic1_hash == 0) && (tdb->header.magic2_hash == 0)) {
		/* older TDB without magic hash references */
		tdb->hash_fn = tdb_old_hash;
	} else if (!check_header_hash(tdb, !hash_fn, &magic1, &magic2)) {
		TDB_LOG((tdb, TDB_DEBUG_FATAL, "tdb_open_ex: "
			 "%s was not created with %s hash function we are using\n"
			 "magic1_hash[0x%08X %s 0x%08X] "
			 "magic2_hash[0x%08X %s 0x%08X]\n",
			 name, hash_alg,
			 tdb->header.magic1_hash,
			 (tdb->header.magic1_hash == magic1) ? "==" : "!=",
			 magic1,
			 tdb->header.magic2_hash,
			 (tdb->header.magic2_hash == magic2) ? "==" : "!=",
			 magic2));
		errno = EINVAL;
		goto fail;
	}

	/* Is it already in the open list?  If so, fail. */
	if (tdb_already_open(st.st_dev, st.st_ino)) {
		TDB_LOG((tdb, TDB_DEBUG_ERROR, "tdb_open_ex: "
			 "%s (%d,%d) is already open in this process\n",
			 name, (int)st.st_dev, (int)st.st_ino));
		errno = EBUSY;
		goto fail;
	}

	tdb->map_size = st.st_size;
	tdb->device = st.st_dev;
	tdb->inode = st.st_ino;
	tdb_mmap(tdb);
	if (locked) {
		if (tdb_nest_unlock(tdb, ACTIVE_LOCK, F_WRLCK, false) == -1) {
			TDB_LOG((tdb, TDB_DEBUG_ERROR, "tdb_open_ex: "
				 "failed to release ACTIVE_LOCK on %s: %s\n",
				 name, strerror(errno)));
			goto fail;
		}

	}

	/* We always need to do this if the CLEAR_IF_FIRST flag is set, even if
	   we didn't get the initial exclusive lock as we need to let all other
	   users know we're using it. */

	if (tdb_flags & TDB_CLEAR_IF_FIRST) {
		/* leave this lock in place to indicate it's in use */
		if (tdb_nest_lock(tdb, ACTIVE_LOCK, F_RDLCK, TDB_LOCK_WAIT) == -1) {
			goto fail;
		}
	}

	/* if needed, run recovery */
	if (tdb_transaction_recover(tdb) == -1) {
		goto fail;
	}

#ifdef TDB_TRACE
	{
		char tracefile[strlen(name) + 32];

		snprintf(tracefile, sizeof(tracefile),
			 "%s.trace.%li", name, (long)getpid());
		tdb->tracefd = open(tracefile, O_WRONLY|O_CREAT|O_EXCL, 0600);
		if (tdb->tracefd >= 0) {
			tdb_enable_seqnum(tdb);
			tdb_trace_open(tdb, "tdb_open", hash_size, tdb_flags,
				       open_flags);
		} else
			TDB_LOG((tdb, TDB_DEBUG_ERROR, "tdb_open_ex: failed to open trace file %s!\n", tracefile));
	}
#endif

 internal:
	/* Internal (memory-only) databases skip all the code above to
	 * do with disk files, and resume here by releasing their
	 * open lock and hooking into the active list. */
	if (tdb_nest_unlock(tdb, OPEN_LOCK, F_WRLCK, false) == -1) {
		goto fail;
	}
	tdb->next = tdbs;
	tdbs = tdb;
	return tdb;

 fail:
	{ int save_errno = errno;

	if (!tdb)
		return NULL;

#ifdef TDB_TRACE
	close(tdb->tracefd);
#endif
	if (tdb->map_ptr) {
		if (tdb->flags & TDB_INTERNAL)
			SAFE_FREE(tdb->map_ptr);
		else
			tdb_munmap(tdb);
	}
	if (tdb->fd != -1)
		if (close(tdb->fd) != 0)
			TDB_LOG((tdb, TDB_DEBUG_ERROR, "tdb_open_ex: failed to close tdb->fd on error!\n"));
	SAFE_FREE(tdb->lockrecs);
	SAFE_FREE(tdb->name);
	SAFE_FREE(tdb);
	errno = save_errno;
	return NULL;
	}
}

/*
 * Set the maximum number of dead records per hash chain
 */

_PUBLIC_ void tdb_set_max_dead(struct tdb_context *tdb, int max_dead)
{
	tdb->max_dead_records = max_dead;
}

/**
 * Close a database.
 *
 * @returns -1 for error; 0 for success.
 **/
_PUBLIC_ int tdb_close(struct tdb_context *tdb)
{
	struct tdb_context **i;
	int ret = 0;

	if (tdb->transaction) {
		tdb_transaction_cancel(tdb);
	}
	tdb_trace(tdb, "tdb_close");

	if (tdb->map_ptr) {
		if (tdb->flags & TDB_INTERNAL)
			SAFE_FREE(tdb->map_ptr);
		else
			tdb_munmap(tdb);
	}
	SAFE_FREE(tdb->name);
	if (tdb->fd != -1) {
		ret = close(tdb->fd);
		tdb->fd = -1;
	}
	SAFE_FREE(tdb->lockrecs);

	/* Remove from contexts list */
	for (i = &tdbs; *i; i = &(*i)->next) {
		if (*i == tdb) {
			*i = tdb->next;
			break;
		}
	}

#ifdef TDB_TRACE
	close(tdb->tracefd);
#endif
	memset(tdb, 0, sizeof(*tdb));
	SAFE_FREE(tdb);

	return ret;
}

/* register a loging function */
_PUBLIC_ void tdb_set_logging_function(struct tdb_context *tdb,
                                       const struct tdb_logging_context *log_ctx)
{
        tdb->log = *log_ctx;
}

_PUBLIC_ void *tdb_get_logging_private(struct tdb_context *tdb)
{
	return tdb->log.log_private;
}

static int tdb_reopen_internal(struct tdb_context *tdb, bool active_lock)
{
#if !defined(LIBREPLACE_PREAD_NOT_REPLACED) || \
	!defined(LIBREPLACE_PWRITE_NOT_REPLACED)
	struct stat st;
#endif

	if (tdb->flags & TDB_INTERNAL) {
		return 0; /* Nothing to do. */
	}

	if (tdb_have_extra_locks(tdb)) {
		TDB_LOG((tdb, TDB_DEBUG_ERROR, "tdb_reopen: reopen not allowed with locks held\n"));
		goto fail;
	}

	if (tdb->transaction != 0) {
		TDB_LOG((tdb, TDB_DEBUG_ERROR, "tdb_reopen: reopen not allowed inside a transaction\n"));
		goto fail;
	}

/* If we have real pread & pwrite, we can skip reopen. */
#if !defined(LIBREPLACE_PREAD_NOT_REPLACED) || \
	!defined(LIBREPLACE_PWRITE_NOT_REPLACED)
	if (tdb_munmap(tdb) != 0) {
		TDB_LOG((tdb, TDB_DEBUG_FATAL, "tdb_reopen: munmap failed (%s)\n", strerror(errno)));
		goto fail;
	}
	if (close(tdb->fd) != 0)
		TDB_LOG((tdb, TDB_DEBUG_FATAL, "tdb_reopen: WARNING closing tdb->fd failed!\n"));
	tdb->fd = open(tdb->name, tdb->open_flags & ~(O_CREAT|O_TRUNC), 0);
	if (tdb->fd == -1) {
		TDB_LOG((tdb, TDB_DEBUG_FATAL, "tdb_reopen: open failed (%s)\n", strerror(errno)));
		goto fail;
	}
	if (fstat(tdb->fd, &st) != 0) {
		TDB_LOG((tdb, TDB_DEBUG_FATAL, "tdb_reopen: fstat failed (%s)\n", strerror(errno)));
		goto fail;
	}
	if (st.st_ino != tdb->inode || st.st_dev != tdb->device) {
		TDB_LOG((tdb, TDB_DEBUG_FATAL, "tdb_reopen: file dev/inode has changed!\n"));
		goto fail;
	}
	tdb_mmap(tdb);
#endif /* fake pread or pwrite */

	/* We may still think we hold the active lock. */
	tdb->num_lockrecs = 0;
	SAFE_FREE(tdb->lockrecs);

	if (active_lock && tdb_nest_lock(tdb, ACTIVE_LOCK, F_RDLCK, TDB_LOCK_WAIT) == -1) {
		TDB_LOG((tdb, TDB_DEBUG_FATAL, "tdb_reopen: failed to obtain active lock\n"));
		goto fail;
	}

	return 0;

fail:
	tdb_close(tdb);
	return -1;
}

/* reopen a tdb - this can be used after a fork to ensure that we have an independent
   seek pointer from our parent and to re-establish locks */
_PUBLIC_ int tdb_reopen(struct tdb_context *tdb)
{
	return tdb_reopen_internal(tdb, tdb->flags & TDB_CLEAR_IF_FIRST);
}

/* reopen all tdb's */
_PUBLIC_ int tdb_reopen_all(int parent_longlived)
{
	struct tdb_context *tdb;

	for (tdb=tdbs; tdb; tdb = tdb->next) {
		bool active_lock = (tdb->flags & TDB_CLEAR_IF_FIRST);

		/*
		 * If the parent is longlived (ie. a
		 * parent daemon architecture), we know
		 * it will keep it's active lock on a
		 * tdb opened with CLEAR_IF_FIRST. Thus
		 * for child processes we don't have to
		 * add an active lock. This is essential
		 * to improve performance on systems that
		 * keep POSIX locks as a non-scalable data
		 * structure in the kernel.
		 */
		if (parent_longlived) {
			/* Ensure no clear-if-first. */
			active_lock = false;
		}

		if (tdb_reopen_internal(tdb, active_lock) != 0)
			return -1;
	}

	return 0;
}
