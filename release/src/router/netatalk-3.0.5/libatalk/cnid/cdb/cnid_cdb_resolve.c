/*
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif /* HAVE_CONFIG_H */

#ifdef CNID_BACKEND_CDB
#include "cnid_cdb_private.h"

/* Return the did/name pair corresponding to a CNID. */
char *cnid_cdb_resolve(struct _cnid_db *cdb, cnid_t *id, void *buffer, size_t len) {
    CNID_private *db;
    DBT key, data;
    int rc;

    if (!cdb || !(db = cdb->_private) || !id || !(*id)) {
        return NULL;
    }

    memset(&key, 0, sizeof(key));
    memset(&data, 0, sizeof(data));

    data.data = buffer;
    data.ulen = len;
    data.flags = DB_DBT_USERMEM;

    key.data = id;
    key.size = sizeof(cnid_t);
    while ((rc = db->db_cnid->get(db->db_cnid, NULL, &key, &data, 0))) {

        if (rc != DB_NOTFOUND) {
            LOG(log_error, logtype_default, "cnid_resolve: Unable to get did/name: %s",
                db_strerror(rc));
        }

        *id = 0;
        return NULL;
    }

    memcpy(id, (char *)data.data +CNID_DID_OFS, sizeof(cnid_t));
#ifdef DEBUG
    LOG(log_debug9, logtype_default, "cnid_resolve: Returning id = %u, did/name = %s",
        ntohl(*id), (char *)data.data + CNID_NAME_OFS);
#endif
    return (char *)data.data +  CNID_NAME_OFS;
}

#endif
