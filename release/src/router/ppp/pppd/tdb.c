/* 
 * Database functions
 * Copyright (C) Andrew Tridgell 1999
 * 
 * Redistribution and use in source and binary forms are permitted
 * provided that the above copyright notice and this paragraph are
 * duplicated in all such forms AND provided that this software or
 * any derived work is only used as part of the PPP daemon (pppd)
 * and related utilities.
 * The name of the author may not be used to endorse or promote products
 * derived from this software without specific prior written permission.
 * THIS SOFTWARE IS PROVIDED ``AS IS'' AND WITHOUT ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, WITHOUT LIMITATION, THE IMPLIED
 * WARRANTIES OF MERCHANTIBILITY AND FITNESS FOR A PARTICULAR PURPOSE.
 *
 * Note: this software is also available under the Gnu Public License
 * version 2 or later.
 */

#include <stdlib.h>
#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include "tdb.h"

#define TDB_VERSION (0x26011967 + 1)
#define TDB_MAGIC (0x26011999U)
#define TDB_FREE_MAGIC (~TDB_MAGIC)
#define TDB_ALIGN 4
#define MIN_REC_SIZE (2*sizeof(struct list_struct) + TDB_ALIGN)
#define DEFAULT_HASH_SIZE 128
#define TDB_PAGE_SIZE 0x2000
#define TDB_LEN_MULTIPLIER 10
#define FREELIST_TOP (sizeof(struct tdb_header))

#define LOCK_SET 1
#define LOCK_CLEAR 0

/* lock offsets */
#define GLOBAL_LOCK 0
#define ACTIVE_LOCK 4
#define LIST_LOCK_BASE 1024

#define BUCKET(hash) ((hash) % tdb->header.hash_size)

#ifndef MAP_FILE
#define MAP_FILE 0
#endif

/* the body of the database is made of one list_struct for the free space
   plus a separate data list for each hash value */
struct list_struct {
	tdb_len rec_len; /* total byte length of record */
	tdb_off next; /* offset of the next record in the list */
	tdb_len key_len; /* byte length of key */
	tdb_len data_len; /* byte length of data */
	unsigned full_hash; /* the full 32 bit hash of the key */
	unsigned magic;   /* try to catch errors */
	/*
	   the following union is implied 
	   union {
              char record[rec_len];
	      struct {
	        char key[key_len];
		char data[data_len];
	      }
           }
	*/
};

/* a null data record - useful for error returns */
static TDB_DATA null_data;

/* a byte range locking function - return 0 on success
   this functions locks/unlocks 1 byte at the specified offset */
static int tdb_brlock(TDB_CONTEXT *tdb, tdb_off offset, 
		      int set, int rw_type, int lck_type)
{
#if NOLOCK
	return 0;
#else
	struct flock fl;

        if (tdb->fd == -1) return 0;   /* for in memory tdb */

	if (tdb->read_only) return -1;

	fl.l_type = set==LOCK_SET?rw_type:F_UNLCK;
	fl.l_whence = SEEK_SET;
	fl.l_start = offset;
	fl.l_len = 1;
	fl.l_pid = 0;

	if (fcntl(tdb->fd, lck_type, &fl) != 0) {
#if TDB_DEBUG
		if (lck_type == F_SETLKW) {
			printf("lock %d failed at %d (%s)\n", 
			       set, offset, strerror(errno));
		}
#endif
		tdb->ecode = TDB_ERR_LOCK;
		return -1;
	}
	return 0;
#endif
}

/* lock a list in the database. list -1 is the alloc list */
static int tdb_lock(TDB_CONTEXT *tdb, int list)
{
	if (list < -1 || list >= (int)tdb->header.hash_size) {
#if TDB_DEBUG
		printf("bad list %d\n", list);
#endif
		return -1;
	}
	if (tdb->locked[list+1] == 0) {
		if (tdb_brlock(tdb, LIST_LOCK_BASE + 4*list, LOCK_SET, 
			       F_WRLCK, F_SETLKW) != 0) {
			return -1;
		}
	}
	tdb->locked[list+1]++;
	return 0;
}

/* unlock the database. */
static int tdb_unlock(TDB_CONTEXT *tdb, int list)
{
	if (list < -1 || list >= (int)tdb->header.hash_size) {
#if TDB_DEBUG
		printf("bad unlock list %d\n", list);
#endif
		return -1;
	}

	if (tdb->locked[list+1] == 0) {
#if TDB_DEBUG
		printf("not locked %d\n", list);
#endif
		tdb->ecode = TDB_ERR_LOCK;
		return -1;
	}
	if (tdb->locked[list+1] == 1) {
		if (tdb_brlock(tdb, LIST_LOCK_BASE + 4*list, LOCK_CLEAR, 
			       F_WRLCK, F_SETLKW) != 0) {
			return -1;
		}
	}
	tdb->locked[list+1]--;
	return 0;
}

/* the hash algorithm - turn a key into an integer
   This is based on the hash agorithm from gdbm */
static unsigned tdb_hash(TDB_DATA *key)
{
	unsigned value;	/* Used to compute the hash value.  */
	unsigned   i;	/* Used to cycle through random values. */

	/* Set the initial value from the key size. */
	value = 0x238F13AF * key->dsize;
	for (i=0; i < key->dsize; i++) {
		value = (value + (key->dptr[i] << (i*5 % 24)));
	}

	value = (1103515243 * value + 12345);  

	return value;
}

/* find the top of the hash chain for an open database */
static tdb_off tdb_hash_top(TDB_CONTEXT *tdb, unsigned hash)
{
	tdb_off ret;
	hash = BUCKET(hash);
	ret = FREELIST_TOP + (hash+1)*sizeof(tdb_off);
	return ret;
}


/* check for an out of bounds access - if it is out of bounds then
   see if the database has been expanded by someone else and expand
   if necessary */
static int tdb_oob(TDB_CONTEXT *tdb, tdb_off offset)
{
	struct stat st;
	if ((offset <= tdb->map_size) || (tdb->fd == -1)) return 0;

	fstat(tdb->fd, &st);
	if (st.st_size <= (ssize_t)offset) {
		tdb->ecode = TDB_ERR_IO;
		return -1;
	}

#if HAVE_MMAP
	if (tdb->map_ptr) {
		munmap(tdb->map_ptr, tdb->map_size);
		tdb->map_ptr = NULL;
	}
#endif

	tdb->map_size = st.st_size;
#if HAVE_MMAP
	tdb->map_ptr = (void *)mmap(NULL, tdb->map_size, 
				    tdb->read_only?PROT_READ:PROT_READ|PROT_WRITE,
				    MAP_SHARED | MAP_FILE, tdb->fd, 0);
#endif	
	return 0;
}


/* write a lump of data at a specified offset */
static int tdb_write(TDB_CONTEXT *tdb, tdb_off offset, const char *buf, tdb_len len)
{
	if (tdb_oob(tdb, offset + len) != 0) {
		/* oops - trying to write beyond the end of the database! */
		return -1;
	}

	if (tdb->map_ptr) {
		memcpy(offset + (char *)tdb->map_ptr, buf, len);
	} else {
		if (lseek(tdb->fd, offset, SEEK_SET) != offset ||
		    write(tdb->fd, buf, len) != (ssize_t)len) {
			tdb->ecode = TDB_ERR_IO;
			return -1;
		}
	}
	return 0;
}

/* read a lump of data at a specified offset */
static int tdb_read(TDB_CONTEXT *tdb, tdb_off offset, char *buf, tdb_len len)
{
	if (tdb_oob(tdb, offset + len) != 0) {
		/* oops - trying to read beyond the end of the database! */
		return -1;
	}

	if (tdb->map_ptr) {
		memcpy(buf, offset + (char *)tdb->map_ptr, len);
	} else {
		if (lseek(tdb->fd, offset, SEEK_SET) != offset ||
		    read(tdb->fd, buf, len) != (ssize_t)len) {
			tdb->ecode = TDB_ERR_IO;
			return -1;
		}
	}
	return 0;
}


/* read a lump of data, allocating the space for it */
static char *tdb_alloc_read(TDB_CONTEXT *tdb, tdb_off offset, tdb_len len)
{
	char *buf;

	buf = (char *)malloc(len);

	if (!buf) {
		tdb->ecode = TDB_ERR_OOM;
		return NULL;
	}

	if (tdb_read(tdb, offset, buf, len) == -1) {
		free(buf);
		return NULL;
	}
	
	return buf;
}

/* convenience routine for writing a record */
static int rec_write(TDB_CONTEXT *tdb, tdb_off offset, struct list_struct *rec)
{
	return tdb_write(tdb, offset, (char *)rec, sizeof(*rec));
}

/* convenience routine for writing a tdb_off */
static int ofs_write(TDB_CONTEXT *tdb, tdb_off offset, tdb_off *d)
{
	return tdb_write(tdb, offset, (char *)d, sizeof(*d));
}

/* read a tdb_off from the store */
static int ofs_read(TDB_CONTEXT *tdb, tdb_off offset, tdb_off *d)
{
	return tdb_read(tdb, offset, (char *)d, sizeof(*d));
}

/* read a record and check for simple errors */
static int rec_read(TDB_CONTEXT *tdb, tdb_off offset, struct list_struct *rec)
{
	if (tdb_read(tdb, offset, (char *)rec, sizeof(*rec)) == -1) return -1;
	if (rec->magic != TDB_MAGIC) {
#if TDB_DEBUG
		printf("bad magic 0x%08x at offset %d\n",
		       rec->magic, offset);
#endif
		tdb->ecode = TDB_ERR_CORRUPT;
		return -1;
	}
	if (tdb_oob(tdb, rec->next) != 0) {
		return -1;
	}
	return 0;
}

/* expand the database at least length bytes by expanding the
   underlying file and doing the mmap again if necessary */
static int tdb_expand(TDB_CONTEXT *tdb, tdb_off length)
{
	struct list_struct rec;
	tdb_off offset, ptr;
	char b = 0;

	tdb_lock(tdb,-1);

	/* make sure we know about any previous expansions by another
           process */
	tdb_oob(tdb,tdb->map_size + 1);

	/* always make room for at least 10 more records */
	length *= TDB_LEN_MULTIPLIER;

	/* and round the database up to a multiple of TDB_PAGE_SIZE */
	length = ((tdb->map_size + length + TDB_PAGE_SIZE) & ~(TDB_PAGE_SIZE - 1)) - tdb->map_size;

	/* expand the file itself */
        if (tdb->fd != -1) {
            lseek(tdb->fd, tdb->map_size + length - 1, SEEK_SET);
            if (write(tdb->fd, &b, 1) != 1) goto fail;
        }

	/* form a new freelist record */
	offset = FREELIST_TOP;
	rec.rec_len = length - sizeof(rec);
	rec.magic = TDB_FREE_MAGIC;
	if (ofs_read(tdb, offset, &rec.next) == -1) {
		goto fail;
	}

#if HAVE_MMAP
	if (tdb->fd != -1 && tdb->map_ptr) {
		munmap(tdb->map_ptr, tdb->map_size);
		tdb->map_ptr = NULL;
	}
#endif

	tdb->map_size += length;

        if (tdb->fd == -1) {
            tdb->map_ptr = realloc(tdb->map_ptr, tdb->map_size);
        }

	/* write it out */
	if (rec_write(tdb, tdb->map_size - length, &rec) == -1) {
		goto fail;
	}

	/* link it into the free list */
	ptr = tdb->map_size - length;
	if (ofs_write(tdb, offset, &ptr) == -1) goto fail;

#if HAVE_MMAP
        if (tdb->fd != -1) {
            tdb->map_ptr = (void *)mmap(NULL, tdb->map_size, 
                                        PROT_READ|PROT_WRITE,
                                        MAP_SHARED | MAP_FILE, tdb->fd, 0);
        }
#endif

	tdb_unlock(tdb, -1);
	return 0;

 fail:
	tdb_unlock(tdb,-1);
	return -1;
}

/* allocate some space from the free list. The offset returned points
   to a unconnected list_struct within the database with room for at
   least length bytes of total data

   0 is returned if the space could not be allocated
 */
static tdb_off tdb_allocate(TDB_CONTEXT *tdb, tdb_len length)
{
	tdb_off offset, rec_ptr, last_ptr;
	struct list_struct rec, lastrec, newrec;

	tdb_lock(tdb, -1);

 again:
	last_ptr = 0;
	offset = FREELIST_TOP;

	/* read in the freelist top */
	if (ofs_read(tdb, offset, &rec_ptr) == -1) {
		goto fail;
	}

	/* keep looking until we find a freelist record that is big
           enough */
	while (rec_ptr) {
		if (tdb_read(tdb, rec_ptr, (char *)&rec, sizeof(rec)) == -1) {
			goto fail;
		}

		if (rec.magic != TDB_FREE_MAGIC) {
#if TDB_DEBUG
			printf("bad magic 0x%08x in free list\n", rec.magic);
#endif
			goto fail;
		}

		if (rec.rec_len >= length) {
			/* found it - now possibly split it up  */
			if (rec.rec_len > length + MIN_REC_SIZE) {
				length = (length + TDB_ALIGN) & ~(TDB_ALIGN-1);

				newrec.rec_len = rec.rec_len - (sizeof(rec) + length);
				newrec.next = rec.next;
				newrec.magic = TDB_FREE_MAGIC;

				rec.rec_len = length;
				rec.next = rec_ptr + sizeof(rec) + rec.rec_len;
				
				if (rec_write(tdb, rec.next, &newrec) == -1) {
					goto fail;
				}

				if (rec_write(tdb, rec_ptr, &rec) == -1) {
					goto fail;
				}
			}

			/* remove it from the list */
			if (last_ptr == 0) {
				offset = FREELIST_TOP;

				if (ofs_write(tdb, offset, &rec.next) == -1) {
					goto fail;
				}				
			} else {
				lastrec.next = rec.next;
				if (rec_write(tdb, last_ptr, &lastrec) == -1) {
					goto fail;
				}
			}

			/* all done - return the new record offset */
			tdb_unlock(tdb, -1);
			return rec_ptr;
		}

		/* move to the next record */
		lastrec = rec;
		last_ptr = rec_ptr;
		rec_ptr = rec.next;
	}

	/* we didn't find enough space. See if we can expand the
	   database and if we can then try again */
	if (tdb_expand(tdb, length + sizeof(rec)) == 0) goto again;

 fail:
#if TDB_DEBUG
	printf("tdb_allocate failed for size %u\n", length);
#endif
	tdb_unlock(tdb, -1);
	return 0;
}

/* initialise a new database with a specified hash size */
static int tdb_new_database(TDB_CONTEXT *tdb, int hash_size)
{
	struct tdb_header header;
	tdb_off offset;
	int i, size = 0;
	tdb_off buf[16];

        /* create the header */
        memset(&header, 0, sizeof(header));
        memcpy(header.magic_food, TDB_MAGIC_FOOD, strlen(TDB_MAGIC_FOOD)+1);
        header.version = TDB_VERSION;
        header.hash_size = hash_size;
        lseek(tdb->fd, 0, SEEK_SET);
        ftruncate(tdb->fd, 0);
        
        if (tdb->fd != -1 && write(tdb->fd, &header, sizeof(header)) != 
            sizeof(header)) {
            tdb->ecode = TDB_ERR_IO;
            return -1;
        } else size += sizeof(header);
	
        /* the freelist and hash pointers */
        offset = 0;
        memset(buf, 0, sizeof(buf));

        for (i=0;(hash_size+1)-i >= 16; i += 16) {
            if (tdb->fd != -1 && write(tdb->fd, buf, sizeof(buf)) != 
                sizeof(buf)) {
                tdb->ecode = TDB_ERR_IO;
                return -1;
            } else size += sizeof(buf);
        }

        for (;i<hash_size+1; i++) {
            if (tdb->fd != -1 && write(tdb->fd, buf, sizeof(tdb_off)) != 
                sizeof(tdb_off)) {
                tdb->ecode = TDB_ERR_IO;
                return -1;
            } else size += sizeof(tdb_off);
        }

        if (tdb->fd == -1) {
            tdb->map_ptr = calloc(size, 1);
            tdb->map_size = size;
            if (tdb->map_ptr == NULL) {
                tdb->ecode = TDB_ERR_IO;
                return -1;
            }
            memcpy(&tdb->header, &header, sizeof(header));
        }

#if TDB_DEBUG
	printf("initialised database of hash_size %u\n", 
	       hash_size);
#endif
	return 0;
}

/* Returns 0 on fail.  On success, return offset of record, and fills
   in rec */
static tdb_off tdb_find(TDB_CONTEXT *tdb, TDB_DATA key, unsigned int hash,
			struct list_struct *rec)
{
	tdb_off offset, rec_ptr;
	
	/* find the top of the hash chain */
	offset = tdb_hash_top(tdb, hash);

	/* read in the hash top */
	if (ofs_read(tdb, offset, &rec_ptr) == -1)
		return 0;

	/* keep looking until we find the right record */
	while (rec_ptr) {
		if (rec_read(tdb, rec_ptr, rec) == -1)
			return 0;

		if (hash == rec->full_hash && key.dsize == rec->key_len) {
			char *k;
			/* a very likely hit - read the key */
			k = tdb_alloc_read(tdb, rec_ptr + sizeof(*rec), 
					   rec->key_len);

			if (!k)
				return 0;

			if (memcmp(key.dptr, k, key.dsize) == 0) {
				free(k);
				return rec_ptr;
			}
			free(k);
		}

		/* move to the next record */
		rec_ptr = rec->next;
	}
	return 0;
}

/* 
   return an error string for the last tdb error
*/
char *tdb_error(TDB_CONTEXT *tdb)
{
	int i;
	static struct {
		enum TDB_ERROR ecode;
		char *estring;
	} emap[] = {
		{TDB_SUCCESS, "Success"},
		{TDB_ERR_CORRUPT, "Corrupt database"},
		{TDB_ERR_IO, "IO Error"},
		{TDB_ERR_LOCK, "Locking error"},
		{TDB_ERR_OOM, "Out of memory"},
		{TDB_ERR_EXISTS, "Record exists"},
		{-1, NULL}};
        if (tdb != NULL) {
            for (i=0;emap[i].estring;i++) {
		if (tdb->ecode == emap[i].ecode) return emap[i].estring;
            }
        } else {
            return "Invalid tdb context";
        }
	return "Invalid error code";
}


/* update an entry in place - this only works if the new data size
   is <= the old data size and the key exists.
   on failure return -1
*/
int tdb_update(TDB_CONTEXT *tdb, TDB_DATA key, TDB_DATA dbuf)
{
	unsigned hash;
	struct list_struct rec;
	tdb_off rec_ptr;
	int ret = -1;

        if (tdb == NULL) {
#ifdef TDB_DEBUG
            printf("tdb_update() called with null context\n");
#endif
            return -1;
        }

	/* find which hash bucket it is in */
	hash = tdb_hash(&key);

	tdb_lock(tdb, BUCKET(hash));
	rec_ptr = tdb_find(tdb, key, hash, &rec);

	if (!rec_ptr)
		goto out;

	/* must be long enough */
	if (rec.rec_len < key.dsize + dbuf.dsize)
		goto out;

	if (tdb_write(tdb, rec_ptr + sizeof(rec) + rec.key_len,
		      dbuf.dptr, dbuf.dsize) == -1)
		goto out;

	if (dbuf.dsize != rec.data_len) {
		/* update size */
		rec.data_len = dbuf.dsize;
		ret = rec_write(tdb, rec_ptr, &rec);
	} else
		ret = 0;

 out:
	tdb_unlock(tdb, BUCKET(hash));
	return ret;
}

/* find an entry in the database given a key */
TDB_DATA tdb_fetch(TDB_CONTEXT *tdb, TDB_DATA key)
{
	unsigned hash;
	tdb_off rec_ptr;
	struct list_struct rec;
	TDB_DATA ret = null_data;

        if (tdb == NULL) {
#ifdef TDB_DEBUG
            printf("tdb_fetch() called with null context\n");
#endif
            return null_data;
        }

	/* find which hash bucket it is in */
	hash = tdb_hash(&key);

	tdb_lock(tdb, BUCKET(hash));
	rec_ptr = tdb_find(tdb, key, hash, &rec);

	if (rec_ptr) {
		ret.dptr = tdb_alloc_read(tdb,
					  rec_ptr + sizeof(rec) + rec.key_len,
					  rec.data_len);
		ret.dsize = rec.data_len;
	}
	
	tdb_unlock(tdb, BUCKET(hash));
	return ret;
}

/* check if an entry in the database exists 

   note that 1 is returned if the key is found and 0 is returned if not found
   this doesn't match the conventions in the rest of this module, but is
   compatible with gdbm
*/
int tdb_exists(TDB_CONTEXT *tdb, TDB_DATA key)
{
	unsigned hash;
	tdb_off rec_ptr;
	struct list_struct rec;
	
        if (tdb == NULL) {
#ifdef TDB_DEBUG
            printf("tdb_exists() called with null context\n");
#endif
            return 0;
        }

	/* find which hash bucket it is in */
	hash = tdb_hash(&key);

	tdb_lock(tdb, BUCKET(hash));
	rec_ptr = tdb_find(tdb, key, hash, &rec);
	tdb_unlock(tdb, BUCKET(hash));

	return rec_ptr != 0;
}

/* traverse the entire database - calling fn(tdb, key, data) on each element.
   return -1 on error or the record count traversed
   if fn is NULL then it is not called
   a non-zero return value from fn() indicates that the traversal should stop
  */
int tdb_traverse(TDB_CONTEXT *tdb, int (*fn)(TDB_CONTEXT *tdb, TDB_DATA key, TDB_DATA dbuf, void* state), void* state)
{
	int count = 0;
	unsigned h;
	tdb_off offset, rec_ptr;
	struct list_struct rec;
	char *data;
	TDB_DATA key, dbuf;

        if (tdb == NULL) {
#ifdef TDB_DEBUG
            printf("tdb_traverse() called with null context\n");
#endif
            return -1;
        }

	/* loop over all hash chains */
	for (h = 0; h < tdb->header.hash_size; h++) {
		tdb_lock(tdb, BUCKET(h));

		/* read in the hash top */
		offset = tdb_hash_top(tdb, h);
		if (ofs_read(tdb, offset, &rec_ptr) == -1) {
			goto fail;
		}

		/* traverse all records for this hash */
		while (rec_ptr) {
			if (rec_read(tdb, rec_ptr, &rec) == -1) {
				goto fail;
			}

			/* now read the full record */
			data = tdb_alloc_read(tdb, rec_ptr + sizeof(rec), 
					     rec.key_len + rec.data_len);
			if (!data) {
				goto fail;
			}

			key.dptr = data;
			key.dsize = rec.key_len;
			dbuf.dptr = data + rec.key_len;
			dbuf.dsize = rec.data_len;
			count++;

			if (fn && fn(tdb, key, dbuf, state) != 0) {
				/* they want us to stop traversing */
				free(data);
				tdb_unlock(tdb, BUCKET(h));
				return count;
			}

			/* a miss - drat */
			free(data);

			/* move to the next record */
			rec_ptr = rec.next;
		}
		tdb_unlock(tdb, BUCKET(h));
	}

	/* return the number traversed */
	return count;

 fail:
	tdb_unlock(tdb, BUCKET(h));
	return -1;
}


/* find the first entry in the database and return its key */
TDB_DATA tdb_firstkey(TDB_CONTEXT *tdb)
{
	tdb_off offset, rec_ptr;
	struct list_struct rec;
	unsigned hash;
	TDB_DATA ret;

        if (tdb == NULL) {
#ifdef TDB_DEBUG
            printf("tdb_firstkey() called with null context\n");
#endif
            return null_data;
        }

	/* look for a non-empty hash chain */
	for (hash = 0, rec_ptr = 0; 
	     hash < tdb->header.hash_size;
	     hash++) {
		/* find the top of the hash chain */
		offset = tdb_hash_top(tdb, hash);

		tdb_lock(tdb, BUCKET(hash));

		/* read in the hash top */
		if (ofs_read(tdb, offset, &rec_ptr) == -1) {
			goto fail;
		}

		if (rec_ptr) break;

		tdb_unlock(tdb, BUCKET(hash));
	}

	if (rec_ptr == 0) return null_data;

	/* we've found a non-empty chain, now read the record */
	if (rec_read(tdb, rec_ptr, &rec) == -1) {
		goto fail;
	}

	/* allocate and read the key space */
	ret.dptr = tdb_alloc_read(tdb, rec_ptr + sizeof(rec), rec.key_len);
	ret.dsize = rec.key_len;
	tdb_unlock(tdb, BUCKET(hash));
	return ret;

 fail:
	tdb_unlock(tdb, BUCKET(hash));
	return null_data;
}

/* find the next entry in the database, returning its key */
TDB_DATA tdb_nextkey(TDB_CONTEXT *tdb, TDB_DATA key)
{
	unsigned hash, hbucket;
	tdb_off rec_ptr, offset;
	struct list_struct rec;
	TDB_DATA ret;

        if (tdb == NULL) {
#ifdef TDB_DEBUG
            printf("tdb_nextkey() called with null context\n");
#endif
            return null_data;
        }

	/* find which hash bucket it is in */
	hash = tdb_hash(&key);
	hbucket = BUCKET(hash);
	
	tdb_lock(tdb, hbucket);
	rec_ptr = tdb_find(tdb, key, hash, &rec);
	if (rec_ptr) {
		/* we want the next record after this one */
		rec_ptr = rec.next;
	}

	/* not found or last in hash: look for next non-empty hash chain */
	while (rec_ptr == 0) {
		tdb_unlock(tdb, hbucket);

		if (++hbucket >= tdb->header.hash_size - 1)
			return null_data;

		offset = tdb_hash_top(tdb, hbucket);
		tdb_lock(tdb, hbucket);
		/* read in the hash top */
		if (ofs_read(tdb, offset, &rec_ptr) == -1) {
			tdb_unlock(tdb, hbucket);
			return null_data;
		}
	}

	/* Read the record. */
	if (rec_read(tdb, rec_ptr, &rec) == -1) {
		tdb_unlock(tdb, hbucket);
		return null_data;
	}
	/* allocate and read the key */
	ret.dptr = tdb_alloc_read(tdb, rec_ptr + sizeof(rec), rec.key_len);
	ret.dsize = rec.key_len;
	tdb_unlock(tdb, hbucket);

	return ret;
}

/* delete an entry in the database given a key */
int tdb_delete(TDB_CONTEXT *tdb, TDB_DATA key)
{
	unsigned hash;
	tdb_off offset, rec_ptr, last_ptr;
	struct list_struct rec, lastrec;
	char *data = NULL;

        if (tdb == NULL) {
#ifdef TDB_DEBUG
            printf("tdb_delete() called with null context\n");
#endif
            return -1;
        }

	/* find which hash bucket it is in */
	hash = tdb_hash(&key);

	tdb_lock(tdb, BUCKET(hash));

	/* find the top of the hash chain */
	offset = tdb_hash_top(tdb, hash);

	/* read in the hash top */
	if (ofs_read(tdb, offset, &rec_ptr) == -1) {
		goto fail;
	}

	last_ptr = 0;

	/* keep looking until we find the right record */
	while (rec_ptr) {
		if (rec_read(tdb, rec_ptr, &rec) == -1) {
			goto fail;
		}

		if (hash == rec.full_hash && key.dsize == rec.key_len) {
			/* a very likely hit - read the record and full key */
			data = tdb_alloc_read(tdb, rec_ptr + sizeof(rec), 
					     rec.key_len);
			if (!data) {
				goto fail;
			}

			if (memcmp(key.dptr, data, key.dsize) == 0) {
				/* a definite match - delete it */
				if (last_ptr == 0) {
					offset = tdb_hash_top(tdb, hash);
					if (ofs_write(tdb, offset, &rec.next) == -1) {
						goto fail;
					}
				} else {
					lastrec.next = rec.next;
					if (rec_write(tdb, last_ptr, &lastrec) == -1) {
						goto fail;
					}					
				}
				tdb_unlock(tdb, BUCKET(hash));
				tdb_lock(tdb, -1);
				/* and recover the space */
				offset = FREELIST_TOP;
				if (ofs_read(tdb, offset, &rec.next) == -1) {
					goto fail2;
				}
				rec.magic = TDB_FREE_MAGIC;
				if (rec_write(tdb, rec_ptr, &rec) == -1) {
					goto fail2;
				}
				if (ofs_write(tdb, offset, &rec_ptr) == -1) {
					goto fail2;
				}

				/* yipee - all done */
				free(data);
				tdb_unlock(tdb, -1);
				return 0;
			}

			/* a miss - drat */
			free(data);
			data = NULL;
		}

		/* move to the next record */
		last_ptr = rec_ptr;
		lastrec = rec;
		rec_ptr = rec.next;
	}

 fail:
	if (data) free(data);
	tdb_unlock(tdb, BUCKET(hash));
	return -1;

 fail2:
	if (data) free(data);
	tdb_unlock(tdb, -1);
	return -1;
}


/* store an element in the database, replacing any existing element
   with the same key 

   return 0 on success, -1 on failure
*/
int tdb_store(TDB_CONTEXT *tdb, TDB_DATA key, TDB_DATA dbuf, int flag)
{
	struct list_struct rec;
	unsigned hash;
	tdb_off rec_ptr, offset;
	char *p = NULL;

        if (tdb == NULL) {
#ifdef TDB_DEBUG
            printf("tdb_store() called with null context\n");
#endif
            return -1;
        }

	/* find which hash bucket it is in */
	hash = tdb_hash(&key);

	/* check for it existing */
	if (flag == TDB_INSERT && tdb_exists(tdb, key)) {
		tdb->ecode = TDB_ERR_EXISTS;
		return -1;
	}

	/* first try in-place update */
	if (flag != TDB_INSERT && tdb_update(tdb, key, dbuf) == 0) {
		return 0;
	}

	rec_ptr = tdb_allocate(tdb, key.dsize + dbuf.dsize);
	if (rec_ptr == 0) {
		return -1;
	}

	tdb_lock(tdb, BUCKET(hash));

	/* delete any existing record - if it doesn't exist we don't care */
	if (flag != TDB_INSERT) {
		tdb_delete(tdb, key);
	}

	/* read the newly created record */
	if (tdb_read(tdb, rec_ptr, (char *)&rec, sizeof(rec)) == -1) {
		goto fail;
	}

	if (rec.magic != TDB_FREE_MAGIC) goto fail;

	/* find the top of the hash chain */
	offset = tdb_hash_top(tdb, hash);

	/* read in the hash top diretcly into our next pointer */
	if (ofs_read(tdb, offset, &rec.next) == -1) {
		goto fail;
	}

	rec.key_len = key.dsize;
	rec.data_len = dbuf.dsize;
	rec.full_hash = hash;
	rec.magic = TDB_MAGIC;

	p = (char *)malloc(sizeof(rec) + key.dsize + dbuf.dsize);
	if (!p) {
		tdb->ecode = TDB_ERR_OOM;
		goto fail;
	}

	memcpy(p, &rec, sizeof(rec));
	memcpy(p+sizeof(rec), key.dptr, key.dsize);
	memcpy(p+sizeof(rec)+key.dsize, dbuf.dptr, dbuf.dsize);

	if (tdb_write(tdb, rec_ptr, p, sizeof(rec)+key.dsize+dbuf.dsize) == -1)
		goto fail;

	free(p); 
	p = NULL;

	/* and point the top of the hash chain at it */
	if (ofs_write(tdb, offset, &rec_ptr) == -1) goto fail;

	tdb_unlock(tdb, BUCKET(hash));
	return 0;

 fail:
#if TDB_DEBUG
	printf("store failed for hash 0x%08x in bucket %u\n", hash, BUCKET(hash));
#endif
	if (p) free(p);
	tdb_unlock(tdb, BUCKET(hash));
	return -1;
}


/* open the database, creating it if necessary 

   The open_flags and mode are passed straight to the open call on the database
   file. A flags value of O_WRONLY is invalid

   The hash size is advisory, use zero for a default value. 

   return is NULL on error
*/
TDB_CONTEXT *tdb_open(char *name, int hash_size, int tdb_flags,
		      int open_flags, mode_t mode)
{
	TDB_CONTEXT tdb, *ret;
	struct stat st;

	memset(&tdb, 0, sizeof(tdb));

	tdb.fd = -1;
	tdb.name = NULL;
	tdb.map_ptr = NULL;

	if ((open_flags & O_ACCMODE) == O_WRONLY) {
		goto fail;
	}

	if (hash_size == 0) hash_size = DEFAULT_HASH_SIZE;

	tdb.read_only = ((open_flags & O_ACCMODE) == O_RDONLY);

        if (name != NULL) {
            tdb.fd = open(name, open_flags, mode);
            if (tdb.fd == -1) {
		goto fail;
            }
        }

	/* ensure there is only one process initialising at once */
	tdb_brlock(&tdb, GLOBAL_LOCK, LOCK_SET, F_WRLCK, F_SETLKW);
	
	if (tdb_flags & TDB_CLEAR_IF_FIRST) {
		/* we need to zero the database if we are the only
		   one with it open */
		if (tdb_brlock(&tdb, ACTIVE_LOCK, LOCK_SET, F_WRLCK, F_SETLK) == 0) {
			ftruncate(tdb.fd, 0);
			tdb_brlock(&tdb, ACTIVE_LOCK, LOCK_CLEAR, F_WRLCK, F_SETLK);
		}
	}

	/* leave this lock in place */
	tdb_brlock(&tdb, ACTIVE_LOCK, LOCK_SET, F_RDLCK, F_SETLKW);

	if (read(tdb.fd, &tdb.header, sizeof(tdb.header)) != sizeof(tdb.header) ||
	    strcmp(tdb.header.magic_food, TDB_MAGIC_FOOD) != 0 ||
	    tdb.header.version != TDB_VERSION) {
		/* its not a valid database - possibly initialise it */
		if (!(open_flags & O_CREAT)) {
			goto fail;
		}
		if (tdb_new_database(&tdb, hash_size) == -1) goto fail;

		lseek(tdb.fd, 0, SEEK_SET);
		if (tdb.fd != -1 && read(tdb.fd, &tdb.header, 
                                         sizeof(tdb.header)) != 
                                         sizeof(tdb.header)) 
                    goto fail;
	}

        if (tdb.fd != -1) {
            fstat(tdb.fd, &st);

            /* map the database and fill in the return structure */
            tdb.name = (char *)strdup(name);
            tdb.map_size = st.st_size;
        }

        tdb.locked = (int *)calloc(tdb.header.hash_size+1, 
                                   sizeof(tdb.locked[0]));
        if (!tdb.locked) {
            goto fail;
        }

#if HAVE_MMAP
        if (tdb.fd != -1) {
            tdb.map_ptr = (void *)mmap(NULL, st.st_size, 
                                       tdb.read_only? PROT_READ : PROT_READ|PROT_WRITE,
                                       MAP_SHARED | MAP_FILE, tdb.fd, 0);
        }
#endif

	ret = (TDB_CONTEXT *)malloc(sizeof(tdb));
	if (!ret) goto fail;

	*ret = tdb;

#if TDB_DEBUG
	printf("mapped database of hash_size %u map_size=%u\n", 
	       hash_size, tdb.map_size);
#endif

	tdb_brlock(&tdb, GLOBAL_LOCK, LOCK_CLEAR, F_WRLCK, F_SETLKW);
	return ret;

 fail:
        if (tdb.name) free(tdb.name);
	if (tdb.fd != -1) close(tdb.fd);
	if (tdb.map_ptr) munmap(tdb.map_ptr, tdb.map_size);

	return NULL;
}

/* close a database */
int tdb_close(TDB_CONTEXT *tdb)
{
	if (!tdb) return -1;

	if (tdb->name) free(tdb->name);
	if (tdb->fd != -1) close(tdb->fd);
	if (tdb->locked) free(tdb->locked);

	if (tdb->map_ptr) {
            if (tdb->fd != -1) {
                munmap(tdb->map_ptr, tdb->map_size);
            } else {
                free(tdb->map_ptr);
            }
        }

	memset(tdb, 0, sizeof(*tdb));
	free(tdb);

	return 0;
}

/* lock the database. If we already have it locked then don't do anything */
int tdb_writelock(TDB_CONTEXT *tdb)
{
        if (tdb == NULL) {
#ifdef TDB_DEBUG
            printf("tdb_writelock() called with null context\n");
#endif
            return -1;
        }

	return tdb_lock(tdb, -1);
}

/* unlock the database. */
int tdb_writeunlock(TDB_CONTEXT *tdb)
{
        if (tdb == NULL) {
#ifdef TDB_DEBUG
            printf("tdb_writeunlock() called with null context\n");
#endif
            return -1;
        }

	return tdb_unlock(tdb, -1);
}

/* lock one hash chain. This is meant to be used to reduce locking
   contention - it cannot guarantee how many records will be locked */
int tdb_lockchain(TDB_CONTEXT *tdb, TDB_DATA key)
{
        if (tdb == NULL) {
#ifdef TDB_DEBUG
            printf("tdb_lockchain() called with null context\n");
#endif
            return -1;
        }

	return tdb_lock(tdb, BUCKET(tdb_hash(&key)));
}


/* unlock one hash chain */
int tdb_unlockchain(TDB_CONTEXT *tdb, TDB_DATA key)
{
        if (tdb == NULL) {
#ifdef TDB_DEBUG
            printf("tdb_unlockchain() called with null context\n");
#endif
            return -1;
        }

	return tdb_unlock(tdb, BUCKET(tdb_hash(&key)));
}
