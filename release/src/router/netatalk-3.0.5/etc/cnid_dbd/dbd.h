/*
 * Copyright (C) Joerg Lenneis 2003
 * Copyright (C) Frank Lahm 2009, 2010
 * All Rights Reserved.  See COPYING.
 */

#ifndef CNID_DBD_DBD_H
#define CNID_DBD_DBD_H 1

#include <arpa/inet.h>

#include <atalk/cnid_dbd_private.h>

extern int add_cnid(DBD *dbd, struct cnid_dbd_rqst *rqst, struct cnid_dbd_rply *rply);
extern int get_cnid(DBD *dbd, struct cnid_dbd_rply *rply);

extern int dbd_add(DBD *dbd, struct cnid_dbd_rqst *, struct cnid_dbd_rply *);
extern int dbd_lookup(DBD *dbd, struct cnid_dbd_rqst *, struct cnid_dbd_rply *);
extern int dbd_get(DBD *dbd, struct cnid_dbd_rqst *, struct cnid_dbd_rply *);
extern int dbd_resolve(DBD *dbd, struct cnid_dbd_rqst *, struct cnid_dbd_rply *);
extern int dbd_update(DBD *dbd, struct cnid_dbd_rqst *, struct cnid_dbd_rply *);
extern int dbd_delete(DBD *dbd, struct cnid_dbd_rqst *, struct cnid_dbd_rply *, int idx);
extern int dbd_getstamp(DBD *dbd, struct cnid_dbd_rqst *, struct cnid_dbd_rply *);
extern int dbd_rebuild_add(DBD *dbd, struct cnid_dbd_rqst *, struct cnid_dbd_rply *);
extern int dbd_search(DBD *dbd, struct cnid_dbd_rqst *, struct cnid_dbd_rply *);
extern int dbd_check_indexes(DBD *dbd, char *);

#endif /* CNID_DBD_DBD_H */
