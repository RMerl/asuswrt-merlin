/*
 *
 * Copyright (c) 1999. Adrian Sun (asun@zoology.washington.edu)
 * All Rights Reserved. See COPYRIGHT.
 *
 * cnid_delete: delete a CNID from the database 
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif /* HAVE_CONFIG_H */

#ifdef CNID_BACKEND_CDB
#include <arpa/inet.h>
#include "cnid_cdb_private.h"

#define tid    NULL

int cnid_cdb_delete(struct _cnid_db *cdb, const cnid_t id) {
    CNID_private *db;
    DBT key;
    int rc;

    if (!cdb || !(db = cdb->_private) || !id || (db->flags & CNIDFLAG_DB_RO)) {
        return -1;
    }

    memset(&key, 0, sizeof(key));

    /* Get from ain CNID database. */
    key.data = (cnid_t *)&id;
    key.size = sizeof(id);
    
    if ((rc = db->db_cnid->del(db->db_cnid, tid, &key, 0))) {
        LOG(log_error, logtype_default, "cnid_delete: Unable to delete CNID %u: %s",
            ntohl(id), db_strerror(rc));
    }
    else {
#ifdef DEBUG
        LOG(log_debug9, logtype_default, "cnid_delete: Deleting CNID %u", ntohl(id));
#endif
    }
    return rc;
}

#endif /* CNID_BACKEND_CDB */
