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

_PUBLIC_ TDB_DATA tdb_null;

/*
  non-blocking increment of the tdb sequence number if the tdb has been opened using
  the TDB_SEQNUM flag
*/
_PUBLIC_ void tdb_increment_seqnum_nonblock(struct tdb_context *tdb)
{
	tdb_off_t seqnum=0;

	if (!(tdb->flags & TDB_SEQNUM)) {
		return;
	}

	/* we ignore errors from this, as we have no sane way of
	   dealing with them.
	*/
	tdb_ofs_read(tdb, TDB_SEQNUM_OFS, &seqnum);
	seqnum++;
	tdb_ofs_write(tdb, TDB_SEQNUM_OFS, &seqnum);
}

/*
  increment the tdb sequence number if the tdb has been opened using
  the TDB_SEQNUM flag
*/
static void tdb_increment_seqnum(struct tdb_context *tdb)
{
	if (!(tdb->flags & TDB_SEQNUM)) {
		return;
	}

	if (tdb_nest_lock(tdb, TDB_SEQNUM_OFS, F_WRLCK,
			  TDB_LOCK_WAIT|TDB_LOCK_PROBE) != 0) {
		return;
	}

	tdb_increment_seqnum_nonblock(tdb);

	tdb_nest_unlock(tdb, TDB_SEQNUM_OFS, F_WRLCK, false);
}

static int tdb_key_compare(TDB_DATA key, TDB_DATA data, void *private_data)
{
	return memcmp(data.dptr, key.dptr, data.dsize);
}

/* Returns 0 on fail.  On success, return offset of record, and fills
   in rec */
static tdb_off_t tdb_find(struct tdb_context *tdb, TDB_DATA key, uint32_t hash,
			struct tdb_record *r)
{
	tdb_off_t rec_ptr;

	/* read in the hash top */
	if (tdb_ofs_read(tdb, TDB_HASH_TOP(hash), &rec_ptr) == -1)
		return 0;

	/* keep looking until we find the right record */
	while (rec_ptr) {
		if (tdb_rec_read(tdb, rec_ptr, r) == -1)
			return 0;

		if (!TDB_DEAD(r) && hash==r->full_hash
		    && key.dsize==r->key_len
		    && tdb_parse_data(tdb, key, rec_ptr + sizeof(*r),
				      r->key_len, tdb_key_compare,
				      NULL) == 0) {
			return rec_ptr;
		}
		/* detect tight infinite loop */
		if (rec_ptr == r->next) {
			tdb->ecode = TDB_ERR_CORRUPT;
			TDB_LOG((tdb, TDB_DEBUG_FATAL, "tdb_find: loop detected.\n"));
			return 0;
		}
		rec_ptr = r->next;
	}
	tdb->ecode = TDB_ERR_NOEXIST;
	return 0;
}

/* As tdb_find, but if you succeed, keep the lock */
tdb_off_t tdb_find_lock_hash(struct tdb_context *tdb, TDB_DATA key, uint32_t hash, int locktype,
			   struct tdb_record *rec)
{
	uint32_t rec_ptr;

	if (tdb_lock(tdb, BUCKET(hash), locktype) == -1)
		return 0;
	if (!(rec_ptr = tdb_find(tdb, key, hash, rec)))
		tdb_unlock(tdb, BUCKET(hash), locktype);
	return rec_ptr;
}

static TDB_DATA _tdb_fetch(struct tdb_context *tdb, TDB_DATA key);

/* update an entry in place - this only works if the new data size
   is <= the old data size and the key exists.
   on failure return -1.
*/
static int tdb_update_hash(struct tdb_context *tdb, TDB_DATA key, uint32_t hash, TDB_DATA dbuf)
{
	struct tdb_record rec;
	tdb_off_t rec_ptr;

	/* find entry */
	if (!(rec_ptr = tdb_find(tdb, key, hash, &rec)))
		return -1;

	/* it could be an exact duplicate of what is there - this is
	 * surprisingly common (eg. with a ldb re-index). */
	if (rec.key_len == key.dsize && 
	    rec.data_len == dbuf.dsize &&
	    rec.full_hash == hash) {
		TDB_DATA data = _tdb_fetch(tdb, key);
		if (data.dsize == dbuf.dsize &&
		    memcmp(data.dptr, dbuf.dptr, data.dsize) == 0) {
			if (data.dptr) {
				free(data.dptr);
			}
			return 0;
		}
		if (data.dptr) {
			free(data.dptr);
		}
	}

	/* must be long enough key, data and tailer */
	if (rec.rec_len < key.dsize + dbuf.dsize + sizeof(tdb_off_t)) {
		tdb->ecode = TDB_SUCCESS; /* Not really an error */
		return -1;
	}

	if (tdb->methods->tdb_write(tdb, rec_ptr + sizeof(rec) + rec.key_len,
		      dbuf.dptr, dbuf.dsize) == -1)
		return -1;

	if (dbuf.dsize != rec.data_len) {
		/* update size */
		rec.data_len = dbuf.dsize;
		return tdb_rec_write(tdb, rec_ptr, &rec);
	}

	return 0;
}

/* find an entry in the database given a key */
/* If an entry doesn't exist tdb_err will be set to
 * TDB_ERR_NOEXIST. If a key has no data attached
 * then the TDB_DATA will have zero length but
 * a non-zero pointer
 */
static TDB_DATA _tdb_fetch(struct tdb_context *tdb, TDB_DATA key)
{
	tdb_off_t rec_ptr;
	struct tdb_record rec;
	TDB_DATA ret;
	uint32_t hash;

	/* find which hash bucket it is in */
	hash = tdb->hash_fn(&key);
	if (!(rec_ptr = tdb_find_lock_hash(tdb,key,hash,F_RDLCK,&rec)))
		return tdb_null;

	ret.dptr = tdb_alloc_read(tdb, rec_ptr + sizeof(rec) + rec.key_len,
				  rec.data_len);
	ret.dsize = rec.data_len;
	tdb_unlock(tdb, BUCKET(rec.full_hash), F_RDLCK);
	return ret;
}

_PUBLIC_ TDB_DATA tdb_fetch(struct tdb_context *tdb, TDB_DATA key)
{
	TDB_DATA ret = _tdb_fetch(tdb, key);

	tdb_trace_1rec_retrec(tdb, "tdb_fetch", key, ret);
	return ret;
}

/*
 * Find an entry in the database and hand the record's data to a parsing
 * function. The parsing function is executed under the chain read lock, so it
 * should be fast and should not block on other syscalls.
 *
 * DON'T CALL OTHER TDB CALLS FROM THE PARSER, THIS MIGHT LEAD TO SEGFAULTS.
 *
 * For mmapped tdb's that do not have a transaction open it points the parsing
 * function directly at the mmap area, it avoids the malloc/memcpy in this
 * case. If a transaction is open or no mmap is available, it has to do
 * malloc/read/parse/free.
 *
 * This is interesting for all readers of potentially large data structures in
 * the tdb records, ldb indexes being one example.
 *
 * Return -1 if the record was not found.
 */

_PUBLIC_ int tdb_parse_record(struct tdb_context *tdb, TDB_DATA key,
		     int (*parser)(TDB_DATA key, TDB_DATA data,
				   void *private_data),
		     void *private_data)
{
	tdb_off_t rec_ptr;
	struct tdb_record rec;
	int ret;
	uint32_t hash;

	/* find which hash bucket it is in */
	hash = tdb->hash_fn(&key);

	if (!(rec_ptr = tdb_find_lock_hash(tdb,key,hash,F_RDLCK,&rec))) {
		/* record not found */
		tdb_trace_1rec_ret(tdb, "tdb_parse_record", key, -1);
		tdb->ecode = TDB_ERR_NOEXIST;
		return -1;
	}
	tdb_trace_1rec_ret(tdb, "tdb_parse_record", key, 0);

	ret = tdb_parse_data(tdb, key, rec_ptr + sizeof(rec) + rec.key_len,
			     rec.data_len, parser, private_data);

	tdb_unlock(tdb, BUCKET(rec.full_hash), F_RDLCK);

	return ret;
}

/* check if an entry in the database exists 

   note that 1 is returned if the key is found and 0 is returned if not found
   this doesn't match the conventions in the rest of this module, but is
   compatible with gdbm
*/
static int tdb_exists_hash(struct tdb_context *tdb, TDB_DATA key, uint32_t hash)
{
	struct tdb_record rec;

	if (tdb_find_lock_hash(tdb, key, hash, F_RDLCK, &rec) == 0)
		return 0;
	tdb_unlock(tdb, BUCKET(rec.full_hash), F_RDLCK);
	return 1;
}

_PUBLIC_ int tdb_exists(struct tdb_context *tdb, TDB_DATA key)
{
	uint32_t hash = tdb->hash_fn(&key);
	int ret;

	ret = tdb_exists_hash(tdb, key, hash);
	tdb_trace_1rec_ret(tdb, "tdb_exists", key, ret);
	return ret;
}

/* actually delete an entry in the database given the offset */
int tdb_do_delete(struct tdb_context *tdb, tdb_off_t rec_ptr, struct tdb_record *rec)
{
	tdb_off_t last_ptr, i;
	struct tdb_record lastrec;

	if (tdb->read_only || tdb->traverse_read) return -1;

	if (((tdb->traverse_write != 0) && (!TDB_DEAD(rec))) ||
	    tdb_write_lock_record(tdb, rec_ptr) == -1) {
		/* Someone traversing here: mark it as dead */
		rec->magic = TDB_DEAD_MAGIC;
		return tdb_rec_write(tdb, rec_ptr, rec);
	}
	if (tdb_write_unlock_record(tdb, rec_ptr) != 0)
		return -1;

	/* find previous record in hash chain */
	if (tdb_ofs_read(tdb, TDB_HASH_TOP(rec->full_hash), &i) == -1)
		return -1;
	for (last_ptr = 0; i != rec_ptr; last_ptr = i, i = lastrec.next)
		if (tdb_rec_read(tdb, i, &lastrec) == -1)
			return -1;

	/* unlink it: next ptr is at start of record. */
	if (last_ptr == 0)
		last_ptr = TDB_HASH_TOP(rec->full_hash);
	if (tdb_ofs_write(tdb, last_ptr, &rec->next) == -1)
		return -1;

	/* recover the space */
	if (tdb_free(tdb, rec_ptr, rec) == -1)
		return -1;
	return 0;
}

static int tdb_count_dead(struct tdb_context *tdb, uint32_t hash)
{
	int res = 0;
	tdb_off_t rec_ptr;
	struct tdb_record rec;

	/* read in the hash top */
	if (tdb_ofs_read(tdb, TDB_HASH_TOP(hash), &rec_ptr) == -1)
		return 0;

	while (rec_ptr) {
		if (tdb_rec_read(tdb, rec_ptr, &rec) == -1)
			return 0;

		if (rec.magic == TDB_DEAD_MAGIC) {
			res += 1;
		}
		rec_ptr = rec.next;
	}
	return res;
}

/*
 * Purge all DEAD records from a hash chain
 */
static int tdb_purge_dead(struct tdb_context *tdb, uint32_t hash)
{
	int res = -1;
	struct tdb_record rec;
	tdb_off_t rec_ptr;

	if (tdb_lock(tdb, -1, F_WRLCK) == -1) {
		return -1;
	}

	/* read in the hash top */
	if (tdb_ofs_read(tdb, TDB_HASH_TOP(hash), &rec_ptr) == -1)
		goto fail;

	while (rec_ptr) {
		tdb_off_t next;

		if (tdb_rec_read(tdb, rec_ptr, &rec) == -1) {
			goto fail;
		}

		next = rec.next;

		if (rec.magic == TDB_DEAD_MAGIC
		    && tdb_do_delete(tdb, rec_ptr, &rec) == -1) {
			goto fail;
		}
		rec_ptr = next;
	}
	res = 0;
 fail:
	tdb_unlock(tdb, -1, F_WRLCK);
	return res;
}

/* delete an entry in the database given a key */
static int tdb_delete_hash(struct tdb_context *tdb, TDB_DATA key, uint32_t hash)
{
	tdb_off_t rec_ptr;
	struct tdb_record rec;
	int ret;

	if (tdb->max_dead_records != 0) {

		/*
		 * Allow for some dead records per hash chain, mainly for
		 * tdb's with a very high create/delete rate like locking.tdb.
		 */

		if (tdb_lock(tdb, BUCKET(hash), F_WRLCK) == -1)
			return -1;

		if (tdb_count_dead(tdb, hash) >= tdb->max_dead_records) {
			/*
			 * Don't let the per-chain freelist grow too large,
			 * delete all existing dead records
			 */
			tdb_purge_dead(tdb, hash);
		}

		if (!(rec_ptr = tdb_find(tdb, key, hash, &rec))) {
			tdb_unlock(tdb, BUCKET(hash), F_WRLCK);
			return -1;
		}

		/*
		 * Just mark the record as dead.
		 */
		rec.magic = TDB_DEAD_MAGIC;
		ret = tdb_rec_write(tdb, rec_ptr, &rec);
	}
	else {
		if (!(rec_ptr = tdb_find_lock_hash(tdb, key, hash, F_WRLCK,
						   &rec)))
			return -1;

		ret = tdb_do_delete(tdb, rec_ptr, &rec);
	}

	if (ret == 0) {
		tdb_increment_seqnum(tdb);
	}

	if (tdb_unlock(tdb, BUCKET(rec.full_hash), F_WRLCK) != 0)
		TDB_LOG((tdb, TDB_DEBUG_WARNING, "tdb_delete: WARNING tdb_unlock failed!\n"));
	return ret;
}

_PUBLIC_ int tdb_delete(struct tdb_context *tdb, TDB_DATA key)
{
	uint32_t hash = tdb->hash_fn(&key);
	int ret;

	ret = tdb_delete_hash(tdb, key, hash);
	tdb_trace_1rec_ret(tdb, "tdb_delete", key, ret);
	return ret;
}

/*
 * See if we have a dead record around with enough space
 */
static tdb_off_t tdb_find_dead(struct tdb_context *tdb, uint32_t hash,
			       struct tdb_record *r, tdb_len_t length)
{
	tdb_off_t rec_ptr;

	/* read in the hash top */
	if (tdb_ofs_read(tdb, TDB_HASH_TOP(hash), &rec_ptr) == -1)
		return 0;

	/* keep looking until we find the right record */
	while (rec_ptr) {
		if (tdb_rec_read(tdb, rec_ptr, r) == -1)
			return 0;

		if (TDB_DEAD(r) && r->rec_len >= length) {
			/*
			 * First fit for simple coding, TODO: change to best
			 * fit
			 */
			return rec_ptr;
		}
		rec_ptr = r->next;
	}
	return 0;
}

static int _tdb_store(struct tdb_context *tdb, TDB_DATA key,
		       TDB_DATA dbuf, int flag, uint32_t hash)
{
	struct tdb_record rec;
	tdb_off_t rec_ptr;
	char *p = NULL;
	int ret = -1;

	/* check for it existing, on insert. */
	if (flag == TDB_INSERT) {
		if (tdb_exists_hash(tdb, key, hash)) {
			tdb->ecode = TDB_ERR_EXISTS;
			goto fail;
		}
	} else {
		/* first try in-place update, on modify or replace. */
		if (tdb_update_hash(tdb, key, hash, dbuf) == 0) {
			goto done;
		}
		if (tdb->ecode == TDB_ERR_NOEXIST &&
		    flag == TDB_MODIFY) {
			/* if the record doesn't exist and we are in TDB_MODIFY mode then
			 we should fail the store */
			goto fail;
		}
	}
	/* reset the error code potentially set by the tdb_update() */
	tdb->ecode = TDB_SUCCESS;

	/* delete any existing record - if it doesn't exist we don't
           care.  Doing this first reduces fragmentation, and avoids
           coalescing with `allocated' block before it's updated. */
	if (flag != TDB_INSERT)
		tdb_delete_hash(tdb, key, hash);

	/* Copy key+value *before* allocating free space in case malloc
	   fails and we are left with a dead spot in the tdb. */

	if (!(p = (char *)malloc(key.dsize + dbuf.dsize))) {
		tdb->ecode = TDB_ERR_OOM;
		goto fail;
	}

	memcpy(p, key.dptr, key.dsize);
	if (dbuf.dsize)
		memcpy(p+key.dsize, dbuf.dptr, dbuf.dsize);

	if (tdb->max_dead_records != 0) {
		/*
		 * Allow for some dead records per hash chain, look if we can
		 * find one that can hold the new record. We need enough space
		 * for key, data and tailer. If we find one, we don't have to
		 * consult the central freelist.
		 */
		rec_ptr = tdb_find_dead(
			tdb, hash, &rec,
			key.dsize + dbuf.dsize + sizeof(tdb_off_t));

		if (rec_ptr != 0) {
			rec.key_len = key.dsize;
			rec.data_len = dbuf.dsize;
			rec.full_hash = hash;
			rec.magic = TDB_MAGIC;
			if (tdb_rec_write(tdb, rec_ptr, &rec) == -1
			    || tdb->methods->tdb_write(
				    tdb, rec_ptr + sizeof(rec),
				    p, key.dsize + dbuf.dsize) == -1) {
				goto fail;
			}
			goto done;
		}
	}

	/*
	 * We have to allocate some space from the freelist, so this means we
	 * have to lock it. Use the chance to purge all the DEAD records from
	 * the hash chain under the freelist lock.
	 */

	if (tdb_lock(tdb, -1, F_WRLCK) == -1) {
		goto fail;
	}

	if ((tdb->max_dead_records != 0)
	    && (tdb_purge_dead(tdb, hash) == -1)) {
		tdb_unlock(tdb, -1, F_WRLCK);
		goto fail;
	}

	/* we have to allocate some space */
	rec_ptr = tdb_allocate(tdb, key.dsize + dbuf.dsize, &rec);

	tdb_unlock(tdb, -1, F_WRLCK);

	if (rec_ptr == 0) {
		goto fail;
	}

	/* Read hash top into next ptr */
	if (tdb_ofs_read(tdb, TDB_HASH_TOP(hash), &rec.next) == -1)
		goto fail;

	rec.key_len = key.dsize;
	rec.data_len = dbuf.dsize;
	rec.full_hash = hash;
	rec.magic = TDB_MAGIC;

	/* write out and point the top of the hash chain at it */
	if (tdb_rec_write(tdb, rec_ptr, &rec) == -1
	    || tdb->methods->tdb_write(tdb, rec_ptr+sizeof(rec), p, key.dsize+dbuf.dsize)==-1
	    || tdb_ofs_write(tdb, TDB_HASH_TOP(hash), &rec_ptr) == -1) {
		/* Need to tdb_unallocate() here */
		goto fail;
	}

 done:
	ret = 0;
 fail:
	if (ret == 0) {
		tdb_increment_seqnum(tdb);
	}

	SAFE_FREE(p); 
	return ret;
}

/* store an element in the database, replacing any existing element
   with the same key

   return 0 on success, -1 on failure
*/
_PUBLIC_ int tdb_store(struct tdb_context *tdb, TDB_DATA key, TDB_DATA dbuf, int flag)
{
	uint32_t hash;
	int ret;

	if (tdb->read_only || tdb->traverse_read) {
		tdb->ecode = TDB_ERR_RDONLY;
		tdb_trace_2rec_flag_ret(tdb, "tdb_store", key, dbuf, flag, -1);
		return -1;
	}

	/* find which hash bucket it is in */
	hash = tdb->hash_fn(&key);
	if (tdb_lock(tdb, BUCKET(hash), F_WRLCK) == -1)
		return -1;

	ret = _tdb_store(tdb, key, dbuf, flag, hash);
	tdb_trace_2rec_flag_ret(tdb, "tdb_store", key, dbuf, flag, ret);
	tdb_unlock(tdb, BUCKET(hash), F_WRLCK);
	return ret;
}

/* Append to an entry. Create if not exist. */
_PUBLIC_ int tdb_append(struct tdb_context *tdb, TDB_DATA key, TDB_DATA new_dbuf)
{
	uint32_t hash;
	TDB_DATA dbuf;
	int ret = -1;

	/* find which hash bucket it is in */
	hash = tdb->hash_fn(&key);
	if (tdb_lock(tdb, BUCKET(hash), F_WRLCK) == -1)
		return -1;

	dbuf = _tdb_fetch(tdb, key);

	if (dbuf.dptr == NULL) {
		dbuf.dptr = (unsigned char *)malloc(new_dbuf.dsize);
	} else {
		unsigned int new_len = dbuf.dsize + new_dbuf.dsize;
		unsigned char *new_dptr;

		/* realloc '0' is special: don't do that. */
		if (new_len == 0)
			new_len = 1;
		new_dptr = (unsigned char *)realloc(dbuf.dptr, new_len);
		if (new_dptr == NULL) {
			free(dbuf.dptr);
		}
		dbuf.dptr = new_dptr;
	}

	if (dbuf.dptr == NULL) {
		tdb->ecode = TDB_ERR_OOM;
		goto failed;
	}

	memcpy(dbuf.dptr + dbuf.dsize, new_dbuf.dptr, new_dbuf.dsize);
	dbuf.dsize += new_dbuf.dsize;

	ret = _tdb_store(tdb, key, dbuf, 0, hash);
	tdb_trace_2rec_retrec(tdb, "tdb_append", key, new_dbuf, dbuf);

failed:
	tdb_unlock(tdb, BUCKET(hash), F_WRLCK);
	SAFE_FREE(dbuf.dptr);
	return ret;
}


/*
  return the name of the current tdb file
  useful for external logging functions
*/
_PUBLIC_ const char *tdb_name(struct tdb_context *tdb)
{
	return tdb->name;
}

/*
  return the underlying file descriptor being used by tdb, or -1
  useful for external routines that want to check the device/inode
  of the fd
*/
_PUBLIC_ int tdb_fd(struct tdb_context *tdb)
{
	return tdb->fd;
}

/*
  return the current logging function
  useful for external tdb routines that wish to log tdb errors
*/
_PUBLIC_ tdb_log_func tdb_log_fn(struct tdb_context *tdb)
{
	return tdb->log.log_fn;
}


/*
  get the tdb sequence number. Only makes sense if the writers opened
  with TDB_SEQNUM set. Note that this sequence number will wrap quite
  quickly, so it should only be used for a 'has something changed'
  test, not for code that relies on the count of the number of changes
  made. If you want a counter then use a tdb record.

  The aim of this sequence number is to allow for a very lightweight
  test of a possible tdb change.
*/
_PUBLIC_ int tdb_get_seqnum(struct tdb_context *tdb)
{
	tdb_off_t seqnum=0;

	tdb_ofs_read(tdb, TDB_SEQNUM_OFS, &seqnum);
	return seqnum;
}

_PUBLIC_ int tdb_hash_size(struct tdb_context *tdb)
{
	return tdb->header.hash_size;
}

_PUBLIC_ size_t tdb_map_size(struct tdb_context *tdb)
{
	return tdb->map_size;
}

_PUBLIC_ int tdb_get_flags(struct tdb_context *tdb)
{
	return tdb->flags;
}

_PUBLIC_ void tdb_add_flags(struct tdb_context *tdb, unsigned flags)
{
	if ((flags & TDB_ALLOW_NESTING) &&
	    (flags & TDB_DISALLOW_NESTING)) {
		tdb->ecode = TDB_ERR_NESTING;
		TDB_LOG((tdb, TDB_DEBUG_FATAL, "tdb_add_flags: "
			"allow_nesting and disallow_nesting are not allowed together!"));
		return;
	}

	if (flags & TDB_ALLOW_NESTING) {
		tdb->flags &= ~TDB_DISALLOW_NESTING;
	}
	if (flags & TDB_DISALLOW_NESTING) {
		tdb->flags &= ~TDB_ALLOW_NESTING;
	}

	tdb->flags |= flags;
}

_PUBLIC_ void tdb_remove_flags(struct tdb_context *tdb, unsigned flags)
{
	if ((flags & TDB_ALLOW_NESTING) &&
	    (flags & TDB_DISALLOW_NESTING)) {
		tdb->ecode = TDB_ERR_NESTING;
		TDB_LOG((tdb, TDB_DEBUG_FATAL, "tdb_remove_flags: "
			"allow_nesting and disallow_nesting are not allowed together!"));
		return;
	}

	if (flags & TDB_ALLOW_NESTING) {
		tdb->flags |= TDB_DISALLOW_NESTING;
	}
	if (flags & TDB_DISALLOW_NESTING) {
		tdb->flags |= TDB_ALLOW_NESTING;
	}

	tdb->flags &= ~flags;
}


/*
  enable sequence number handling on an open tdb
*/
_PUBLIC_ void tdb_enable_seqnum(struct tdb_context *tdb)
{
	tdb->flags |= TDB_SEQNUM;
}


/*
  add a region of the file to the freelist. Length is the size of the region in bytes, 
  which includes the free list header that needs to be added
 */
static int tdb_free_region(struct tdb_context *tdb, tdb_off_t offset, ssize_t length)
{
	struct tdb_record rec;
	if (length <= sizeof(rec)) {
		/* the region is not worth adding */
		return 0;
	}
	if (length + offset > tdb->map_size) {
		TDB_LOG((tdb, TDB_DEBUG_FATAL,"tdb_free_region: adding region beyond end of file\n"));
		return -1;		
	}
	memset(&rec,'\0',sizeof(rec));
	rec.rec_len = length - sizeof(rec);
	if (tdb_free(tdb, offset, &rec) == -1) {
		TDB_LOG((tdb, TDB_DEBUG_FATAL,"tdb_free_region: failed to add free record\n"));
		return -1;
	}
	return 0;
}

/*
  wipe the entire database, deleting all records. This can be done
  very fast by using a allrecord lock. The entire data portion of the
  file becomes a single entry in the freelist.

  This code carefully steps around the recovery area, leaving it alone
 */
_PUBLIC_ int tdb_wipe_all(struct tdb_context *tdb)
{
	int i;
	tdb_off_t offset = 0;
	ssize_t data_len;
	tdb_off_t recovery_head;
	tdb_len_t recovery_size = 0;

	if (tdb_lockall(tdb) != 0) {
		return -1;
	}

	tdb_trace(tdb, "tdb_wipe_all");

	/* see if the tdb has a recovery area, and remember its size
	   if so. We don't want to lose this as otherwise each
	   tdb_wipe_all() in a transaction will increase the size of
	   the tdb by the size of the recovery area */
	if (tdb_ofs_read(tdb, TDB_RECOVERY_HEAD, &recovery_head) == -1) {
		TDB_LOG((tdb, TDB_DEBUG_FATAL, "tdb_wipe_all: failed to read recovery head\n"));
		goto failed;
	}

	if (recovery_head != 0) {
		struct tdb_record rec;
		if (tdb->methods->tdb_read(tdb, recovery_head, &rec, sizeof(rec), DOCONV()) == -1) {
			TDB_LOG((tdb, TDB_DEBUG_FATAL, "tdb_wipe_all: failed to read recovery record\n"));
			return -1;
		}	
		recovery_size = rec.rec_len + sizeof(rec);
	}

	/* wipe the hashes */
	for (i=0;i<tdb->header.hash_size;i++) {
		if (tdb_ofs_write(tdb, TDB_HASH_TOP(i), &offset) == -1) {
			TDB_LOG((tdb, TDB_DEBUG_FATAL,"tdb_wipe_all: failed to write hash %d\n", i));
			goto failed;
		}
	}

	/* wipe the freelist */
	if (tdb_ofs_write(tdb, FREELIST_TOP, &offset) == -1) {
		TDB_LOG((tdb, TDB_DEBUG_FATAL,"tdb_wipe_all: failed to write freelist\n"));
		goto failed;
	}

	/* add all the rest of the file to the freelist, possibly leaving a gap 
	   for the recovery area */
	if (recovery_size == 0) {
		/* the simple case - the whole file can be used as a freelist */
		data_len = (tdb->map_size - TDB_DATA_START(tdb->header.hash_size));
		if (tdb_free_region(tdb, TDB_DATA_START(tdb->header.hash_size), data_len) != 0) {
			goto failed;
		}
	} else {
		/* we need to add two freelist entries - one on either
		   side of the recovery area 

		   Note that we cannot shift the recovery area during
		   this operation. Only the transaction.c code may
		   move the recovery area or we risk subtle data
		   corruption
		*/
		data_len = (recovery_head - TDB_DATA_START(tdb->header.hash_size));
		if (tdb_free_region(tdb, TDB_DATA_START(tdb->header.hash_size), data_len) != 0) {
			goto failed;
		}
		/* and the 2nd free list entry after the recovery area - if any */
		data_len = tdb->map_size - (recovery_head+recovery_size);
		if (tdb_free_region(tdb, recovery_head+recovery_size, data_len) != 0) {
			goto failed;
		}
	}

	if (tdb_unlockall(tdb) != 0) {
		TDB_LOG((tdb, TDB_DEBUG_FATAL,"tdb_wipe_all: failed to unlock\n"));
		goto failed;
	}

	return 0;

failed:
	tdb_unlockall(tdb);
	return -1;
}

struct traverse_state {
	bool error;
	struct tdb_context *dest_db;
};

/*
  traverse function for repacking
 */
static int repack_traverse(struct tdb_context *tdb, TDB_DATA key, TDB_DATA data, void *private_data)
{
	struct traverse_state *state = (struct traverse_state *)private_data;
	if (tdb_store(state->dest_db, key, data, TDB_INSERT) != 0) {
		state->error = true;
		return -1;
	}
	return 0;
}

/*
  repack a tdb
 */
_PUBLIC_ int tdb_repack(struct tdb_context *tdb)
{
	struct tdb_context *tmp_db;
	struct traverse_state state;

	tdb_trace(tdb, "tdb_repack");

	if (tdb_transaction_start(tdb) != 0) {
		TDB_LOG((tdb, TDB_DEBUG_FATAL, __location__ " Failed to start transaction\n"));
		return -1;
	}

	tmp_db = tdb_open("tmpdb", tdb_hash_size(tdb), TDB_INTERNAL, O_RDWR|O_CREAT, 0);
	if (tmp_db == NULL) {
		TDB_LOG((tdb, TDB_DEBUG_FATAL, __location__ " Failed to create tmp_db\n"));
		tdb_transaction_cancel(tdb);
		return -1;
	}

	state.error = false;
	state.dest_db = tmp_db;

	if (tdb_traverse_read(tdb, repack_traverse, &state) == -1) {
		TDB_LOG((tdb, TDB_DEBUG_FATAL, __location__ " Failed to traverse copying out\n"));
		tdb_transaction_cancel(tdb);
		tdb_close(tmp_db);
		return -1;		
	}

	if (state.error) {
		TDB_LOG((tdb, TDB_DEBUG_FATAL, __location__ " Error during traversal\n"));
		tdb_transaction_cancel(tdb);
		tdb_close(tmp_db);
		return -1;
	}

	if (tdb_wipe_all(tdb) != 0) {
		TDB_LOG((tdb, TDB_DEBUG_FATAL, __location__ " Failed to wipe database\n"));
		tdb_transaction_cancel(tdb);
		tdb_close(tmp_db);
		return -1;
	}

	state.error = false;
	state.dest_db = tdb;

	if (tdb_traverse_read(tmp_db, repack_traverse, &state) == -1) {
		TDB_LOG((tdb, TDB_DEBUG_FATAL, __location__ " Failed to traverse copying back\n"));
		tdb_transaction_cancel(tdb);
		tdb_close(tmp_db);
		return -1;		
	}

	if (state.error) {
		TDB_LOG((tdb, TDB_DEBUG_FATAL, __location__ " Error during second traversal\n"));
		tdb_transaction_cancel(tdb);
		tdb_close(tmp_db);
		return -1;
	}

	tdb_close(tmp_db);

	if (tdb_transaction_commit(tdb) != 0) {
		TDB_LOG((tdb, TDB_DEBUG_FATAL, __location__ " Failed to commit\n"));
		return -1;
	}

	return 0;
}

/* Even on files, we can get partial writes due to signals. */
bool tdb_write_all(int fd, const void *buf, size_t count)
{
	while (count) {
		ssize_t ret;
		ret = write(fd, buf, count);
		if (ret < 0)
			return false;
		buf = (const char *)buf + ret;
		count -= ret;
	}
	return true;
}

#ifdef TDB_TRACE
static void tdb_trace_write(struct tdb_context *tdb, const char *str)
{
	if (!tdb_write_alltdb->tracefd, str, strlen(str)) {
		close(tdb->tracefd);
		tdb->tracefd = -1;
	}
}

static void tdb_trace_start(struct tdb_context *tdb)
{
	tdb_off_t seqnum=0;
	char msg[sizeof(tdb_off_t) * 4 + 1];

	tdb_ofs_read(tdb, TDB_SEQNUM_OFS, &seqnum);
	snprintf(msg, sizeof(msg), "%u ", seqnum);
	tdb_trace_write(tdb, msg);
}

static void tdb_trace_end(struct tdb_context *tdb)
{
	tdb_trace_write(tdb, "\n");
}

static void tdb_trace_end_ret(struct tdb_context *tdb, int ret)
{
	char msg[sizeof(ret) * 4 + 4];
	snprintf(msg, sizeof(msg), " = %i\n", ret);
	tdb_trace_write(tdb, msg);
}

static void tdb_trace_record(struct tdb_context *tdb, TDB_DATA rec)
{
	char msg[20 + rec.dsize*2], *p;
	unsigned int i;

	/* We differentiate zero-length records from non-existent ones. */
	if (rec.dptr == NULL) {
		tdb_trace_write(tdb, " NULL");
		return;
	}

	/* snprintf here is purely cargo-cult programming. */
	p = msg;
	p += snprintf(p, sizeof(msg), " %zu:", rec.dsize);
	for (i = 0; i < rec.dsize; i++)
		p += snprintf(p, 2, "%02x", rec.dptr[i]);

	tdb_trace_write(tdb, msg);
}

void tdb_trace(struct tdb_context *tdb, const char *op)
{
	tdb_trace_start(tdb);
	tdb_trace_write(tdb, op);
	tdb_trace_end(tdb);
}

void tdb_trace_seqnum(struct tdb_context *tdb, uint32_t seqnum, const char *op)
{
	char msg[sizeof(tdb_off_t) * 4 + 1];

	snprintf(msg, sizeof(msg), "%u ", seqnum);
	tdb_trace_write(tdb, msg);
	tdb_trace_write(tdb, op);
	tdb_trace_end(tdb);
}

void tdb_trace_open(struct tdb_context *tdb, const char *op,
		    unsigned hash_size, unsigned tdb_flags, unsigned open_flags)
{
	char msg[128];

	snprintf(msg, sizeof(msg),
		 "%s %u 0x%x 0x%x", op, hash_size, tdb_flags, open_flags);
	tdb_trace_start(tdb);
	tdb_trace_write(tdb, msg);
	tdb_trace_end(tdb);
}

void tdb_trace_ret(struct tdb_context *tdb, const char *op, int ret)
{
	tdb_trace_start(tdb);
	tdb_trace_write(tdb, op);
	tdb_trace_end_ret(tdb, ret);
}

void tdb_trace_retrec(struct tdb_context *tdb, const char *op, TDB_DATA ret)
{
	tdb_trace_start(tdb);
	tdb_trace_write(tdb, op);
	tdb_trace_write(tdb, " =");
	tdb_trace_record(tdb, ret);
	tdb_trace_end(tdb);
}

void tdb_trace_1rec(struct tdb_context *tdb, const char *op,
		    TDB_DATA rec)
{
	tdb_trace_start(tdb);
	tdb_trace_write(tdb, op);
	tdb_trace_record(tdb, rec);
	tdb_trace_end(tdb);
}

void tdb_trace_1rec_ret(struct tdb_context *tdb, const char *op,
			TDB_DATA rec, int ret)
{
	tdb_trace_start(tdb);
	tdb_trace_write(tdb, op);
	tdb_trace_record(tdb, rec);
	tdb_trace_end_ret(tdb, ret);
}

void tdb_trace_1rec_retrec(struct tdb_context *tdb, const char *op,
			   TDB_DATA rec, TDB_DATA ret)
{
	tdb_trace_start(tdb);
	tdb_trace_write(tdb, op);
	tdb_trace_record(tdb, rec);
	tdb_trace_write(tdb, " =");
	tdb_trace_record(tdb, ret);
	tdb_trace_end(tdb);
}

void tdb_trace_2rec_flag_ret(struct tdb_context *tdb, const char *op,
			     TDB_DATA rec1, TDB_DATA rec2, unsigned flag,
			     int ret)
{
	char msg[1 + sizeof(ret) * 4];

	snprintf(msg, sizeof(msg), " %#x", flag);
	tdb_trace_start(tdb);
	tdb_trace_write(tdb, op);
	tdb_trace_record(tdb, rec1);
	tdb_trace_record(tdb, rec2);
	tdb_trace_write(tdb, msg);
	tdb_trace_end_ret(tdb, ret);
}

void tdb_trace_2rec_retrec(struct tdb_context *tdb, const char *op,
			   TDB_DATA rec1, TDB_DATA rec2, TDB_DATA ret)
{
	tdb_trace_start(tdb);
	tdb_trace_write(tdb, op);
	tdb_trace_record(tdb, rec1);
	tdb_trace_record(tdb, rec2);
	tdb_trace_write(tdb, " =");
	tdb_trace_record(tdb, ret);
	tdb_trace_end(tdb);
}
#endif
