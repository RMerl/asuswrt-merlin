/*
 * Copyright (C) Joerg Lenneis 2003
 * Copyright (C) Frank Lahm 2009,2010
 * All Rights Reserved.  See COPYING.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif /* HAVE_CONFIG_H */

#include <string.h>
#include <errno.h>
#include <atalk/logger.h>
#include <arpa/inet.h>

#include <atalk/cnid_dbd_private.h>


#include "pack.h"
#include "dbif.h"
#include "dbd.h"


/* 
   cnid_update: takes the given cnid and updates the metadata.
   First, delete given CNID, then re-insert.
*/

int dbd_update(DBD *dbd, struct cnid_dbd_rqst *rqst, struct cnid_dbd_rply *rply)
{
    DBT key, data;

    memset(&key, 0, sizeof(key));
    memset(&data, 0, sizeof(data));
    rply->namelen = 0;

    /* Try to wipe everything, also using the indexes */
    if (dbd_delete(dbd, rqst, rply, DBIF_CNID) < 0)
        goto err_db;
    if (dbd_delete(dbd, rqst, rply, DBIF_IDX_DEVINO) < 0)
        goto err_db;
    if (dbd_delete(dbd, rqst, rply, DBIF_IDX_DIDNAME) < 0)
        goto err_db;

    /* Make a new entry. */
    key.data = &rqst->cnid;
    key.size = sizeof(rqst->cnid);
    data.data = pack_cnid_data(rqst);
    data.size = CNID_HEADER_LEN + rqst->namelen + 1;
    memcpy(data.data, &rqst->cnid, sizeof(rqst->cnid));

    if (dbif_put(dbd, DBIF_CNID, &key, &data, 0) < 0)
        goto err_db;

    LOG(log_debug, logtype_cnid, "dbd_update: Updated dbd with dev/ino: 0x%llx/0x%llx, did: %u, name: %s, cnid: %u",
        (unsigned long long)rqst->dev, (unsigned long long)rqst->ino, ntohl(rqst->did), rqst->name, ntohl(rqst->cnid));

    rply->result = CNID_DBD_RES_OK;
    return 1;

err_db:
    LOG(log_error, logtype_cnid, "dbd_update: Unable to update CNID: %u, dev/ino: 0x%llx/0x%llx, DID: %u: %s",
        ntohl(rqst->cnid), (unsigned long long)rqst->dev, (unsigned long long)rqst->ino, ntohl(rqst->did), rqst->name);

    rply->result = CNID_DBD_RES_ERR_DB;
    return -1;
}
