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

#define TDB_NEXT_LOCK_ERR ((tdb_off_t)-1)

/* Uses traverse lock: 0 = finish, TDB_NEXT_LOCK_ERR = error,
   other = record offset */
static tdb_off_t tdb_next_lock(struct tdb_context *tdb, struct tdb_traverse_lock *tlock,
			 struct tdb_record *rec)
{
	int want_next = (tlock->off != 0);

	/* Lock each chain from the start one. */
	for (; tlock->hash < tdb->header.hash_size; tlock->hash++) {
		if (!tlock->off && tlock->hash != 0) {
			/* this is an optimisation for the common case where
			   the hash chain is empty, which is particularly
			   common for the use of tdb with ldb, where large
			   hashes are used. In that case we spend most of our
			   time in tdb_brlock(), locking empty hash chains.

			   To avoid this, we do an unlocked pre-check to see
			   if the hash chain is empty before starting to look
			   inside it. If it is empty then we can avoid that
			   hash chain. If it isn't empty then we can't believe
			   the value we get back, as we read it without a
			   lock, so instead we get the lock and re-fetch the
			   value below.

			   Notice that not doing this optimisation on the
			   first hash chain is critical. We must guarantee
			   that we have done at least one fcntl lock at the
			   start of a search to guarantee that memory is
			   coherent on SMP systems. If records are added by
			   others during the search then thats OK, and we
			   could possibly miss those with this trick, but we
			   could miss them anyway without this trick, so the
			   semantics don't change.

			   With a non-indexed ldb search this trick gains us a
			   factor of around 80 in speed on a linux 2.6.x
			   system (testing using ldbtest).
			*/
			tdb->methods->next_hash_chain(tdb, &tlock->hash);
			if (tlock->hash == tdb->header.hash_size) {
				continue;
			}
		}

		if (tdb_lock(tdb, tlock->hash, tlock->lock_rw) == -1)
			return TDB_NEXT_LOCK_ERR;

		/* No previous record?  Start at top of chain. */
		if (!tlock->off) {
			if (tdb_ofs_read(tdb, TDB_HASH_TOP(tlock->hash),
				     &tlock->off) == -1)
				goto fail;
		} else {
			/* Otherwise unlock the previous record. */
			if (tdb_unlock_record(tdb, tlock->off) != 0)
				goto fail;
		}

		if (want_next) {
			/* We have offset of old record: grab next */
			if (tdb_rec_read(tdb, tlock->off, rec) == -1)
				goto fail;
			tlock->off = rec->next;
		}

		/* Iterate through chain */
		while( tlock->off) {
			tdb_off_t current;
			if (tdb_rec_read(tdb, tlock->off, rec) == -1)
				goto fail;

			/* Detect infinite loops. From "Shlomi Yaakobovich" <Shlomi@exanet.com>. */
			if (tlock->off == rec->next) {
				tdb->ecode = TDB_ERR_CORRUPT;
				TDB_LOG((tdb, TDB_DEBUG_FATAL, "tdb_next_lock: loop detected.\n"));
				goto fail;
			}

			if (!TDB_DEAD(rec)) {
				/* Woohoo: we found one! */
				if (tdb_lock_record(tdb, tlock->off) != 0)
					goto fail;
				return tlock->off;
			}

			/* Try to clean dead ones from old traverses */
			current = tlock->off;
			tlock->off = rec->next;
			if (!(tdb->read_only || tdb->traverse_read) && 
			    tdb_do_delete(tdb, current, rec) != 0)
				goto fail;
		}
		tdb_unlock(tdb, tlock->hash, tlock->lock_rw);
		want_next = 0;
	}
	/* We finished iteration without finding anything */
	tdb->ecode = TDB_SUCCESS;
	return 0;

 fail:
	tlock->off = 0;
	if (tdb_unlock(tdb, tlock->hash, tlock->lock_rw) != 0)
		TDB_LOG((tdb, TDB_DEBUG_FATAL, "tdb_next_lock: On error unlock failed!\n"));
	return TDB_NEXT_LOCK_ERR;
}

/* traverse the entire database - calling fn(tdb, key, data) on each element.
   return -1 on error or the record count traversed
   if fn is NULL then it is not called
   a non-zero return value from fn() indicates that the traversal should stop
  */
static int tdb_traverse_internal(struct tdb_context *tdb, 
				 tdb_traverse_func fn, void *private_data,
				 struct tdb_traverse_lock *tl)
{
	TDB_DATA key, dbuf;
	struct tdb_record rec;
	int ret = 0, count = 0;
	tdb_off_t off;

	/* This was in the initializaton, above, but the IRIX compiler
	 * did not like it.  crh
	 */
	tl->next = tdb->travlocks.next;

	/* fcntl locks don't stack: beware traverse inside traverse */
	tdb->travlocks.next = tl;

	/* tdb_next_lock places locks on the record returned, and its chain */
	while ((off = tdb_next_lock(tdb, tl, &rec)) != 0) {
		if (off == TDB_NEXT_LOCK_ERR) {
			ret = -1;
			goto out;
		}
		count++;
		/* now read the full record */
		key.dptr = tdb_alloc_read(tdb, tl->off + sizeof(rec), 
					  rec.key_len + rec.data_len);
		if (!key.dptr) {
			ret = -1;
			if (tdb_unlock(tdb, tl->hash, tl->lock_rw) != 0)
				goto out;
			if (tdb_unlock_record(tdb, tl->off) != 0)
				TDB_LOG((tdb, TDB_DEBUG_FATAL, "tdb_traverse: key.dptr == NULL and unlock_record failed!\n"));
			goto out;
		}
		key.dsize = rec.key_len;
		dbuf.dptr = key.dptr + rec.key_len;
		dbuf.dsize = rec.data_len;

		tdb_trace_1rec_retrec(tdb, "traverse", key, dbuf);

		/* Drop chain lock, call out */
		if (tdb_unlock(tdb, tl->hash, tl->lock_rw) != 0) {
			ret = -1;
			SAFE_FREE(key.dptr);
			goto out;
		}
		if (fn && fn(tdb, key, dbuf, private_data)) {
			/* They want us to terminate traversal */
			tdb_trace_ret(tdb, "tdb_traverse_end", count);
			if (tdb_unlock_record(tdb, tl->off) != 0) {
				TDB_LOG((tdb, TDB_DEBUG_FATAL, "tdb_traverse: unlock_record failed!\n"));;
				ret = -1;
			}
			SAFE_FREE(key.dptr);
			goto out;
		}
		SAFE_FREE(key.dptr);
	}
	tdb_trace(tdb, "tdb_traverse_end");
out:
	tdb->travlocks.next = tl->next;
	if (ret < 0)
		return -1;
	else
		return count;
}


/*
  a write style traverse - temporarily marks the db read only
*/
_PUBLIC_ int tdb_traverse_read(struct tdb_context *tdb, 
		      tdb_traverse_func fn, void *private_data)
{
	struct tdb_traverse_lock tl = { NULL, 0, 0, F_RDLCK };
	int ret;

	/* we need to get a read lock on the transaction lock here to
	   cope with the lock ordering semantics of solaris10 */
	if (tdb_transaction_lock(tdb, F_RDLCK, TDB_LOCK_WAIT)) {
		return -1;
	}

	tdb->traverse_read++;
	tdb_trace(tdb, "tdb_traverse_read_start");
	ret = tdb_traverse_internal(tdb, fn, private_data, &tl);
	tdb->traverse_read--;

	tdb_transaction_unlock(tdb, F_RDLCK);

	return ret;
}

/*
  a write style traverse - needs to get the transaction lock to
  prevent deadlocks

  WARNING: The data buffer given to the callback fn does NOT meet the
  alignment restrictions malloc gives you.
*/
_PUBLIC_ int tdb_traverse(struct tdb_context *tdb, 
		 tdb_traverse_func fn, void *private_data)
{
	struct tdb_traverse_lock tl = { NULL, 0, 0, F_WRLCK };
	int ret;

	if (tdb->read_only || tdb->traverse_read) {
		return tdb_traverse_read(tdb, fn, private_data);
	}

	if (tdb_transaction_lock(tdb, F_WRLCK, TDB_LOCK_WAIT)) {
		return -1;
	}

	tdb->traverse_write++;
	tdb_trace(tdb, "tdb_traverse_start");
	ret = tdb_traverse_internal(tdb, fn, private_data, &tl);
	tdb->traverse_write--;

	tdb_transaction_unlock(tdb, F_WRLCK);

	return ret;
}


/* find the first entry in the database and return its key */
_PUBLIC_ TDB_DATA tdb_firstkey(struct tdb_context *tdb)
{
	TDB_DATA key;
	struct tdb_record rec;
	tdb_off_t off;

	/* release any old lock */
	if (tdb_unlock_record(tdb, tdb->travlocks.off) != 0)
		return tdb_null;
	tdb->travlocks.off = tdb->travlocks.hash = 0;
	tdb->travlocks.lock_rw = F_RDLCK;

	/* Grab first record: locks chain and returned record. */
	off = tdb_next_lock(tdb, &tdb->travlocks, &rec);
	if (off == 0 || off == TDB_NEXT_LOCK_ERR) {
		tdb_trace_retrec(tdb, "tdb_firstkey", tdb_null);
		return tdb_null;
	}
	/* now read the key */
	key.dsize = rec.key_len;
	key.dptr =tdb_alloc_read(tdb,tdb->travlocks.off+sizeof(rec),key.dsize);

	tdb_trace_retrec(tdb, "tdb_firstkey", key);

	/* Unlock the hash chain of the record we just read. */
	if (tdb_unlock(tdb, tdb->travlocks.hash, tdb->travlocks.lock_rw) != 0)
		TDB_LOG((tdb, TDB_DEBUG_FATAL, "tdb_firstkey: error occurred while tdb_unlocking!\n"));
	return key;
}

/* find the next entry in the database, returning its key */
_PUBLIC_ TDB_DATA tdb_nextkey(struct tdb_context *tdb, TDB_DATA oldkey)
{
	uint32_t oldhash;
	TDB_DATA key = tdb_null;
	struct tdb_record rec;
	unsigned char *k = NULL;
	tdb_off_t off;

	/* Is locked key the old key?  If so, traverse will be reliable. */
	if (tdb->travlocks.off) {
		if (tdb_lock(tdb,tdb->travlocks.hash,tdb->travlocks.lock_rw))
			return tdb_null;
		if (tdb_rec_read(tdb, tdb->travlocks.off, &rec) == -1
		    || !(k = tdb_alloc_read(tdb,tdb->travlocks.off+sizeof(rec),
					    rec.key_len))
		    || memcmp(k, oldkey.dptr, oldkey.dsize) != 0) {
			/* No, it wasn't: unlock it and start from scratch */
			if (tdb_unlock_record(tdb, tdb->travlocks.off) != 0) {
				tdb_trace_1rec_retrec(tdb, "tdb_nextkey",
						      oldkey, tdb_null);
				SAFE_FREE(k);
				return tdb_null;
			}
			if (tdb_unlock(tdb, tdb->travlocks.hash, tdb->travlocks.lock_rw) != 0) {
				SAFE_FREE(k);
				return tdb_null;
			}
			tdb->travlocks.off = 0;
		}

		SAFE_FREE(k);
	}

	if (!tdb->travlocks.off) {
		/* No previous element: do normal find, and lock record */
		tdb->travlocks.off = tdb_find_lock_hash(tdb, oldkey, tdb->hash_fn(&oldkey), tdb->travlocks.lock_rw, &rec);
		if (!tdb->travlocks.off) {
			tdb_trace_1rec_retrec(tdb, "tdb_nextkey", oldkey, tdb_null);
			return tdb_null;
		}
		tdb->travlocks.hash = BUCKET(rec.full_hash);
		if (tdb_lock_record(tdb, tdb->travlocks.off) != 0) {
			TDB_LOG((tdb, TDB_DEBUG_FATAL, "tdb_nextkey: lock_record failed (%s)!\n", strerror(errno)));
			return tdb_null;
		}
	}
	oldhash = tdb->travlocks.hash;

	/* Grab next record: locks chain and returned record,
	   unlocks old record */
	off = tdb_next_lock(tdb, &tdb->travlocks, &rec);
	if (off != TDB_NEXT_LOCK_ERR && off != 0) {
		key.dsize = rec.key_len;
		key.dptr = tdb_alloc_read(tdb, tdb->travlocks.off+sizeof(rec),
					  key.dsize);
		/* Unlock the chain of this new record */
		if (tdb_unlock(tdb, tdb->travlocks.hash, tdb->travlocks.lock_rw) != 0)
			TDB_LOG((tdb, TDB_DEBUG_FATAL, "tdb_nextkey: WARNING tdb_unlock failed!\n"));
	}
	/* Unlock the chain of old record */
	if (tdb_unlock(tdb, BUCKET(oldhash), tdb->travlocks.lock_rw) != 0)
		TDB_LOG((tdb, TDB_DEBUG_FATAL, "tdb_nextkey: WARNING tdb_unlock failed!\n"));
	tdb_trace_1rec_retrec(tdb, "tdb_nextkey", oldkey, key);
	return key;
}

