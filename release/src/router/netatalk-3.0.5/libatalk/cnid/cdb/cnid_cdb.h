/* 
 * interface for database access to cnids. i do it this way to abstract
 * things a bit in case we want to change the underlying implementation.
 */

#ifndef _ATALK_CNID_CDB__H
#define _ATALK_CNID_CDB__H 1

#include <sys/stat.h>
#include <unistd.h>
#include <string.h>

#include <atalk/cnid.h>

/* cnid_open.c */
extern struct _cnid_module cnid_cdb_module;
extern struct _cnid_db *cnid_cdb_open (struct cnid_open_args *args);

/* cnid_close.c */
extern void cnid_cdb_close (struct _cnid_db *);

/* cnid_add.c */
extern cnid_t cnid_cdb_add (struct _cnid_db *, const struct stat *, cnid_t,
                            const char *, size_t, cnid_t);
extern int cnid_cdb_getstamp (struct _cnid_db *, void *, const size_t );

/* cnid_get.c */
extern cnid_t cnid_cdb_get (struct _cnid_db *, cnid_t, const char *, size_t); 
extern char *cnid_cdb_resolve (struct _cnid_db *, cnid_t *, void *, size_t ); 
extern cnid_t cnid_cdb_lookup (struct _cnid_db *, const struct stat *, cnid_t,
                               const char *, size_t);

/* cnid_update.c */
extern int cnid_cdb_update (struct _cnid_db *, cnid_t, const struct stat *,
                            cnid_t, const char *, size_t);

/* cnid_delete.c */
extern int cnid_cdb_delete (struct _cnid_db *, const cnid_t);

/* cnid_nextid.c */
extern cnid_t cnid_cdb_nextid (struct _cnid_db *);

extern int cnid_cdb_lock   (void *);
extern int cnid_cdb_unlock (void *);

extern cnid_t cnid_cdb_rebuild_add (struct _cnid_db *, const struct stat *,
                                    cnid_t, const char *, size_t, cnid_t);


#endif /* include/atalk/cnid_cdb.h */

