/*
 * Copyright (C) Frank Lahm 2010
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

int dbd_search(DBD *dbd, struct cnid_dbd_rqst *rqst, struct cnid_dbd_rply *rply)
{
    DBT key;
    int results;
    static char resbuf[DBD_MAX_SRCH_RSLTS * sizeof(cnid_t)];

    LOG(log_debug, logtype_cnid, "dbd_search(\"%s\"):", rqst->name);

    memset(&key, 0, sizeof(key));
    rply->name = resbuf;
    rply->namelen = 0;

    key.data = (char *)rqst->name;
    key.size = rqst->namelen;

    if ((results = dbif_search(dbd, &key, resbuf)) < 0) {
        LOG(log_error, logtype_cnid, "dbd_search(\"%s\"): db error", rqst->name);
        rply->result = CNID_DBD_RES_ERR_DB;
        return -1;
    }
    if (results) {
        LOG(log_debug, logtype_cnid, "dbd_search(\"%s\"): %d matches", rqst->name, results);
        rply->namelen = results * sizeof(cnid_t);
        rply->result = CNID_DBD_RES_OK;
    } else {
        LOG(log_debug, logtype_cnid, "dbd_search(\"%s\"): no matches", rqst->name);
        rply->result = CNID_DBD_RES_NOTFOUND;
    }

    return 1;
}
