/*
 * Copyright (C) Joerg Lenneis 2003
 * Copyright (c) Frank Lahm 2009
 * All Rights Reserved.  See COPYING.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif /* HAVE_CONFIG_H */

#include <unistd.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <signal.h>
#include <string.h>
#include <sys/types.h>
#include <sys/param.h>
#include <sys/stat.h>
#include <time.h>
#include <sys/file.h>
#include <arpa/inet.h>

#include <atalk/cnid_dbd_private.h>
#include <atalk/logger.h>
#include <atalk/errchk.h>
#include <atalk/bstrlib.h>
#include <atalk/bstradd.h>
#include <atalk/netatalk_conf.h>
#include <atalk/util.h>

#include "db_param.h"
#include "dbif.h"
#include "dbd.h"
#include "comm.h"
#include "pack.h"

/* 
   Note: DB_INIT_LOCK is here so we can run the db_* utilities while netatalk is running.
   It's a likey performance hit, but it might we worth it.
 */
#define DBOPTIONS (DB_CREATE | DB_INIT_LOG | DB_INIT_MPOOL | DB_INIT_LOCK | DB_INIT_TXN)

static DBD *dbd;
static int exit_sig = 0;
static int db_locked;
static bstring dbpath;
static struct db_param *dbp;
static struct vol *vol;

static void sig_exit(int signo)
{
    exit_sig = signo;
    return;
}

static void block_sigs_onoff(int block)
{
    sigset_t set;

    sigemptyset(&set);
    sigaddset(&set, SIGINT);
    sigaddset(&set, SIGTERM);
    if (block)
        sigprocmask(SIG_BLOCK, &set, NULL);
    else
        sigprocmask(SIG_UNBLOCK, &set, NULL);
    return;
}

/*
  The dbd_XXX and comm_XXX functions all obey the same protocol for return values:

  1: Success, if transactions are used commit.
  0: Failure, but we continue to serve requests. If transactions are used abort/rollback.
  -1: Fatal error, either from t
  he database or from the socket. Abort the transaction if applicable
  (which might fail as well) and then exit.

  We always try to notify the client process about the outcome, the result field
  of the cnid_dbd_rply structure contains further details.

*/

/*!
 * Get lock on db lock file
 *
 * @args cmd       (r) lock command:
 *                     LOCK_FREE:   close lockfd
 *                     LOCK_UNLOCK: unlock lockm keep lockfd open
 *                     LOCK_EXCL:   F_WRLCK on lockfd
 *                     LOCK_SHRD:   F_RDLCK on lockfd
 * @args dbpath    (r) path to lockfile, only used on first call,
 *                     later the stored fd is used
 * @returns            LOCK_FREE/LOCK_UNLOCK return 0 on success, -1 on error
 *                     LOCK_EXCL/LOCK_SHRD return LOCK_EXCL or LOCK_SHRD respectively on
 *                     success, 0 if the lock couldn't be acquired, -1 on other errors
 */
static int get_lock(int cmd, const char *dbpath)
{
    static int lockfd = -1;
    int ret;
    char lockpath[PATH_MAX];
    struct stat st;

    LOG(log_debug, logtype_cnid, "get_lock(%s, \"%s\")",
        cmd == LOCK_EXCL ? "LOCK_EXCL" :
        cmd == LOCK_SHRD ? "LOCK_SHRD" :
        cmd == LOCK_FREE ? "LOCK_FREE" :
        cmd == LOCK_UNLOCK ? "LOCK_UNLOCK" : "UNKNOWN",
        dbpath ? dbpath : "");

    switch (cmd) {
    case LOCK_FREE:
        if (lockfd == -1)
            return -1;
        close(lockfd);
        lockfd = -1;
        return 0;

    case LOCK_UNLOCK:
        if (lockfd == -1)
            return -1;
        return unlock(lockfd, 0, SEEK_SET, 0);

    case LOCK_EXCL:
    case LOCK_SHRD:
        if (lockfd == -1) {
            if ( (strlen(dbpath) + strlen(LOCKFILENAME+1)) > (PATH_MAX - 1) ) {
                LOG(log_error, logtype_cnid, ".AppleDB pathname too long");
                return -1;
            }
            strncpy(lockpath, dbpath, PATH_MAX - 1);
            strcat(lockpath, "/");
            strcat(lockpath, LOCKFILENAME);

            if ((lockfd = open(lockpath, O_RDWR | O_CREAT, 0644)) < 0) {
                LOG(log_error, logtype_cnid, "Error opening lockfile: %s", strerror(errno));
                return -1;
            }

            if ((stat(dbpath, &st)) != 0) {
                LOG(log_error, logtype_cnid, "Error statting lockfile: %s", strerror(errno));
                return -1;
            }

            if ((chown(lockpath, st.st_uid, st.st_gid)) != 0) {
                LOG(log_error, logtype_cnid, "Error inheriting lockfile permissions: %s",
                         strerror(errno));
                return -1;
            }
        }
    
        if (cmd == LOCK_EXCL)
            ret = write_lock(lockfd, 0, SEEK_SET, 0);
        else
            ret = read_lock(lockfd, 0, SEEK_SET, 0);

        if (ret != 0) {
            if (cmd == LOCK_SHRD)
                LOG(log_error, logtype_cnid, "Volume CNID db is locked, try again...");
            return 0; 
        }

        LOG(log_debug, logtype_cnid, "get_lock: got %s lock",
            cmd == LOCK_EXCL ? "LOCK_EXCL" : "LOCK_SHRD");    
        return cmd;

    default:
        return -1;
    } /* switch(cmd) */

    /* deadc0de, never get here */
    return -1;
}

static int open_db(void)
{
    EC_INIT;

    /* Get db lock */
    if ((db_locked = get_lock(LOCK_EXCL, bdata(dbpath))) != LOCK_EXCL) {
        LOG(log_error, logtype_cnid, "main: fatal db lock error");
        EC_FAIL;
    }

    if (NULL == (dbd = dbif_init(bdata(dbpath), "cnid2.db")))
        EC_FAIL;

    /* Only recover if we got the lock */
    if (dbif_env_open(dbd, dbp, DBOPTIONS | DB_RECOVER) < 0)
        EC_FAIL;

    LOG(log_debug, logtype_cnid, "Finished initializing BerkeleyDB environment");

    if (dbif_open(dbd, dbp, 0) < 0)
        EC_FAIL;

    LOG(log_debug, logtype_cnid, "Finished opening BerkeleyDB databases");

EC_CLEANUP:
    if (ret != 0) {
        if (dbd) {
            (void)dbif_close(dbd);
            dbd = NULL;
        }
    }

    EC_EXIT;
}

static int delete_db(void)
{
    EC_INIT;
    int cwd = -1;

    EC_ZERO( get_lock(LOCK_FREE, bdata(dbpath)) );
    EC_NEG1( cwd = open(".", O_RDONLY) );
    chdir(cfrombstr(dbpath));
    system("rm -f cnid2.db lock log.* __db.*");

    if ((db_locked = get_lock(LOCK_EXCL, bdata(dbpath))) != LOCK_EXCL) {
        LOG(log_error, logtype_cnid, "main: fatal db lock error");
        EC_FAIL;
    }

    LOG(log_warning, logtype_cnid, "Recreated CNID BerkeleyDB databases of volume \"%s\"", vol->v_localname);

EC_CLEANUP:
    if (cwd != -1) {
        fchdir(cwd);
        close(cwd);
    }
    EC_EXIT;
}


/**
 * Close dbd if open, delete it, reopen
 *
 * Also tries to copy the rootinfo key, that would allow for keeping the db stamp
 * and last used CNID
 **/
static int reinit_db(void)
{
    EC_INIT;
    DBT key, data;
    bool copyRootInfo = false;

    if (dbd) {
        memset(&key, 0, sizeof(key));
        memset(&data, 0, sizeof(data));

        key.data = ROOTINFO_KEY;
        key.size = ROOTINFO_KEYLEN;

        if (dbif_get(dbd, DBIF_CNID, &key, &data, 0) <= 0) {
            LOG(log_error, logtype_cnid, "dbif_copy_rootinfokey: Error getting rootinfo record");
            copyRootInfo = false;
        } else {
            copyRootInfo = true;
        }
        (void)dbif_close(dbd);
    }

    EC_ZERO_LOG( delete_db() );
    EC_ZERO_LOG( open_db() );

    if (copyRootInfo == true) {
        memset(&key, 0, sizeof(key));
        key.data = ROOTINFO_KEY;
        key.size = ROOTINFO_KEYLEN;

        if (dbif_put(dbd, DBIF_CNID, &key, &data, 0) != 0) {
            LOG(log_error, logtype_cnid, "dbif_copy_rootinfokey: Error writing rootinfo key");
            EC_FAIL;
        }
    }

EC_CLEANUP:
    EC_EXIT;
}

static int loop(struct db_param *dbp)
{
    struct cnid_dbd_rqst rqst;
    struct cnid_dbd_rply rply;
    time_t timeout;
    int ret, cret;
    int count;
    time_t now, time_next_flush, time_last_rqst;
    char timebuf[64];
    static char namebuf[MAXPATHLEN + 1];
    sigset_t set;

    sigemptyset(&set);
    sigprocmask(SIG_SETMASK, NULL, &set);
    sigdelset(&set, SIGINT);
    sigdelset(&set, SIGTERM);

    count = 0;
    now = time(NULL);
    time_next_flush = now + dbp->flush_interval;
    time_last_rqst = now;

    rqst.name = namebuf;

    strftime(timebuf, 63, "%b %d %H:%M:%S.",localtime(&time_next_flush));
    LOG(log_debug, logtype_cnid, "Checkpoint interval: %d seconds. Next checkpoint: %s",
        dbp->flush_interval, timebuf);

    while (1) {
        timeout = MIN(time_next_flush, time_last_rqst + dbp->idle_timeout);
        if (timeout > now)
            timeout -= now;
        else
            timeout = 1;

        if ((cret = comm_rcv(&rqst, timeout, &set, &now)) < 0)
            return -1;

        if (cret == 0) {
            /* comm_rcv returned from select without receiving anything. */
            if (exit_sig) {
                /* Received signal (TERM|INT) */
                return 0;
            }
            if (now - time_last_rqst >= dbp->idle_timeout && comm_nbe() <= 0) {
                /* Idle timeout */
                return 0;
            }
            /* still active connections, reset time_last_rqst */
            time_last_rqst = now;
        } else {
            /* We got a request */
            time_last_rqst = now;

            memset(&rply, 0, sizeof(rply));
            switch(rqst.op) {
                /* ret gets set here */
            case CNID_DBD_OP_OPEN:
            case CNID_DBD_OP_CLOSE:
                /* open/close are noops for now. */
                rply.namelen = 0;
                ret = 1;
                break;
            case CNID_DBD_OP_ADD:
                ret = dbd_add(dbd, &rqst, &rply);
                break;
            case CNID_DBD_OP_GET:
                ret = dbd_get(dbd, &rqst, &rply);
                break;
            case CNID_DBD_OP_RESOLVE:
                ret = dbd_resolve(dbd, &rqst, &rply);
                break;
            case CNID_DBD_OP_LOOKUP:
                ret = dbd_lookup(dbd, &rqst, &rply);
                break;
            case CNID_DBD_OP_UPDATE:
                ret = dbd_update(dbd, &rqst, &rply);
                break;
            case CNID_DBD_OP_DELETE:
                ret = dbd_delete(dbd, &rqst, &rply, DBIF_CNID);
                break;
            case CNID_DBD_OP_GETSTAMP:
                ret = dbd_getstamp(dbd, &rqst, &rply);
                break;
            case CNID_DBD_OP_REBUILD_ADD:
                ret = dbd_rebuild_add(dbd, &rqst, &rply);
                break;
            case CNID_DBD_OP_SEARCH:
                ret = dbd_search(dbd, &rqst, &rply);
                break;
            case CNID_DBD_OP_WIPE:
                ret = reinit_db();
                break;
            default:
                LOG(log_error, logtype_cnid, "loop: unknown op %d", rqst.op);
                ret = -1;
                break;
            }

            if ((cret = comm_snd(&rply)) < 0 || ret < 0) {
                dbif_txn_abort(dbd);
                return -1;
            }
            
            if (ret == 0 || cret == 0) {
                if (dbif_txn_abort(dbd) < 0)
                    return -1;
            } else {
                ret = dbif_txn_commit(dbd);
                if (  ret < 0)
                    return -1;
                else if ( ret > 0 )
                    /* We had a designated txn because we wrote to the db */
                    count++;
            }
        } /* got a request */

        /*
          Shall we checkpoint bdb ?
          "flush_interval" seconds passed ?
        */
        if (now >= time_next_flush) {
            LOG(log_info, logtype_cnid, "Checkpointing BerkeleyDB for volume '%s'", dbp->dir);
            if (dbif_txn_checkpoint(dbd, 0, 0, 0) < 0)
                return -1;
            count = 0;
            time_next_flush = now + dbp->flush_interval;

            strftime(timebuf, 63, "%b %d %H:%M:%S.",localtime(&time_next_flush));
            LOG(log_debug, logtype_cnid, "Checkpoint interval: %d seconds. Next checkpoint: %s",
                dbp->flush_interval, timebuf);
        }

        /* 
           Shall we checkpoint bdb ?
           Have we commited "count" more changes than "flush_frequency" ?
        */
        if (count > dbp->flush_frequency) {
            LOG(log_info, logtype_cnid, "Checkpointing BerkeleyDB after %d writes for volume '%s'", count, dbp->dir);
            if (dbif_txn_checkpoint(dbd, 0, 0, 0) < 0)
                return -1;
            count = 0;
        }
    } /* while(1) */
}

/* ------------------------ */
static void switch_to_user(char *dir)
{
    struct stat st;

    if (chdir(dir) < 0) {
        LOG(log_error, logtype_cnid, "chdir to %s failed: %s", dir, strerror(errno));
        exit(1);
    }

    if (stat(".", &st) < 0) {
        LOG(log_error, logtype_cnid, "error in stat for %s: %s", dir, strerror(errno));
        exit(1);
    }
    if (!getuid()) {
        LOG(log_debug, logtype_cnid, "Setting uid/gid to %i/%i", st.st_uid, st.st_gid);
        if (setgid(st.st_gid) < 0 || setuid(st.st_uid) < 0) {
            LOG(log_error, logtype_cnid, "uid/gid: %s", strerror(errno));
            exit(1);
        }
    }
}


/* ----------------------- */
static void set_signal(void)
{
    struct sigaction sv;

    sv.sa_handler = sig_exit;
    sv.sa_flags = 0;
    sigemptyset(&sv.sa_mask);
    sigaddset(&sv.sa_mask, SIGINT);
    sigaddset(&sv.sa_mask, SIGTERM);
    if (sigaction(SIGINT, &sv, NULL) < 0 || sigaction(SIGTERM, &sv, NULL) < 0) {
        LOG(log_error, logtype_cnid, "main: sigaction: %s", strerror(errno));
        exit(1);
    }
    sv.sa_handler = SIG_IGN;
    sigemptyset(&sv.sa_mask);
    if (sigaction(SIGPIPE, &sv, NULL) < 0) {
        LOG(log_error, logtype_cnid, "main: sigaction: %s", strerror(errno));
        exit(1);
    }
}

/* ------------------------ */
int main(int argc, char *argv[])
{
    EC_INIT;
    int delete_bdb = 0;
    int ctrlfd = -1, clntfd = -1;
    AFPObj obj = { 0 };
    char *volpath = NULL;

    while (( ret = getopt( argc, argv, "dF:l:p:t:vV")) != -1 ) {
        switch (ret) {
        case 'd':
            /* this is now just ignored, as we do it automatically anyway */
            delete_bdb = 1;
            break;
        case 'F':
            obj.cmdlineconfigfile = strdup(optarg);
            break;
        case 'p':
            volpath = strdup(optarg);
            break;
        case 'l':
            clntfd = atoi(optarg);
            break;
        case 't':
            ctrlfd = atoi(optarg);
            break;
        case 'v':
        case 'V':
            printf("cnid_dbd (Netatalk %s)\n", VERSION);
            return -1;
        }
    }

    if (ctrlfd == -1 || clntfd == -1 || !volpath) {
        LOG(log_error, logtype_cnid, "main: bad IPC fds");
        exit(EXIT_FAILURE);
    }

    EC_ZERO( afp_config_parse(&obj, "cnid_dbd") );

    EC_ZERO( load_volumes(&obj) );
    EC_NULL( vol = getvolbypath(&obj, volpath) );
    EC_ZERO( load_charset(vol) );
    pack_setvol(vol);

    EC_NULL( dbpath = bfromcstr(vol->v_dbpath) );
    EC_ZERO( bcatcstr(dbpath, "/.AppleDB") );

    LOG(log_debug, logtype_cnid, "db dir: \"%s\"", bdata(dbpath));

    switch_to_user(bdata(dbpath));

    set_signal();

    /* SIGINT and SIGTERM are always off, unless we are in pselect */
    block_sigs_onoff(1);

    if ((dbp = db_param_read(bdata(dbpath))) == NULL)
        EC_FAIL;
    LOG(log_maxdebug, logtype_cnid, "Finished parsing db_param config file");

    if (open_db() != 0) {
        LOG(log_error, logtype_cnid, "Failed to open CNID database for volume \"%s\"", vol->v_localname);
        EC_ZERO_LOG( reinit_db() );
    }

    if (comm_init(dbp, ctrlfd, clntfd) < 0) {
        ret = -1;
        goto close_db;
    }

    if (loop(dbp) < 0) {
        ret = -1;
        goto close_db;
    }

close_db:
    if (dbif_close(dbd) < 0)
        ret = -1;

    if (dbif_env_remove(bdata(dbpath)) < 0)
        ret = -1;

EC_CLEANUP:
    if (ret != 0)
        exit(1);

    if (exit_sig)
        LOG(log_info, logtype_cnid, "main: Exiting on signal %i", exit_sig);
    else
        LOG(log_info, logtype_cnid, "main: Idle timeout, exiting");

    EC_EXIT;
}
