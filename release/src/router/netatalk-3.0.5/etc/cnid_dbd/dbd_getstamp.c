
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

/* Return the unique stamp associated with this database */

int dbd_getstamp(DBD *dbd, struct cnid_dbd_rqst *rqst _U_, struct cnid_dbd_rply *rply)
{
    DBT key, data;
    int rc;


    memset(&key, 0, sizeof(key));
    memset(&data, 0, sizeof(data));

    rply->namelen = 0;

    key.data = ROOTINFO_KEY;
    key.size = ROOTINFO_KEYLEN;

    if ((rc = dbif_get(dbd, DBIF_CNID, &key, &data, 0)) < 0) {
        LOG(log_error, logtype_cnid, "dbd_getstamp: Error getting rootinfo record");
        rply->result = CNID_DBD_RES_ERR_DB;
        return -1;
    }
     
    if (rc == 0) {
	LOG(log_error, logtype_cnid, "dbd_getstamp: No rootinfo record found");
        rply->result = CNID_DBD_RES_NOTFOUND;
        return 1;
    }
    
    rply->namelen = CNID_DEV_LEN;
    rply->name = (char *)data.data + CNID_DEV_OFS;
    rply->result = CNID_DBD_RES_OK;
    return 1;
}
