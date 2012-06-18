#define STANDALONE	1
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

typedef unsigned tdb_len;
typedef unsigned tdb_off;

#define TDB_MAGIC_FOOD "TDB file\n"

/* this is stored at the front of every database */
struct tdb_header {
	char magic_food[32]; /* for /etc/magic */
	unsigned version; /* version of the code */
	unsigned hash_size; /* number of hash entries */
};

typedef struct {
	char *dptr;
	size_t dsize;
} TDB_DATA;

/* this is the context structure that is returned from a db open */
typedef struct {
	char *name; /* the name of the database */
	void *map_ptr; /* where it is currently mapped */
	int fd; /* open file descriptor for the database */
	tdb_len map_size; /* how much space has been mapped */
	int read_only; /* opened read-only */
	int *locked; /* set if we have a chain locked */
	int ecode; /* error code for last tdb error */
	struct tdb_header header; /* a cached copy of the header */
} TDB_CONTEXT;

/* flags to tdb_store() */
#define TDB_REPLACE 1
#define TDB_INSERT 2

/* flags for tdb_open() */
#define TDB_CLEAR_IF_FIRST 1

/* error codes */
enum TDB_ERROR {TDB_SUCCESS=0, TDB_ERR_CORRUPT, TDB_ERR_IO, TDB_ERR_LOCK, 
		TDB_ERR_OOM, TDB_ERR_EXISTS};

#if STANDALONE
TDB_CONTEXT *tdb_open(char *name, int hash_size, int tdb_flags,
		      int open_flags, mode_t mode);
char *tdb_error(TDB_CONTEXT *tdb);
int tdb_writelock(TDB_CONTEXT *tdb);
int tdb_writeunlock(TDB_CONTEXT *tdb);
TDB_DATA tdb_fetch(TDB_CONTEXT *tdb, TDB_DATA key);
int tdb_delete(TDB_CONTEXT *tdb, TDB_DATA key);
int tdb_store(TDB_CONTEXT *tdb, TDB_DATA key, TDB_DATA dbuf, int flag);
int tdb_close(TDB_CONTEXT *tdb);
TDB_DATA tdb_firstkey(TDB_CONTEXT *tdb);
TDB_DATA tdb_nextkey(TDB_CONTEXT *tdb, TDB_DATA key);
int tdb_traverse(TDB_CONTEXT *tdb, 
	int (*fn)(TDB_CONTEXT *tdb, TDB_DATA key, TDB_DATA dbuf, void *state),
	void *state);
int tdb_exists(TDB_CONTEXT *tdb, TDB_DATA key);
#endif
