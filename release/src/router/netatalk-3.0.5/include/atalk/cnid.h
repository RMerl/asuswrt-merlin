/*
 * Copyright (c) 2003 the Netatalk Team
 * Copyright (c) 2003 Rafal Lewczuk <rlewczuk@pronet.pl>
 * Copyright (c) 2010 Frank Lahm
 *
 * This program is free software; you can redistribute and/or modify
 * it under the terms of the GNU General Public License as published
 * by the Free Software Foundation version 2 of the License or later
 * version if explicitly stated by any of above copyright holders.
 *
 */

/*
 * This file contains all generic CNID related stuff
 * declarations. Included:
 * - CNID factory, which retrieves (eventually instantiates)
 *   CNID objects on demand
 * - selection of CNID backends (default, detected by volume)
 * - full set of CNID operations needed by server core.
 */

#ifndef _ATALK_CNID__H
#define _ATALK_CNID__H 1

#include <atalk/adouble.h>
#include <atalk/list.h>

/* CNID object flags */
#define CNID_FLAG_PERSISTENT   0x01      /* This backend implements DID persistence */
#define CNID_FLAG_MANGLING     0x02      /* This backend has name mangling feature. */
#define CNID_FLAG_SETUID       0x04      /* Set db owner to parent folder owner. */
#define CNID_FLAG_BLOCK        0x08      /* block signals in update. */
#define CNID_FLAG_NODEV        0x10      /* don't use device number only inode */
#define CNID_FLAG_LAZY_INIT    0x20      /* */
#define CNID_FLAG_MEMORY       0x40  /* this is a memory only db */
#define CNID_FLAG_INODE        0x80  /* in cnid_add the inode is authoritative */

#define CNID_INVALID   0
/* first valid ID */
#define CNID_START     17

#define CNID_ERR_PARAM 0x80000001
#define CNID_ERR_PATH  0x80000002
#define CNID_ERR_DB    0x80000003
#define CNID_ERR_CLOSE 0x80000004   /* the db was not open */
#define CNID_ERR_MAX   0x80000005

/*
 * This is instance of CNID database object.
 */
struct _cnid_db {
    uint32_t flags;             /* Flags describing some CNID backend aspects. */
    char *volpath;               /* Volume path this particular CNID db refers to. */
    void *_private;              /* back-end speficic data */

    cnid_t (*cnid_add)         (struct _cnid_db *cdb, const struct stat *st, cnid_t did,
                                const char *name, size_t, cnid_t hint);
    int    (*cnid_delete)      (struct _cnid_db *cdb, cnid_t id);
    cnid_t (*cnid_get)         (struct _cnid_db *cdb, cnid_t did, const char *name, size_t);
    cnid_t (*cnid_lookup)      (struct _cnid_db *cdb, const struct stat *st, cnid_t did,
                                const char *name, size_t);
    cnid_t (*cnid_nextid)      (struct _cnid_db *cdb);
    char * (*cnid_resolve)     (struct _cnid_db *cdb, cnid_t *id, void *buffer, size_t len);
    int    (*cnid_update)      (struct _cnid_db *cdb, cnid_t id, const struct stat *st,
                                cnid_t did, const char *name, size_t len);
    void   (*cnid_close)       (struct _cnid_db *cdb);
    int    (*cnid_getstamp)    (struct _cnid_db *cdb, void *buffer, const size_t len);
    cnid_t (*cnid_rebuild_add) (struct _cnid_db *, const struct stat *, cnid_t,
                                const char *, size_t, cnid_t);
    int    (*cnid_find)        (struct _cnid_db *cdb, const char *name, size_t namelen,
                                void *buffer, size_t buflen);
    int    (*cnid_wipe)        (struct _cnid_db *cdb);
};
typedef struct _cnid_db cnid_db;

/*
 * Consolidation of args passedn from main cnid_open to modules cnid_XXX_open, so
 * that it's easier to add aditional args as required.
 */
struct cnid_open_args {
    const char *dir;
    mode_t mask;
    uint32_t flags;
    const char *cnidserver;      /* for dbd */
    const char *cnidport;        /* for dbd */
};

/*
 * CNID module - represents particular CNID implementation
 */
struct _cnid_module {
    char *name;
    struct list_head db_list;   /* CNID modules are also stored on a bidirectional list. */
    struct _cnid_db *(*cnid_open)(struct cnid_open_args *args);
    uint32_t flags;            /* Flags describing some CNID backend aspects. */

};
typedef struct _cnid_module cnid_module;

/* Inititalize the CNID backends */
void cnid_init();

/* Registers new CNID backend module */
void cnid_register(struct _cnid_module *module);

/* This function opens a CNID database for selected volume. */
struct _cnid_db *cnid_open(const char *volpath,
                           mode_t mask,
                           char *type,
                           int flags,
                           const char *cnidsrv,
                           const char *cnidport);
cnid_t cnid_add        (struct _cnid_db *cdb, const struct stat *st, const cnid_t did,
                        const char *name, const size_t len, cnid_t hint);
int    cnid_delete     (struct _cnid_db *cdb, cnid_t id);
cnid_t cnid_get        (struct _cnid_db *cdb, const cnid_t did, char *name,const size_t len);
int    cnid_getstamp   (struct _cnid_db *cdb, void *buffer, const size_t len);
cnid_t cnid_lookup     (struct _cnid_db *cdb, const struct stat *st, const cnid_t did,
                        char *name, const size_t len);
char  *cnid_resolve    (struct _cnid_db *cdb, cnid_t *id, void *buffer, size_t len);
int    cnid_update     (struct _cnid_db *cdb, const cnid_t id, const struct stat *st,
                        const cnid_t did, char *name, const size_t len);
cnid_t cnid_rebuild_add(struct _cnid_db *cdb, const struct stat *st, const cnid_t did,
                        char *name, const size_t len, cnid_t hint);
int    cnid_find       (struct _cnid_db *cdb, const char *name, size_t namelen,
                        void *buffer, size_t buflen);
int    cnid_wipe       (struct _cnid_db *cdb);
void   cnid_close      (struct _cnid_db *db);

#endif
