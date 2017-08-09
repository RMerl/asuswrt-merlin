 /*
   Unix SMB/CIFS implementation.

   trivial database library

   Copyright (C) Rusty Russell		   2009

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

/* Since we opened it, these shouldn't fail unless it's recent corruption. */
static bool tdb_check_header(struct tdb_context *tdb, tdb_off_t *recovery)
{
	struct tdb_header hdr;
	uint32_t h1, h2;

	if (tdb->methods->tdb_read(tdb, 0, &hdr, sizeof(hdr), 0) == -1)
		return false;
	if (strcmp(hdr.magic_food, TDB_MAGIC_FOOD) != 0)
		goto corrupt;

	CONVERT(hdr);
	if (hdr.version != TDB_VERSION)
		goto corrupt;

	if (hdr.rwlocks != 0 && hdr.rwlocks != TDB_HASH_RWLOCK_MAGIC)
		goto corrupt;

	tdb_header_hash(tdb, &h1, &h2);
	if (hdr.magic1_hash && hdr.magic2_hash &&
	    (hdr.magic1_hash != h1 || hdr.magic2_hash != h2))
		goto corrupt;

	if (hdr.hash_size == 0)
		goto corrupt;

	if (hdr.hash_size != tdb->header.hash_size)
		goto corrupt;

	if (hdr.recovery_start != 0 &&
	    hdr.recovery_start < TDB_DATA_START(tdb->header.hash_size))
		goto corrupt;

	*recovery = hdr.recovery_start;
	return true;

corrupt:
	tdb->ecode = TDB_ERR_CORRUPT;
	TDB_LOG((tdb, TDB_DEBUG_ERROR, "Header is corrupt\n"));
	return false;
}

/* Generic record header check. */
static bool tdb_check_record(struct tdb_context *tdb,
			     tdb_off_t off,
			     const struct tdb_record *rec)
{
	tdb_off_t tailer;

	/* Check rec->next: 0 or points to record offset, aligned. */
	if (rec->next > 0 && rec->next < TDB_DATA_START(tdb->header.hash_size)){
		TDB_LOG((tdb, TDB_DEBUG_ERROR,
			 "Record offset %d too small next %d\n",
			 off, rec->next));
		goto corrupt;
	}
	if (rec->next + sizeof(*rec) < rec->next) {
		TDB_LOG((tdb, TDB_DEBUG_ERROR,
			 "Record offset %d too large next %d\n",
			 off, rec->next));
		goto corrupt;
	}
	if ((rec->next % TDB_ALIGNMENT) != 0) {
		TDB_LOG((tdb, TDB_DEBUG_ERROR,
			 "Record offset %d misaligned next %d\n",
			 off, rec->next));
		goto corrupt;
	}
	if (tdb->methods->tdb_oob(tdb, rec->next+sizeof(*rec), 0))
		goto corrupt;

	/* Check rec_len: similar to rec->next, implies next record. */
	if ((rec->rec_len % TDB_ALIGNMENT) != 0) {
		TDB_LOG((tdb, TDB_DEBUG_ERROR,
			 "Record offset %d misaligned length %d\n",
			 off, rec->rec_len));
		goto corrupt;
	}
	/* Must fit tailer. */
	if (rec->rec_len < sizeof(tailer)) {
		TDB_LOG((tdb, TDB_DEBUG_ERROR,
			 "Record offset %d too short length %d\n",
			 off, rec->rec_len));
		goto corrupt;
	}
	/* OOB allows "right at the end" access, so this works for last rec. */
	if (tdb->methods->tdb_oob(tdb, off+sizeof(*rec)+rec->rec_len, 0))
		goto corrupt;

	/* Check tailer. */
	if (tdb_ofs_read(tdb, off+sizeof(*rec)+rec->rec_len-sizeof(tailer),
			 &tailer) == -1)
		goto corrupt;
	if (tailer != sizeof(*rec) + rec->rec_len) {
		TDB_LOG((tdb, TDB_DEBUG_ERROR,
			 "Record offset %d invalid tailer\n", off));
		goto corrupt;
	}

	return true;

corrupt:
	tdb->ecode = TDB_ERR_CORRUPT;
	return false;
}

/* Grab some bytes: may copy if can't use mmap.
   Caller has already done bounds check. */
static TDB_DATA get_bytes(struct tdb_context *tdb,
			  tdb_off_t off, tdb_len_t len)
{
	TDB_DATA d;

	d.dsize = len;

	if (tdb->transaction == NULL && tdb->map_ptr != NULL)
		d.dptr = (unsigned char *)tdb->map_ptr + off;
	else
		d.dptr = tdb_alloc_read(tdb, off, d.dsize);
	return d;
}

/* Frees data if we're not able to simply use mmap. */
static void put_bytes(struct tdb_context *tdb, TDB_DATA d)
{
	if (tdb->transaction == NULL && tdb->map_ptr != NULL)
		return;
	free(d.dptr);
}

/* We use the excellent Jenkins lookup3 hash; this is based on hash_word2.
 * See: http://burtleburtle.net/bob/c/lookup3.c
 */
#define rot(x,k) (((x)<<(k)) | ((x)>>(32-(k))))
static void hash(uint32_t key, uint32_t *pc, uint32_t *pb)
{
	uint32_t a,b,c;

	/* Set up the internal state */
	a = b = c = 0xdeadbeef + *pc;
	c += *pb;
	a += key;
	c ^= b; c -= rot(b,14);
	a ^= c; a -= rot(c,11);
	b ^= a; b -= rot(a,25);
	c ^= b; c -= rot(b,16);
	a ^= c; a -= rot(c,4);
	b ^= a; b -= rot(a,14);
	c ^= b; c -= rot(b,24);
	*pc=c; *pb=b;
}

/*
  We want to check that all free records are in the free list
  (only once), and all free list entries are free records.  Similarly
  for each hash chain of used records.

  Doing that naively (without walking hash chains, since we want to be
  linear) means keeping a list of records which have been seen in each
  hash chain, and another of records pointed to (ie. next pointers
  from records and the initial hash chain heads).  These two lists
  should be equal.  This will take 8 bytes per record, and require
  sorting at the end.

  So instead, we record each offset in a bitmap such a way that
  recording it twice will cancel out.  Since each offset should appear
  exactly twice, the bitmap should be zero at the end.

  The approach was inspired by Bloom Filters (see Wikipedia).  For
  each value, we flip K bits in a bitmap of size N.  The number of
  distinct arrangements is:

	N! / (K! * (N-K)!)

  Of course, not all arrangements are actually distinct, but testing
  shows this formula to be close enough.

  So, if K == 8 and N == 256, the probability of two things flipping the same
  bits is 1 in 409,663,695,276,000.

  Given that ldb uses a hash size of 10000, using 32 bytes per hash chain
  (320k) seems reasonable.
*/
#define NUM_HASHES 8
#define BITMAP_BITS 256

static void bit_flip(unsigned char bits[], unsigned int idx)
{
	bits[idx / CHAR_BIT] ^= (1 << (idx % CHAR_BIT));
}

/* We record offsets in a bitmap for the particular chain it should be in.  */
static void record_offset(unsigned char bits[], tdb_off_t off)
{
	uint32_t h1 = off, h2 = 0;
	unsigned int i;

	/* We get two good hash values out of jhash2, so we use both.  Then
	 * we keep going to produce further hash values. */
	for (i = 0; i < NUM_HASHES / 2; i++) {
		hash(off, &h1, &h2);
		bit_flip(bits, h1 % BITMAP_BITS);
		bit_flip(bits, h2 % BITMAP_BITS);
		h2++;
	}
}

/* Check that an in-use record is valid. */
static bool tdb_check_used_record(struct tdb_context *tdb,
				  tdb_off_t off,
				  const struct tdb_record *rec,
				  unsigned char **hashes,
				  int (*check)(TDB_DATA, TDB_DATA, void *),
				  void *private_data)
{
	TDB_DATA key, data;

	if (!tdb_check_record(tdb, off, rec))
		return false;

	/* key + data + tailer must fit in record */
	if (rec->key_len + rec->data_len + sizeof(tdb_off_t) > rec->rec_len) {
		TDB_LOG((tdb, TDB_DEBUG_ERROR,
			 "Record offset %d too short for contents\n", off));
		return false;
	}

	key = get_bytes(tdb, off + sizeof(*rec), rec->key_len);
	if (!key.dptr)
		return false;

	if (tdb->hash_fn(&key) != rec->full_hash) {
		TDB_LOG((tdb, TDB_DEBUG_ERROR,
			 "Record offset %d has incorrect hash\n", off));
		goto fail_put_key;
	}

	/* Mark this offset as a known value for this hash bucket. */
	record_offset(hashes[BUCKET(rec->full_hash)+1], off);
	/* And similarly if the next pointer is valid. */
	if (rec->next)
		record_offset(hashes[BUCKET(rec->full_hash)+1], rec->next);

	/* If they supply a check function and this record isn't dead,
	   get data and feed it. */
	if (check && rec->magic != TDB_DEAD_MAGIC) {
		data = get_bytes(tdb, off + sizeof(*rec) + rec->key_len,
				 rec->data_len);
		if (!data.dptr)
			goto fail_put_key;

		if (check(key, data, private_data) == -1)
			goto fail_put_data;
		put_bytes(tdb, data);
	}

	put_bytes(tdb, key);
	return true;

fail_put_data:
	put_bytes(tdb, data);
fail_put_key:
	put_bytes(tdb, key);
	return false;
}

/* Check that an unused record is valid. */
static bool tdb_check_free_record(struct tdb_context *tdb,
				  tdb_off_t off,
				  const struct tdb_record *rec,
				  unsigned char **hashes)
{
	if (!tdb_check_record(tdb, off, rec))
		return false;

	/* Mark this offset as a known value for the free list. */
	record_offset(hashes[0], off);
	/* And similarly if the next pointer is valid. */
	if (rec->next)
		record_offset(hashes[0], rec->next);
	return true;
}

/* Slow, but should be very rare. */
size_t tdb_dead_space(struct tdb_context *tdb, tdb_off_t off)
{
	size_t len;

	for (len = 0; off + len < tdb->map_size; len++) {
		char c;
		if (tdb->methods->tdb_read(tdb, off, &c, 1, 0))
			return 0;
		if (c != 0 && c != 0x42)
			break;
	}
	return len;
}

_PUBLIC_ int tdb_check(struct tdb_context *tdb,
	      int (*check)(TDB_DATA key, TDB_DATA data, void *private_data),
	      void *private_data)
{
	unsigned int h;
	unsigned char **hashes;
	tdb_off_t off, recovery_start;
	struct tdb_record rec;
	bool found_recovery = false;
	tdb_len_t dead;
	bool locked;

	/* Read-only databases use no locking at all: it's best-effort.
	 * We may have a write lock already, so skip that case too. */
	if (tdb->read_only || tdb->allrecord_lock.count != 0) {
		locked = false;
	} else {
		if (tdb_lockall_read(tdb) == -1)
			return -1;
		locked = true;
	}

	/* Make sure we know true size of the underlying file. */
	tdb->methods->tdb_oob(tdb, tdb->map_size + 1, 1);

	/* Header must be OK: also gets us the recovery ptr, if any. */
	if (!tdb_check_header(tdb, &recovery_start))
		goto unlock;

	/* We should have the whole header, too. */
	if (tdb->map_size < TDB_DATA_START(tdb->header.hash_size)) {
		tdb->ecode = TDB_ERR_CORRUPT;
		TDB_LOG((tdb, TDB_DEBUG_ERROR, "File too short for hashes\n"));
		goto unlock;
	}

	/* One big malloc: pointers then bit arrays. */
	hashes = (unsigned char **)calloc(
			1, sizeof(hashes[0]) * (1+tdb->header.hash_size)
			+ BITMAP_BITS / CHAR_BIT * (1+tdb->header.hash_size));
	if (!hashes) {
		tdb->ecode = TDB_ERR_OOM;
		goto unlock;
	}

	/* Initialize pointers */
	hashes[0] = (unsigned char *)(&hashes[1+tdb->header.hash_size]);
	for (h = 1; h < 1+tdb->header.hash_size; h++)
		hashes[h] = hashes[h-1] + BITMAP_BITS / CHAR_BIT;

	/* Freelist and hash headers are all in a row: read them. */
	for (h = 0; h < 1+tdb->header.hash_size; h++) {
		if (tdb_ofs_read(tdb, FREELIST_TOP + h*sizeof(tdb_off_t),
				 &off) == -1)
			goto free;
		if (off)
			record_offset(hashes[h], off);
	}

	/* For each record, read it in and check it's ok. */
	for (off = TDB_DATA_START(tdb->header.hash_size);
	     off < tdb->map_size;
	     off += sizeof(rec) + rec.rec_len) {
		if (tdb->methods->tdb_read(tdb, off, &rec, sizeof(rec),
					   DOCONV()) == -1)
			goto free;
		switch (rec.magic) {
		case TDB_MAGIC:
		case TDB_DEAD_MAGIC:
			if (!tdb_check_used_record(tdb, off, &rec, hashes,
						   check, private_data))
				goto free;
			break;
		case TDB_FREE_MAGIC:
			if (!tdb_check_free_record(tdb, off, &rec, hashes))
				goto free;
			break;
		/* If we crash after ftruncate, we can get zeroes or fill. */
		case TDB_RECOVERY_INVALID_MAGIC:
		case 0x42424242:
			if (recovery_start == off) {
				found_recovery = true;
				break;
			}
			dead = tdb_dead_space(tdb, off);
			if (dead < sizeof(rec))
				goto corrupt;

			TDB_LOG((tdb, TDB_DEBUG_ERROR,
				 "Dead space at %d-%d (of %u)\n",
				 off, off + dead, tdb->map_size));
			rec.rec_len = dead - sizeof(rec);
			break;
		case TDB_RECOVERY_MAGIC:
			if (recovery_start != off) {
				TDB_LOG((tdb, TDB_DEBUG_ERROR,
					 "Unexpected recovery record at offset %d\n",
					 off));
				goto free;
			}
			found_recovery = true;
			break;
		default: ;
		corrupt:
			tdb->ecode = TDB_ERR_CORRUPT;
			TDB_LOG((tdb, TDB_DEBUG_ERROR,
				 "Bad magic 0x%x at offset %d\n",
				 rec.magic, off));
			goto free;
		}
	}

	/* Now, hashes should all be empty: each record exists and is referred
	 * to by one other. */
	for (h = 0; h < 1+tdb->header.hash_size; h++) {
		unsigned int i;
		for (i = 0; i < BITMAP_BITS / CHAR_BIT; i++) {
			if (hashes[h][i] != 0) {
				tdb->ecode = TDB_ERR_CORRUPT;
				TDB_LOG((tdb, TDB_DEBUG_ERROR,
					 "Hashes do not match records\n"));
				goto free;
			}
		}
	}

	/* We must have found recovery area if there was one. */
	if (recovery_start != 0 && !found_recovery) {
		TDB_LOG((tdb, TDB_DEBUG_ERROR,
			 "Expected a recovery area at %u\n",
			 recovery_start));
		goto free;
	}

	free(hashes);
	if (locked) {
		tdb_unlockall_read(tdb);
	}
	return 0;

free:
	free(hashes);
unlock:
	if (locked) {
		tdb_unlockall_read(tdb);
	}
	return -1;
}
