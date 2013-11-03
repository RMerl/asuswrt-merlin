/*
 *
 * Copyright (C) Joerg Lenneis 2003
 * All Rights Reserved.  See COPYING.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif /* HAVE_CONFIG_H */

#include <string.h>
#include <atalk/logger.h>
#include <errno.h>
#include <arpa/inet.h>

#include <atalk/cnid_dbd_private.h>

#include "dbif.h"
#include "dbd.h"
#include "pack.h"

/* Return the did/name pair corresponding to a CNID. */

int dbd_resolve(DBD *dbd, struct cnid_dbd_rqst *rqst, struct cnid_dbd_rply *rply)
{
    DBT key, data;
    int rc;

    memset(&key, 0, sizeof(key));
    memset(&data, 0, sizeof(data));

    rply->namelen = 0;

    key.data = (void *) &rqst->cnid;
    key.size = sizeof(cnid_t);

    if ((rc = dbif_get(dbd, DBIF_CNID, &key, &data, 0)) < 0) {
        LOG(log_error, logtype_cnid, "dbd_resolve: DB Error resolving CNID %u", ntohl(rqst->cnid));
        rply->result = CNID_DBD_RES_ERR_DB;
        return -1;
    }
     
    if (rc == 0) {

        LOG(log_debug, logtype_cnid, "dbd_resolve: Could not resolve CNID %u", ntohl(rqst->cnid));
    
        rply->result = CNID_DBD_RES_NOTFOUND;
        return 1;
    }

    memcpy(&rply->did, (char *) data.data + CNID_DID_OFS, sizeof(cnid_t));

    rply->namelen = data.size;
    rply->name = (char *)data.data;

    LOG(log_debug, logtype_cnid, "dbd_resolve(CNID: %u): did: %u, name: \"%s\"",
        ntohl(rqst->cnid), ntohl(rply->did), rply->name + CNID_NAME_OFS);

    rply->result = CNID_DBD_RES_OK;
    return 1;
}
