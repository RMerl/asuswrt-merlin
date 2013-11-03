/*
 * Copyright (c) 1999. Adrian Sun (asun@zoology.washington.edu)
 * All Rights Reserved. See COPYRIGHT.
 *
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#ifdef CNID_BACKEND_TDB
#include "cnid_tdb.h"
#include <atalk/util.h>
#include <sys/param.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <atalk/logger.h>

static void make_devino_data(unsigned char *buf, dev_t dev, ino_t ino)
{
    buf[CNID_DEV_LEN - 1] = dev; dev >>= 8;
    buf[CNID_DEV_LEN - 2] = dev; dev >>= 8;
    buf[CNID_DEV_LEN - 3] = dev; dev >>= 8;
    buf[CNID_DEV_LEN - 4] = dev; dev >>= 8;
    buf[CNID_DEV_LEN - 5] = dev; dev >>= 8;
    buf[CNID_DEV_LEN - 6] = dev; dev >>= 8;
    buf[CNID_DEV_LEN - 7] = dev; dev >>= 8;
    buf[CNID_DEV_LEN - 8] = dev;

    buf[CNID_DEV_LEN + CNID_INO_LEN - 1] = ino; ino >>= 8;
    buf[CNID_DEV_LEN + CNID_INO_LEN - 2] = ino; ino >>= 8;
    buf[CNID_DEV_LEN + CNID_INO_LEN - 3] = ino; ino >>= 8;
    buf[CNID_DEV_LEN + CNID_INO_LEN - 4] = ino; ino >>= 8;
    buf[CNID_DEV_LEN + CNID_INO_LEN - 5] = ino; ino >>= 8;
    buf[CNID_DEV_LEN + CNID_INO_LEN - 6] = ino; ino >>= 8;
    buf[CNID_DEV_LEN + CNID_INO_LEN - 7] = ino; ino >>= 8;
    buf[CNID_DEV_LEN + CNID_INO_LEN - 8] = ino;    
}

unsigned char *make_tdb_data(uint32_t flags, const struct stat *st,const cnid_t did,
                     const char *name, const size_t len)
{
    static unsigned char start[CNID_HEADER_LEN + MAXPATHLEN + 1];
    unsigned char *buf = start  +CNID_LEN;
    uint32_t i;

    if (len > MAXPATHLEN)
        return NULL;

    make_devino_data(buf, !(flags & CNID_FLAG_NODEV)?st->st_dev:0, st->st_ino);
    buf += CNID_DEVINO_LEN;

    i = S_ISDIR(st->st_mode)?1:0;
    i = htonl(i);
    memcpy(buf, &i, sizeof(i));
    buf += sizeof(i);
    
    /* did is already in network byte order */
    memcpy(buf, &did, sizeof(did));
    buf += sizeof(did);

    memcpy(buf, name, len);
    *(buf + len) = '\0';

    return start;
}    

/* add an entry to the CNID databases. we do this as a transaction
 * to prevent messiness. 
 * key:   cnid
 * data:
 */
static int add_cnid (struct _cnid_tdb_private *db, TDB_DATA *key, TDB_DATA *data) {
    TDB_DATA altkey, altdata;

    memset(&altkey, 0, sizeof(altkey));
    memset(&altdata, 0, sizeof(altdata));


    /* main database */
    if (tdb_store(db->tdb_cnid, *key, *data, TDB_REPLACE)) {
        goto abort;
    }

    /* dev/ino database */
    altkey.dptr = data->dptr +CNID_DEVINO_OFS;
    altkey.dsize = CNID_DEVINO_LEN;
    altdata.dptr = key->dptr;
    altdata.dsize = key->dsize;
    if (tdb_store(db->tdb_devino, altkey, altdata, TDB_REPLACE)) {
        goto abort;
    }

    /* did/name database */
    altkey.dptr = data->dptr +CNID_DID_OFS;
    altkey.dsize = data->dsize -CNID_DID_OFS;
    if (tdb_store(db->tdb_didname, altkey, altdata, TDB_REPLACE)) {
        goto abort;
    }
    return 0;

abort:
    return -1;
}

/* ----------------------- */
static cnid_t get_cnid(struct _cnid_tdb_private *db)
{
    TDB_DATA rootinfo_key, data;
    cnid_t hint,id;
    
    memset(&rootinfo_key, 0, sizeof(rootinfo_key));
    memset(&data, 0, sizeof(data));
    rootinfo_key.dptr = (unsigned char *)ROOTINFO_KEY;
    rootinfo_key.dsize = ROOTINFO_KEYLEN;
    
    tdb_chainlock(db->tdb_didname, rootinfo_key);  
    data = tdb_fetch(db->tdb_didname, rootinfo_key);
    if (data.dptr)
    {
        memcpy(&hint, data.dptr, sizeof(cnid_t));
        free(data.dptr);
        id = ntohl(hint);
        /* If we've hit the max CNID allowed, we return a fatal error.  CNID
         * needs to be recycled before proceding. */
        if (++id == CNID_INVALID) {
            LOG(log_error, logtype_default, "cnid_add: FATAL: CNID database has reached its limit.");
            errno = CNID_ERR_MAX;
            goto cleanup;
        }
        hint = htonl(id);
    }
    else {
        hint = htonl(CNID_START);
    }
    
    memset(&data, 0, sizeof(data));
    data.dptr = (unsigned char *)&hint;
    data.dsize = sizeof(hint);
    if (tdb_store(db->tdb_didname, rootinfo_key, data, TDB_REPLACE)) {
        goto cleanup;
    }

    tdb_chainunlock(db->tdb_didname, rootinfo_key );  
    return hint;
cleanup:
    tdb_chainunlock(db->tdb_didname, rootinfo_key);  
    return CNID_INVALID;
}


/* ------------------------ */
cnid_t cnid_tdb_add(struct _cnid_db *cdb, const struct stat *st,
                    cnid_t did, const char *name, size_t len, cnid_t hint)
{
    const struct stat *lstp;
    cnid_t id;
    struct _cnid_tdb_private *priv;
    TDB_DATA key, data; 
    int rc;      
    
    if (!cdb || !(priv = cdb->_private) || !st || !name) {
        errno = CNID_ERR_PARAM;
        return CNID_INVALID;
    }
    /* Do a lookup. */
    id = cnid_tdb_lookup(cdb, st, did, name, len);
    /* ... Return id if it is valid, or if Rootinfo is read-only. */
    if (id || (priv->flags & CNIDFLAG_DB_RO)) {
        return id;
    }

#if 0
    struct stat lst;
    lstp = lstat(name, &lst) < 0 ? st : &lst;
#endif
    lstp = st;

    /* Initialize our DBT data structures. */
    memset(&key, 0, sizeof(key));
    memset(&data, 0, sizeof(data));

    key.dptr = (unsigned char *)&hint;
    key.dsize = sizeof(cnid_t);
    if ((data.dptr = make_tdb_data(cdb->flags, lstp, did, name, len)) == NULL) {
        LOG(log_error, logtype_default, "tdb_add: Path name is too long");
        errno = CNID_ERR_PATH;
        return CNID_INVALID;
    }
    data.dsize = CNID_HEADER_LEN + len + 1;
    hint = get_cnid(priv);
    if (hint == 0) {
        errno = CNID_ERR_DB;
        return CNID_INVALID;
    }
    memcpy(data.dptr, &hint, sizeof(hint));
    
    /* Now we need to add the CNID data to the databases. */
    rc = add_cnid(priv, &key, &data);
    if (rc) {
        LOG(log_error, logtype_default, "tdb_add: Failed to add CNID for %s to database using hint %u", name, ntohl(hint));
        errno = CNID_ERR_DB;
        return CNID_INVALID;
    }

    return hint;
}

#endif
