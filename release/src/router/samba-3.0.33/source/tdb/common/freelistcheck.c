/*
   Unix SMB/CIFS implementation.

   trivial database library

   Copyright (C) Jeremy Allison                    2006

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

/* Check the freelist is good and contains no loops.
   Very memory intensive - only do this as a consistency
   checker. Heh heh - uses an in memory tdb as the storage
   for the "seen" record list. For some reason this strikes
   me as extremely clever as I don't have to write another tree
   data structure implementation :-).
 */

static int seen_insert(struct tdb_context *mem_tdb, tdb_off_t rec_ptr)
{
	TDB_DATA key, data;

	memset(&data, '\0', sizeof(data));
	key.dptr = (char *)&rec_ptr;
	key.dsize = sizeof(rec_ptr);
	return tdb_store(mem_tdb, key, data, TDB_INSERT);
}

int tdb_validate_freelist(struct tdb_context *tdb, int *pnum_entries)
{
	struct tdb_context *mem_tdb = NULL;
	struct list_struct rec;
	tdb_off_t rec_ptr, last_ptr;
	int ret = -1;

	*pnum_entries = 0;

	mem_tdb = tdb_open("flval", tdb->header.hash_size,
				TDB_INTERNAL, O_RDWR, 0600);
	if (!mem_tdb) {
		return -1;
	}

	if (tdb_lock(tdb, -1, F_WRLCK) == -1) {
		tdb_close(mem_tdb);
		return 0;
	}

	last_ptr = FREELIST_TOP;

	/* Store the FREELIST_TOP record. */
	if (seen_insert(mem_tdb, last_ptr) == -1) {
		ret = TDB_ERRCODE(TDB_ERR_CORRUPT, -1);
		goto fail;
	}

	/* read in the freelist top */
	if (tdb_ofs_read(tdb, FREELIST_TOP, &rec_ptr) == -1) {
		goto fail;
	}

	while (rec_ptr) {

		/* If we can't store this record (we've seen it
		   before) then the free list has a loop and must
		   be corrupt. */

		if (seen_insert(mem_tdb, rec_ptr)) {
			ret = TDB_ERRCODE(TDB_ERR_CORRUPT, -1);
			goto fail;
		}

		if (rec_free_read(tdb, rec_ptr, &rec) == -1) {
			goto fail;
		}

		/* move to the next record */
		last_ptr = rec_ptr;
		rec_ptr = rec.next;
		*pnum_entries += 1;
	}

	ret = 0;

  fail:

	tdb_close(mem_tdb);
	tdb_unlock(tdb, -1, F_WRLCK);
	return ret;
}
