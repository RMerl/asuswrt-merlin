 /* 
   Unix SMB/CIFS implementation.

   trivial database library - private includes

   Copyright (C) Andrew Tridgell              2005

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

#include "replace.h"
#include "system/filesys.h"
#include "system/time.h"
#include "system/shmem.h"
#include "system/select.h"
#include "system/wait.h"
#include "tdb.h"

/* #define TDB_TRACE 1 */
#ifndef HAVE_GETPAGESIZE
#define getpagesize() 0x2000
#endif

typedef uint32_t tdb_len_t;
typedef uint32_t tdb_off_t;

#ifndef offsetof
#define offsetof(t,f) ((unsigned int)&((t *)0)->f)
#endif

#define TDB_MAGIC_FOOD "TDB file\n"
#define TDB_VERSION (0x26011967 + 6)
#define TDB_MAGIC (0x26011999U)
#define TDB_FREE_MAGIC (~TDB_MAGIC)
#define TDB_DEAD_MAGIC (0xFEE1DEAD)
#define TDB_RECOVERY_MAGIC (0xf53bc0e7U)
#define TDB_RECOVERY_INVALID_MAGIC (0x0)
#define TDB_HASH_RWLOCK_MAGIC (0xbad1a51U)
#define TDB_ALIGNMENT 4
#define DEFAULT_HASH_SIZE 131
#define FREELIST_TOP (sizeof(struct tdb_header))
#define TDB_ALIGN(x,a) (((x) + (a)-1) & ~((a)-1))
#define TDB_BYTEREV(x) (((((x)&0xff)<<24)|((x)&0xFF00)<<8)|(((x)>>8)&0xFF00)|((x)>>24))
#define TDB_DEAD(r) ((r)->magic == TDB_DEAD_MAGIC)
#define TDB_BAD_MAGIC(r) ((r)->magic != TDB_MAGIC && !TDB_DEAD(r))
#define TDB_HASH_TOP(hash) (FREELIST_TOP + (BUCKET(hash)+1)*sizeof(tdb_off_t))
#define TDB_HASHTABLE_SIZE(tdb) ((tdb->header.hash_size+1)*sizeof(tdb_off_t))
#define TDB_DATA_START(hash_size) (TDB_HASH_TOP(hash_size-1) + sizeof(tdb_off_t))
#define TDB_RECOVERY_HEAD offsetof(struct tdb_header, recovery_start)
#define TDB_SEQNUM_OFS    offsetof(struct tdb_header, sequence_number)
#define TDB_PAD_BYTE 0x42
#define TDB_PAD_U32  0x42424242

/* NB assumes there is a local variable called "tdb" that is the
 * current context, also takes doubly-parenthesized print-style
 * argument. */
#ifdef VERBOSE_DEBUG
#define TDB_LOG(x) tdb->log.log_fn x
#else
#define TDB_LOG(x) do {} while(0)
#endif

#ifdef TDB_TRACE
void tdb_trace(struct tdb_context *tdb, const char *op);
void tdb_trace_seqnum(struct tdb_context *tdb, uint32_t seqnum, const char *op);
void tdb_trace_open(struct tdb_context *tdb, const char *op,
		    unsigned hash_size, unsigned tdb_flags, unsigned open_flags);
void tdb_trace_ret(struct tdb_context *tdb, const char *op, int ret);
void tdb_trace_retrec(struct tdb_context *tdb, const char *op, TDB_DATA ret);
void tdb_trace_1rec(struct tdb_context *tdb, const char *op,
		    TDB_DATA rec);
void tdb_trace_1rec_ret(struct tdb_context *tdb, const char *op,
			TDB_DATA rec, int ret);
void tdb_trace_1rec_retrec(struct tdb_context *tdb, const char *op,
			   TDB_DATA rec, TDB_DATA ret);
void tdb_trace_2rec_flag_ret(struct tdb_context *tdb, const char *op,
			     TDB_DATA rec1, TDB_DATA rec2, unsigned flag,
			     int ret);
void tdb_trace_2rec_retrec(struct tdb_context *tdb, const char *op,
			   TDB_DATA rec1, TDB_DATA rec2, TDB_DATA ret);
#else
#define tdb_trace(tdb, op)
#define tdb_trace_seqnum(tdb, seqnum, op)
#define tdb_trace_open(tdb, op, hash_size, tdb_flags, open_flags)
#define tdb_trace_ret(tdb, op, ret)
#define tdb_trace_retrec(tdb, op, ret)
#define tdb_trace_1rec(tdb, op, rec)
#define tdb_trace_1rec_ret(tdb, op, rec, ret)
#define tdb_trace_1rec_retrec(tdb, op, rec, ret)
#define tdb_trace_2rec_flag_ret(tdb, op, rec1, rec2, flag, ret)
#define tdb_trace_2rec_retrec(tdb, op, rec1, rec2, ret)
#endif /* !TDB_TRACE */

/* lock offsets */
#define OPEN_LOCK        0
#define ACTIVE_LOCK      4
#define TRANSACTION_LOCK 8

/* free memory if the pointer is valid and zero the pointer */
#ifndef SAFE_FREE
#define SAFE_FREE(x) do { if ((x) != NULL) {free(x); (x)=NULL;} } while(0)
#endif

#define BUCKET(hash) ((hash) % tdb->header.hash_size)

#define DOCONV() (tdb->flags & TDB_CONVERT)
#define CONVERT(x) (DOCONV() ? tdb_convert(&x, sizeof(x)) : &x)


/* the body of the database is made of one tdb_record for the free space
   plus a separate data list for each hash value */
struct tdb_record {
	tdb_off_t next; /* offset of the next record in the list */
	tdb_len_t rec_len; /* total byte length of record */
	tdb_len_t key_len; /* byte length of key */
	tdb_len_t data_len; /* byte length of data */
	uint32_t full_hash; /* the full 32 bit hash of the key */
	uint32_t magic;   /* try to catch errors */
	/* the following union is implied:
		union {
			char record[rec_len];
			struct {
				char key[key_len];
				char data[data_len];
			}
			uint32_t totalsize; (tailer)
		}
	*/
};


/* this is stored at the front of every database */
struct tdb_header {
	char magic_food[32]; /* for /etc/magic */
	uint32_t version; /* version of the code */
	uint32_t hash_size; /* number of hash entries */
	tdb_off_t rwlocks; /* obsolete - kept to detect old formats */
	tdb_off_t recovery_start; /* offset of transaction recovery region */
	tdb_off_t sequence_number; /* used when TDB_SEQNUM is set */
	uint32_t magic1_hash; /* hash of TDB_MAGIC_FOOD. */
	uint32_t magic2_hash; /* hash of TDB_MAGIC. */
	tdb_off_t reserved[27];
};

struct tdb_lock_type {
	uint32_t off;
	uint32_t count;
	uint32_t ltype;
};

struct tdb_traverse_lock {
	struct tdb_traverse_lock *next;
	uint32_t off;
	uint32_t hash;
	int lock_rw;
};

enum tdb_lock_flags {
	/* WAIT == F_SETLKW, NOWAIT == F_SETLK */
	TDB_LOCK_NOWAIT = 0,
	TDB_LOCK_WAIT = 1,
	/* If set, don't log an error on failure. */
	TDB_LOCK_PROBE = 2,
	/* If set, don't actually lock at all. */
	TDB_LOCK_MARK_ONLY = 4,
};

struct tdb_methods {
	int (*tdb_read)(struct tdb_context *, tdb_off_t , void *, tdb_len_t , int );
	int (*tdb_write)(struct tdb_context *, tdb_off_t, const void *, tdb_len_t);
	void (*next_hash_chain)(struct tdb_context *, uint32_t *);
	int (*tdb_oob)(struct tdb_context *, tdb_off_t , int );
	int (*tdb_expand_file)(struct tdb_context *, tdb_off_t , tdb_off_t );
};

struct tdb_context {
	char *name; /* the name of the database */
	void *map_ptr; /* where it is currently mapped */
	int fd; /* open file descriptor for the database */
	tdb_len_t map_size; /* how much space has been mapped */
	int read_only; /* opened read-only */
	int traverse_read; /* read-only traversal */
	int traverse_write; /* read-write traversal */
	struct tdb_lock_type allrecord_lock; /* .offset == upgradable */
	int num_lockrecs;
	struct tdb_lock_type *lockrecs; /* only real locks, all with count>0 */
	enum TDB_ERROR ecode; /* error code for last tdb error */
	struct tdb_header header; /* a cached copy of the header */
	uint32_t flags; /* the flags passed to tdb_open */
	struct tdb_traverse_lock travlocks; /* current traversal locks */
	struct tdb_context *next; /* all tdbs to avoid multiple opens */
	dev_t device;	/* uniquely identifies this tdb */
	ino_t inode;	/* uniquely identifies this tdb */
	struct tdb_logging_context log;
	unsigned int (*hash_fn)(TDB_DATA *key);
	int open_flags; /* flags used in the open - needed by reopen */
	const struct tdb_methods *methods;
	struct tdb_transaction *transaction;
	int page_size;
	int max_dead_records;
#ifdef TDB_TRACE
	int tracefd;
#endif
	volatile sig_atomic_t *interrupt_sig_ptr;
};


/*
  internal prototypes
*/
int tdb_munmap(struct tdb_context *tdb);
void tdb_mmap(struct tdb_context *tdb);
int tdb_lock(struct tdb_context *tdb, int list, int ltype);
int tdb_lock_nonblock(struct tdb_context *tdb, int list, int ltype);
int tdb_nest_lock(struct tdb_context *tdb, uint32_t offset, int ltype,
		  enum tdb_lock_flags flags);
int tdb_nest_unlock(struct tdb_context *tdb, uint32_t offset, int ltype,
		    bool mark_lock);
int tdb_unlock(struct tdb_context *tdb, int list, int ltype);
int tdb_brlock(struct tdb_context *tdb,
	       int rw_type, tdb_off_t offset, size_t len,
	       enum tdb_lock_flags flags);
int tdb_brunlock(struct tdb_context *tdb,
		 int rw_type, tdb_off_t offset, size_t len);
bool tdb_have_extra_locks(struct tdb_context *tdb);
void tdb_release_transaction_locks(struct tdb_context *tdb);
int tdb_transaction_lock(struct tdb_context *tdb, int ltype,
			 enum tdb_lock_flags lockflags);
int tdb_transaction_unlock(struct tdb_context *tdb, int ltype);
int tdb_recovery_area(struct tdb_context *tdb,
		      const struct tdb_methods *methods,
		      tdb_off_t *recovery_offset,
		      struct tdb_record *rec);
int tdb_allrecord_lock(struct tdb_context *tdb, int ltype,
		       enum tdb_lock_flags flags, bool upgradable);
int tdb_allrecord_unlock(struct tdb_context *tdb, int ltype, bool mark_lock);
int tdb_allrecord_upgrade(struct tdb_context *tdb);
int tdb_write_lock_record(struct tdb_context *tdb, tdb_off_t off);
int tdb_write_unlock_record(struct tdb_context *tdb, tdb_off_t off);
int tdb_ofs_read(struct tdb_context *tdb, tdb_off_t offset, tdb_off_t *d);
int tdb_ofs_write(struct tdb_context *tdb, tdb_off_t offset, tdb_off_t *d);
void *tdb_convert(void *buf, uint32_t size);
int tdb_free(struct tdb_context *tdb, tdb_off_t offset, struct tdb_record *rec);
tdb_off_t tdb_allocate(struct tdb_context *tdb, tdb_len_t length, struct tdb_record *rec);
int tdb_ofs_read(struct tdb_context *tdb, tdb_off_t offset, tdb_off_t *d);
int tdb_ofs_write(struct tdb_context *tdb, tdb_off_t offset, tdb_off_t *d);
int tdb_lock_record(struct tdb_context *tdb, tdb_off_t off);
int tdb_unlock_record(struct tdb_context *tdb, tdb_off_t off);
bool tdb_needs_recovery(struct tdb_context *tdb);
int tdb_rec_read(struct tdb_context *tdb, tdb_off_t offset, struct tdb_record *rec);
int tdb_rec_write(struct tdb_context *tdb, tdb_off_t offset, struct tdb_record *rec);
int tdb_do_delete(struct tdb_context *tdb, tdb_off_t rec_ptr, struct tdb_record *rec);
unsigned char *tdb_alloc_read(struct tdb_context *tdb, tdb_off_t offset, tdb_len_t len);
int tdb_parse_data(struct tdb_context *tdb, TDB_DATA key,
		   tdb_off_t offset, tdb_len_t len,
		   int (*parser)(TDB_DATA key, TDB_DATA data,
				 void *private_data),
		   void *private_data);
tdb_off_t tdb_find_lock_hash(struct tdb_context *tdb, TDB_DATA key, uint32_t hash, int locktype,
			   struct tdb_record *rec);
void tdb_io_init(struct tdb_context *tdb);
int tdb_expand(struct tdb_context *tdb, tdb_off_t size);
int tdb_rec_free_read(struct tdb_context *tdb, tdb_off_t off,
		      struct tdb_record *rec);
bool tdb_write_all(int fd, const void *buf, size_t count);
int tdb_transaction_recover(struct tdb_context *tdb);
void tdb_header_hash(struct tdb_context *tdb,
		     uint32_t *magic1_hash, uint32_t *magic2_hash);
unsigned int tdb_old_hash(TDB_DATA *key);
size_t tdb_dead_space(struct tdb_context *tdb, tdb_off_t off);
