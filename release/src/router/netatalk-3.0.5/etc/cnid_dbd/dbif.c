/*
 * Copyright (C) Joerg Lenneis 2003
 * Copyright (C) Frank Lahm 2009
 * All Rights Reserved.  See COPYING.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif /* HAVE_CONFIG_H */

#include <stdio.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <arpa/inet.h>

#include <db.h>

#include <atalk/logger.h>
#include <atalk/util.h>
#include <atalk/errchk.h>
#include <atalk/cnid.h>

#include "db_param.h"
#include "dbif.h"
#include "pack.h"

#define DB_ERRLOGFILE "db_errlog"

/*!
 * Get the db stamp which is the st_ctime of the file "cnid2.db" and store it in buffer
 */
static int dbif_stamp(DBD *dbd, void *buffer, int size)
{
    EC_INIT;
    struct stat st;
    int cwd = -1;

    if (size < 8)
        EC_FAIL;

    /* Remember cwd */
    if ((cwd = open(".", O_RDONLY)) < 0) {
        LOG(log_error, logtype_cnid, "error opening cwd: %s", strerror(errno));
        EC_FAIL;
    }

    /* chdir to db_envhome */
    if ((chdir(dbd->db_envhome)) != 0) {
        LOG(log_error, logtype_cnid, "error chdiring to db_env '%s': %s", dbd->db_envhome, strerror(errno));        
        EC_FAIL;
    }

    if (stat(dbd->db_table[DBIF_CNID].name, &st) < 0) {
        LOG(log_error, logtype_cnid, "error stating database %s: %s", dbd->db_table[DBIF_CNID].name, db_strerror(errno));
        EC_FAIL;
    }

    LOG(log_maxdebug, logtype_cnid,"stamp: %s", asctime(localtime(&st.st_ctime)));

    memset(buffer, 0, size);
    memcpy(buffer, &st.st_ctime, sizeof(st.st_ctime));

EC_CLEANUP:
    if (cwd != -1) {
        if (fchdir(cwd) != 0) {
            LOG(log_error, logtype_cnid, "error chdiring back: %s", strerror(errno));        
            EC_STATUS(-1);
        }
        close(cwd);
    }
    EC_EXIT;
}

/*!
 * Inititialize rootinfo key (which has CNID 0 as key)
 *
 * This also "stamps" the database, which means storing st.st_ctime of the
 * "cnid2.db" file in the rootinfo data at the DEV offset
 *
 * @param dbd      (rw) database handle
 * @param version  (r)  database version number
 *
 * @returns -1 on error, 0 on success
 */
static int dbif_init_rootinfo(DBD *dbd, int version)
{
    DBT key, data;
    uint32_t uint32;
    char buf[ROOTINFO_DATALEN];

    LOG(log_debug, logtype_cnid, "Setting CNID database version to %u", version);


    memset(&key, 0, sizeof(key));
    memset(&data, 0, sizeof(data));
    key.data = ROOTINFO_KEY;
    key.size = ROOTINFO_KEYLEN;
    data.data = buf;
    data.size = ROOTINFO_DATALEN;

    memcpy(buf, ROOTINFO_DATA, ROOTINFO_DATALEN);

    uint32 = htonl(CNID_START - 1);
    memcpy(buf + CNID_TYPE_OFS, &uint32, sizeof(uint32));

    uint32 = htonl(version);
    memcpy(buf + CNID_DID_OFS, &uint32, sizeof(uint32));

    if (dbif_stamp(dbd, buf + CNID_DEV_OFS, CNID_DEV_LEN) < 0)
        return -1;

    if (dbif_put(dbd, DBIF_CNID, &key, &data, 0) < 0)
        return -1;
    if (dbif_txn_commit(dbd) != 1) {
        LOG(log_error, logtype_cnid, "dbif_init_rootinfo: cant commit txn");
        return -1;
    }

    return 0;
}

/*!
 * Return CNID database version number
 *
 * Returns version in *version
 *
 * @returns -1 on error, 0 if theres no rootinfo key yet, 1 if *version is returned
 */
static int dbif_getversion(DBD *dbd, uint32_t *version)
{
    DBT key, data;
    int ret;

    LOG(log_maxdebug, logtype_cnid, "dbif_getversion: reading version info");

    *version = -1;
    memset(&key, 0, sizeof(key));
    memset(&data, 0, sizeof(data));
    key.data = ROOTINFO_KEY;
    key.size = ROOTINFO_KEYLEN;

    switch (dbif_get(dbd, DBIF_CNID, &key, &data, 0)) {
    case 1: /* found */
        memcpy(version, (char *)data.data + CNID_DID_OFS, sizeof(uint32_t));
        *version = ntohl(*version);
        LOG(log_debug, logtype_cnid, "CNID database version %u", *version);
        ret = 1;
        break;
    case 0: /* not found */
        LOG(log_debug, logtype_cnid, "dbif_getversion: no version info found");
        ret = 0;
        break;
    default:
        LOG(log_error, logtype_cnid, "dbif_getversion: database error");
        ret = -1;
        break;
    }

    return ret;
}

/*!
 * Set CNID database version number
 *
 * Initializes rootinfo key as neccessary
 * @returns -1 on error, 0 on success
 */
static int dbif_setversion(DBD *dbd, uint32_t version)
{
    int ret;
    DBT key, data;
    uint32_t v;

    LOG(log_debug, logtype_cnid, "Setting CNID database version to %u", version);

    v = version;
    v = htonl(v);

    memset(&key, 0, sizeof(key));
    memset(&data, 0, sizeof(data));
    key.data = ROOTINFO_KEY;
    key.size = ROOTINFO_KEYLEN;

    if ((ret = dbif_get(dbd, DBIF_CNID, &key, &data, 0)) == -1)
        return -1;
    if (ret == 0) {
        /* No rootinfo key yet, init it */
        if (dbif_init_rootinfo(dbd, CNID_VERSION) != 0)
            return -1;
        /* Now try again */
        if (dbif_get(dbd, DBIF_CNID, &key, &data, 0) == -1)
            return -1;
    }
    memcpy((char *)data.data + CNID_DID_OFS, &v, sizeof(v));
    data.size = ROOTINFO_DATALEN;
    if (dbif_put(dbd, DBIF_CNID, &key, &data, 0) < 0)
        return -1;

    return 0;
}

/*!
 * Upgrade CNID database versions, initialize rootinfo key as as necessary in dbif_setversion()
 *
 * For now this does nothing, as upgrading from ver. 0 to 1 is done in dbif_open
 */
#define UNINTIALIZED_DB UINT32_MAX
static int dbif_upgrade(DBD *dbd)
{
    uint32_t version = CNID_VERSION_UNINTIALIZED_DB;

    if (dbif_getversion(dbd, &version) == -1)
        return -1;
    if (version == CNID_VERSION_UNINTIALIZED_DB) {
        version = CNID_VERSION;
        if (dbif_setversion(dbd, CNID_VERSION) != 0)
            return -1;
    }

    /* 
     * Do upgrade stuff ...
     */

    /* Write current version to database */
    if (version != CNID_VERSION) {
        if (dbif_setversion(dbd, CNID_VERSION) != 0)
            return -1;
    }

    LOG(log_debug, logtype_cnid, "Finished CNID database version upgrade check");

    return 0;
}

/* --------------- */
static int dbif_openlog(DBD *dbd)
{
    int ret = 0;
    int cwd = -1;

    if ( ! dbd->db_filename)
        /* in memory db */
        return 0;

    /* Remember cwd */
    if ((cwd = open(".", O_RDONLY)) < 0) {
        LOG(log_error, logtype_cnid, "error opening cwd: %s", strerror(errno));
        return -1;
    }

    /* chdir to db_envhome */
    if ((chdir(dbd->db_envhome)) != 0) {
        LOG(log_error, logtype_cnid, "error chdiring to db_env '%s': %s", dbd->db_envhome, strerror(errno));        
        ret = -1;
        goto exit;
    }

    if ((dbd->db_errlog = fopen(DB_ERRLOGFILE, "a")) == NULL)
        LOG(log_warning, logtype_cnid, "error creating/opening DB errlogfile: %s", strerror(errno));

    if (dbd->db_errlog != NULL) {
        dbd->db_env->set_errfile(dbd->db_env, dbd->db_errlog);
        dbd->db_env->set_msgfile(dbd->db_env, dbd->db_errlog);
    }

exit:
    if (cwd != -1) {
        if ((fchdir(cwd)) != 0) {
            LOG(log_error, logtype_cnid, "error chdiring back: %s", strerror(errno));        
            ret = -1;
        }
        close(cwd);
    }
    return ret;
}

/* --------------- */
static int dbif_logautorem(DBD *dbd)
{
    int ret = 0;
    int cwd = -1;
    char **logfiles = NULL;
    char **file;

    if ( ! dbd->db_filename)
        /* in memory db */
        return 0;

    /* Remember cwd */
    if ((cwd = open(".", O_RDONLY)) < 0) {
        LOG(log_error, logtype_cnid, "error opening cwd: %s", strerror(errno));
        return -1;
    }

    /* chdir to db_envhome */
    if ((chdir(dbd->db_envhome)) != 0) {
        LOG(log_error, logtype_cnid, "error chdiring to db_env '%s': %s", dbd->db_envhome, strerror(errno));        
        ret = -1;
        goto exit;
    }

    if ((ret = dbd->db_env->log_archive(dbd->db_env, &logfiles, 0)) != 0) {
        LOG(log_error, logtype_cnid, "error getting list of stale logfiles: %s",
            db_strerror(ret));
        dbd->db_env->close(dbd->db_env, 0);
        dbd->db_env = NULL;
        ret = -1;
        goto exit;
    }

    if (logfiles != NULL) {
        for (file = logfiles; *file != NULL; file++) {
            if (unlink(*file) < 0)
                LOG(log_warning, logtype_cnid, "Error removing stale logfile %s: %s", *file, strerror(errno));
        }
        free(logfiles);
    }

exit:
    if (cwd != -1) {
        if ((fchdir(cwd)) != 0) {
            LOG(log_error, logtype_cnid, "error chdiring back: %s", strerror(errno));        
            ret = -1;
        }
        close(cwd);
    }
    return ret;
}

/* --------------- */
DBD *dbif_init(const char *envhome, const char *filename)
{
    DBD *dbd;

    if ( NULL == (dbd = calloc(sizeof(DBD), 1)) )
        return NULL;

    /* filename == NULL means in memory db */
    if (filename) {
        if (! envhome)
            return NULL;

        dbd->db_envhome = strdup(envhome);
        if (NULL == dbd->db_envhome) {
            free(dbd);
            return NULL;
        }

        dbd->db_filename = strdup(filename);
        if (NULL == dbd->db_filename) {
            free(dbd->db_envhome);
            free(dbd);
            return NULL;
        }
    }
    
    dbd->db_table[DBIF_CNID].name        = "cnid2.db";
    dbd->db_table[DBIF_IDX_DEVINO].name  = "devino.db";
    dbd->db_table[DBIF_IDX_DIDNAME].name = "didname.db";
    dbd->db_table[DBIF_IDX_NAME].name    = "name.db";

    dbd->db_table[DBIF_CNID].type        = DB_BTREE;
    dbd->db_table[DBIF_IDX_DEVINO].type  = DB_BTREE;
    dbd->db_table[DBIF_IDX_DIDNAME].type = DB_BTREE;
    dbd->db_table[DBIF_IDX_NAME].type    = DB_BTREE;

    dbd->db_table[DBIF_CNID].openflags        = DB_CREATE;
    dbd->db_table[DBIF_IDX_DEVINO].openflags  = DB_CREATE;
    dbd->db_table[DBIF_IDX_DIDNAME].openflags = DB_CREATE;
    dbd->db_table[DBIF_IDX_NAME].openflags    = DB_CREATE;

    dbd->db_table[DBIF_IDX_NAME].flags = DB_DUPSORT;

    return dbd;
}

/* 
   We must open the db_env with an absolute pathname, as `dbd` keeps chdir'ing, which
   breaks e.g. bdb logfile-rotation with relative pathnames.
   But still we use relative paths with DB_ERRLOGFILE
   in order to avoid creating absolute paths by copying. Both have no problem with
   a relative path.
*/
int dbif_env_open(DBD *dbd, struct db_param *dbp, uint32_t dbenv_oflags)
{
    int ret;

    if ((ret = db_env_create(&dbd->db_env, 0))) {
        LOG(log_error, logtype_cnid, "error creating DB environment: %s",
            db_strerror(ret));
        dbd->db_env = NULL;
        return -1;
    }

    dbd->db_param = *dbp;

    if ((dbif_openlog(dbd)) != 0)
        return -1;

    if (dbenv_oflags & DB_RECOVER) {

        LOG(log_debug, logtype_cnid, "Running recovery");

        dbd->db_env->set_verbose(dbd->db_env, DB_VERB_RECOVERY, 1);
        /* Open the database for recovery using DB_PRIVATE option which is faster */
        if ((ret = dbd->db_env->open(dbd->db_env, dbd->db_envhome, dbenv_oflags | DB_PRIVATE, 0))) {
            LOG(log_error, logtype_cnid, "error opening DB environment: %s",
                db_strerror(ret));
            dbd->db_env->close(dbd->db_env, 0);
            dbd->db_env = NULL;
            return -1;
        }
        dbenv_oflags = (dbenv_oflags & ~DB_RECOVER);

        if (dbd->db_errlog != NULL)
            fflush(dbd->db_errlog);

        if ((ret = dbd->db_env->close(dbd->db_env, 0))) {
            LOG(log_error, logtype_cnid, "error closing DB environment after recovery: %s",
                db_strerror(ret));
            dbd->db_env = NULL;
            return -1;
        }
        dbd->db_errlog = NULL;        

        if ((ret = db_env_create(&dbd->db_env, 0))) {
            LOG(log_error, logtype_cnid, "error creating DB environment after recovery: %s",
                db_strerror(ret));
            dbd->db_env = NULL;
            return -1;
        }

        if ((dbif_openlog(dbd)) != 0)
            return -1;

        LOG(log_debug, logtype_cnid, "Finished recovery.");
    }

    if ((ret = dbd->db_env->set_cachesize(dbd->db_env, 0, 1024 * dbp->cachesize, 0))) {
        LOG(log_error, logtype_cnid, "error setting DB environment cachesize to %i: %s",
            dbp->cachesize, db_strerror(ret));
        dbd->db_env->close(dbd->db_env, 0);
        dbd->db_env = NULL;
        return -1;
    }

    if ((ret = dbd->db_env->set_lk_max_locks(dbd->db_env, dbp->maxlocks))) {
        LOG(log_error, logtype_cnid, "error setting DB environment maxlocks to %i: %s",
            10000, db_strerror(ret));
        dbd->db_env->close(dbd->db_env, 0);
        dbd->db_env = NULL;
        return -1;
    }

    if ((ret = dbd->db_env->set_lk_max_objects(dbd->db_env, dbp->maxlockobjs))) {
        LOG(log_error, logtype_cnid, "error setting DB environment max lockobjects to %i: %s",
            10000, db_strerror(ret));
        dbd->db_env->close(dbd->db_env, 0);
        dbd->db_env = NULL;
        return -1;
    }

    if ((ret = dbd->db_env->open(dbd->db_env, dbd->db_envhome, dbenv_oflags, 0))) {
        LOG(log_error, logtype_cnid, "error opening DB environment after recovery: %s",
            db_strerror(ret));
        dbd->db_env->close(dbd->db_env, 0);
        dbd->db_env = NULL;
        return -1;
    }

    if ((ret = dbd->db_env->set_flags(dbd->db_env, DB_AUTO_COMMIT, 1))) {
        LOG(log_error, logtype_cnid, "error setting DB_AUTO_COMMIT flag: %s",
            db_strerror(ret));
        dbd->db_env->close(dbd->db_env, 0);
        dbd->db_env = NULL;
        return -1;
    }

    if (dbp->logfile_autoremove) {
        if ((dbif_logautorem(dbd)) != 0)
            return -1;

#if DB_VERSION_MAJOR > 4 || (DB_VERSION_MAJOR == 4 && DB_VERSION_MINOR >= 7)
        if ((ret = dbd->db_env->log_set_config(dbd->db_env, DB_LOG_AUTO_REMOVE, 1))) {
            LOG(log_error, logtype_cnid, "error setting DB_LOG_AUTO_REMOVE flag: %s",
            db_strerror(ret));
            dbd->db_env->close(dbd->db_env, 0);
            dbd->db_env = NULL;
            return -1;
        }
#else
        if ((ret = dbd->db_env->set_flags(dbd->db_env, DB_LOG_AUTOREMOVE, 1))) {
            LOG(log_error, logtype_cnid, "error setting DB_LOG_AUTOREMOVE flag: %s",
                db_strerror(ret));
            dbd->db_env->close(dbd->db_env, 0);
            dbd->db_env = NULL;
            return -1;
        }
#endif
    }

    return 0;
}

/* --------------- */
int dbif_open(DBD *dbd, struct db_param *dbp, int reindex)
{
    int ret, i, cwd;
    u_int32_t count;
    struct stat st;
    DB *upgrade_db;

    /* Try to upgrade if it's a normal on-disk database */
    if (dbd->db_envhome) {
        /* Remember cwd */
        if ((cwd = open(".", O_RDONLY)) < 0) {
            LOG(log_error, logtype_cnid, "error opening cwd: %s", strerror(errno));
            return -1;
        }
        
        /* chdir to db_envhome. makes it easier checking for old db files and creating db_errlog file  */
        if ((chdir(dbd->db_envhome)) != 0) {
            LOG(log_error, logtype_cnid, "error chdiring to db_env '%s': %s", dbd->db_envhome, strerror(errno));        
            return -1;
        }
        
        if ((stat(dbd->db_filename, &st)) == 0) {
            LOG(log_debug, logtype_cnid, "See if we can upgrade the CNID database...");
            if ((ret = db_create(&upgrade_db, dbd->db_env, 0))) {
                LOG(log_error, logtype_cnid, "error creating handle for database: %s", db_strerror(ret));
                return -1;
            }
            if ((ret = upgrade_db->upgrade(upgrade_db, dbd->db_filename, 0))) {
                LOG(log_error, logtype_cnid, "error upgarding database: %s", db_strerror(ret));
                return -1;
            }
            if ((ret = upgrade_db->close(upgrade_db, 0))) {
                LOG(log_error, logtype_cnid, "error closing database: %s", db_strerror(ret));
                return -1;
            }
            if ((ret = dbd->db_env->txn_checkpoint(dbd->db_env, 0, 0, DB_FORCE))) {
                LOG(log_error, logtype_cnid, "error forcing checkpoint: %s", db_strerror(ret));
                return -1;
            }
            LOG(log_debug, logtype_cnid, "Finished BerkeleyBD upgrade check");
        }
        
        if ((fchdir(cwd)) != 0) {
            LOG(log_error, logtype_cnid, "error chdiring back: %s", strerror(errno));        
            return -1;
        }
    }

    /* Now open databases ... */
    for (i = 0; i != DBIF_DB_CNT; i++) {
        if ((ret = db_create(&dbd->db_table[i].db, dbd->db_env, 0))) {
            LOG(log_error, logtype_cnid, "error creating handle for database %s: %s",
                dbd->db_table[i].name, db_strerror(ret));
            return -1;
        }

        if (dbd->db_table[i].flags) {
            if ((ret = dbd->db_table[i].db->set_flags(dbd->db_table[i].db,
                                                      dbd->db_table[i].flags))) {
                LOG(log_error, logtype_cnid, "error setting flags for database %s: %s",
                    dbd->db_table[i].name, db_strerror(ret));
                return -1;
            }
        }

        if ( ! dbd->db_env) {   /* In memory db */
            if ((ret = dbd->db_table[i].db->set_cachesize(dbd->db_table[i].db,
                                                          0,
                                                          dbp->cachesize,
                                                          4)) /* split in 4 memory chunks */
                < 0)  {
                LOG(log_error, logtype_cnid, "error setting cachesize %u KB for database %s: %s",
                    dbp->cachesize / 1024, dbd->db_table[i].name, db_strerror(ret));
                return -1;
            }
        }

        if (dbd->db_table[i].db->open(dbd->db_table[i].db,
                                      dbd->db_txn,
                                      dbd->db_filename,
                                      dbd->db_table[i].name,
                                      dbd->db_table[i].type,
                                      dbd->db_table[i].openflags,
                                      0664) < 0) {
            LOG(log_error, logtype_cnid, "Cant open database");
            return -1;
        }

        if (reindex && i > 0) {
            LOG(log_info, logtype_cnid, "Truncating CNID index.");
            if ((ret = dbd->db_table[i].db->truncate(dbd->db_table[i].db, NULL, &count, 0))) {
                LOG(log_error, logtype_cnid, "error truncating database %s: %s",
                    dbd->db_table[i].name, db_strerror(ret));
                return -1;
            }
        }
    }

    /* TODO: Implement CNID DB versioning info on new databases. */

    /* Associate the secondary with the primary. */
    if (reindex)
        LOG(log_info, logtype_cnid, "Reindexing did/name index...");
    if ((ret = dbd->db_table[0].db->associate(dbd->db_table[DBIF_CNID].db,
                                              dbd->db_txn,
                                              dbd->db_table[DBIF_IDX_DIDNAME].db, 
                                              didname,
                                              (reindex) ? DB_CREATE : 0))
         != 0) {
        LOG(log_error, logtype_cnid, "Failed to associate didname database: %s",db_strerror(ret));
        return -1;
    }
    if (reindex)
        LOG(log_info, logtype_cnid, "... done.");

    if (reindex)
        LOG(log_info, logtype_cnid, "Reindexing dev/ino index...");
    if ((ret = dbd->db_table[0].db->associate(dbd->db_table[0].db, 
                                              dbd->db_txn,
                                              dbd->db_table[DBIF_IDX_DEVINO].db, 
                                              devino,
                                              (reindex) ? DB_CREATE : 0))
        != 0) {
        LOG(log_error, logtype_cnid, "Failed to associate devino database: %s",db_strerror(ret));
        return -1;
    }
    if (reindex)
        LOG(log_info, logtype_cnid, "... done.");

    if (reindex)
        LOG(log_info, logtype_cnid, "Reindexing name index...");

    /*
     * Upgrading from version 0 to 1 requires adding the name index below which
     * must be done by specifying the DB_CREATE flag
     */
    uint32_t version = CNID_VERSION;
    if (dbd->db_envhome && !reindex) {
        if (dbif_getversion(dbd, &version) == -1)
            return -1;
    }

    if ((ret = dbd->db_table[0].db->associate(dbd->db_table[0].db, 
                                              dbd->db_txn,
                                              dbd->db_table[DBIF_IDX_NAME].db, 
                                              idxname,
                                              (reindex
                                               || 
                                               ((CNID_VERSION == CNID_VERSION_1) && (version == CNID_VERSION_0)))
                                              ? DB_CREATE : 0)) != 0) {
        LOG(log_error, logtype_cnid, "Failed to associate name index: %s", db_strerror(ret));
        return -1;
    }
    if (reindex)
        LOG(log_info, logtype_cnid, "... done.");

    if ((dbd->db_envhome) && ((ret = dbif_upgrade(dbd)) != 0)) {
        LOG(log_error, logtype_cnid, "Error upgrading CNID database to version %d", CNID_VERSION);
        return -1;
    }
    
    return 0;
}

/* ------------------------ */
static int dbif_closedb(DBD *dbd)
{
    int i;
    int ret;
    int err = 0;

    for (i = DBIF_DB_CNT -1; i >= 0; i--) {
        if (dbd->db_table[i].db != NULL && (ret = dbd->db_table[i].db->close(dbd->db_table[i].db, 0))) {
            LOG(log_error, logtype_cnid, "error closing database %s: %s", dbd->db_table[i].name, db_strerror(ret));
            err++;
        }
    }
    if (err)
        return -1;
    return 0;
}

/* ------------------------ */
int dbif_close(DBD *dbd)
{
    int ret;
    int err = 0;

    if (dbif_closedb(dbd))
        err++;

    if (dbd->db_env != NULL && (ret = dbd->db_env->close(dbd->db_env, 0))) {
        LOG(log_error, logtype_cnid, "error closing DB environment: %s", db_strerror(ret));
        err++;
    }
    if (dbd->db_errlog != NULL && fclose(dbd->db_errlog) == EOF) {
        LOG(log_error, logtype_cnid, "error closing DB logfile: %s", strerror(errno));
        err++;
    }

    free(dbd->db_filename);
    free(dbd);
    dbd = NULL;

    if (err)
        return -1;
    return 0;
}

/* 
   In order to support silent database upgrades:
   destroy env at cnid_dbd shutdown.
 */
int dbif_env_remove(const char *path)
{
    int ret;
    DBD *dbd;

    LOG(log_debug, logtype_cnid, "Trying to remove BerkeleyDB environment");

    if (NULL == (dbd = dbif_init(path, "cnid2.db")))
        return -1;

    /* Get db_env handle */
    if ((ret = db_env_create(&dbd->db_env, 0))) {
        LOG(log_error, logtype_cnid, "error creating DB environment: %s", db_strerror(ret));
        dbd->db_env = NULL;
        return -1;
    }

    if ((dbif_openlog(dbd)) != 0)
        return -1;

    /* Open environment with recovery */
    if ((ret = dbd->db_env->open(dbd->db_env, 
                                 dbd->db_envhome,
                                 DB_CREATE | DB_INIT_LOG | DB_INIT_MPOOL | DB_INIT_LOCK | DB_INIT_TXN | DB_RECOVER | DB_PRIVATE,
                                 0))) {
        LOG(log_error, logtype_cnid, "error opening DB environment: %s",
            db_strerror(ret));
        dbd->db_env->close(dbd->db_env, 0);
        dbd->db_env = NULL;
        return -1;
    }

    if (dbd->db_errlog != NULL)
        fflush(dbd->db_errlog);

    /* Remove logfiles */
    if ((ret = dbd->db_env->log_archive(dbd->db_env, NULL, DB_ARCH_REMOVE))) {
         LOG(log_error, logtype_cnid, "error removing transaction logfiles: %s", db_strerror(ret));
         return -1;
    }

    if ((ret = dbd->db_env->close(dbd->db_env, 0))) {
        LOG(log_error, logtype_cnid, "error closing DB environment after recovery: %s", db_strerror(ret));
        dbd->db_env = NULL;
        return -1;
    }

    LOG(log_debug, logtype_cnid, "BerkeleyDB environment recovery done.");

    /* Get a new db_env handle and then remove environment */
    if ((ret = db_env_create(&dbd->db_env, 0))) {
        LOG(log_error, logtype_cnid, "error acquiring db_end handle: %s", db_strerror(ret));
        dbd->db_env = NULL;
        return -1;
    }
    if ((ret = dbd->db_env->remove(dbd->db_env, dbd->db_envhome, 0))) {
        LOG(log_error, logtype_cnid, "error removing BerkeleyDB environment: %s", db_strerror(ret));
        return -1;
    }

    LOG(log_debug, logtype_cnid, "Removed BerkeleyDB environment.");

    return 0;
}

/*
 *  The following three functions are wrappers for DB->get(), DB->put() and DB->del().
 *  All three return -1 on error. dbif_get()/dbif_del return 1 if the key was found and 0
 *  otherwise. dbif_put() returns 0 if key/val was successfully updated and 1 if
 *  the DB_NOOVERWRITE flag was specified and the key already exists.
 *
 *  All return codes other than DB_NOTFOUND and DB_KEYEXIST from the DB->()
 *  functions are not expected and therefore error conditions.
 */

int dbif_get(DBD *dbd, const int dbi, DBT *key, DBT *val, u_int32_t flags)
{
    int ret;

    ret = dbd->db_table[dbi].db->get(dbd->db_table[dbi].db,
                                     dbd->db_txn,
                                     key,
                                     val,
                                     flags);

    if (ret == DB_NOTFOUND)
        return 0;
    if (ret) {
        LOG(log_error, logtype_cnid, "error retrieving value from %s: %s",
            dbd->db_table[dbi].name, db_strerror(ret));
        return -1;
    } else
        return 1;
}

/* search by secondary return primary */
int dbif_pget(DBD *dbd, const int dbi, DBT *key, DBT *pkey, DBT *val, u_int32_t flags)
{
    int ret;

    ret = dbd->db_table[dbi].db->pget(dbd->db_table[dbi].db,
                                      dbd->db_txn,
                                      key,
                                      pkey,
                                      val,
                                      flags);

    if (ret == DB_NOTFOUND || ret == DB_SECONDARY_BAD) {
        return 0;
    }
    if (ret) {
        LOG(log_error, logtype_cnid, "error retrieving value from %s: %s",
            dbd->db_table[dbi].name, db_strerror(ret));
        return -1;
   } else
        return 1;
}

/* -------------------------- */
int dbif_put(DBD *dbd, const int dbi, DBT *key, DBT *val, u_int32_t flags)
{
    int ret;

    if (dbif_txn_begin(dbd) < 0) {
        LOG(log_error, logtype_cnid, "error setting key/value in %s", dbd->db_table[dbi].name);
        return -1;
    }

    ret = dbd->db_table[dbi].db->put(dbd->db_table[dbi].db,
                                     dbd->db_txn,
                                     key,
                                     val,
                                     flags);

    
    if (ret) {
        if ((flags & DB_NOOVERWRITE) && ret == DB_KEYEXIST) {
            return 1;
        } else {
            LOG(log_error, logtype_cnid, "error setting key/value in %s: %s",
                dbd->db_table[dbi].name, db_strerror(ret));
            return -1;
        }
    } else
        return 0;
}

int dbif_del(DBD *dbd, const int dbi, DBT *key, u_int32_t flags)
{
    int ret;

    /* For cooperation with the dbd utility and its usage of a cursor */
    if (dbd->db_cur) {
        dbd->db_cur->close(dbd->db_cur);
        dbd->db_cur = NULL;
    }    

    if (dbif_txn_begin(dbd) < 0) {
        LOG(log_error, logtype_cnid, "error deleting key/value from %s", dbd->db_table[dbi].name);
        return -1;
    }

    ret = dbd->db_table[dbi].db->del(dbd->db_table[dbi].db,
                                     dbd->db_txn,
                                     key,
                                     flags);
    
    if (ret == DB_NOTFOUND) {
        LOG(log_debug, logtype_cnid, "key not found");
        return 0;
    }
    if (ret) {
        LOG(log_error, logtype_cnid, "error deleting key/value from %s: %s",
            dbd->db_table[dbi].name, db_strerror(ret));
        return -1;
    } else
        return 1;
}

/*!
 * Search the database by name
 *
 * @param resbuf    (w) buffer for search results CNIDs, maxsize is assumed to be
 *                      DBD_MAX_SRCH_RSLTS * sizefof(cnid_t)
 *
 * @returns -1 on error, 0 when nothing found, else the number of matches
 */
int dbif_search(DBD *dbd, DBT *key, char *resbuf)
{
    int ret = 0;
    int count = 0;
    DBC *cursorp = NULL;
    DBT pkey, data;
    char *cnids = resbuf;
    cnid_t cnid;
    char *namebkp = key->data;
    int namelenbkp = key->size;

    memset(&pkey, 0, sizeof(DBT));
    memset(&data, 0, sizeof(DBT));

    /* Get a cursor */
    ret = dbd->db_table[DBIF_IDX_NAME].db->cursor(dbd->db_table[DBIF_IDX_NAME].db,
                                                  NULL,
                                                  &cursorp,
                                                  0);
    if (ret != 0) {
        LOG(log_error, logtype_cnid, "Couldn't create cursor: %s", db_strerror(ret));
        ret = -1;
        goto exit;
    }

    ret = cursorp->pget(cursorp, key, &pkey, &data, DB_SET_RANGE);
    while (count < DBD_MAX_SRCH_RSLTS && ret != DB_NOTFOUND) {
        if (!((namelenbkp <= key->size) && (strncmp(namebkp, key->data, namelenbkp) == 0)))
            break;
        count++;
        memcpy(cnids, pkey.data, sizeof(cnid_t));
        memcpy(&cnid, pkey.data, sizeof(cnid_t));
        cnids += sizeof(cnid_t);
        LOG(log_debug, logtype_cnid, "match: CNID %" PRIu32, ntohl(cnid));

        ret = cursorp->pget(cursorp, key, &pkey, &data, DB_NEXT);
    }

    ret = count;

exit:
    if (cursorp != NULL)
        cursorp->close(cursorp);
    return ret;
}

int dbif_txn_begin(DBD *dbd)
{
    int ret;

    /* If we already have an active txn, just return */
    if (dbd->db_txn)
        return 0;

    /* If our DBD has no env, just return (-> in memory db) */
    if (dbd->db_env == NULL)
        return 0;

    ret = dbd->db_env->txn_begin(dbd->db_env, NULL, &dbd->db_txn, 0);

    if (ret) {
        LOG(log_error, logtype_cnid, "error starting transaction: %s", db_strerror(ret));
        return -1;
    } else
        return 0;
}

int dbif_txn_commit(DBD *dbd)
{
    int ret;

    if (! dbd->db_txn)
        return 0;

    /* If our DBD has no env, just return (-> in memory db) */
    if (dbd->db_env == NULL)
        return 0;

    ret = dbd->db_txn->commit(dbd->db_txn, 0);
    dbd->db_txn = NULL;
    
    if (ret) {
        LOG(log_error, logtype_cnid, "error committing transaction: %s", db_strerror(ret));
        return -1;
    } else
        return 1;
}

int dbif_txn_abort(DBD *dbd)
{
    int ret;

    if (! dbd->db_txn)
        return 0;

    /* If our DBD has no env, just return (-> in memory db) */
    if (dbd->db_env == NULL)
        return 0;

    ret = dbd->db_txn->abort(dbd->db_txn);
    dbd->db_txn = NULL;
    
    if (ret) {
        LOG(log_error, logtype_cnid, "error aborting transaction: %s", db_strerror(ret));
        return -1;
    } else
        return 0;
}

/* 
   ret = 1 -> commit txn if db_param.txn_frequency
   ret = 0 -> abort txn db_param.txn_frequency -> exit!
   anything else -> exit!

   @returns 0 on success (abort or commit), -1 on error
*/
int dbif_txn_close(DBD *dbd, int ret)
{
    if (ret == 0) {
        if (dbif_txn_abort(dbd) < 0) {
            LOG( log_error, logtype_cnid, "Fatal error aborting transaction. Exiting!");
            return -1;
        }
    } else if (ret == 1) {
        ret = dbif_txn_commit(dbd);
        if (  ret < 0) {
            LOG( log_error, logtype_cnid, "Fatal error committing transaction. Exiting!");
            return -1;
        }
    } else {
        return -1;
    }

    return 0;
}

int dbif_txn_checkpoint(DBD *dbd, u_int32_t kbyte, u_int32_t min, u_int32_t flags)
{
    int ret;
    ret = dbd->db_env->txn_checkpoint(dbd->db_env, kbyte, min, flags);
    if (ret) {
        LOG(log_error, logtype_cnid, "error checkpointing transaction susystem: %s", db_strerror(ret));
        return -1;
    } else
        return 0;
}

int dbif_count(DBD *dbd, const int dbi, u_int32_t *count)
{
    int ret;
    DB_BTREE_STAT *sp;
    DB *db = dbd->db_table[dbi].db;

    ret = db->stat(db, NULL, &sp, 0);

    if (ret) {
        LOG(log_error, logtype_cnid, "error getting stat infotmation on database: %s", db_strerror(ret));
        return -1;
    }

    *count = sp->bt_ndata;
    free(sp);

    return 0;
}

int dbif_copy_rootinfokey(DBD *srcdbd, DBD *destdbd)
{
    DBT key, data;
    int rc;

    memset(&key, 0, sizeof(key));
    memset(&data, 0, sizeof(data));

    key.data = ROOTINFO_KEY;
    key.size = ROOTINFO_KEYLEN;

    if ((rc = dbif_get(srcdbd, DBIF_CNID, &key, &data, 0)) <= 0) {
        LOG(log_error, logtype_cnid, "dbif_copy_rootinfokey: Error getting rootinfo record");
        return -1;
    }

    memset(&key, 0, sizeof(key));
    key.data = ROOTINFO_KEY;
    key.size = ROOTINFO_KEYLEN;

    if ((rc = dbif_put(destdbd, DBIF_CNID, &key, &data, 0))) {
        LOG(log_error, logtype_cnid, "dbif_copy_rootinfokey: Error writing rootinfo key");
        return -1;
    }

    return 0;
}

int dbif_dump(DBD *dbd, int dumpindexes)
{
    int rc;
    uint32_t max = 0, count = 0, cnid, type, did, lastid, version;
    uint64_t dev, ino;
    time_t stamp;
    DBC *cur;
    DBT key = { 0 }, data = { 0 };
    DB *db = dbd->db_table[DBIF_CNID].db;
    char *typestring[2] = {"f", "d"};
    char timebuf[64];

    printf("CNID database dump:\n");

    rc = db->cursor(db, NULL, &cur, 0);
    if (rc) {
        LOG(log_error, logtype_cnid, "Couldn't create cursor: %s", db_strerror(rc));
        return -1;
    }

    cur->c_get(cur, &key, &data, DB_FIRST);
    while (rc == 0) {
        /* Parse and print data */
        memcpy(&cnid, key.data, 4);
        cnid = ntohl(cnid);
        if (cnid > max)
            max = cnid;

        /* Rootinfo node ? */
        if (cnid == 0) {
            memcpy(&stamp, (char *)data.data + CNID_DEV_OFS, sizeof(time_t));
            memcpy(&lastid, (char *)data.data + CNID_TYPE_OFS, CNID_TYPE_LEN);
            lastid = ntohl(lastid);
            memcpy(&version, (char *)data.data + CNID_DID_OFS, CNID_DID_LEN);
            version = ntohl(version);

            strftime(timebuf, sizeof(timebuf), "%b %d %Y %H:%M:%S", localtime(&stamp));
            printf("CNID db version: %u, dbd stamp: 0x%08x (%s), next free CNID: %u\n",
                   version, (unsigned int)stamp, timebuf, lastid + 1);
        } else {
            /* dev */
            memcpy(&dev, (char *)data.data + CNID_DEV_OFS, 8);
            dev = ntoh64(dev);
            /* ino */
            memcpy(&ino, (char *)data.data + CNID_INO_OFS, 8);
            ino = ntoh64(ino);
            /* type */
            memcpy(&type, (char *)data.data + CNID_TYPE_OFS, 4);
            type = ntohl(type);
            /* did */
            memcpy(&did, (char *)data.data + CNID_DID_OFS, 4);
            did = ntohl(did);

            count++;
            printf("id: %10u, did: %10u, type: %s, dev: 0x%llx, ino: 0x%016llx, name: %s\n", 
                   cnid, did, typestring[type],
                   (long long unsigned int)dev, (long long unsigned int)ino, 
                   (char *)data.data + CNID_NAME_OFS);

        }

        rc = cur->c_get(cur, &key, &data, DB_NEXT);
    }

    if (rc != DB_NOTFOUND) {
        LOG(log_error, logtype_cnid, "Error iterating over btree: %s", db_strerror(rc));
        return -1;
    }

    rc = cur->c_close(cur);
    if (rc) {
        LOG(log_error, logtype_cnid, "Couldn't close cursor: %s", db_strerror(rc));
        return -1;
    }
    printf("%u CNIDs in database. Max CNID: %u.\n", count, max);

    /* Dump indexes too ? */
    if (dumpindexes) {
        /* DBIF_IDX_DEVINO */
        printf("\ndev/inode index:\n");
        count = 0;
        db = dbd->db_table[DBIF_IDX_DEVINO].db;
        rc = db->cursor(db, NULL, &cur, 0);
        if (rc) {
            LOG(log_error, logtype_cnid, "Couldn't create cursor: %s", db_strerror(rc));
            return -1;
        }
        
        cur->c_get(cur, &key, &data, DB_FIRST);
        while (rc == 0) {
            /* Parse and print data */

            /* cnid */
            memcpy(&cnid, data.data, CNID_LEN);
            cnid = ntohl(cnid);
            if (cnid == 0) {
                /* Rootinfo node */
            } else {
                /* dev */
                memcpy(&dev, key.data, CNID_DEV_LEN);
                dev = ntoh64(dev);
                /* ino */
                memcpy(&ino, (char *)key.data + CNID_DEV_LEN, CNID_INO_LEN);
                ino = ntoh64(ino);
                
                printf("id: %10u <== dev: 0x%llx, ino: 0x%016llx\n", 
                       cnid, (unsigned long long int)dev, (unsigned long long int)ino);
                count++;
            }
            rc = cur->c_get(cur, &key, &data, DB_NEXT);
        }
        if (rc != DB_NOTFOUND) {
            LOG(log_error, logtype_cnid, "Error iterating over btree: %s", db_strerror(rc));
            return -1;
        }
        
        rc = cur->c_close(cur);
        if (rc) {
            LOG(log_error, logtype_cnid, "Couldn't close cursor: %s", db_strerror(rc));
            return -1;
        }
        printf("%u items\n", count);

        /* Now dump DBIF_IDX_DIDNAME */
        printf("\ndid/name index:\n");
        count = 0;
        db = dbd->db_table[DBIF_IDX_DIDNAME].db;
        rc = db->cursor(db, NULL, &cur, 0);
        if (rc) {
            LOG(log_error, logtype_cnid, "Couldn't create cursor: %s", db_strerror(rc));
            return -1;
        }
        
        cur->c_get(cur, &key, &data, DB_FIRST);
        while (rc == 0) {
            /* Parse and print data */

            /* cnid */
            memcpy(&cnid, data.data, CNID_LEN);
            cnid = ntohl(cnid);
            if (cnid == 0) {
                /* Rootinfo node */
            } else {
                /* did */
                memcpy(&did, key.data, CNID_LEN);
                did = ntohl(did);

                printf("id: %10u <== did: %10u, name: %s\n", cnid, did, (char *)key.data + CNID_LEN);
                count++;
            }
            rc = cur->c_get(cur, &key, &data, DB_NEXT);
        }
        if (rc != DB_NOTFOUND) {
            LOG(log_error, logtype_cnid, "Error iterating over btree: %s", db_strerror(rc));
            return -1;
        }
        
        rc = cur->c_close(cur);
        if (rc) {
            LOG(log_error, logtype_cnid, "Couldn't close cursor: %s", db_strerror(rc));
            return -1;
        }
        printf("%u items\n", count);
    }

    return 0;
}

/* 
   Iterates over dbd, returning cnids.
   Uses in-value of cnid to seek to that cnid, then gets next and return that in cnid.
   If close=1, close cursor.
   Return -1 on error, 0 on EOD (end-of-database), 1 if returning cnid.
*/
int dbif_idwalk(DBD *dbd, cnid_t *cnid, int close)
{
    int rc;
    int flag;
    cnid_t id;

    static DBT key = { 0 }, data = { 0 };
    DB *db = dbd->db_table[DBIF_CNID].db;

    if (close) {
        if (dbd->db_cur) {
            dbd->db_cur->close(dbd->db_cur);
            dbd->db_cur = NULL;
        }
        return 0;
    }

    /* An dbif_del will have closed our cursor too */
    if ( ! dbd->db_cur ) {
        if ((rc = db->cursor(db, NULL, &dbd->db_cur, 0)) != 0) {
            LOG(log_error, logtype_cnid, "Couldn't create cursor: %s", db_strerror(rc));
            return -1;
        }
        flag = DB_SET_RANGE;    /* This will seek to next cnid after the one just deleted */
        id = htonl(*cnid);
        key.data = &id;
        key.size = sizeof(cnid_t);
    } else
        flag = DB_NEXT;

    if ((rc = dbd->db_cur->get(dbd->db_cur, &key, &data, flag)) == 0) {
        memcpy(cnid, key.data, sizeof(cnid_t));
        *cnid = ntohl(*cnid);
        return 1;
    }

    if (rc != DB_NOTFOUND) {
        LOG(log_error, logtype_cnid, "Error iterating over btree: %s", db_strerror(rc));
        dbd->db_cur->close(dbd->db_cur);
        dbd->db_cur = NULL;
        return -1;
    }

    if (dbd->db_cur) {
        dbd->db_cur->close(dbd->db_cur);
        dbd->db_cur = NULL;
    }    

    return 0;
}
