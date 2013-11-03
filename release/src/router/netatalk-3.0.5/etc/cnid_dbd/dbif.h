/*
  Copyright (C) Joerg Lenneis 2003
  Copyright (C) Frank Lahm 2009
  All Rights Reserved.  See COPYING.


  API usage
  =========

  Initialisation
  --------------
  1. Provide storage for a DBD * handle
     DBD *dbd;
  2. Call dbif_init with a filename to receive a DBD handle:
     dbd = dbif_init("cnid2.db");
     Pass NULL to create an in-memory db.
     Note: the DBD type is NOT from BerkeleyDB ! We've defined it.
  3. Call dbif_env_open to open an dbd environment if you called dbif_init
     with a filename. Pass a db_param here for on-disk databases.
  4. Call dbif_open to finally open the CNID database itself. Pass db_param
     here for in-memory database.
  
  Querying the CNID database
  --------------------------
  Call dbif_[get|pget|put|del]. They map to the corresponding BerkeleyDB calls
  with the same names.

  Transactions
  ------------
  We use AUTO_COMMIT for the BDB database accesses. This avoids explicit transactions
  for every bdb access which speeds up reads. But in order to be able to rollback
  in case of errors we start a transaction once we encounter the first write from
  dbif_put or dbif_del.
  Thus you shouldn't call dbif_txn_[begin|abort|commit], they're used internally.

  Checkpoiting
  ------------
  Call dbif_txn_checkpoint.

  Closing
  -------
  Call dbif_close.

  Silent Upgrade Support
  ----------------------

  On cnid_dbd shutdown we reopen the environment with recovery, close and then
  remove it. This enables an upgraded netatalk installation possibly linked against
  a newer bdb lib to succesfully open/create an environment and then silently
  upgrade the database itself. How nice!
*/

#ifndef CNID_DBD_DBIF_H
#define CNID_DBD_DBIF_H 1

#include <db.h>
#include <atalk/adouble.h>
#include "db_param.h"

#define DBIF_DB_CNT 4
 
#define DBIF_CNID          0
#define DBIF_IDX_DEVINO    1
#define DBIF_IDX_DIDNAME   2
#define DBIF_IDX_NAME      3

#define LOCKFILENAME  "lock"
#define LOCK_FREE          0
#define LOCK_UNLOCK        1
#define LOCK_EXCL          2
#define LOCK_SHRD          3

/* Structures */
typedef struct {
    char     *name;
    DB       *db;
    uint32_t flags;
    uint32_t openflags;
    DBTYPE   type;
} db_table;

typedef struct {
    DB_ENV   *db_env;
    struct db_param db_param;
    DB_TXN   *db_txn;
    DBC      *db_cur;              /* for dbif_walk */
    char     *db_envhome;
    char     *db_filename;
    FILE     *db_errlog;
    db_table db_table[DBIF_DB_CNT];
} DBD;

extern DBD *dbif_init(const char *envhome, const char *dbname);
extern int dbif_env_open(DBD *dbd, struct db_param *dbp, uint32_t dbenv_oflags);
extern int dbif_open(DBD *dbd, struct db_param *dbp, int reindex);
extern int dbif_close(DBD *dbd);
extern int dbif_env_remove(const char *path);

extern int dbif_get(DBD *, const int, DBT *, DBT *, u_int32_t);
extern int dbif_pget(DBD *, const int, DBT *, DBT *, DBT *, u_int32_t);
extern int dbif_put(DBD *, const int, DBT *, DBT *, u_int32_t);
extern int dbif_del(DBD *, const int, DBT *, u_int32_t);
extern int dbif_count(DBD *, const int, u_int32_t *);
extern int dbif_search(DBD *dbd, DBT *key, char *resbuf);
extern int dbif_copy_rootinfokey(DBD *srcdbd, DBD *destdbd);
extern int dbif_txn_begin(DBD *);
extern int dbif_txn_commit(DBD *);
extern int dbif_txn_abort(DBD *);
extern int dbif_txn_close(DBD *dbd, int ret); /* Switch between commit+abort */
extern int dbif_txn_checkpoint(DBD *, u_int32_t, u_int32_t, u_int32_t);

extern int dbif_dump(DBD *dbd, int dumpindexes);
extern int dbif_idwalk(DBD *dbd, cnid_t *cnid, int close);
#endif
