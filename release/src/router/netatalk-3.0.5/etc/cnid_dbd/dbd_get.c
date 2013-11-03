/*
 *
 * Copyright (C) Joerg Lenneis 2003
 * All Rights Reserved.  See COPYING.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif /* HAVE_CONFIG_H */

#include <string.h>
#include <sys/param.h>
#include <atalk/logger.h>
#include <errno.h>
#include <arpa/inet.h>

#include <atalk/cnid_dbd_private.h>


#include "dbif.h"
#include "dbd.h"
#include "pack.h"


/* Return CNID for a given did/name. */

int dbd_get(DBD *dbd, struct cnid_dbd_rqst *rqst, struct cnid_dbd_rply *rply)
{
    char start[CNID_DID_LEN + MAXPATHLEN + 1], *buf;
    DBT key, data;
    int rc;

    memset(&key, 0, sizeof(key));
    memset(&data, 0, sizeof(data));

    rply->namelen = 0;

    buf = start;
    memcpy(buf, &rqst->did, sizeof(rqst->did));
    buf += sizeof(rqst->did);
    memcpy(buf, rqst->name, rqst->namelen);
    *(buf + rqst->namelen) = '\0'; /* Make it a C-string. */
    key.data = start;
    key.size = CNID_DID_LEN + rqst->namelen + 1;

    if ((rc = dbif_get(dbd, DBIF_IDX_DIDNAME, &key, &data, 0)) < 0) {
        LOG(log_error, logtype_cnid, "dbd_get: Unable to get CNID %u, name %s", ntohl(rqst->did), rqst->name);
        rply->result = CNID_DBD_RES_ERR_DB;
        return -1;
    }

    if (rc == 0) {
	LOG(log_debug, logtype_cnid, "cnid_get: CNID not found for did %u name %s",
	    ntohl(rqst->did), rqst->name);
    rply->result = CNID_DBD_RES_NOTFOUND;
    return 1;
    }

    memcpy(&rply->cnid, data.data, sizeof(rply->cnid));

    LOG(log_debug, logtype_cnid, "cnid_get: Returning CNID did %u name %s as %u",
        ntohl(rqst->did), rqst->name, ntohl(rply->cnid));

    rply->result = CNID_DBD_RES_OK;
    return 1;
}
