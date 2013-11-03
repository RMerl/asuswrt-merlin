#ifdef HAVE_CONFIG_H
#include "config.h"
#endif /* HAVE_CONFIG_H */

#ifdef CNID_BACKEND_CDB
#include "cnid_cdb_private.h"

/* Return CNID for a given did/name. */
cnid_t cnid_cdb_get(struct _cnid_db *cdb, cnid_t did, const char *name, size_t len)
{
    char start[CNID_DID_LEN + MAXPATHLEN + 1], *buf;
    CNID_private *db;
    DBT key, data;
    cnid_t id;
    int rc;

    if (!cdb || !(db = cdb->_private) || (len > MAXPATHLEN)) {
        return 0;
    }

    memset(&key, 0, sizeof(key));
    memset(&data, 0, sizeof(data));

    buf = start;
    memcpy(buf, &did, sizeof(did));
    buf += sizeof(did);
    memcpy(buf, name, len);
    *(buf + len) = '\0'; /* Make it a C-string. */
    key.data = start;
    key.size = CNID_DID_LEN + len + 1;

    while ((rc = db->db_didname->get(db->db_didname, NULL, &key, &data, 0))) {

        if (rc != DB_NOTFOUND) {
            LOG(log_error, logtype_default, "cnid_get: Unable to get CNID %u, name %s: %s",
                ntohl(did), name, db_strerror(rc));
        }

        return 0;
    }

    memcpy(&id, data.data, sizeof(id));
#ifdef DEBUG
    LOG(log_debug9, logtype_default, "cnid_get: Returning CNID for %u, name %s as %u",
        ntohl(did), name, ntohl(id));
#endif
    return id;
}

#endif /* CNID_BACKEND_CDB */
