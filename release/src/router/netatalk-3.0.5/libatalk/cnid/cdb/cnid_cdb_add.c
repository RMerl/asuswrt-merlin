/*
 * Copyright (c) 1999. Adrian Sun (asun@zoology.washington.edu)
 * All Rights Reserved. See COPYRIGHT.
 *
 * cnid_add (db, dev, ino, did, name, hint):
 * add a name to the CNID database. we use both dev/ino and did/name
 * to keep track of things.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif /* HAVE_CONFIG_H */

#ifdef CNID_BACKEND_CDB
#include <arpa/inet.h>
#include "cnid_cdb_private.h"

extern int cnid_cdb_update(struct _cnid_db *cdb, cnid_t id, const struct stat *st,
                           cnid_t did, const char *name, size_t len);

#define tid    NULL

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

unsigned char *make_cnid_data(u_int32_t flags, const struct stat *st, const cnid_t did,
                     const char *name, const size_t len)
{
    static unsigned char start[CNID_HEADER_LEN + MAXPATHLEN + 1];
    unsigned char *buf = start  +CNID_LEN;
    u_int32_t i;

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

/* --------------- */
static int db_stamp(void *buffer, size_t size)
{
time_t t;
    memset(buffer, 0, size);
    /* return the current time. */
    if (size < sizeof(time_t))
        return -1;
    t = time(NULL);
    memcpy(buffer,&t, sizeof(time_t));
    return 0;

}


/* ----------------------------- */
static cnid_t get_cnid(CNID_private *db)
{
    DBT rootinfo_key, rootinfo_data;
    DBC  *cursor;
    int rc;
    int flag, setstamp=0;
    cnid_t hint,id;
    char buf[ROOTINFO_DATALEN];
    char stamp[CNID_DEV_LEN];
     
    if ((rc = db->db_cnid->cursor(db->db_cnid, NULL, &cursor, DB_WRITECURSOR) ) != 0) {
        LOG(log_error, logtype_default, "get_cnid: Unable to get a cursor: %s", db_strerror(rc));
        return CNID_INVALID;
    }

    memset(&rootinfo_key, 0, sizeof(rootinfo_key));
    memset(&rootinfo_data, 0, sizeof(rootinfo_data));
    rootinfo_key.data = ROOTINFO_KEY;
    rootinfo_key.size = ROOTINFO_KEYLEN;

    switch (rc = cursor->c_get(cursor, &rootinfo_key, &rootinfo_data, DB_SET)) {
    case 0:
        memcpy(&hint, (char *)rootinfo_data.data +CNID_TYPE_OFS, sizeof(hint));
        id = ntohl(hint);
        /* If we've hit the max CNID allowed, we return a fatal error.  CNID
         * needs to be recycled before proceding. */
        if (++id == CNID_INVALID) {
            LOG(log_error, logtype_default, "cnid_add: FATAL: CNID database has reached its limit.");
            errno = CNID_ERR_MAX;
            goto cleanup;
        }
        hint = htonl(id);
        flag = DB_CURRENT;
        break;
    case DB_NOTFOUND:
        hint = htonl(CNID_START);
        flag = DB_KEYFIRST;
	setstamp = 1;
        break;
    default:
        LOG(log_error, logtype_default, "cnid_add: Unable to lookup rootinfo: %s", db_strerror(rc));
	errno = CNID_ERR_DB; 
        goto cleanup;
    }
    
    memcpy(buf, ROOTINFO_DATA, ROOTINFO_DATALEN);
    rootinfo_data.data = buf;
    rootinfo_data.size = ROOTINFO_DATALEN;
    memcpy((char *)rootinfo_data.data +CNID_TYPE_OFS, &hint, sizeof(hint));
    if (setstamp) {
        if (db_stamp(stamp, CNID_DEV_LEN) < 0) {
            goto cleanup;
        }
        memcpy((char *)rootinfo_data.data +CNID_DEV_OFS, stamp, sizeof(stamp));
    }

    
    switch (rc = cursor->c_put(cursor, &rootinfo_key, &rootinfo_data, flag)) {
    case 0:
        break;
    default:
        LOG(log_error, logtype_default, "cnid_add: Unable to update rootinfo: %s", db_strerror(rc));
	errno = CNID_ERR_DB; 
        goto cleanup;
    }
    if ((rc = cursor->c_close(cursor)) != 0) {
        LOG(log_error, logtype_default, "get_cnid: Unable to close cursor: %s", db_strerror(rc));
	errno = CNID_ERR_DB; 
        return CNID_INVALID;
    }
    return hint;
cleanup:
    if ((rc = cursor->c_close(cursor)) != 0) {
        LOG(log_error, logtype_default, "get_cnid: Unable to close cursor: %s", db_strerror(rc));
        return CNID_INVALID;
    }
    return CNID_INVALID;
}

/* ------------------------ */
cnid_t cnid_cdb_add(struct _cnid_db *cdb, const struct stat *st,
                    cnid_t did, const char *name, size_t len, cnid_t hint)
{
    CNID_private *db;
    DBT key, data;
    cnid_t id;
    int rc;

    if (!cdb || !(db = cdb->_private) || !st || !name) {
        errno = CNID_ERR_PARAM;
        return CNID_INVALID;
    }

    /* Do a lookup. */
    id = cnid_cdb_lookup(cdb, st, did, name, len);
    /* ... Return id if it is valid, or if Rootinfo is read-only. */
    if (id || (db->flags & CNIDFLAG_DB_RO)) {
#ifdef DEBUG
        LOG(log_debug9, logtype_default, "cnid_add: Looked up did %u, name %s as %u", ntohl(did), name, ntohl(id));
#endif
        return id;
    }

    /* Initialize our DBT data structures. */
    memset(&key, 0, sizeof(key));
    memset(&data, 0, sizeof(data));

    if ((data.data = make_cnid_data(cdb->flags, st, did, name, len)) == NULL) {
        LOG(log_error, logtype_default, "cnid_add: Path name is too long");
        errno = CNID_ERR_PATH;
        return CNID_INVALID;
    }
    data.size = CNID_HEADER_LEN + len + 1;

    if ((hint = get_cnid(db)) == 0) {
         errno = CNID_ERR_DB;
         return CNID_INVALID;
    }
    memcpy(data.data, &hint, sizeof(hint));
    
    key.data = &hint;
    key.size = sizeof(hint);

    /* Now we need to add the CNID data to the databases. */
    if ((rc = db->db_cnid->put(db->db_cnid, tid, &key, &data, DB_NOOVERWRITE))) {
        if (rc == EINVAL) {
            /* if we have a duplicate
             * on cnid it's a fatal error.
             * on dev:inode
             *   - leftover should have been delete before.
             *   - a second process already updated the db
             *   - it's a new file eg our file is already deleted and replaced
             * on did:name leftover
            */
            if (cnid_cdb_update(cdb, hint, st, did, name, len)) {
                errno = CNID_ERR_DB;
                return CNID_INVALID;
            }
        }
        else {
            LOG(log_error, logtype_default
                   , "cnid_add: Failed to add CNID for %s to database using hint %u: %s", 
                   name, ntohl(hint), db_strerror(rc));  
            errno = CNID_ERR_DB;
            return CNID_INVALID;
        }
    }

#ifdef DEBUG
    LOG(log_debug9, logtype_default, "cnid_add: Returned CNID for did %u, name %s as %u", ntohl(did), name, ntohl(hint));
#endif

    return hint;
}

/* cnid_cbd_getstamp */
/*-----------------------*/
int cnid_cdb_getstamp(struct _cnid_db *cdb, void *buffer, const size_t len)
{
    DBT key, data;
    int rc;
    CNID_private *db;

    if (!cdb || !(db = cdb->_private) || !buffer || !len) {
        errno = CNID_ERR_PARAM;
        return -1;
    }

    memset(buffer, 0, len);
    memset(&key, 0, sizeof(key));
    memset(&data, 0, sizeof(data));

    key.data = ROOTINFO_KEY;
    key.size = ROOTINFO_KEYLEN;

    if (0 != (rc = db->db_cnid->get(db->db_cnid, NULL, &key, &data, 0 )) ) {
        if (rc != DB_NOTFOUND) {
            LOG(log_error, logtype_default, "cnid_lookup: Unable to get database stamp: %s",
               db_strerror(rc));
            errno = CNID_ERR_DB;
            return -1;
        }
	/* we waste a single ID here... */
        get_cnid(db);
        memset(&key, 0, sizeof(key));
        memset(&data, 0, sizeof(data));
        key.data = ROOTINFO_KEY;
        key.size = ROOTINFO_KEYLEN;
	if (0 != (rc = db->db_cnid->get(db->db_cnid, NULL, &key, &data, 0 )) ) {
	    LOG(log_error, logtype_default, "cnid_getstamp: failed to get rootinfo: %s", 
               db_strerror(rc));
            errno = CNID_ERR_DB;
	    return -1;
        }
    }

    memcpy(buffer, (char*)data.data + CNID_DEV_OFS, len);
#ifdef DEBUG
    LOG(log_debug9, logtype_cnid, "cnid_getstamp: Returning stamp");
#endif
   return 0;
}


#endif /* CNID_BACKEND_CDB */
