/* 
 * interface for database access to cnids. i do it this way to abstract
 * things a bit in case we want to change the underlying implementation.
 */

#ifndef _ATALK_CNID_LAST__H
#define _ATALK_CNID_LAST__H 1

#include <sys/stat.h>
#include <unistd.h>
#include <string.h>

#include <atalk/cnid.h>

struct _cnid_last_private {
    cnid_t last_did;
};

extern struct _cnid_module cnid_last_module;
extern struct _cnid_db *cnid_last_open (struct cnid_open_args *args);
extern void cnid_last_close (struct _cnid_db *);
extern cnid_t cnid_last_add (struct _cnid_db *, const struct stat *, cnid_t,
                             const char *, size_t, cnid_t);
extern cnid_t cnid_last_get (struct _cnid_db *, cnid_t, const char *, size_t);
extern char *cnid_last_resolve (struct _cnid_db *, cnid_t *, void *, size_t);
extern cnid_t cnid_last_lookup (struct _cnid_db *, const struct stat *, cnid_t, const char *, size_t);
extern int cnid_last_update (struct _cnid_db *, cnid_t, const struct stat *,
                             cnid_t, const char *, size_t);
extern int cnid_last_delete (struct _cnid_db *, cnid_t);

#endif /* include/atalk/cnid_last.h */
