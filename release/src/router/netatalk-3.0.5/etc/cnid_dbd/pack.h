/*
 * Copyright (C) Joerg Lenneis 2003
 * Copyright (C) Frank Lahm 2010
 * All Rights Reserved.  See COPYING.
 */

#ifndef CNID_DBD_PACK_H
#define CNID_DBD_PACK_H 1

#include <db.h>
#include <atalk/cnid_dbd_private.h>

extern unsigned char *pack_cnid_data(struct cnid_dbd_rqst *);
extern int didname(DB *dbp, const DBT *pkey, const DBT *pdata, DBT *skey);
extern int devino(DB *dbp, const DBT *pkey, const DBT *pdata, DBT *skey);
extern int idxname(DB *dbp, const DBT *pkey, const DBT *pdata, DBT *skey);
extern void pack_setvol(const struct vol *vol);
#endif /* CNID_DBD_PACK_H */
