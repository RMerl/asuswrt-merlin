 /* 
   Unix SMB/CIFS implementation.

   trivial database library

   Copyright (C) Andrew Tridgell              2005

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

/*
  transaction design:

  - only allow a single transaction at a time per database. This makes
    using the transaction API simpler, as otherwise the caller would
    have to cope with temporary failures in transactions that conflict
    with other current transactions

  - keep the transaction recovery information in the same file as the
    database, using a special 'transaction recovery' record pointed at
    by the header. This removes the need for extra journal files as
    used by some other databases

  - dynamically allocated the transaction recover record, re-using it
    for subsequent transactions. If a larger record is needed then
    tdb_free() the old record to place it on the normal tdb freelist
    before allocating the new record

  - during transactions, keep a linked list of writes all that have
    been performed by intercepting all tdb_write() calls. The hooked
    transaction versions of tdb_read() and tdb_write() check this
    linked list and try to use the elements of the list in preference
    to the real database.

  - don't allow any locks to be held when a transaction starts,
    otherwise we can end up with deadlock (plus lack of lock nesting
    in posix locks would mean the lock is lost)

  - if the caller gains a lock during the transaction but doesn't
    release it then fail the commit

  - allow for nested calls to tdb_transaction_start(), re-using the
    existing transaction record. If the inner transaction is cancelled
    then a subsequent commit will fail
 
  - keep a mirrored copy of the tdb hash chain heads to allow for the
    fast hash heads scan on traverse, updating the mirrored copy in
    the transaction version of tdb_write

  - allow callers to mix transaction and non-transaction use of tdb,
    although once a transaction is started then an exclusive lock is
    gained until the transaction is committed or cancelled

  - the commit stategy involves first saving away all modified data
    into a linearised buffer in the transaction recovery area, then
    marking the transaction recovery area with a magic value to
    indicate a valid recovery record. In total 4 fsync/msync calls are
    needed per commit to prevent race conditions. It might be possible
    to reduce this to 3 or even 2 with some more work.

  - check for a valid recovery record on open of the tdb, while the
    global lock is held. Automatically recover from the transaction
    recovery area if needed, then continue with the open as
    usual. This allows for smooth crash recovery with no administrator
    intervention.

  - if TDB_NOSYNC is passed to flags in tdb_open then transactions are
    still available, but no transaction recovery area is used and no
    fsync/msync calls are made.

*/

int transaction_brlock(struct tdb_context *tdb, tdb_off_t offset, 
		       int rw_type, int lck_type, int probe, size_t len);

struct tdb_transaction_el {
	struct tdb_transaction_el *next, *prev;
	tdb_off_t offset;
	tdb_len_t length;
	unsigned char *data;
};

/*
  hold the context of any current transaction
*/
struct tdb_transaction {
	/* we keep a mirrored copy of the tdb hash heads here so
	   tdb_next_hash_chain() can operate efficiently */
	u32 *hash_heads;

	/* the original io methods - used to do IOs to the real db */
	const struct tdb_methods *io_methods;

	/* the list of transaction elements. We use a doubly linked
	   list with a last pointer to allow us to keep the list
	   ordered, with first element at the front of the list. It
	   needs to be doubly linked as the read/write traversals need
	   to be backwards, while the commit needs to be forwards */
	struct tdb_transaction_el *elements, *elements_last;

	/* non-zero when an internal transaction error has
	   occurred. All write operations will then fail until the
	   transaction is ended */
	int transaction_error;

	/* when inside a transaction we need to keep track of any
	   nested tdb_transaction_start() calls, as these are allowed,
	   but don't create a new transaction */
	int nesting;

	/* old file size before transaction */
	tdb_len_t old_map_size;
};


/*
  read while in a transaction. We need to check first if the data is in our list
  of transaction elements, then if not do a real read
*/
static int transaction_read(struct tdb_context *tdb, tdb_off_t off, void *buf, 
			    tdb_len_t len, int cv)
{
	struct tdb_transaction_el *el;

	/* we need to walk the list backwards to get the most recent data */
	for (el=tdb->transaction->elements_last;el;el=el->prev) {
		tdb_len_t partial;

		if (off+len <= el->offset) {
			continue;
		}
		if (off >= el->offset + el->length) {
			continue;
		}

		/* an overlapping read - needs to be split into up to
		   2 reads and a memcpy */
		if (off < el->offset) {
			partial = el->offset - off;
			if (transaction_read(tdb, off, buf, partial, cv) != 0) {
				goto fail;
			}
			len -= partial;
			off += partial;
			buf = (void *)(partial + (char *)buf);
		}
		if (off + len <= el->offset + el->length) {
			partial = len;
		} else {
			partial = el->offset + el->length - off;
		}
		memcpy(buf, el->data + (off - el->offset), partial);
		if (cv) {
			tdb_convert(buf, len);
		}
		len -= partial;
		off += partial;
		buf = (void *)(partial + (char *)buf);
		
		if (len != 0 && transaction_read(tdb, off, buf, len, cv) != 0) {
			goto fail;
		}

		return 0;
	}

	/* its not in the transaction elements - do a real read */
	return tdb->transaction->io_methods->tdb_read(tdb, off, buf, len, cv);

fail:
	TDB_LOG((tdb, TDB_DEBUG_FATAL, "transaction_read: failed at off=%d len=%d\n", off, len));
	tdb->ecode = TDB_ERR_IO;
	tdb->transaction->transaction_error = 1;
	return -1;
}


/*
  write while in a transaction
*/
static int transaction_write(struct tdb_context *tdb, tdb_off_t off, 
			     const void *buf, tdb_len_t len)
{
	struct tdb_transaction_el *el, *best_el=NULL;

	if (len == 0) {
		return 0;
	}
	
	/* if the write is to a hash head, then update the transaction
	   hash heads */
	if (len == sizeof(tdb_off_t) && off >= FREELIST_TOP &&
	    off < FREELIST_TOP+TDB_HASHTABLE_SIZE(tdb)) {
		u32 chain = (off-FREELIST_TOP) / sizeof(tdb_off_t);
		memcpy(&tdb->transaction->hash_heads[chain], buf, len);
	}

	/* first see if we can replace an existing entry */
	for (el=tdb->transaction->elements_last;el;el=el->prev) {
		tdb_len_t partial;

		if (best_el == NULL && off == el->offset+el->length) {
			best_el = el;
		}

		if (off+len <= el->offset) {
			continue;
		}
		if (off >= el->offset + el->length) {
			continue;
		}

		/* an overlapping write - needs to be split into up to
		   2 writes and a memcpy */
		if (off < el->offset) {
			partial = el->offset - off;
			if (transaction_write(tdb, off, buf, partial) != 0) {
				goto fail;
			}
			len -= partial;
			off += partial;
			buf = (const void *)(partial + (const char *)buf);
		}
		if (off + len <= el->offset + el->length) {
			partial = len;
		} else {
			partial = el->offset + el->length - off;
		}
		memcpy(el->data + (off - el->offset), buf, partial);
		len -= partial;
		off += partial;
		buf = (const void *)(partial + (const char *)buf);
		
		if (len != 0 && transaction_write(tdb, off, buf, len) != 0) {
			goto fail;
		}

		return 0;
	}

	/* see if we can append the new entry to an existing entry */
	if (best_el && best_el->offset + best_el->length == off && 
	    (off+len < tdb->transaction->old_map_size ||
	     off > tdb->transaction->old_map_size)) {
		unsigned char *data = best_el->data;
		el = best_el;
		el->data = (unsigned char *)realloc(el->data,
						    el->length + len);
		if (el->data == NULL) {
			tdb->ecode = TDB_ERR_OOM;
			tdb->transaction->transaction_error = 1;
			el->data = data;
			return -1;
		}
		if (buf) {
			memcpy(el->data + el->length, buf, len);
		} else {
			memset(el->data + el->length, TDB_PAD_BYTE, len);
		}
		el->length += len;
		return 0;
	}

	/* add a new entry at the end of the list */
	el = (struct tdb_transaction_el *)malloc(sizeof(*el));
	if (el == NULL) {
		tdb->ecode = TDB_ERR_OOM;
		tdb->transaction->transaction_error = 1;		
		return -1;
	}
	el->next = NULL;
	el->prev = tdb->transaction->elements_last;
	el->offset = off;
	el->length = len;
	el->data = (unsigned char *)malloc(len);
	if (el->data == NULL) {
		free(el);
		tdb->ecode = TDB_ERR_OOM;
		tdb->transaction->transaction_error = 1;		
		return -1;
	}
	if (buf) {
		memcpy(el->data, buf, len);
	} else {
		memset(el->data, TDB_PAD_BYTE, len);
	}
	if (el->prev) {
		el->prev->next = el;
	} else {
		tdb->transaction->elements = el;
	}
	tdb->transaction->elements_last = el;
	return 0;

fail:
	TDB_LOG((tdb, TDB_DEBUG_FATAL, "transaction_write: failed at off=%d len=%d\n", off, len));
	tdb->ecode = TDB_ERR_IO;
	tdb->transaction->transaction_error = 1;
	return -1;
}

/*
  accelerated hash chain head search, using the cached hash heads
*/
static void transaction_next_hash_chain(struct tdb_context *tdb, u32 *chain)
{
	u32 h = *chain;
	for (;h < tdb->header.hash_size;h++) {
		/* the +1 takes account of the freelist */
		if (0 != tdb->transaction->hash_heads[h+1]) {
			break;
		}
	}
	(*chain) = h;
}

/*
  out of bounds check during a transaction
*/
static int transaction_oob(struct tdb_context *tdb, tdb_off_t len, int probe)
{
	if (len <= tdb->map_size) {
		return 0;
	}
	return TDB_ERRCODE(TDB_ERR_IO, -1);
}

/*
  transaction version of tdb_expand().
*/
static int transaction_expand_file(struct tdb_context *tdb, tdb_off_t size, 
				   tdb_off_t addition)
{
	/* add a write to the transaction elements, so subsequent
	   reads see the zero data */
	if (transaction_write(tdb, size, NULL, addition) != 0) {
		return -1;
	}

	return 0;
}

/*
  brlock during a transaction - ignore them
*/
int transaction_brlock(struct tdb_context *tdb, tdb_off_t offset, 
		       int rw_type, int lck_type, int probe, size_t len)
{
	return 0;
}

static const struct tdb_methods transaction_methods = {
	transaction_read,
	transaction_write,
	transaction_next_hash_chain,
	transaction_oob,
	transaction_expand_file,
	transaction_brlock
};


/*
  start a tdb transaction. No token is returned, as only a single
  transaction is allowed to be pending per tdb_context
*/
int tdb_transaction_start(struct tdb_context *tdb)
{
	/* some sanity checks */
	if (tdb->read_only || (tdb->flags & TDB_INTERNAL) || tdb->traverse_read) {
		TDB_LOG((tdb, TDB_DEBUG_ERROR, "tdb_transaction_start: cannot start a transaction on a read-only or internal db\n"));
		tdb->ecode = TDB_ERR_EINVAL;
		return -1;
	}

	/* cope with nested tdb_transaction_start() calls */
	if (tdb->transaction != NULL) {
		tdb->transaction->nesting++;
		TDB_LOG((tdb, TDB_DEBUG_TRACE, "tdb_transaction_start: nesting %d\n", 
			 tdb->transaction->nesting));
		return 0;
	}

	if (tdb->num_locks != 0 || tdb->global_lock.count) {
		/* the caller must not have any locks when starting a
		   transaction as otherwise we'll be screwed by lack
		   of nested locks in posix */
		TDB_LOG((tdb, TDB_DEBUG_ERROR, "tdb_transaction_start: cannot start a transaction with locks held\n"));
		tdb->ecode = TDB_ERR_LOCK;
		return -1;
	}

	if (tdb->travlocks.next != NULL) {
		/* you cannot use transactions inside a traverse (although you can use
		   traverse inside a transaction) as otherwise you can end up with
		   deadlock */
		TDB_LOG((tdb, TDB_DEBUG_ERROR, "tdb_transaction_start: cannot start a transaction within a traverse\n"));
		tdb->ecode = TDB_ERR_LOCK;
		return -1;
	}

	tdb->transaction = (struct tdb_transaction *)
		calloc(sizeof(struct tdb_transaction), 1);
	if (tdb->transaction == NULL) {
		tdb->ecode = TDB_ERR_OOM;
		return -1;
	}

	/* get the transaction write lock. This is a blocking lock. As
	   discussed with Volker, there are a number of ways we could
	   make this async, which we will probably do in the future */
	if (tdb_brlock(tdb, TRANSACTION_LOCK, F_WRLCK, F_SETLKW, 0, 1) == -1) {
		TDB_LOG((tdb, TDB_DEBUG_ERROR, "tdb_transaction_start: failed to get transaction lock\n"));
		tdb->ecode = TDB_ERR_LOCK;
		SAFE_FREE(tdb->transaction);
		return -1;
	}
	
	/* get a read lock from the freelist to the end of file. This
	   is upgraded to a write lock during the commit */
	if (tdb_brlock(tdb, FREELIST_TOP, F_RDLCK, F_SETLKW, 0, 0) == -1) {
		TDB_LOG((tdb, TDB_DEBUG_ERROR, "tdb_transaction_start: failed to get hash locks\n"));
		tdb->ecode = TDB_ERR_LOCK;
		goto fail;
	}

	/* setup a copy of the hash table heads so the hash scan in
	   traverse can be fast */
	tdb->transaction->hash_heads = (u32 *)
		calloc(tdb->header.hash_size+1, sizeof(u32));
	if (tdb->transaction->hash_heads == NULL) {
		tdb->ecode = TDB_ERR_OOM;
		goto fail;
	}
	if (tdb->methods->tdb_read(tdb, FREELIST_TOP, tdb->transaction->hash_heads,
				   TDB_HASHTABLE_SIZE(tdb), 0) != 0) {
		TDB_LOG((tdb, TDB_DEBUG_FATAL, "tdb_transaction_start: failed to read hash heads\n"));
		tdb->ecode = TDB_ERR_IO;
		goto fail;
	}

	/* make sure we know about any file expansions already done by
	   anyone else */
	tdb->methods->tdb_oob(tdb, tdb->map_size + 1, 1);
	tdb->transaction->old_map_size = tdb->map_size;

	/* finally hook the io methods, replacing them with
	   transaction specific methods */
	tdb->transaction->io_methods = tdb->methods;
	tdb->methods = &transaction_methods;

	/* by calling this transaction write here, we ensure that we don't grow the
	   transaction linked list due to hash table updates */
	if (transaction_write(tdb, FREELIST_TOP, tdb->transaction->hash_heads, 
			      TDB_HASHTABLE_SIZE(tdb)) != 0) {
		TDB_LOG((tdb, TDB_DEBUG_FATAL, "tdb_transaction_start: failed to prime hash table\n"));
		tdb->ecode = TDB_ERR_IO;
		goto fail;
	}

	return 0;
	
fail:
	tdb_brlock(tdb, FREELIST_TOP, F_UNLCK, F_SETLKW, 0, 0);
	tdb_brlock(tdb, TRANSACTION_LOCK, F_UNLCK, F_SETLKW, 0, 1);
	SAFE_FREE(tdb->transaction->hash_heads);
	SAFE_FREE(tdb->transaction);
	return -1;
}


/*
  cancel the current transaction
*/
int tdb_transaction_cancel(struct tdb_context *tdb)
{	
	if (tdb->transaction == NULL) {
		TDB_LOG((tdb, TDB_DEBUG_ERROR, "tdb_transaction_cancel: no transaction\n"));
		return -1;
	}

	if (tdb->transaction->nesting != 0) {
		tdb->transaction->transaction_error = 1;
		tdb->transaction->nesting--;
		return 0;
	}		

	tdb->map_size = tdb->transaction->old_map_size;

	/* free all the transaction elements */
	while (tdb->transaction->elements) {
		struct tdb_transaction_el *el = tdb->transaction->elements;
		tdb->transaction->elements = el->next;
		free(el->data);
		free(el);
	}

	/* remove any global lock created during the transaction */
	if (tdb->global_lock.count != 0) {
		tdb_brlock(tdb, FREELIST_TOP, F_UNLCK, F_SETLKW, 0, 4*tdb->header.hash_size);
		tdb->global_lock.count = 0;
	}

	/* remove any locks created during the transaction */
	if (tdb->num_locks != 0) {
		int i;
		for (i=0;i<tdb->num_lockrecs;i++) {
			tdb_brlock(tdb,FREELIST_TOP+4*tdb->lockrecs[i].list,
				   F_UNLCK,F_SETLKW, 0, 1);
		}
		tdb->num_locks = 0;
		tdb->num_lockrecs = 0;
		SAFE_FREE(tdb->lockrecs);
	}

	/* restore the normal io methods */
	tdb->methods = tdb->transaction->io_methods;

	tdb_brlock(tdb, FREELIST_TOP, F_UNLCK, F_SETLKW, 0, 0);
	tdb_brlock(tdb, TRANSACTION_LOCK, F_UNLCK, F_SETLKW, 0, 1);
	SAFE_FREE(tdb->transaction->hash_heads);
	SAFE_FREE(tdb->transaction);
	
	return 0;
}

/*
  sync to disk
*/
static int transaction_sync(struct tdb_context *tdb, tdb_off_t offset, tdb_len_t length)
{	
	if (fsync(tdb->fd) != 0) {
		tdb->ecode = TDB_ERR_IO;
		TDB_LOG((tdb, TDB_DEBUG_FATAL, "tdb_transaction: fsync failed\n"));
		return -1;
	}
#ifdef MS_SYNC
	if (tdb->map_ptr) {
		tdb_off_t moffset = offset & ~(tdb->page_size-1);
		if (msync(moffset + (char *)tdb->map_ptr, 
			  length + (offset - moffset), MS_SYNC) != 0) {
			tdb->ecode = TDB_ERR_IO;
			TDB_LOG((tdb, TDB_DEBUG_FATAL, "tdb_transaction: msync failed - %s\n",
				 strerror(errno)));
			return -1;
		}
	}
#endif
	return 0;
}


/*
  work out how much space the linearised recovery data will consume
*/
static tdb_len_t tdb_recovery_size(struct tdb_context *tdb)
{
	struct tdb_transaction_el *el;
	tdb_len_t recovery_size = 0;

	recovery_size = sizeof(u32);
	for (el=tdb->transaction->elements;el;el=el->next) {
		if (el->offset >= tdb->transaction->old_map_size) {
			continue;
		}
		recovery_size += 2*sizeof(tdb_off_t) + el->length;
	}

	return recovery_size;
}

/*
  allocate the recovery area, or use an existing recovery area if it is
  large enough
*/
static int tdb_recovery_allocate(struct tdb_context *tdb, 
				 tdb_len_t *recovery_size,
				 tdb_off_t *recovery_offset,
				 tdb_len_t *recovery_max_size)
{
	struct list_struct rec;
	const struct tdb_methods *methods = tdb->transaction->io_methods;
	tdb_off_t recovery_head;

	if (tdb_ofs_read(tdb, TDB_RECOVERY_HEAD, &recovery_head) == -1) {
		TDB_LOG((tdb, TDB_DEBUG_FATAL, "tdb_recovery_allocate: failed to read recovery head\n"));
		return -1;
	}

	rec.rec_len = 0;

	if (recovery_head != 0 && 
	    methods->tdb_read(tdb, recovery_head, &rec, sizeof(rec), DOCONV()) == -1) {
		TDB_LOG((tdb, TDB_DEBUG_FATAL, "tdb_recovery_allocate: failed to read recovery record\n"));
		return -1;
	}

	*recovery_size = tdb_recovery_size(tdb);

	if (recovery_head != 0 && *recovery_size <= rec.rec_len) {
		/* it fits in the existing area */
		*recovery_max_size = rec.rec_len;
		*recovery_offset = recovery_head;
		return 0;
	}

	/* we need to free up the old recovery area, then allocate a
	   new one at the end of the file. Note that we cannot use
	   tdb_allocate() to allocate the new one as that might return
	   us an area that is being currently used (as of the start of
	   the transaction) */
	if (recovery_head != 0) {
		if (tdb_free(tdb, recovery_head, &rec) == -1) {
			TDB_LOG((tdb, TDB_DEBUG_FATAL, "tdb_recovery_allocate: failed to free previous recovery area\n"));
			return -1;
		}
	}

	/* the tdb_free() call might have increased the recovery size */
	*recovery_size = tdb_recovery_size(tdb);

	/* round up to a multiple of page size */
	*recovery_max_size = TDB_ALIGN(sizeof(rec) + *recovery_size, tdb->page_size) - sizeof(rec);
	*recovery_offset = tdb->map_size;
	recovery_head = *recovery_offset;

	if (methods->tdb_expand_file(tdb, tdb->transaction->old_map_size, 
				     (tdb->map_size - tdb->transaction->old_map_size) +
				     sizeof(rec) + *recovery_max_size) == -1) {
		TDB_LOG((tdb, TDB_DEBUG_FATAL, "tdb_recovery_allocate: failed to create recovery area\n"));
		return -1;
	}

	/* remap the file (if using mmap) */
	methods->tdb_oob(tdb, tdb->map_size + 1, 1);

	/* we have to reset the old map size so that we don't try to expand the file
	   again in the transaction commit, which would destroy the recovery area */
	tdb->transaction->old_map_size = tdb->map_size;

	/* write the recovery header offset and sync - we can sync without a race here
	   as the magic ptr in the recovery record has not been set */
	CONVERT(recovery_head);
	if (methods->tdb_write(tdb, TDB_RECOVERY_HEAD, 
			       &recovery_head, sizeof(tdb_off_t)) == -1) {
		TDB_LOG((tdb, TDB_DEBUG_FATAL, "tdb_recovery_allocate: failed to write recovery head\n"));
		return -1;
	}

	return 0;
}


/*
  setup the recovery data that will be used on a crash during commit
*/
static int transaction_setup_recovery(struct tdb_context *tdb, 
				      tdb_off_t *magic_offset)
{
	struct tdb_transaction_el *el;
	tdb_len_t recovery_size;
	unsigned char *data, *p;
	const struct tdb_methods *methods = tdb->transaction->io_methods;
	struct list_struct *rec;
	tdb_off_t recovery_offset, recovery_max_size;
	tdb_off_t old_map_size = tdb->transaction->old_map_size;
	u32 magic, tailer;

	/*
	  check that the recovery area has enough space
	*/
	if (tdb_recovery_allocate(tdb, &recovery_size, 
				  &recovery_offset, &recovery_max_size) == -1) {
		return -1;
	}

	data = (unsigned char *)malloc(recovery_size + sizeof(*rec));
	if (data == NULL) {
		tdb->ecode = TDB_ERR_OOM;
		return -1;
	}

	rec = (struct list_struct *)data;
	memset(rec, 0, sizeof(*rec));

	rec->magic    = 0;
	rec->data_len = recovery_size;
	rec->rec_len  = recovery_max_size;
	rec->key_len  = old_map_size;
	CONVERT(rec);

	/* build the recovery data into a single blob to allow us to do a single
	   large write, which should be more efficient */
	p = data + sizeof(*rec);
	for (el=tdb->transaction->elements;el;el=el->next) {
		if (el->offset >= old_map_size) {
			continue;
		}
		if (el->offset + el->length > tdb->transaction->old_map_size) {
			TDB_LOG((tdb, TDB_DEBUG_FATAL, "tdb_transaction_setup_recovery: transaction data over new region boundary\n"));
			free(data);
			tdb->ecode = TDB_ERR_CORRUPT;
			return -1;
		}
		memcpy(p, &el->offset, 4);
		memcpy(p+4, &el->length, 4);
		if (DOCONV()) {
			tdb_convert(p, 8);
		}
		/* the recovery area contains the old data, not the
		   new data, so we have to call the original tdb_read
		   method to get it */
		if (methods->tdb_read(tdb, el->offset, p + 8, el->length, 0) != 0) {
			free(data);
			tdb->ecode = TDB_ERR_IO;
			return -1;
		}
		p += 8 + el->length;
	}

	/* and the tailer */
	tailer = sizeof(*rec) + recovery_max_size;
	memcpy(p, &tailer, 4);
	CONVERT(p);

	/* write the recovery data to the recovery area */
	if (methods->tdb_write(tdb, recovery_offset, data, sizeof(*rec) + recovery_size) == -1) {
		TDB_LOG((tdb, TDB_DEBUG_FATAL, "tdb_transaction_setup_recovery: failed to write recovery data\n"));
		free(data);
		tdb->ecode = TDB_ERR_IO;
		return -1;
	}

	/* as we don't have ordered writes, we have to sync the recovery
	   data before we update the magic to indicate that the recovery
	   data is present */
	if (transaction_sync(tdb, recovery_offset, sizeof(*rec) + recovery_size) == -1) {
		free(data);
		return -1;
	}

	free(data);

	magic = TDB_RECOVERY_MAGIC;
	CONVERT(magic);

	*magic_offset = recovery_offset + offsetof(struct list_struct, magic);

	if (methods->tdb_write(tdb, *magic_offset, &magic, sizeof(magic)) == -1) {
		TDB_LOG((tdb, TDB_DEBUG_FATAL, "tdb_transaction_setup_recovery: failed to write recovery magic\n"));
		tdb->ecode = TDB_ERR_IO;
		return -1;
	}

	/* ensure the recovery magic marker is on disk */
	if (transaction_sync(tdb, *magic_offset, sizeof(magic)) == -1) {
		return -1;
	}

	return 0;
}

/*
  commit the current transaction
*/
int tdb_transaction_commit(struct tdb_context *tdb)
{	
	const struct tdb_methods *methods;
	tdb_off_t magic_offset = 0;
	u32 zero = 0;

	if (tdb->transaction == NULL) {
		TDB_LOG((tdb, TDB_DEBUG_ERROR, "tdb_transaction_commit: no transaction\n"));
		return -1;
	}

	if (tdb->transaction->transaction_error) {
		tdb->ecode = TDB_ERR_IO;
		tdb_transaction_cancel(tdb);
		TDB_LOG((tdb, TDB_DEBUG_ERROR, "tdb_transaction_commit: transaction error pending\n"));
		return -1;
	}

	if (tdb->transaction->nesting != 0) {
		tdb->transaction->nesting--;
		return 0;
	}		

	/* check for a null transaction */
	if (tdb->transaction->elements == NULL) {
		tdb_transaction_cancel(tdb);
		return 0;
	}

	methods = tdb->transaction->io_methods;
	
	/* if there are any locks pending then the caller has not
	   nested their locks properly, so fail the transaction */
	if (tdb->num_locks || tdb->global_lock.count) {
		tdb->ecode = TDB_ERR_LOCK;
		TDB_LOG((tdb, TDB_DEBUG_ERROR, "tdb_transaction_commit: locks pending on commit\n"));
		tdb_transaction_cancel(tdb);
		return -1;
	}

	/* upgrade the main transaction lock region to a write lock */
	if (tdb_brlock_upgrade(tdb, FREELIST_TOP, 0) == -1) {
		TDB_LOG((tdb, TDB_DEBUG_ERROR, "tdb_transaction_start: failed to upgrade hash locks\n"));
		tdb->ecode = TDB_ERR_LOCK;
		tdb_transaction_cancel(tdb);
		return -1;
	}

	/* get the global lock - this prevents new users attaching to the database
	   during the commit */
	if (tdb_brlock(tdb, GLOBAL_LOCK, F_WRLCK, F_SETLKW, 0, 1) == -1) {
		TDB_LOG((tdb, TDB_DEBUG_ERROR, "tdb_transaction_commit: failed to get global lock\n"));
		tdb->ecode = TDB_ERR_LOCK;
		tdb_transaction_cancel(tdb);
		return -1;
	}

	if (!(tdb->flags & TDB_NOSYNC)) {
		/* write the recovery data to the end of the file */
		if (transaction_setup_recovery(tdb, &magic_offset) == -1) {
			TDB_LOG((tdb, TDB_DEBUG_FATAL, "tdb_transaction_commit: failed to setup recovery data\n"));
			tdb_brlock(tdb, GLOBAL_LOCK, F_UNLCK, F_SETLKW, 0, 1);
			tdb_transaction_cancel(tdb);
			return -1;
		}
	}

	/* expand the file to the new size if needed */
	if (tdb->map_size != tdb->transaction->old_map_size) {
		if (methods->tdb_expand_file(tdb, tdb->transaction->old_map_size, 
					     tdb->map_size - 
					     tdb->transaction->old_map_size) == -1) {
			tdb->ecode = TDB_ERR_IO;
			TDB_LOG((tdb, TDB_DEBUG_FATAL, "tdb_transaction_commit: expansion failed\n"));
			tdb_brlock(tdb, GLOBAL_LOCK, F_UNLCK, F_SETLKW, 0, 1);
			tdb_transaction_cancel(tdb);
			return -1;
		}
		tdb->map_size = tdb->transaction->old_map_size;
		methods->tdb_oob(tdb, tdb->map_size + 1, 1);
	}

	/* perform all the writes */
	while (tdb->transaction->elements) {
		struct tdb_transaction_el *el = tdb->transaction->elements;

		if (methods->tdb_write(tdb, el->offset, el->data, el->length) == -1) {
			TDB_LOG((tdb, TDB_DEBUG_FATAL, "tdb_transaction_commit: write failed during commit\n"));
			
			/* we've overwritten part of the data and
			   possibly expanded the file, so we need to
			   run the crash recovery code */
			tdb->methods = methods;
			tdb_transaction_recover(tdb); 

			tdb_transaction_cancel(tdb);
			tdb_brlock(tdb, GLOBAL_LOCK, F_UNLCK, F_SETLKW, 0, 1);

			TDB_LOG((tdb, TDB_DEBUG_FATAL, "tdb_transaction_commit: write failed\n"));
			return -1;
		}
		tdb->transaction->elements = el->next;
		free(el->data); 
		free(el);
	} 

	if (!(tdb->flags & TDB_NOSYNC)) {
		/* ensure the new data is on disk */
		if (transaction_sync(tdb, 0, tdb->map_size) == -1) {
			return -1;
		}

		/* remove the recovery marker */
		if (methods->tdb_write(tdb, magic_offset, &zero, 4) == -1) {
			TDB_LOG((tdb, TDB_DEBUG_FATAL, "tdb_transaction_commit: failed to remove recovery magic\n"));
			return -1;
		}

		/* ensure the recovery marker has been removed on disk */
		if (transaction_sync(tdb, magic_offset, 4) == -1) {
			return -1;
		}
	}

	tdb_brlock(tdb, GLOBAL_LOCK, F_UNLCK, F_SETLKW, 0, 1);

	/*
	  TODO: maybe write to some dummy hdr field, or write to magic
	  offset without mmap, before the last sync, instead of the
	  utime() call
	*/

	/* on some systems (like Linux 2.6.x) changes via mmap/msync
	   don't change the mtime of the file, this means the file may
	   not be backed up (as tdb rounding to block sizes means that
	   file size changes are quite rare too). The following forces
	   mtime changes when a transaction completes */
#ifdef HAVE_UTIME
	utime(tdb->name, NULL);
#endif

	/* use a transaction cancel to free memory and remove the
	   transaction locks */
	tdb_transaction_cancel(tdb);
	return 0;
}


/*
  recover from an aborted transaction. Must be called with exclusive
  database write access already established (including the global
  lock to prevent new processes attaching)
*/
int tdb_transaction_recover(struct tdb_context *tdb)
{
	tdb_off_t recovery_head, recovery_eof;
	unsigned char *data, *p;
	u32 zero = 0;
	struct list_struct rec;

	/* find the recovery area */
	if (tdb_ofs_read(tdb, TDB_RECOVERY_HEAD, &recovery_head) == -1) {
		TDB_LOG((tdb, TDB_DEBUG_FATAL, "tdb_transaction_recover: failed to read recovery head\n"));
		tdb->ecode = TDB_ERR_IO;
		return -1;
	}

	if (recovery_head == 0) {
		/* we have never allocated a recovery record */
		return 0;
	}

	/* read the recovery record */
	if (tdb->methods->tdb_read(tdb, recovery_head, &rec, 
				   sizeof(rec), DOCONV()) == -1) {
		TDB_LOG((tdb, TDB_DEBUG_FATAL, "tdb_transaction_recover: failed to read recovery record\n"));		
		tdb->ecode = TDB_ERR_IO;
		return -1;
	}

	if (rec.magic != TDB_RECOVERY_MAGIC) {
		/* there is no valid recovery data */
		return 0;
	}

	if (tdb->read_only) {
		TDB_LOG((tdb, TDB_DEBUG_FATAL, "tdb_transaction_recover: attempt to recover read only database\n"));
		tdb->ecode = TDB_ERR_CORRUPT;
		return -1;
	}

	recovery_eof = rec.key_len;

	data = (unsigned char *)malloc(rec.data_len);
	if (data == NULL) {
		TDB_LOG((tdb, TDB_DEBUG_FATAL, "tdb_transaction_recover: failed to allocate recovery data\n"));		
		tdb->ecode = TDB_ERR_OOM;
		return -1;
	}

	/* read the full recovery data */
	if (tdb->methods->tdb_read(tdb, recovery_head + sizeof(rec), data,
				   rec.data_len, 0) == -1) {
		TDB_LOG((tdb, TDB_DEBUG_FATAL, "tdb_transaction_recover: failed to read recovery data\n"));		
		tdb->ecode = TDB_ERR_IO;
		return -1;
	}

	/* recover the file data */
	p = data;
	while (p+8 < data + rec.data_len) {
		u32 ofs, len;
		if (DOCONV()) {
			tdb_convert(p, 8);
		}
		memcpy(&ofs, p, 4);
		memcpy(&len, p+4, 4);

		if (tdb->methods->tdb_write(tdb, ofs, p+8, len) == -1) {
			free(data);
			TDB_LOG((tdb, TDB_DEBUG_FATAL, "tdb_transaction_recover: failed to recover %d bytes at offset %d\n", len, ofs));
			tdb->ecode = TDB_ERR_IO;
			return -1;
		}
		p += 8 + len;
	}

	free(data);

	if (transaction_sync(tdb, 0, tdb->map_size) == -1) {
		TDB_LOG((tdb, TDB_DEBUG_FATAL, "tdb_transaction_recover: failed to sync recovery\n"));
		tdb->ecode = TDB_ERR_IO;
		return -1;
	}

	/* if the recovery area is after the recovered eof then remove it */
	if (recovery_eof <= recovery_head) {
		if (tdb_ofs_write(tdb, TDB_RECOVERY_HEAD, &zero) == -1) {
			TDB_LOG((tdb, TDB_DEBUG_FATAL, "tdb_transaction_recover: failed to remove recovery head\n"));
			tdb->ecode = TDB_ERR_IO;
			return -1;			
		}
	}

	/* remove the recovery magic */
	if (tdb_ofs_write(tdb, recovery_head + offsetof(struct list_struct, magic), 
			  &zero) == -1) {
		TDB_LOG((tdb, TDB_DEBUG_FATAL, "tdb_transaction_recover: failed to remove recovery magic\n"));
		tdb->ecode = TDB_ERR_IO;
		return -1;			
	}
	
	/* reduce the file size to the old size */
	tdb_munmap(tdb);
	if (ftruncate(tdb->fd, recovery_eof) != 0) {
		TDB_LOG((tdb, TDB_DEBUG_FATAL, "tdb_transaction_recover: failed to reduce to recovery size\n"));
		tdb->ecode = TDB_ERR_IO;
		return -1;			
	}
	tdb->map_size = recovery_eof;
	tdb_mmap(tdb);

	if (transaction_sync(tdb, 0, recovery_eof) == -1) {
		TDB_LOG((tdb, TDB_DEBUG_FATAL, "tdb_transaction_recover: failed to sync2 recovery\n"));
		tdb->ecode = TDB_ERR_IO;
		return -1;
	}

	TDB_LOG((tdb, TDB_DEBUG_TRACE, "tdb_transaction_recover: recovered %d byte database\n", 
		 recovery_eof));

	/* all done */
	return 0;
}
