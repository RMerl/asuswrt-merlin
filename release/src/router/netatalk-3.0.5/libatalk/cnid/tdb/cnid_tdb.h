/* 
 * interface for database access to cnids. i do it this way to abstract
 * things a bit in case we want to change the underlying implementation.
 */

#ifndef _ATALK_CNID_TDB__H
#define _ATALK_CNID_TDB__H 1

#include <sys/stat.h>
#include <unistd.h>
#include <string.h>
#include <sys/param.h>
#include <arpa/inet.h>

#include <atalk/cnid.h>
#include <atalk/cnid_private.h>
#define STANDALONE 1

#include <stdlib.h>
#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <signal.h>
#include <atalk/tdb.h>

#define TDB_ERROR_LINK  1
#define TDB_ERROR_DEV   2
#define TDB_ERROR_INODE 4

struct _cnid_tdb_private {
    dev_t  st_dev;
    int    st_set;
    int    error;
    int    flags;
    TDB_CONTEXT *tdb_cnid;
    TDB_CONTEXT *tdb_didname;
    TDB_CONTEXT *tdb_devino;

};

/* cnid_open.c */
extern struct _cnid_module cnid_tdb_module;
extern struct _cnid_db *cnid_tdb_open (struct cnid_open_args *args);

/* cnid_close.c */
extern void cnid_tdb_close (struct _cnid_db *);

/* cnid_add.c */
extern cnid_t cnid_tdb_add (struct _cnid_db *, const struct stat *, cnid_t,
                            const char *, size_t, cnid_t);

/* cnid_get.c */
extern cnid_t cnid_tdb_get (struct _cnid_db *, cnid_t, const char *, size_t);
extern char *cnid_tdb_resolve (struct _cnid_db *, cnid_t *, void *, size_t);
extern cnid_t cnid_tdb_lookup (struct _cnid_db *, const struct stat *, cnid_t, const char *, size_t);

/* cnid_update.c */
extern int cnid_tdb_update (struct _cnid_db *, cnid_t, const struct stat *,
                            cnid_t, const char *, size_t);

/* cnid_delete.c */
extern int cnid_tdb_delete (struct _cnid_db *, const cnid_t);

/* cnid_nextid.c */
extern cnid_t cnid_tdb_nextid (struct _cnid_db *);

/* construct db_cnid data. NOTE: this is not re-entrant.  */
extern unsigned char *make_tdb_data(uint32_t flags, const struct stat *st, const cnid_t did, const char *name, const size_t len);

#endif /* include/atalk/cnid_tdb.h */
