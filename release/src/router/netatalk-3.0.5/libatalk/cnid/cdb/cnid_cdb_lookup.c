#ifdef HAVE_CONFIG_H
#include "config.h"
#endif /* HAVE_CONFIG_H */

#ifdef CNID_BACKEND_CDB
#include "cnid_cdb_private.h"

#define LOGFILEMAX    100  /* kbytes */
#define CHECKTIMEMAX   30  /* minutes */

/* This returns the CNID corresponding to a particular file.  It will
 * also fix up the various databases if there's a problem. */
cnid_t cnid_cdb_lookup(struct _cnid_db *cdb, const struct stat *st, cnid_t did,
                       const char *name, size_t len)
{
    unsigned char *buf;
    CNID_private *db;
    DBT key, devdata, diddata;
    char dev[CNID_DEV_LEN];
    char ino[CNID_INO_LEN];  
    int devino = 1, didname = 1; 
    u_int32_t type_devino  = (unsigned)-1;
    u_int32_t type_didname = (unsigned)-1;
    u_int32_t type;
    int update = 0;
    cnid_t id_devino, id_didname,id = 0;
    int rc;

    if (!cdb || !(db = cdb->_private) || !st || !name) {
        return 0;
    }
    
    if ((buf = make_cnid_data(cdb->flags, st, did, name, len)) == NULL) {
        LOG(log_error, logtype_default, "cnid_lookup: Pathname is too long");
        return 0;
    }

    memcpy(&type, buf +CNID_TYPE_OFS, sizeof(type));
    type = ntohl(type);

    memset(&key, 0, sizeof(key));
    memset(&diddata, 0, sizeof(diddata));
    memset(&devdata, 0, sizeof(devdata));

    /* Look for a CNID for our did/name */
    key.data = buf +CNID_DEVINO_OFS;
    key.size = CNID_DEVINO_LEN;

    memcpy(dev, buf + CNID_DEV_OFS, CNID_DEV_LEN);
    memcpy(ino, buf + CNID_INO_OFS, CNID_INO_LEN);
    
    if (0 != (rc = db->db_didname->get(db->db_devino, NULL, &key, &devdata, 0 )) ) {
        if (rc != DB_NOTFOUND) {
            LOG(log_error, logtype_default, "cnid_lookup: Unable to get CNID did 0x%x, name %s: %s",
               did, name, db_strerror(rc));
            return 0;
        }
        devino = 0;
    }
    else {
        memcpy(&id_devino, devdata.data, sizeof(cnid_t));
        memcpy(&type_devino, (char *)devdata.data +CNID_TYPE_OFS, sizeof(type_devino));
        type_devino = ntohl(type_devino);
    }

    buf = make_cnid_data(cdb->flags, st, did, name, len);
    key.data = buf +CNID_DID_OFS;
    key.size = CNID_DID_LEN + len + 1;
    
    if (0 != (rc = db->db_didname->get(db->db_didname, NULL, &key, &diddata, 0 ) ) ) {
        if (rc != DB_NOTFOUND) {
            LOG(log_error, logtype_default, "cnid_lookup: Unable to get CNID did 0x%x, name %s: %s",
               did, name, db_strerror(rc));
            return 0;
        }
        didname = 0;
    }
    else {
        memcpy(&id_didname, diddata.data, sizeof(cnid_t));
        memcpy(&type_didname, (char *)diddata.data +CNID_TYPE_OFS, sizeof(type_didname));
        type_didname = ntohl(type_didname);
    }

    if (!devino && !didname) {  
        return 0;
    }

    if (devino && didname && id_devino == id_didname && type_devino == type) {
        /* the same */
        return id_didname;
    }
 
    if (didname) {
        id = id_didname;
        /* we have a did:name 
         * if it's the same dev or not the same type
         * just delete it
        */
        if (!memcmp(dev, (char *)diddata.data + CNID_DEV_OFS, CNID_DEV_LEN) ||
                   type_didname != type) {
            if (cnid_cdb_delete(cdb, id) < 0) {
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
            if (cnid_cdb_delete(cdb, id) < 0) {
                return 0;
            }
        }
        else {
            update = 1;
        }
    }
    if (!update) {
        return 0;
    }
    /* Fix up the database. assume it was a file move and rename */
    cnid_cdb_update(cdb, id, st, did, name, len);

#ifdef DEBUG
    LOG(log_debug9, logtype_default, "cnid_lookup: Looked up did %u, name %s, as %u (needed update)", ntohl(did), name, ntohl(id));
#endif
    return id;
}

#endif /* CNID_BACKEND_CDB */
