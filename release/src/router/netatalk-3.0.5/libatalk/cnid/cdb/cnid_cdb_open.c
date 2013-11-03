/*
 * Copyright (c) 1999. Adrian Sun (asun@zoology.washington.edu)
 * All Rights Reserved. See COPYRIGHT.
 *
 * CNID database support. 
 *
 * here's the deal:
 *  1) afpd already caches did's. 
 *  2) the database stores cnid's as both did/name and dev/ino pairs. 
 *  3) RootInfo holds the value of the NextID.
 *  4) the cnid database gets called in the following manner --
 *     start a database:
 *     cnid = cnid_open(root_dir);
 *
 *     allocate a new id: 
 *     newid = cnid_add(cnid, dev, ino, parent did,
 *     name, id); id is a hint for a specific id. pass 0 if you don't
 *     care. if the id is already assigned, you won't get what you
 *     requested.
 *
 *     given an id, get a did/name and dev/ino pair.
 *     name = cnid_get(cnid, &id); given an id, return the corresponding
 *     info.
 *     return code = cnid_delete(cnid, id); delete an entry. 
 *
 * with AFP, CNIDs 0-2 have special meanings. here they are:
 * 0 -- invalid cnid
 * 1 -- parent of root directory (handled by afpd) 
 * 2 -- root directory (handled by afpd)
 *
 * CNIDs 4-16 are reserved according to page 31 of the AFP 3.0 spec so, 
 * CNID_START begins at 17.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif /* HAVE_CONFIG_H */

#ifdef CNID_BACKEND_CDB

#include <atalk/cnid_private.h>
#include "cnid_cdb_private.h"

#ifndef MIN
#define MIN(a, b)  ((a) < (b) ? (a) : (b))
#endif /* ! MIN */

#define DBHOME        ".AppleDB"
#define DBCNID        "cnid2.db"
#define DBDEVINO      "devino.db"
#define DBDIDNAME     "didname.db"      /* did/full name mapping */
#define DBLOCKFILE    "cnid.lock"

#define DBHOMELEN    8
#define DBLEN        10

#define DBOPTIONS    (DB_CREATE | DB_INIT_CDB | DB_INIT_MPOOL)

#define MAXITER     0xFFFF      /* maximum number of simultaneously open CNID
                                 * databases. */

static char *old_dbfiles[] = {"cnid.db", NULL};

/* --------------- */
static int didname(DB *dbp _U_, const DBT *pkey _U_, const DBT *pdata, DBT *skey)
{
int len;
 
    memset(skey, 0, sizeof(DBT));
    skey->data = (char *)pdata->data + CNID_DID_OFS;
    len = strlen((char *)skey->data + CNID_DID_LEN);
    skey->size = CNID_DID_LEN + len + 1;
    return (0);
}
 
/* --------------- */
static int devino(DB *dbp _U_, const DBT *pkey _U_, const DBT *pdata, DBT *skey)
{
    memset(skey, 0, sizeof(DBT));
    skey->data = (char *)pdata->data + CNID_DEVINO_OFS;
    skey->size = CNID_DEVINO_LEN;
    return (0);
}
 
/* --------------- */
static int  my_associate (DB *p, DB *s,
                   int (*callback)(DB *, const DBT *,const DBT *, DBT *),
                   u_int32_t flags)
{
#if DB_VERSION_MAJOR > 4 || (DB_VERSION_MAJOR == 4 && DB_VERSION_MINOR >= 1)
    return p->associate(p, NULL, s, callback, flags);
#else
#if (DB_VERSION_MAJOR == 4 && DB_VERSION_MINOR == 0)
    return p->associate(p,       s, callback, flags);
#else
    return 0; /* FIXME */
#endif
#endif
}

/* --------------- */
static int my_open(DB * p, const char *f, const char *d, DBTYPE t, u_int32_t flags, int mode)
{
#if DB_VERSION_MAJOR > 4 || (DB_VERSION_MAJOR == 4 && DB_VERSION_MINOR >= 1)
    return p->open(p, NULL, f, d, t, flags, mode);
#else
    return p->open(p, f, d, t, flags, mode);
#endif
}

/* --------------- */
static struct _cnid_db *cnid_cdb_new(const char *volpath)
{
    struct _cnid_db *cdb;
    int major, minor, patch;
    char *version_str;

    version_str = db_version(&major, &minor, &patch);

    /* check library match, ignore if only patch level changed */
    if ( major != DB_VERSION_MAJOR || minor != DB_VERSION_MINOR)
    {
        LOG(log_error, logtype_cnid, "cnid_cdb_new: the Berkeley DB library version used does not match the version compiled with: (%u.%u)/(%u.%u)", DB_VERSION_MAJOR, DB_VERSION_MINOR, major, minor); 
	return NULL;
    }

    if ((cdb = (struct _cnid_db *) calloc(1, sizeof(struct _cnid_db))) == NULL)
        return NULL;

    if ((cdb->volpath = strdup(volpath)) == NULL) {
        free(cdb);
        return NULL;
    }

    cdb->flags = CNID_FLAG_PERSISTENT;

    cdb->cnid_add = cnid_cdb_add;
    cdb->cnid_delete = cnid_cdb_delete;
    cdb->cnid_get = cnid_cdb_get;
    cdb->cnid_lookup = cnid_cdb_lookup;
    cdb->cnid_nextid = NULL;    /*cnid_cdb_nextid;*/
    cdb->cnid_resolve = cnid_cdb_resolve;
    cdb->cnid_update = cnid_cdb_update;
    cdb->cnid_close = cnid_cdb_close;
    cdb->cnid_getstamp = cnid_cdb_getstamp;
    cdb->cnid_rebuild_add = cnid_cdb_rebuild_add;
    cdb->cnid_wipe = NULL;
    return cdb;
}

/* --------------- */
static int upgrade_required(char *dbdir)
{
    char path[MAXPATHLEN + 1];
    int len, i;
    int found = 0;
    struct stat st;
    
    strcpy(path, dbdir);

    len = strlen(path);
    if (path[len - 1] != '/') {
        strcat(path, "/");
        len++;
    }
    
    for (i = 0; old_dbfiles[i] != NULL; i++) {
	strcpy(path + len, old_dbfiles[i]);
	if ( !(stat(path, &st) < 0) ) {
	    found++;
	    continue;
	}
	if (errno != ENOENT) {
	    LOG(log_error, logtype_default, "cnid_open: Checking %s gave %s", path, strerror(errno));
	    found++;
	}
    }
    return found;
}

/* --------------- */
struct _cnid_db *cnid_cdb_open(struct cnid_open_args *args)
{
    struct stat st;
    char path[MAXPATHLEN + 1];
    CNID_private *db;
    struct _cnid_db *cdb;
    int open_flag, len;
    static int first = 0;
    int rc;

    if (!args->dir || *args->dir == 0) {
        return NULL;
    }

    /* this checks .AppleDB.
       We need space for dir + '/' + DBHOMELEN + '/' + DBLEN */
    if ((len = strlen(args->dir)) > (MAXPATHLEN - DBHOMELEN - DBLEN - 2)) {
        LOG(log_error, logtype_default, "cnid_open: Pathname too large: %s", args->dir);
        return NULL;
    }

    if ((cdb = cnid_cdb_new(args->dir)) == NULL) {
        LOG(log_error, logtype_default, "cnid_open: Unable to allocate memory for database");
        return NULL;
    }

    if ((db = (CNID_private *) calloc(1, sizeof(CNID_private))) == NULL) {
        LOG(log_error, logtype_default, "cnid_open: Unable to allocate memory for database");
        goto fail_cdb;
    }

    cdb->_private = (void *) db;
    db->magic = CNID_DB_MAGIC;

    strcpy(path, args->dir);
    if (path[len - 1] != '/') {
        strcat(path, "/");
        len++;
    }

    strcpy(path + len, DBHOME);
    if ((stat(path, &st) < 0) && (ad_mkdir(path, 0777 & ~args->mask) < 0)) {
        LOG(log_error, logtype_default, "cnid_open: DBHOME mkdir failed for %s", path);
        goto fail_adouble;
    }

    if (upgrade_required(path)) {
	LOG(log_error, logtype_default, "cnid_open: Found version 1 of the CNID database. Please upgrade to version 2");
	goto fail_adouble;
    }

    /* Print out the version of BDB we're linked against. */
    if (!first) {
        first = 1;
        LOG(log_info, logtype_default, "CNID DB initialized using %s", db_version(NULL, NULL, NULL));
    }

    open_flag = DB_CREATE;

    /* We need to be able to open the database environment with full
     * transaction, logging, and locking support if we ever hope to 
     * be a true multi-acess file server. */
    if ((rc = db_env_create(&db->dbenv, 0)) != 0) {
        LOG(log_error, logtype_default, "cnid_open: db_env_create: %s", db_strerror(rc));
        goto fail_lock;
    }

    /* Open the database environment. */
    if ((rc = db->dbenv->open(db->dbenv, path, DBOPTIONS, 0666 & ~args->mask)) != 0) {
	LOG(log_error, logtype_default, "cnid_open: dbenv->open (rw) of %s failed: %s", path, db_strerror(rc));
	/* FIXME: This should probably go. Even if it worked, any use for a read-only DB? Didier? */
        if (rc == DB_RUNRECOVERY) {
            /* This is the mother of all errors.  We _must_ fail here. */
            LOG(log_error, logtype_default,
                "cnid_open: CATASTROPHIC ERROR opening database environment %s.  Run db_recovery -c immediately", path);
            goto fail_lock;
        }

        /* We can't get a full transactional environment, so multi-access
         * is out of the question.  Let's assume a read-only environment,
         * and try to at least get a shared memory pool. */
        if ((rc = db->dbenv->open(db->dbenv, path, DB_INIT_MPOOL, 0666 & ~args->mask)) != 0) {
            /* Nope, not a MPOOL, either.  Last-ditch effort: we'll try to
             * open the environment with no flags. */
            if ((rc = db->dbenv->open(db->dbenv, path, 0, 0666 & ~args->mask)) != 0) {
                LOG(log_error, logtype_default, "cnid_open: dbenv->open of %s failed: %s", path, db_strerror(rc));
                goto fail_lock;
            }
        }
        db->flags |= CNIDFLAG_DB_RO;
        open_flag = DB_RDONLY;
        LOG(log_info, logtype_default, "cnid_open: Obtained read-only database environment %s", path);
    }

    /* ---------------------- */
    /* Main CNID database.  Use a hash for this one. */

    if ((rc = db_create(&db->db_cnid, db->dbenv, 0)) != 0) {
        LOG(log_error, logtype_default, "cnid_open: Failed to create cnid database: %s",
            db_strerror(rc));
        goto fail_appinit;
    }

    if ((rc = my_open(db->db_cnid, DBCNID, DBCNID, DB_BTREE, open_flag, 0666 & ~args->mask)) != 0) {
        LOG(log_error, logtype_default, "cnid_open: Failed to open dev/ino database: %s",
            db_strerror(rc));
        goto fail_appinit;
    }

    /* ---------------------- */
    /* did/name reverse mapping.  We use a BTree for this one. */

    if ((rc = db_create(&db->db_didname, db->dbenv, 0)) != 0) {
        LOG(log_error, logtype_default, "cnid_open: Failed to create did/name database: %s",
            db_strerror(rc));
        goto fail_appinit;
    }

    if ((rc = my_open(db->db_didname, DBCNID, DBDIDNAME, DB_BTREE, open_flag, 0666 & ~args->mask))) {
        LOG(log_error, logtype_default, "cnid_open: Failed to open did/name database: %s",
            db_strerror(rc));
        goto fail_appinit;
    }

    /* ---------------------- */
    /* dev/ino reverse mapping.  Use a hash for this one. */

    if ((rc = db_create(&db->db_devino, db->dbenv, 0)) != 0) {
        LOG(log_error, logtype_default, "cnid_open: Failed to create dev/ino database: %s",
            db_strerror(rc));
        goto fail_appinit;
    }

    if ((rc = my_open(db->db_devino, DBCNID, DBDEVINO, DB_BTREE, open_flag, 0666 & ~args->mask)) != 0) {
        LOG(log_error, logtype_default, "cnid_open: Failed to open devino database: %s",
            db_strerror(rc));
        goto fail_appinit;
    }

    /* ---------------------- */
    /* Associate the secondary with the primary. */ 
    if ((rc = my_associate(db->db_cnid, db->db_didname, didname, 0)) != 0) {
        LOG(log_error, logtype_default, "cnid_open: Failed to associate didname database: %s",
            db_strerror(rc));
        goto fail_appinit;
    }
 
    if ((rc = my_associate(db->db_cnid, db->db_devino, devino, 0)) != 0) {
        LOG(log_error, logtype_default, "cnid_open: Failed to associate devino database: %s",
            db_strerror(rc));
        goto fail_appinit;
    }
 
    /* ---------------------- */
    /* Check for version. "cdb" only supports CNID_VERSION_0, cf cnid_private.h */

    DBT key, data;
    uint32_t version;

    memset(&key, 0, sizeof(key));
    memset(&data, 0, sizeof(data));
    key.data = ROOTINFO_KEY;
    key.size = ROOTINFO_KEYLEN;

    if ((rc = db->db_cnid->get(db->db_cnid, NULL, &key, &data, 0)) == 0) {
        /* If not found, ignore it */
        memcpy(&version, data.data + CNID_DID_OFS, sizeof(version));
        version = ntohl(version);
        LOG(log_debug, logtype_default, "CNID db version %u", version);
        if (version != CNID_VERSION_0) {
            LOG(log_error, logtype_default, "Unsupported CNID db version %u, use CNID backend \"dbd\"", version);
            goto fail_appinit;
        }
    }

    return cdb;

  fail_appinit:
    if (db->db_didname)
        db->db_didname->close(db->db_didname, 0);
    if (db->db_devino)
        db->db_devino->close(db->db_devino, 0);
    if (db->db_cnid)
        db->db_cnid->close(db->db_cnid, 0);
    LOG(log_error, logtype_default, "cnid_open: Failed to setup CNID DB environment");
    db->dbenv->close(db->dbenv, 0);

  fail_lock:

  fail_adouble:

    free(db);

  fail_cdb:
    if (cdb->volpath != NULL)
        free(cdb->volpath);
    free(cdb);

    return NULL;
}

struct _cnid_module cnid_cdb_module = {
    "cdb",
    {NULL, NULL},
    cnid_cdb_open,
    CNID_FLAG_SETUID | CNID_FLAG_BLOCK
};


#endif /* CNID_BACKEND_CDB */
