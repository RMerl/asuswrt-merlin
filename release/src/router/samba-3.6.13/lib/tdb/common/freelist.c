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

/* 'right' merges can involve O(n^2) cost when combined with a
   traverse, so they are disabled until we find a way to do them in 
   O(1) time
*/
#define USE_RIGHT_MERGES 0

/* read a freelist record and check for simple errors */
int tdb_rec_free_read(struct tdb_context *tdb, tdb_off_t off, struct tdb_record *rec)
{
	if (tdb->methods->tdb_read(tdb, off, rec, sizeof(*rec),DOCONV()) == -1)
		return -1;

	if (rec->magic == TDB_MAGIC) {
		/* this happens when a app is showdown while deleting a record - we should
		   not completely fail when this happens */
		TDB_LOG((tdb, TDB_DEBUG_WARNING, "tdb_rec_free_read non-free magic 0x%x at offset=%d - fixing\n", 
			 rec->magic, off));
		rec->magic = TDB_FREE_MAGIC;
		if (tdb->methods->tdb_write(tdb, off, rec, sizeof(*rec)) == -1)
			return -1;
	}

	if (rec->magic != TDB_FREE_MAGIC) {
		/* Ensure ecode is set for log fn. */
		tdb->ecode = TDB_ERR_CORRUPT;
		TDB_LOG((tdb, TDB_DEBUG_WARNING, "tdb_rec_free_read bad magic 0x%x at offset=%d\n", 
			   rec->magic, off));
		return -1;
	}
	if (tdb->methods->tdb_oob(tdb, rec->next+sizeof(*rec), 0) != 0)
		return -1;
	return 0;
}


#if USE_RIGHT_MERGES
/* Remove an element from the freelist.  Must have alloc lock. */
static int remove_from_freelist(struct tdb_context *tdb, tdb_off_t off, tdb_off_t next)
{
	tdb_off_t last_ptr, i;

	/* read in the freelist top */
	last_ptr = FREELIST_TOP;
	while (tdb_ofs_read(tdb, last_ptr, &i) != -1 && i != 0) {
		if (i == off) {
			/* We've found it! */
			return tdb_ofs_write(tdb, last_ptr, &next);
		}
		/* Follow chain (next offset is at start of record) */
		last_ptr = i;
	}
	tdb->ecode = TDB_ERR_CORRUPT;
	TDB_LOG((tdb, TDB_DEBUG_FATAL,"remove_from_freelist: not on list at off=%d\n", off));
	return -1;
}
#endif


/* update a record tailer (must hold allocation lock) */
static int update_tailer(struct tdb_context *tdb, tdb_off_t offset,
			 const struct tdb_record *rec)
{
	tdb_off_t totalsize;

	/* Offset of tailer from record header */
	totalsize = sizeof(*rec) + rec->rec_len;
	return tdb_ofs_write(tdb, offset + totalsize - sizeof(tdb_off_t),
			 &totalsize);
}

/* Add an element into the freelist. Merge adjacent records if
   necessary. */
int tdb_free(struct tdb_context *tdb, tdb_off_t offset, struct tdb_record *rec)
{
	/* Allocation and tailer lock */
	if (tdb_lock(tdb, -1, F_WRLCK) != 0)
		return -1;

	/* set an initial tailer, so if we fail we don't leave a bogus record */
	if (update_tailer(tdb, offset, rec) != 0) {
		TDB_LOG((tdb, TDB_DEBUG_FATAL, "tdb_free: update_tailer failed!\n"));
		goto fail;
	}

#if USE_RIGHT_MERGES
	/* Look right first (I'm an Australian, dammit) */
	if (offset + sizeof(*rec) + rec->rec_len + sizeof(*rec) <= tdb->map_size) {
		tdb_off_t right = offset + sizeof(*rec) + rec->rec_len;
		struct tdb_record r;

		if (tdb->methods->tdb_read(tdb, right, &r, sizeof(r), DOCONV()) == -1) {
			TDB_LOG((tdb, TDB_DEBUG_FATAL, "tdb_free: right read failed at %u\n", right));
			goto left;
		}

		/* If it's free, expand to include it. */
		if (r.magic == TDB_FREE_MAGIC) {
			if (remove_from_freelist(tdb, right, r.next) == -1) {
				TDB_LOG((tdb, TDB_DEBUG_FATAL, "tdb_free: right free failed at %u\n", right));
				goto left;
			}
			rec->rec_len += sizeof(r) + r.rec_len;
			if (update_tailer(tdb, offset, rec) == -1) {
				TDB_LOG((tdb, TDB_DEBUG_FATAL, "tdb_free: update_tailer failed at %u\n", offset));
				goto fail;
			}
		}
	}
left:
#endif

	/* Look left */
	if (offset - sizeof(tdb_off_t) > TDB_DATA_START(tdb->header.hash_size)) {
		tdb_off_t left = offset - sizeof(tdb_off_t);
		struct tdb_record l;
		tdb_off_t leftsize;

		/* Read in tailer and jump back to header */
		if (tdb_ofs_read(tdb, left, &leftsize) == -1) {
			TDB_LOG((tdb, TDB_DEBUG_FATAL, "tdb_free: left offset read failed at %u\n", left));
			goto update;
		}

		/* it could be uninitialised data */
		if (leftsize == 0 || leftsize == TDB_PAD_U32) {
			goto update;
		}

		left = offset - leftsize;

		if (leftsize > offset ||
		    left < TDB_DATA_START(tdb->header.hash_size)) {
			goto update;
		}

		/* Now read in the left record */
		if (tdb->methods->tdb_read(tdb, left, &l, sizeof(l), DOCONV()) == -1) {
			TDB_LOG((tdb, TDB_DEBUG_FATAL, "tdb_free: left read failed at %u (%u)\n", left, leftsize));
			goto update;
		}

		/* If it's free, expand to include it. */
		if (l.magic == TDB_FREE_MAGIC) {
			/* we now merge the new record into the left record, rather than the other 
			   way around. This makes the operation O(1) instead of O(n). This change
			   prevents traverse from being O(n^2) after a lot of deletes */
			l.rec_len += sizeof(*rec) + rec->rec_len;
			if (tdb_rec_write(tdb, left, &l) == -1) {
				TDB_LOG((tdb, TDB_DEBUG_FATAL, "tdb_free: update_left failed at %u\n", left));
				goto fail;
			}
			if (update_tailer(tdb, left, &l) == -1) {
				TDB_LOG((tdb, TDB_DEBUG_FATAL, "tdb_free: update_tailer failed at %u\n", offset));
				goto fail;
			}
			tdb_unlock(tdb, -1, F_WRLCK);
			return 0;
		}
	}

update:

	/* Now, prepend to free list */
	rec->magic = TDB_FREE_MAGIC;

	if (tdb_ofs_read(tdb, FREELIST_TOP, &rec->next) == -1 ||
	    tdb_rec_write(tdb, offset, rec) == -1 ||
	    tdb_ofs_write(tdb, FREELIST_TOP, &offset) == -1) {
		TDB_LOG((tdb, TDB_DEBUG_FATAL, "tdb_free record write failed at offset=%d\n", offset));
		goto fail;
	}

	/* And we're done. */
	tdb_unlock(tdb, -1, F_WRLCK);
	return 0;

 fail:
	tdb_unlock(tdb, -1, F_WRLCK);
	return -1;
}



/* 
   the core of tdb_allocate - called when we have decided which
   free list entry to use

   Note that we try to allocate by grabbing data from the end of an existing record,
   not the beginning. This is so the left merge in a free is more likely to be
   able to free up the record without fragmentation
 */
static tdb_off_t tdb_allocate_ofs(struct tdb_context *tdb, 
				  tdb_len_t length, tdb_off_t rec_ptr,
				  struct tdb_record *rec, tdb_off_t last_ptr)
{
#define MIN_REC_SIZE (sizeof(struct tdb_record) + sizeof(tdb_off_t) + 8)

	if (rec->rec_len < length + MIN_REC_SIZE) {
		/* we have to grab the whole record */

		/* unlink it from the previous record */
		if (tdb_ofs_write(tdb, last_ptr, &rec->next) == -1) {
			return 0;
		}

		/* mark it not free */
		rec->magic = TDB_MAGIC;
		if (tdb_rec_write(tdb, rec_ptr, rec) == -1) {
			return 0;
		}
		return rec_ptr;
	}

	/* we're going to just shorten the existing record */
	rec->rec_len -= (length + sizeof(*rec));
	if (tdb_rec_write(tdb, rec_ptr, rec) == -1) {
		return 0;
	}
	if (update_tailer(tdb, rec_ptr, rec) == -1) {
		return 0;
	}

	/* and setup the new record */
	rec_ptr += sizeof(*rec) + rec->rec_len;	

	memset(rec, '\0', sizeof(*rec));
	rec->rec_len = length;
	rec->magic = TDB_MAGIC;

	if (tdb_rec_write(tdb, rec_ptr, rec) == -1) {
		return 0;
	}

	if (update_tailer(tdb, rec_ptr, rec) == -1) {
		return 0;
	}

	return rec_ptr;
}

/* allocate some space from the free list. The offset returned points
   to a unconnected tdb_record within the database with room for at
   least length bytes of total data

   0 is returned if the space could not be allocated
 */
tdb_off_t tdb_allocate(struct tdb_context *tdb, tdb_len_t length, struct tdb_record *rec)
{
	tdb_off_t rec_ptr, last_ptr, newrec_ptr;
	struct {
		tdb_off_t rec_ptr, last_ptr;
		tdb_len_t rec_len;
	} bestfit;
	float multiplier = 1.0;

	if (tdb_lock(tdb, -1, F_WRLCK) == -1)
		return 0;

	/* over-allocate to reduce fragmentation */
	length *= 1.25;

	/* Extra bytes required for tailer */
	length += sizeof(tdb_off_t);
	length = TDB_ALIGN(length, TDB_ALIGNMENT);

 again:
	last_ptr = FREELIST_TOP;

	/* read in the freelist top */
	if (tdb_ofs_read(tdb, FREELIST_TOP, &rec_ptr) == -1)
		goto fail;

	bestfit.rec_ptr = 0;
	bestfit.last_ptr = 0;
	bestfit.rec_len = 0;

	/* 
	   this is a best fit allocation strategy. Originally we used
	   a first fit strategy, but it suffered from massive fragmentation
	   issues when faced with a slowly increasing record size.
	 */
	while (rec_ptr) {
		if (tdb_rec_free_read(tdb, rec_ptr, rec) == -1) {
			goto fail;
		}

		if (rec->rec_len >= length) {
			if (bestfit.rec_ptr == 0 ||
			    rec->rec_len < bestfit.rec_len) {
				bestfit.rec_len = rec->rec_len;
				bestfit.rec_ptr = rec_ptr;
				bestfit.last_ptr = last_ptr;
			}
		}

		/* move to the next record */
		last_ptr = rec_ptr;
		rec_ptr = rec->next;

		/* if we've found a record that is big enough, then
		   stop searching if its also not too big. The
		   definition of 'too big' changes as we scan
		   through */
		if (bestfit.rec_len > 0 &&
		    bestfit.rec_len < length * multiplier) {
			break;
		}

		/* this multiplier means we only extremely rarely
		   search more than 50 or so records. At 50 records we
		   accept records up to 11 times larger than what we
		   want */
		multiplier *= 1.05;
	}

	if (bestfit.rec_ptr != 0) {
		if (tdb_rec_free_read(tdb, bestfit.rec_ptr, rec) == -1) {
			goto fail;
		}

		newrec_ptr = tdb_allocate_ofs(tdb, length, bestfit.rec_ptr, 
					      rec, bestfit.last_ptr);
		tdb_unlock(tdb, -1, F_WRLCK);
		return newrec_ptr;
	}

	/* we didn't find enough space. See if we can expand the
	   database and if we can then try again */
	if (tdb_expand(tdb, length + sizeof(*rec)) == 0)
		goto again;
 fail:
	tdb_unlock(tdb, -1, F_WRLCK);
	return 0;
}



/* 
   return the size of the freelist - used to decide if we should repack 
*/
_PUBLIC_ int tdb_freelist_size(struct tdb_context *tdb)
{
	tdb_off_t ptr;
	int count=0;

	if (tdb_lock(tdb, -1, F_RDLCK) == -1) {
		return -1;
	}

	ptr = FREELIST_TOP;
	while (tdb_ofs_read(tdb, ptr, &ptr) == 0 && ptr != 0) {
		count++;
	}

	tdb_unlock(tdb, -1, F_RDLCK);
	return count;
}
