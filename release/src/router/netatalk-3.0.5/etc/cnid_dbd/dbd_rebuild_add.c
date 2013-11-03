/*
 *
 * Copyright (C) Joerg Lenneis 2005
 * All Rights Reserved.  See COPYING.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif /* HAVE_CONFIG_H */

#include <stdio.h>
#include <string.h>
#include <arpa/inet.h>

#include <atalk/logger.h>
#include <atalk/cnid_dbd_private.h>

#include "pack.h"
#include "dbif.h"
#include "dbd.h"


/* rebuild_add: Enter all fields (including the CNID) into the database and
   update the current cnid, for emergency repairs. */

int dbd_rebuild_add(DBD *dbd, struct cnid_dbd_rqst *rqst, struct cnid_dbd_rply *rply)
{
    DBT key, data;
    cnid_t cur, tmp, id;

    memset(&key, 0, sizeof(key));
    memset(&data, 0, sizeof(data));

    rply->namelen = 0;

    key.data = &rqst->cnid;
    key.size = sizeof(cnid_t);

    data.data = pack_cnid_data(rqst);
    data.size = CNID_HEADER_LEN + rqst->namelen + 1;
    memcpy(data.data, &rqst->cnid, sizeof(cnid_t));

    /* FIXME: In cnid_cdb.c Bjoern does a lookup here and returns the CNID found if sucessful. Why? */

    if (dbif_put(dbd, DBIF_CNID, &key, &data, 0) < 0) {
        rply->result = CNID_DBD_RES_ERR_DB;
        return -1;
    }

    LOG(log_debug, logtype_cnid,
        "dbd_rebuild_add(CNID: %u, did: %u, name: \"%s\", dev/ino:0x%llx/0x%llx): success",
        ntohl(rqst->cnid), ntohl(rqst->did), rqst->name,
        (unsigned long long)rqst->dev, (unsigned long long)rqst->ino);

    key.data = ROOTINFO_KEY;
    key.size = ROOTINFO_KEYLEN;

    if (dbif_get(dbd, DBIF_CNID, &key, &data, 0) <= 0) {
        /* FIXME: If we cannot find ROOTINFO_KEY, should this be considered
           fatal or should we just return 0 and roll back? */
        rply->result = CNID_DBD_RES_ERR_DB;
        return -1;
    }

    memcpy(&tmp, (char *) data.data + CNID_TYPE_OFS, sizeof(cnid_t));
    cur = ntohl(tmp);
    id  = ntohl(rqst->cnid);

    if (id > cur) {
        data.size = ROOTINFO_DATALEN;
        memcpy((char *) data.data + CNID_TYPE_OFS, &rqst->cnid, sizeof(cnid_t));
        if (dbif_put(dbd, DBIF_CNID, &key, &data, 0) < 0) {
            rply->result = CNID_DBD_RES_ERR_DB;
            return -1;
        }
    }

    rply->cnid = rqst->cnid;
    rply->result = CNID_DBD_RES_OK;
    return 1;
}


