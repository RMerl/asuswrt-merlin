/*
 * Copyright (c) 1999. Adrian Sun (asun@zoology.washington.edu)
 * All Rights Reserved. See COPYRIGHT.
 *
 */
#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#ifdef CNID_BACKEND_TDB
#include <sys/param.h>   

#include "cnid_tdb.h"
#include <atalk/logger.h>
#include <stdlib.h>
#define DBHOME       ".AppleDB"
#define DBHOMELEN    9                  /* strlen(DBHOME) +1 for / */
#define DBLEN        12
#define DBCNID       "cnid2.tdb"

#define DBVERSION_KEY    "\0\0\0\0Version"
#define DBVERSION_KEYLEN (sizeof(DBVERSION_KEY))
#define DBVERSION2       0x00000002U
#define DBVERSION        DBVERSION2

static struct _cnid_db *cnid_tdb_new(const char *volpath)
{
    struct _cnid_db *cdb;
    struct _cnid_tdb_private *priv;

    if ((cdb = (struct _cnid_db *) calloc(1, sizeof(struct _cnid_db))) == NULL)
        return NULL;

    if ((cdb->volpath = strdup(volpath)) == NULL) {
        free(cdb);
        return NULL;
    }

    if ((cdb->_private = calloc(1, sizeof(struct _cnid_tdb_private))) == NULL) {
        free(cdb->volpath);
        free(cdb);
        return NULL;
    }

    /* Set up private state */
    priv = (struct _cnid_tdb_private *) (cdb->_private);

    /* Set up standard fields */
    cdb->flags = CNID_FLAG_PERSISTENT;

    cdb->cnid_add = cnid_tdb_add;
    cdb->cnid_delete = cnid_tdb_delete;
    cdb->cnid_get = cnid_tdb_get;
    cdb->cnid_lookup = cnid_tdb_lookup;
    cdb->cnid_nextid = NULL;    /*cnid_tdb_nextid;*/
    cdb->cnid_resolve = cnid_tdb_resolve;
    cdb->cnid_update = cnid_tdb_update;
    cdb->cnid_close = cnid_tdb_close;
    cdb->cnid_wipe = NULL;

    return cdb;
}

/* ---------------------------- */
struct _cnid_db *cnid_tdb_open(struct cnid_open_args *args)
{
    struct stat               st;
    struct _cnid_db           *cdb;
    struct _cnid_tdb_private *db;
    size_t                    len;
    char                      path[MAXPATHLEN + 1];
    TDB_DATA                  key, data;
    int 		      hash_size = 131071;
    int                       tdb_flags = 0;

    if (!args->dir) {
        /* note: dir and path are not used for in memory db */
        return NULL;
    }

    if ((len = strlen(args->dir)) > (MAXPATHLEN - DBLEN - 1)) {
        LOG(log_error, logtype_default, "tdb_open: Pathname too large: %s", args->dir);
        return NULL;
    }
    
    if ((cdb = cnid_tdb_new(args->dir)) == NULL) {
        LOG(log_error, logtype_default, "tdb_open: Unable to allocate memory for tdb");
        return NULL;
    }
    
    strcpy(path, args->dir);
    if (path[len - 1] != '/') {
        strcat(path, "/");
        len++;
    }
 
    strcpy(path + len, DBHOME);
    if (!(args->flags & CNID_FLAG_MEMORY)) {
        if ((stat(path, &st) < 0) && (ad_mkdir(path, 0777 & ~args->mask) < 0)) {
            LOG(log_error, logtype_default, "tdb_open: DBHOME mkdir failed for %s", path);
            goto fail;
        }
    }
    else {
        hash_size = 0;
        tdb_flags = TDB_INTERNAL;
    }
    strcat(path, "/");
 
    db = (struct _cnid_tdb_private *)cdb->_private;

    path[len + DBHOMELEN] = '\0';
    strcat(path, DBCNID);

    db->tdb_cnid = tdb_open(path, hash_size, tdb_flags , O_RDWR | O_CREAT, 0666 & ~args->mask);
    if (!db->tdb_cnid) {
        LOG(log_error, logtype_default, "tdb_open: unable to open tdb", path);
        goto fail;
    }
    /* ------------- */
    db->tdb_didname = db->tdb_cnid;
    db->tdb_devino = db->tdb_cnid;

    /* Check for version.  This way we can update the database if we need
     * to change the format in any way. */
    memset(&key, 0, sizeof(key));
    memset(&data, 0, sizeof(data));
    key.dptr = (unsigned char *)DBVERSION_KEY;
    key.dsize = DBVERSION_KEYLEN;

    data = tdb_fetch(db->tdb_didname, key);
    if (!data.dptr) {
        uint32_t version = htonl(DBVERSION);

        data.dptr = (unsigned char *)&version;
        data.dsize = sizeof(version);
        if (tdb_store(db->tdb_didname, key, data, TDB_REPLACE)) {
            LOG(log_error, logtype_default, "tdb_open: Error putting new version");
            goto fail;
        }
    }
    else {
        free(data.dptr);
    }
        
    return cdb;

fail:
    free(cdb->_private);
    free(cdb->volpath);
    free(cdb);
    
    return NULL;
}

struct _cnid_module cnid_tdb_module = {
    "tdb",
    {NULL, NULL},
    cnid_tdb_open,
    CNID_FLAG_SETUID | CNID_FLAG_BLOCK
};


#endif
