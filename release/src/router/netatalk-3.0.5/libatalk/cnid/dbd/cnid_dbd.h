/*
 * Copyright (C) Joerg Lenneis 2003
 * Copyright (C) Frank Lahm 2010
 * All Rights Reserved.  See COPYING.
 */


#ifndef _ATALK_CNID_DBD__H
#define _ATALK_CNID_DBD__H 1

#include <sys/stat.h>
#include <unistd.h>
#include <string.h>
#include <arpa/inet.h>

#include <atalk/cnid.h>

extern struct _cnid_module cnid_dbd_module;
extern struct _cnid_db *cnid_dbd_open (struct cnid_open_args *args);
extern void   cnid_dbd_close      (struct _cnid_db *);
extern cnid_t cnid_dbd_add        (struct _cnid_db *, const struct stat *, cnid_t,
                                   const char *, size_t, cnid_t);
extern cnid_t cnid_dbd_get        (struct _cnid_db *, cnid_t, const char *, size_t); 
extern char  *cnid_dbd_resolve    (struct _cnid_db *, cnid_t *, void *, size_t ); 
extern int    cnid_dbd_getstamp   (struct _cnid_db *, void *, const size_t ); 
extern cnid_t cnid_dbd_lookup     (struct _cnid_db *, const struct stat *, cnid_t,
                                   const char *, size_t);
extern int    cnid_dbd_find       (struct _cnid_db *cdb, const char *name, size_t namelen,
                                   void *buffer, size_t buflen);
extern int    cnid_dbd_update     (struct _cnid_db *, cnid_t, const struct stat *,
                                   cnid_t, const char *, size_t);
extern int    cnid_dbd_delete     (struct _cnid_db *, const cnid_t);
extern cnid_t cnid_dbd_rebuild_add(struct _cnid_db *, const struct stat *,
                                   cnid_t, const char *, size_t, cnid_t);
extern int    cnid_dbd_wipe       (struct _cnid_db *cdb);
/* FIXME: These functions could be static in cnid_dbd.c */

#endif /* include/atalk/cnid_dbd.h */

