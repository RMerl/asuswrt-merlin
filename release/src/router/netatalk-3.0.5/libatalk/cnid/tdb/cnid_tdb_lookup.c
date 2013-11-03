#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#ifdef CNID_BACKEND_TDB

#include "cnid_tdb.h"
#include <atalk/logger.h>

cnid_t cnid_tdb_lookup(struct _cnid_db *cdb, const struct stat *st, cnid_t did, const char *name, size_t len)
{
    char *buf;
    struct _cnid_tdb_private *db;
    TDB_DATA key, devdata, diddata, cniddata;
    int devino = 1, didname = 1;
    char dev[CNID_DEV_LEN];
    char ino[CNID_INO_LEN];  
    uint32_t type_devino  = (unsigned)-1;
    uint32_t type_didname = (unsigned)-1;
    uint32_t type;
    int update = 0;
    cnid_t id_devino = 0, id_didname = 0,id = 0;

    if (!cdb || !(db = cdb->_private) || !st || !name) {
        return 0;
    }

    if ((buf = (char *)make_tdb_data(cdb->flags, st, did, name, len)) == NULL) {
        LOG(log_error, logtype_default, "tdb_lookup: Pathname is too long");
        return 0;
    }
    memcpy(&type, buf +CNID_TYPE_OFS, sizeof(type));
    type = ntohl(type);

    memset(&key, 0, sizeof(key));
    memset(&devdata, 0, sizeof(devdata));
    memset(&diddata, 0, sizeof(diddata));
    memset(&cniddata, 0, sizeof(cniddata));

    /* Look for a CNID.  We have two options: dev/ino or did/name.  If we
    * only get a match in one of them, that means a file has moved. */
    memcpy(dev, buf + CNID_DEV_OFS, CNID_DEV_LEN);
    memcpy(ino, buf + CNID_INO_OFS, CNID_INO_LEN);

    key.dptr = (unsigned char *)buf + CNID_DEVINO_OFS;
    key.dsize  = CNID_DEVINO_LEN;
    cniddata = tdb_fetch(db->tdb_devino, key);
    if (!cniddata.dptr) {
         devino = 0;
    }
    else {
        
        key.dptr = cniddata.dptr;
        key.dsize = sizeof(id);

        devdata = tdb_fetch(db->tdb_cnid, key);
        free(cniddata.dptr);
        if (devdata.dptr) {
            memcpy(&id_devino, devdata.dptr, sizeof(cnid_t));
            memcpy(&type_devino, (char *)devdata.dptr +CNID_TYPE_OFS, sizeof(type_devino));
            type_devino = ntohl(type_devino);
        }
        else {
             devino = 0;
        }
    }

    /* did/name now */
    key.dptr = (unsigned char *)buf + CNID_DID_OFS;
    key.dsize = CNID_DID_LEN + len + 1;
    cniddata = tdb_fetch(db->tdb_didname, key);
    if (!cniddata.dptr) {
        didname = 0;
    }
    else {
        
        key.dptr = cniddata.dptr;
        key.dsize = sizeof(id);

        diddata = tdb_fetch(db->tdb_cnid, key);
        free(cniddata.dptr);
        if (diddata.dptr) {
            memcpy(&id_didname, diddata.dptr, sizeof(cnid_t));
            memcpy(&type_didname, (char *)diddata.dptr +CNID_TYPE_OFS, sizeof(type_didname));
            type_didname = ntohl(type_didname);
        }
        else {
             didname = 0;
        }
    }
    /* Set id.  Honor did/name over dev/ino as dev/ino isn't necessarily
     * 1-1. */
    if (!devino && !didname) {  
        free(devdata.dptr);
        free(diddata.dptr);
        return 0;
    }

    if (devino && didname && id_devino == id_didname && type_devino == type) {
        /* the same */
        free(devdata.dptr);
        free(diddata.dptr);
        return id_didname;
    }
 
    if (didname) {
        id = id_didname;
        /* we have a did:name 
         * if it's the same dev or not the same type
         * just delete it
        */
        if (!memcmp(dev, (char *)diddata.dptr + CNID_DEV_OFS, CNID_DEV_LEN) ||
                   type_didname != type) {
            if (cnid_tdb_delete(cdb, id) < 0) {
                free(devdata.dptr);
                free(diddata.dptr);
                return 0;
            }
        }
        else {
            update = 1;
        }
    }

    if (devino) {
        id = id_devino;
        if (type_devino != type) {
            /* same dev:inode but not same type one is a folder the other 
             * is a file,it's an inode reused, delete the record
            */
            if (cnid_tdb_delete(cdb, id) < 0) {
                free(devdata.dptr);
                free(diddata.dptr);
                return 0;
            }
        }
        else {
            update = 1;
        }
    }
    free(devdata.dptr);
    free(diddata.dptr);
    if (!update) {
        return 0;
    }

    /* Fix up the database. */
    cnid_tdb_update(cdb, id, st, did, name, len);
    return id;
}

#endif
