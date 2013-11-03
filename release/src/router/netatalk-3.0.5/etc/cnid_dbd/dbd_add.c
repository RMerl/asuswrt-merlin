/*
 * Copyright (C) Joerg Lenneis 2003
 * Copyright (C) Frank Lahm 2010
 * All Rights Reserved.  See COPYING.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif /* HAVE_CONFIG_H */

#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/time.h>

#include <atalk/logger.h>
#include <atalk/cnid_dbd_private.h>
#include <atalk/cnid.h>
#include <db.h>

#include "dbif.h"
#include "pack.h"
#include "dbd.h"

int add_cnid(DBD *dbd, struct cnid_dbd_rqst *rqst, struct cnid_dbd_rply *rply)
{
    DBT key, data;
    int rc;

    memset(&key, 0, sizeof(key));
    memset(&data, 0, sizeof(data));

    key.data = &rply->cnid;
    key.size = sizeof(rply->cnid);

    data.data = pack_cnid_data(rqst);
    data.size = CNID_HEADER_LEN + rqst->namelen + 1;
    memcpy(data.data, &rply->cnid, sizeof(rply->cnid));

    /* main database */
    if ((rc = dbif_put(dbd, DBIF_CNID, &key, &data, DB_NOOVERWRITE))) {
        /* This could indicate a database error or that the key already exists
         (because of DB_NOOVERWRITE). In that case we still look at some sort of
         database corruption since that is not supposed to happen. */
        
        switch (rc) {
        case 1:
            rply->result = CNID_DBD_RES_ERR_DUPLCNID;
            break;
        case -1:
	    /* FIXME: Should that not be logged for case 1:? */
            LOG(log_error, logtype_cnid, "add_cnid: duplicate %x %s", rply->cnid
             , (char *)data.data + CNID_NAME_OFS);
            
            rqst->cnid = rply->cnid;
            rc = dbd_update(dbd, rqst, rply);
            if (rc < 0) {
                rply->result = CNID_DBD_RES_ERR_DB;
                return -1;
            }
            else 
               return 0;
            break;
        }
        return -1;
    }

    return 0;
}

/* ---------------------- */
int get_cnid(DBD *dbd, struct cnid_dbd_rply *rply)
{
    static cnid_t id;
    static char buf[ROOTINFO_DATALEN];
    DBT rootinfo_key, rootinfo_data, key, data;
    int rc;
    cnid_t hint;

    memset(&rootinfo_key, 0, sizeof(rootinfo_key));
    memset(&rootinfo_data, 0, sizeof(rootinfo_data));
    rootinfo_key.data = ROOTINFO_KEY;
    rootinfo_key.size = ROOTINFO_KEYLEN;

    memset(&key, 0, sizeof(key));
    memset(&data, 0, sizeof(data));

    if (id == 0) {
        if ((rc = dbif_get(dbd, DBIF_CNID, &rootinfo_key, &rootinfo_data, 0)) != 1) {
            rply->result = CNID_DBD_RES_ERR_DB;
            return -1;
        }
        memcpy(buf, (char *)rootinfo_data.data, ROOTINFO_DATALEN);
        memcpy(&hint, buf + CNID_TYPE_OFS, sizeof(hint));
        id = ntohl(hint);
        if (id < CNID_START - 1)
            id = CNID_START - 1;
    }

    cnid_t trycnid, tmp;

    while (true) {
        if (rply->cnid != CNID_INVALID) {
            trycnid = ntohl(rply->cnid);
            rply->cnid = CNID_INVALID;
        } else {
            if (++id == CNID_INVALID)
                id = CNID_START;
            trycnid = id;
        }
        tmp = htonl(trycnid);
        key.data = &tmp;
        key.size = sizeof(cnid_t);
        rc = dbif_get(dbd, DBIF_CNID, &key, &data, 0);
        if (rc == 0) {
            break;
        } else if (rc == -1) {
            rply->result = CNID_DBD_RES_ERR_DB;
            return -1;
        }
    }

    if (trycnid == id) {
        rootinfo_data.data = buf;
        rootinfo_data.size = ROOTINFO_DATALEN;
        hint = htonl(id);
        memcpy(buf + CNID_TYPE_OFS, &hint, sizeof(hint));

        if (dbif_put(dbd, DBIF_CNID, &rootinfo_key, &rootinfo_data, 0) < 0) {
            rply->result = CNID_DBD_RES_ERR_DB;
            return -1;
        }
    }

    rply->cnid = htonl(trycnid);
    return 0;
}

/* ------------------------ */
/* We need a nolookup version for `dbd` */
int dbd_add(DBD *dbd, struct cnid_dbd_rqst *rqst, struct cnid_dbd_rply *rply)
{
    rply->namelen = 0;

    LOG(log_debug, logtype_cnid, "dbd_add(did:%u, '%s', dev/ino:0x%llx/0x%llx) {start}",
        ntohl(rqst->did), rqst->name, (unsigned long long)rqst->dev, (unsigned long long)rqst->ino);

    /* See if we have an entry already and return it if yes */
    if (dbd_lookup(dbd, rqst, rply) < 0) {
        LOG(log_debug, logtype_cnid, "dbd_add(did:%u, '%s', dev/ino:0x%llx/0x%llx): error in dbd_lookup",
            ntohl(rqst->did), rqst->name, (unsigned long long)rqst->dev, (unsigned long long)rqst->ino);
        return -1;
    }

    if (rply->result == CNID_DBD_RES_OK) {
        /* Found it. rply->cnid is the correct CNID now. */
        LOG(log_debug, logtype_cnid, "dbd_add: dbd_lookup success --> CNID: %u", ntohl(rply->cnid));
        return 1;
    }

    LOG(log_debug, logtype_cnid, "dbd_add(did:%u, '%s', dev/ino:0x%llx/0x%llx): {adding to database ...}",
        ntohl(rqst->did), rqst->name, (unsigned long long)rqst->dev, (unsigned long long)rqst->ino);

    if (rqst->cnid) {
        /* rqst->cnid is the cnid "hint"/backup from the adouble file */
        rply->cnid = rqst->cnid;
    }
    if (get_cnid(dbd, rply) < 0) {
        if (rply->result == CNID_DBD_RES_ERR_MAX) {
            LOG(log_error, logtype_cnid, "dbd_add: FATAL: CNID database has reached its limit.");
            /* This will cause an abort/rollback if transactions are used */
            return 0;
        } else {
            LOG(log_error, logtype_cnid, "dbd_add: Failed to compute CNID for %s, error reading/updating Rootkey", rqst->name);
            return -1;
        }
    }
    
    if (add_cnid(dbd, rqst, rply) < 0) {
        if (rply->result == CNID_DBD_RES_ERR_DUPLCNID) {
            LOG(log_error, logtype_cnid, "dbd_add(DID: %u/\"%s\", dev/ino 0x%llx/0x%llx): Cannot add CNID: %u",
                ntohl(rqst->did), rqst->name, (unsigned long long)rqst->dev, (unsigned long long)rqst->ino, ntohl(rply->cnid));
            /* abort/rollback, see above */
            return 0;
        } else {
            LOG(log_error, logtype_cnid, "dbd_add: Failed to add CNID for %s to database", rqst->name);
            return -1;
        }
    }
    LOG(log_debug, logtype_cnid, "dbd_add(did:%u, '%s', dev/ino:0x%llx/0x%llx): Added with CNID: %u",
        ntohl(rqst->did), rqst->name, (unsigned long long)rqst->dev, (unsigned long long)rqst->ino, ntohl(rply->cnid));

    rply->result = CNID_DBD_RES_OK;
    return 1;
}
