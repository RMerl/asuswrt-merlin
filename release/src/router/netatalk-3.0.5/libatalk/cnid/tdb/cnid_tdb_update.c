#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#ifdef CNID_BACKEND_TDB

#include "cnid_tdb.h"
#include <atalk/logger.h>

int cnid_tdb_update(struct _cnid_db *cdb, cnid_t id, const struct stat *st,
                    cnid_t did, const char *name, size_t len)
{
    struct _cnid_tdb_private *db;
    TDB_DATA key, data, altdata;

    if (!cdb || !(db = cdb->_private) || !id || !st || !name || (db->flags & CNIDFLAG_DB_RO)) {
        return -1;
    }

    memset(&key, 0, sizeof(key));
    memset(&altdata, 0, sizeof(altdata));


    /* Get the old info. search by dev/ino */
    data.dptr = make_tdb_data(cdb->flags, st, did, name, len);
    data.dsize = CNID_HEADER_LEN + len + 1;
    key.dptr = data.dptr +CNID_DEVINO_OFS;
    key.dsize = CNID_DEVINO_LEN;
    altdata = tdb_fetch(db->tdb_devino, key);
    if (altdata.dptr) {
        tdb_delete(db->tdb_devino, key); 

        key.dptr = altdata.dptr;
        key.dsize = sizeof(id);

        data = tdb_fetch(db->tdb_cnid, key);
        tdb_delete(db->tdb_cnid, key); 
        free(altdata.dptr);

        if (data.dptr) {
            key.dptr = (unsigned char *)data.dptr +CNID_DID_OFS;
            key.dsize = data.dsize - CNID_DID_OFS;
            tdb_delete(db->tdb_didname, key); 
        
            free(data.dptr);
        }
    }

    /* search by did/name */
    data.dptr = make_tdb_data(cdb->flags, st, did, name, len);
    data.dsize = CNID_HEADER_LEN + len + 1;
    key.dptr = (unsigned char *)data.dptr +CNID_DID_OFS;
    key.dsize = data.dsize - CNID_DID_OFS;
    altdata = tdb_fetch(db->tdb_didname, key);
    if (altdata.dptr) {
        tdb_delete(db->tdb_didname, key); 

        key.dptr = altdata.dptr;
        key.dsize = sizeof(id);
        data = tdb_fetch(db->tdb_cnid, key);
        tdb_delete(db->tdb_cnid, key); 
        free(altdata.dptr);

        if (data.dptr) {
            key.dptr = data.dptr +CNID_DEVINO_OFS;
            key.dsize = CNID_DEVINO_LEN;
            tdb_delete(db->tdb_devino, key); 
            free(data.dptr);
        }
    }
    

    /* Make a new entry. */
    data.dptr = make_tdb_data(cdb->flags, st, did, name, len);
    data.dsize = CNID_HEADER_LEN + len + 1;
    memcpy(data.dptr, &id, sizeof(id));

    /* Update the old CNID with the new info. */
    key.dptr = (unsigned char *) &id;
    key.dsize = sizeof(id);
    if (tdb_store(db->tdb_cnid, key, data, TDB_REPLACE)) {
        goto update_err;
    }

    /* Put in a new dev/ino mapping. */
    key.dptr = data.dptr +CNID_DEVINO_OFS;
    key.dsize = CNID_DEVINO_LEN;
    altdata.dptr  = (unsigned char *) &id;
    altdata.dsize = sizeof(id);
    if (tdb_store(db->tdb_devino, key, altdata, TDB_REPLACE)) {
        goto update_err;
    }
    /* put in a new did/name mapping. */
    key.dptr = (unsigned char *) data.dptr +CNID_DID_OFS;
    key.dsize = data.dsize -CNID_DID_OFS;

    if (tdb_store(db->tdb_didname, key, altdata, TDB_REPLACE)) {
        goto update_err;
    }

    return 0;
update_err:
    LOG(log_error, logtype_default, "cnid_update: Unable to update CNID %u", ntohl(id));
    return -1;

}

#endif
