/*
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif /* HAVE_CONFIG_H */

#ifdef CNID_BACKEND_CDB
#include "cnid_cdb_private.h"

void cnid_cdb_close(struct _cnid_db *cdb) {
    CNID_private *db;

    if (!cdb) {
	    LOG(log_error, logtype_afpd, "cnid_close called with NULL argument !");
	    return;
    }

    if (!(db = cdb->_private)) {
        return;
    }
    db->db_didname->sync(db->db_didname, 0); 
    db->db_devino->sync(db->db_devino, 0);
    db->db_cnid->sync(db->db_cnid, 0);
    
    db->db_didname->close(db->db_didname, 0);
    db->db_devino->close(db->db_devino, 0);
    db->db_cnid->close(db->db_cnid, 0);

    db->dbenv->close(db->dbenv, 0);

    free(db);
    free(cdb->volpath);
    free(cdb);
}

#endif /* CNID_BACKEND_CDB */
