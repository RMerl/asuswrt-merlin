/*
 *
 * Copyright (C) Joerg Lenneis 2003
 * All Rights Reserved.  See COPYING.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif /* HAVE_CONFIG_H */

#include <string.h>
#include <errno.h>
#include <arpa/inet.h>

#include <atalk/logger.h>
#include <atalk/cnid_dbd_private.h>

#include "dbif.h"
#include "dbd.h"
#include "pack.h"

int dbd_delete(DBD *dbd, struct cnid_dbd_rqst *rqst, struct cnid_dbd_rply *rply, int idx)
{
    DBT key;
    int rc;
    unsigned char *buf;

    memset(&key, 0, sizeof(key));
    rply->namelen = 0;

    switch (idx) {
    case DBIF_IDX_DEVINO:
        buf = pack_cnid_data(rqst);
        key.data = buf + CNID_DEVINO_OFS;
        key.size = CNID_DEVINO_LEN;
        if ((rc = dbif_del(dbd, DBIF_IDX_DEVINO, &key, 0)) < 0) {
            LOG(log_error, logtype_cnid, "dbd_delete: Unable to delete entry for dev/ino: 0x%llx/0x%llx",
                (unsigned long long)rqst->dev, (unsigned long long)rqst->ino);
            rply->result = CNID_DBD_RES_ERR_DB;
            return -1;
        }
        if (rc) {
            LOG(log_debug, logtype_cnid, "cnid_delete: deleted dev/ino: 0x%llx/0x%llx",
                (unsigned long long)rqst->dev, (unsigned long long)rqst->ino);
            rply->result = CNID_DBD_RES_OK;
        } else {
            LOG(log_debug, logtype_cnid, "cnid_delete: dev/ino: 0x%llx/0x%llx not in database", 
                (unsigned long long)rqst->dev, (unsigned long long)rqst->ino);
            rply->result = CNID_DBD_RES_NOTFOUND;
        }
        break;
    case DBIF_IDX_DIDNAME:
        buf = pack_cnid_data(rqst);
        key.data = buf + CNID_DID_OFS;
        key.size = CNID_DID_LEN + rqst->namelen + 1;
        if ((rc = dbif_del(dbd, DBIF_IDX_DIDNAME, &key, 0)) < 0) {
            LOG(log_error, logtype_cnid, "dbd_delete: Unable to delete entry for DID: %lu, name: %s",
                ntohl(rqst->did), rqst->name);
            rply->result = CNID_DBD_RES_ERR_DB;
            return -1;
        }
        if (rc) {
            LOG(log_debug, logtype_cnid, "cnid_delete: deleted DID: %lu, name: %s",
                ntohl(rqst->did), rqst->name);
            rply->result = CNID_DBD_RES_OK;
        } else {
            LOG(log_debug, logtype_cnid, "cnid_delete: DID: %lu, name: %s not in database",
                ntohl(rqst->did), rqst->name);
            rply->result = CNID_DBD_RES_NOTFOUND;
        }
        break;
    default:
        key.data = (void *) &rqst->cnid;
        key.size = sizeof(rqst->cnid);

        if ((rc = dbif_del(dbd, DBIF_CNID, &key, 0)) < 0) {
            LOG(log_error, logtype_cnid, "dbd_delete: Unable to delete entry for CNID %u", ntohl(rqst->cnid));
            rply->result = CNID_DBD_RES_ERR_DB;
            return -1;
        }
        if (rc) {
            LOG(log_debug, logtype_cnid, "cnid_delete: CNID %u deleted", ntohl(rqst->cnid));
            rply->result = CNID_DBD_RES_OK;
        } else {
            LOG(log_debug, logtype_cnid, "cnid_delete: CNID %u not in database", ntohl(rqst->cnid));
            rply->result = CNID_DBD_RES_NOTFOUND;
        }
    }

    return 1;
}
