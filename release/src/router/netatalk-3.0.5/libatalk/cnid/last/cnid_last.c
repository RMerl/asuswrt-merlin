
/*
 *
 * Copyright (c) 1999. Adrian Sun (asun@zoology.washington.edu)
 * All Rights Reserved. See COPYRIGHT.
 *
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif /* HAVE_CONFIG_H */

#ifdef CNID_BACKEND_LAST
#include <stdlib.h>
#include "cnid_last.h"
#include <atalk/util.h>
#include <atalk/logger.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <arpa/inet.h>

/* ------------------------ */
cnid_t cnid_last_add(struct _cnid_db *cdb, const struct stat *st,
                     cnid_t did _U_, const char *name _U_, size_t len _U_, cnid_t hint _U_)
{

    /* FIXME: it relies on fact, that this is never called twice for the same file/dir. */
    /* Propably we should look through DID tree. */

    /*
     * First thing:  DID and FNUMs are
     * in the same space for purposes of enumerate (and several
     * other wierd places).  While we consider this Apple's bug,
     * this is the work-around:  In order to maintain constant and
     * unique DIDs and FNUMs, we monotonically generate the DIDs
     * during the session, and derive the FNUMs from the filesystem.
     * Since the DIDs are small, we insure that the FNUMs are fairly
     * large by setting thier high bits to the device number.
     *
     * AFS already does something very similar to this for the
     * inode number, so we don't repeat the procedure.
     *
     * new algorithm:
     * due to complaints over did's being non-persistent,
     * here's the current hack to provide semi-persistent
     * did's:
     *      1) we reserve the first bit for file ids.
     *      2) the next 7 bits are for the device.
     *      3) the remaining 24 bits are for the inode.
     *
     * both the inode and device information are actually hashes
     * that are then truncated to the requisite bit length.
     *
     * it should be okay to use lstat to deal with symlinks.
     */

    struct _cnid_last_private *priv;

    if (!cdb || !(cdb->_private))
        return CNID_INVALID;

    priv = (struct _cnid_last_private *) (cdb->_private);

    if (S_ISDIR(st->st_mode))
        return htonl(priv->last_did++);
    else
        return htonl((st->st_dev << 16) | (st->st_ino & 0x0000ffff));
}



void cnid_last_close(struct _cnid_db *cdb)
{
    free(cdb->volpath);
    free(cdb->_private);
    free(cdb);
}



int cnid_last_delete(struct _cnid_db *cdb _U_, const cnid_t id _U_)
{
    return CNID_INVALID;
}


/* Return CNID for a given did/name. */
cnid_t cnid_last_get(struct _cnid_db *cdb _U_, cnid_t did _U_, const char *name _U_, size_t len _U_)
{
    /* FIXME: it relies on fact, that this is never called twice for the same file/dir. */
    /* Propably we should look through DID tree. */
    return CNID_INVALID;
}


/* */
cnid_t cnid_last_lookup(struct _cnid_db *cdb _U_, const struct stat *st _U_, cnid_t did _U_, 
                        const char *name _U_, size_t len _U_)
{
    /* FIXME: this function doesn't work in [last] scheme ! */
    /* Should be never called or CNID should be somewhat refactored again. */
    return CNID_INVALID;
}


static struct _cnid_db *cnid_last_new(const char *volpath)
{
    struct _cnid_db *cdb;
    struct _cnid_last_private *priv;

    if ((cdb = (struct _cnid_db *) calloc(1, sizeof(struct _cnid_db))) == NULL)
        return NULL;

    if ((cdb->volpath = strdup(volpath)) == NULL) {
        free(cdb);
        return NULL;
    }

    if ((cdb->_private = calloc(1, sizeof(struct _cnid_last_private))) == NULL) {
        free(cdb->volpath);
        free(cdb);
        return NULL;
    }

    /* Set up private state */
    priv = (struct _cnid_last_private *) (cdb->_private);
    priv->last_did = 17;

    /* Set up standard fields */
    cdb->flags = 0;
    cdb->cnid_add = cnid_last_add;
    cdb->cnid_delete = cnid_last_delete;
    cdb->cnid_get = cnid_last_get;
    cdb->cnid_lookup = cnid_last_lookup;
    cdb->cnid_nextid = NULL;    /* cnid_last_nextid; */
    cdb->cnid_resolve = cnid_last_resolve;
    cdb->cnid_update = cnid_last_update;
    cdb->cnid_close = cnid_last_close;
    cdb->cnid_wipe = NULL;

    return cdb;
}

struct _cnid_db *cnid_last_open(struct cnid_open_args *args)
{
    struct _cnid_db *cdb;

    if (!args->dir) {
        return NULL;
    }

    if ((cdb = cnid_last_new(args->dir)) == NULL) {
        LOG(log_error, logtype_default, "cnid_open: Unable to allocate memory for database");
        return NULL;
    }

    return cdb;
}

struct _cnid_module cnid_last_module = {
    "last",
    {NULL, NULL},
    cnid_last_open,
    0
};

/* Return the did/name pair corresponding to a CNID. */
char *cnid_last_resolve(struct _cnid_db *cdb _U_, cnid_t * id _U_, void *buffer _U_, size_t len _U_)
{
    /* FIXME: frankly, it does not work. As get, add and other functions. */
    return NULL;
}


int cnid_last_update(struct _cnid_db *cdb _U_, cnid_t id _U_, const struct stat *st _U_,
                     cnid_t did  _U_, const char *name  _U_, size_t len _U_)
{
    return 0;
}


#endif /* CNID_BACKEND_LAST */
