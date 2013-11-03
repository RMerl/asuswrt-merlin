/* 
 * Copyright (c) 2003 the Netatalk Team
 * Copyright (c) 2003 Rafal Lewczuk <rlewczuk@pronet.pl>
 * 
 * This program is free software; you can redistribute and/or modify
 * it under the terms of the GNU General Public License as published
 * by the Free Software Foundation version 2 of the License or later
 * version if explicitly stated by any of above copyright holders.
 *
 */
#define USE_LIST

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif /* HAVE_CONFIG_H */

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/param.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <signal.h>
#include <unistd.h>
#include <errno.h>

#include <atalk/cnid.h>
#include <atalk/list.h>
#include <atalk/logger.h>
#include <atalk/util.h>
#include <atalk/compat.h>

/* List of all registered modules. */
static struct list_head modules = ATALK_LIST_HEAD_INIT(modules);

static sigset_t sigblockset;
static const struct itimerval none = {{0, 0}, {0, 0}};

/* Registers new CNID backend module. */

/* Once module has been registered, it cannot be unregistered. */
void cnid_register(struct _cnid_module *module)
{
    struct list_head *ptr;

    /* Check if our module is already registered. */
    list_for_each(ptr, &modules)
        if (0 == strcmp(list_entry(ptr, cnid_module, db_list)->name, module->name)) {
        LOG(log_error, logtype_afpd, "Module with name [%s] is already registered !", module->name);
        return;
    }

    LOG(log_info, logtype_afpd, "Registering CNID module [%s]", module->name);
    ptr = &(module->db_list);
    list_add_tail(ptr, &modules);
}

/* --------------- */
static int cnid_dir(const char *dir, mode_t mask)
{
   struct stat st, st1; 
   char tmp[MAXPATHLEN];

   if (stat(dir, &st) < 0) {
       if (errno != ENOENT) 
           return -1;
       if (ad_stat( dir, &st) < 0)
          return -1;

       LOG(log_info, logtype_cnid, "Setting uid/gid to %d/%d", st.st_uid, st.st_gid);
       if (setegid(st.st_gid) < 0 || seteuid(st.st_uid) < 0) {
           LOG(log_error, logtype_cnid, "uid/gid: %s", strerror(errno));
           return -1;
       }

       if (mkdir(dir, 0777 & ~mask) < 0)
           return -1;
   } else {
       strlcpy(tmp, dir, sizeof(tmp));
       strlcat(tmp, "/.AppleDB", sizeof(tmp));
       if (stat(tmp, &st1) < 0) /* use .AppleDB owner, if folder already exists */
           st1 = st; 
       LOG(log_info, logtype_cnid, "Setting uid/gid to %d/%d", st1.st_uid, st1.st_gid);
       if (setegid(st1.st_gid) < 0 || seteuid(st1.st_uid) < 0) {
           LOG(log_error, logtype_cnid, "uid/gid: %s", strerror(errno));
           return -1;
       }
   }
   return 0;
}

/* Opens CNID database using particular back-end */
struct _cnid_db *cnid_open(const char *volpath, mode_t mask, char *type, int flags,
                           const char *cnidsrv, const char *cnidport)
{
    struct _cnid_db *db;
    cnid_module *mod = NULL;
    struct list_head *ptr;
    uid_t uid = -1;  
    gid_t gid = -1;

    list_for_each(ptr, &modules) {
        if (0 == strcmp(list_entry(ptr, cnid_module, db_list)->name, type)) {
	    mod = list_entry(ptr, cnid_module, db_list);
        break;
        }
    }

    if (NULL == mod) {
        LOG(log_error, logtype_afpd, "Cannot find module named [%s] in registered module list!", type);
        return NULL;
    }

    if ((mod->flags & CNID_FLAG_SETUID) && !(flags & CNID_FLAG_MEMORY)) {
        uid = geteuid();
        gid = getegid();
        if (seteuid(0)) {
            LOG(log_error, logtype_afpd, "seteuid failed %s", strerror(errno));
            return NULL;
        }
        if (cnid_dir(volpath, mask) < 0) {
            if ( setegid(gid) < 0 || seteuid(uid) < 0) {
                LOG(log_error, logtype_afpd, "can't seteuid back %s", strerror(errno));
                exit(EXITERR_SYS);
            }
            return NULL;
        }
    }

    struct cnid_open_args args = {volpath, mask, flags, cnidsrv, cnidport};
    db = mod->cnid_open(&args);

    if ((mod->flags & CNID_FLAG_SETUID) && !(flags & CNID_FLAG_MEMORY)) {
        seteuid(0);
        if ( setegid(gid) < 0 || seteuid(uid) < 0) {
            LOG(log_error, logtype_afpd, "can't seteuid back %s", strerror(errno));
            exit(EXITERR_SYS);
        }
    }

    if (NULL == db) {
        LOG(log_error, logtype_afpd, "Cannot open CNID db at [%s].", volpath);
        return NULL;
    }
    /* FIXME should module know about it ? */
    if ((flags & CNID_FLAG_NODEV)) {
        db->flags |= CNID_FLAG_NODEV;
    }
    db->flags |= mod->flags;

    if ((db->flags & CNID_FLAG_BLOCK)) {
        sigemptyset(&sigblockset);
        sigaddset(&sigblockset, SIGTERM);
        sigaddset(&sigblockset, SIGHUP);
        sigaddset(&sigblockset, SIGUSR1);
        sigaddset(&sigblockset, SIGUSR2);
        sigaddset(&sigblockset, SIGALRM);
    }

    return db;
}

/* ------------------- */
static void block_signal(uint32_t flags)
{
    if ((flags & CNID_FLAG_BLOCK)) {
        pthread_sigmask(SIG_BLOCK, &sigblockset, NULL);
    }
}

/* ------------------- */
static void unblock_signal(uint32_t flags)
{
    if ((flags & CNID_FLAG_BLOCK)) {
        pthread_sigmask(SIG_UNBLOCK, &sigblockset, NULL);
    }
}

/* ------------------- 
  protect against bogus value from the DB.
  adddir really doesn't like 2
*/
static cnid_t valide(cnid_t id)
{
  if (id == CNID_INVALID)
      return id;
      
  if (id < CNID_START) {
    static int err = 0;
    if (!err) {
        err = 1;
        LOG(log_error, logtype_afpd, "Error: Invalid cnid, corrupted DB?");
    }
    return CNID_INVALID;
  }
  return id;
}

/* Closes CNID database. Currently it's just a wrapper around db->cnid_close(). */
void cnid_close(struct _cnid_db *db)
{
    uint32_t flags;

    if (NULL == db) {
        LOG(log_error, logtype_afpd, "Error: cnid_close called with NULL argument !");
        return;
    }
    /* cnid_close free db */
    flags = db->flags;
    block_signal(flags);
    db->cnid_close(db);
    unblock_signal(flags);
}

/* --------------- */
cnid_t cnid_add(struct _cnid_db *cdb, const struct stat *st, const cnid_t did, 
                const char *name, const size_t len, cnid_t hint)
{
    cnid_t ret;

    if (len == 0)
        return CNID_INVALID;

    block_signal(cdb->flags);
    ret = valide(cdb->cnid_add(cdb, st, did, name, len, hint));
    unblock_signal(cdb->flags);
    return ret;
}

/* --------------- */
int cnid_delete(struct _cnid_db *cdb, cnid_t id)
{
int ret;

    block_signal(cdb->flags);
    ret = cdb->cnid_delete(cdb, id);
    unblock_signal(cdb->flags);
    return ret;
}


/* --------------- */
cnid_t cnid_get(struct _cnid_db *cdb, const cnid_t did, char *name,const size_t len)
{
cnid_t ret;

    block_signal(cdb->flags);
    ret = valide(cdb->cnid_get(cdb, did, name, len));
    unblock_signal(cdb->flags);
    return ret;
}

/* --------------- */
int cnid_getstamp(struct _cnid_db *cdb,  void *buffer, const size_t len)
{
cnid_t ret;
time_t t;

    if (!cdb->cnid_getstamp) {
        memset(buffer, 0, len);
    	/* return the current time. it will invalide cache */
    	if (len < sizeof(time_t))
    	    return -1;
    	t = time(NULL);
    	memcpy(buffer, &t, sizeof(time_t));
        return 0;
    }
    block_signal(cdb->flags);
    ret = cdb->cnid_getstamp(cdb, buffer, len);
    unblock_signal(cdb->flags);
    return ret;
}

/* --------------- */
cnid_t cnid_lookup(struct _cnid_db *cdb, const struct stat *st, const cnid_t did,
                   char *name, const size_t len)
{
    cnid_t ret;

    block_signal(cdb->flags);
    ret = valide(cdb->cnid_lookup(cdb, st, did, name, len));
    unblock_signal(cdb->flags);
    return ret;
}

/* --------------- */
int cnid_find(struct _cnid_db *cdb, const char *name, size_t namelen, void *buffer, size_t buflen)
{
    int ret;
    
    if (cdb->cnid_find == NULL) {
        LOG(log_error, logtype_cnid, "cnid_find not supported by CNID backend");        
        return -1;
    }

    block_signal(cdb->flags);
    ret = cdb->cnid_find(cdb, name, namelen, buffer, buflen);
    unblock_signal(cdb->flags);
    return ret;
}

/* --------------- */
char *cnid_resolve(struct _cnid_db *cdb, cnid_t *id, void *buffer, size_t len)
{
char *ret;

    block_signal(cdb->flags);
    ret = cdb->cnid_resolve(cdb, id, buffer, len);
    unblock_signal(cdb->flags);
    if (ret && !strcmp(ret, "..")) {
        LOG(log_error, logtype_afpd, "cnid_resolve: name is '..', corrupted db? ");
        ret = NULL;
    }
    return ret;
}

/* --------------- */
int cnid_update   (struct _cnid_db *cdb, const cnid_t id, const struct stat *st, 
			const cnid_t did, char *name, const size_t len)
{
int ret;

    block_signal(cdb->flags);
    ret = cdb->cnid_update(cdb, id, st, did, name, len);
    unblock_signal(cdb->flags);
    return ret;
}
			
/* --------------- */
cnid_t cnid_rebuild_add(struct _cnid_db *cdb, const struct stat *st, const cnid_t did,
                       char *name, const size_t len, cnid_t hint)
{
cnid_t ret;

    block_signal(cdb->flags);
    ret = cdb->cnid_rebuild_add(cdb, st, did, name, len, hint);
    unblock_signal(cdb->flags);
    return ret;
}

/* --------------- */
int cnid_wipe(struct _cnid_db *cdb)
{
    int ret = 0;

    block_signal(cdb->flags);
    if (cdb->cnid_wipe)
        ret = cdb->cnid_wipe(cdb);
    unblock_signal(cdb->flags);
    return ret;
}
