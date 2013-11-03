#ifdef HAVE_CONFIG_H
#include "config.h"
#endif /* HAVE_CONFIG_H */

#ifdef CNID_BACKEND_CDB
#include "cnid_cdb_private.h"

#define tid    NULL

/* cnid_update: takes the given cnid and updates the metadata.  To
 * handle the did/name data, there are a bunch of functions to get
 * and set the various fields. */
int cnid_cdb_update(struct _cnid_db *cdb, cnid_t id, const struct stat *st,
                    cnid_t did, const char *name, size_t len)
{
    unsigned char *buf;
    CNID_private *db;
    DBT key, pkey, data;
    int rc;
    int notfound = 0;
    char getbuf[CNID_HEADER_LEN + MAXPATHLEN +1];

    if (!cdb || !(db = cdb->_private) || !id || !st || !name || (db->flags & CNIDFLAG_DB_RO)) {
        return -1;
    }

    memset(&key, 0, sizeof(key));
    memset(&pkey, 0, sizeof(pkey));
    memset(&data, 0, sizeof(data));

    buf = make_cnid_data(cdb->flags, st, did, name, len);

    key.data = buf +CNID_DEVINO_OFS;
    key.size = CNID_DEVINO_LEN;
    data.data = getbuf;
    data.size = CNID_HEADER_LEN + MAXPATHLEN + 1;

    if (0 != (rc = db->db_devino->pget(db->db_devino, tid, &key, &pkey, &data, 0)) ) {
#if DB_VERSION_MAJOR >= 4
        if (rc != DB_NOTFOUND && rc != DB_SECONDARY_BAD) {
#else
	if (rc != DB_NOTFOUND) {
#endif
           LOG(log_error, logtype_default, "cnid_update: Unable to get devino CNID %u, name %s: %s",
               ntohl(did), name, db_strerror(rc));
           goto fin;
        }
        notfound = 1;
    } else {
        if ((rc = db->db_cnid->del(db->db_cnid, tid, &pkey, 0))) {
            LOG(log_error, logtype_default, "cnid_update: Unable to delete CNID %u: %s",
                ntohl(id), db_strerror(rc));
	}
    }

    memset(&pkey, 0, sizeof(pkey));
    buf = make_cnid_data(cdb->flags, st, did, name, len);
    key.data = buf + CNID_DID_OFS;
    key.size = CNID_DID_LEN + len + 1;

    if (0 != (rc = db->db_didname->pget(db->db_didname, tid, &key, &pkey, &data, 0)) ) {
#if DB_VERSION_MAJOR >= 4
        if (rc != DB_NOTFOUND && rc != DB_SECONDARY_BAD) {
#else
	if (rc != DB_NOTFOUND) {
#endif
           LOG(log_error, logtype_default, "cnid_update: Unable to get didname CNID %u, name %s: %s",
               ntohl(did), name, db_strerror(rc));
           goto fin;
        }
        notfound |= 2;
    } else {
        if ((rc = db->db_cnid->del(db->db_cnid, tid, &pkey, 0))) {
            LOG(log_error, logtype_default, "cnid_update: Unable to delete CNID %u: %s",
                ntohl(id), db_strerror(rc));
	}
    }


    memset(&key, 0, sizeof(key));
    key.data = (cnid_t *)&id;
    key.size = sizeof(id);

    memset(&data, 0, sizeof(data));
    /* Make a new entry. */
    buf = make_cnid_data(cdb->flags, st, did, name, len);
    data.data = buf;
    memcpy(data.data, &id, sizeof(id));
    data.size = CNID_HEADER_LEN + len + 1;

    /* Update the old CNID with the new info. */
    if ((rc = db->db_cnid->put(db->db_cnid, tid, &key, &data, 0))) {
        LOG(log_error, logtype_default, "cnid_update: (%d) Unable to update CNID %u:%s: %s",
            notfound, ntohl(id), name, db_strerror(rc));
        goto fin;
    }

    return 0;
fin:
    return -1;
 
}

#endif
