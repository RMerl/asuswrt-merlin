/*
 *
 * All Rights Reserved. See COPYRIGHT.
 *
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif /* HAVE_CONFIG_H */

#ifdef CNID_BACKEND_CDB
#include "cnid_cdb_private.h"

#define tid    NULL
#define DEBUG 1

#include "cnid_cdb.h"

/* ----------------------------- */
static cnid_t set_max_cnid(CNID_private *db, cnid_t hint)
{
    DBT rootinfo_key, rootinfo_data;
    int rc;
    char buf[ROOTINFO_DATALEN];
    cnid_t id, id1;
    time_t t;
    
    memset(&rootinfo_key, 0, sizeof(rootinfo_key));
    memset(&rootinfo_data, 0, sizeof(rootinfo_data));
    
    rootinfo_key.data = ROOTINFO_KEY;
    rootinfo_key.size = ROOTINFO_KEYLEN;

    switch ((rc = db->db_cnid->get(db->db_cnid, tid, &rootinfo_key, &rootinfo_data, 0))) {
    case 0:
	memcpy(buf, (char *)rootinfo_data.data, ROOTINFO_DATALEN);
        break;
    case DB_NOTFOUND:
	/* FIXME: This duplicates stuff from cnid_cdb_add.c. 
	   We also implicitely assume that sizeof(time_t) <= CNID_DEV_LEN */
	memcpy(buf, ROOTINFO_DATA, ROOTINFO_DATALEN);
	t = time(NULL);
	memset(buf + CNID_DEV_OFS, 0, CNID_DEV_LEN);
	memcpy(buf + CNID_DEV_OFS, &t, sizeof(time_t));
        id = htonl(CNID_START);
	memcpy(buf + CNID_TYPE_OFS, &id, sizeof(id));
	break;
    default:
        LOG(log_error, logtype_default, "set_max_cnid: Unable to read rootinfo: %s", db_strerror(rc));
	errno = CNID_ERR_DB; 
        goto cleanup;
    }

    memcpy(&id, buf + CNID_TYPE_OFS, sizeof(id));
    id = ntohl(id);
    id1 = ntohl(hint);

    if (id1 > id) {
	memcpy(buf + CNID_TYPE_OFS, &hint, sizeof(hint));
	rootinfo_data.data = buf;
	rootinfo_data.size = ROOTINFO_DATALEN;
	if ((rc = db->db_cnid->put(db->db_cnid, tid, &rootinfo_key, &rootinfo_data, 0))) {
	    LOG(log_error, logtype_default, "set_max_cnid: Unable to write rootinfo: %s", db_strerror(rc));
	    errno = CNID_ERR_DB; 
	    goto cleanup;
	}
    }

    return hint;

cleanup:
    return CNID_INVALID;
}

cnid_t cnid_cdb_rebuild_add(struct _cnid_db *cdb, const struct stat *st,
                            cnid_t did, const char *name, size_t len, cnid_t hint)
{
    CNID_private *db;
    DBT key, data;
    int rc;

    if (!cdb || !(db = cdb->_private) || !st || !name || hint == CNID_INVALID || hint < CNID_START) {
        errno = CNID_ERR_PARAM;
        return CNID_INVALID;
    }

#if 0
    /* FIXME: Bjoern does a lookup. Should we not overwrite unconditionally? */
    /* Do a lookup. */
    id = cnid_cdb_lookup(cdb, st, did, name, len);
    /* ... Return id if it is valid, or if Rootinfo is read-only. */
    if (id || (db->flags & CNIDFLAG_DB_RO)) {
#ifdef DEBUG
        LOG(log_debug9, logtype_default, "cnid_add: Looked up did %u, name %s as %u", ntohl(did), name, ntohl(id));
#endif
        return id;
    }
#endif

    /* Initialize our DBT data structures. */
    memset(&key, 0, sizeof(key));
    memset(&data, 0, sizeof(data));

    if ((data.data = make_cnid_data(cdb->flags, st, did, name, len)) == NULL) {
        LOG(log_error, logtype_default, "cnid_add: Path name is too long");
        errno = CNID_ERR_PATH;
        return CNID_INVALID;
    }
    data.size = CNID_HEADER_LEN + len + 1;
    
    memcpy(data.data, &hint, sizeof(hint));
    
    key.data = &hint;
    key.size = sizeof(hint);

    /* Now we need to add the CNID data to the databases. */
    if ((rc = db->db_cnid->put(db->db_cnid, tid, &key, &data, 0))) {
            LOG(log_error, logtype_default
                   , "cnid_add: Failed to add CNID for %s to database using hint %u: %s", 
                   name, ntohl(hint), db_strerror(rc));  
            errno = CNID_ERR_DB;
	    goto cleanup;
    }

    if (set_max_cnid(db, hint) == CNID_INVALID) {
	    errno = CNID_ERR_DB;
	    goto cleanup;
    }

#ifdef DEBUG
    LOG(log_debug9, logtype_default, "cnid_add: Returned CNID for did %u, name %s as %u", ntohl(did), name, ntohl(hint));
#endif

    return hint;

cleanup:
    return CNID_INVALID;
}

#endif /* CNID_BACKEND_CDB */
