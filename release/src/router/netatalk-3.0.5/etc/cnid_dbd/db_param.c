/*
 *
 * Copyright (C) Joerg Lenneis 2003
 * Copyright (c) Frank Lahm 2009
 * All Rights Reserved.  See COPYING.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif /* HAVE_CONFIG_H */

#include <unistd.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <sys/param.h>
#include <sys/un.h>
#include <sys/select.h>
#include <atalk/logger.h>

#include "db_param.h"

#define DB_PARAM_FN       "db_param"
#define MAXKEYLEN         64

static struct db_param params;
static int parse_err;

static size_t usock_maxlen(void)
{
    struct sockaddr_un addr;
    return sizeof(addr.sun_path) - 1;
}

static int make_pathname(char *path, char *dir, char *fn, size_t maxlen)
{
    size_t len;

    if (fn[0] != '/') {
        len = strlen(dir);
        if (len + 1 + strlen(fn) > maxlen)
            return -1;      
        strcpy(path, dir);
        if (path[len - 1] != '/')
            strcat(path, "/");
        strcat(path, fn);
    } else {
        if (strlen(fn) > maxlen)
            return -1;
        strcpy(path, fn);
    }
    return 0;
}

static void default_params(struct db_param *dbp, char *dir)
{        
    dbp->logfile_autoremove  = DEFAULT_LOGFILE_AUTOREMOVE;
    dbp->cachesize           = DEFAULT_CACHESIZE;
    dbp->maxlocks            = DEFAULT_MAXLOCKS;
    dbp->maxlockobjs         = DEFAULT_MAXLOCKOBJS;
    dbp->flush_frequency     = DEFAULT_FLUSH_FREQUENCY;
    dbp->flush_interval      = DEFAULT_FLUSH_INTERVAL;
    if (make_pathname(dbp->usock_file, dir, DEFAULT_USOCK_FILE, usock_maxlen()) < 0) {
        /* Not an error yet, it might be set in the config file */
        dbp->usock_file[0] = '\0';
    }
    dbp->fd_table_size       = DEFAULT_FD_TABLE_SIZE;
    if ( dbp->fd_table_size > FD_SETSIZE -1)
        dbp->fd_table_size = FD_SETSIZE -1;
    dbp->idle_timeout        = DEFAULT_IDLE_TIMEOUT;

    return;
}

static int parse_int(char *val)
{
    char *tmp;
    int   result = 0;

    result = strtol(val, &tmp, 10);
    if (tmp[0] != '\0') {
        LOG(log_error, logtype_cnid, "invalid characters in token %s", val);
        parse_err++;
    }
    return result;
}


/* TODO: This configuration file reading routine is neither very robust (%s
   buffer overflow) nor elegant, we need to add support for whitespace in
   filenames as well. */

struct db_param *db_param_read(char *dir)
{
    FILE *fp;
    static char key[MAXKEYLEN + 1];
    static char val[MAXPATHLEN + 1];
    static char pfn[MAXPATHLEN + 1];
    int    items;
    
    default_params(&params, dir);
    params.dir = dir;
    
    if (make_pathname(pfn, dir, DB_PARAM_FN, MAXPATHLEN) < 0) {
        LOG(log_error, logtype_cnid, "Parameter filename too long");
        return NULL;
    }

    if ((fp = fopen(pfn, "r")) == NULL) {
        if (errno == ENOENT) {
            if (strlen(params.usock_file) == 0) {
                LOG(log_error, logtype_cnid, "default usock filename too long");
                return NULL;
            } else {
                return &params;
            }
        } else {
            LOG(log_error, logtype_cnid, "error opening %s: %s", pfn, strerror(errno));
            return NULL;
        }
    }
    parse_err = 0;

    while ((items = fscanf(fp, " %s %s", key, val)) != EOF) {
        if (items != 2) {
            LOG(log_error, logtype_cnid, "error parsing config file");
            parse_err++;
            break;
        }

        /* Config for both cnid_meta and dbd */
        if (! strcmp(key, "usock_file")) {
            if (make_pathname(params.usock_file, dir, val, usock_maxlen()) < 0) {
                LOG(log_error, logtype_cnid, "usock filename %s too long", val);
                parse_err++;
            } else
                LOG(log_info, logtype_cnid, "db_param: setting UNIX domain socket filename to %s", params.usock_file);
        }

        if (! strcmp(key, "fd_table_size")) {
            params.fd_table_size = parse_int(val);
            LOG(log_info, logtype_cnid, "db_param: setting max number of concurrent afpd connections per volume (fd_table_size) to %d", params.fd_table_size);
        } else if (! strcmp(key, "logfile_autoremove")) {
            params.logfile_autoremove = parse_int(val);
            LOG(log_info, logtype_cnid, "db_param: setting logfile_autoremove to %d", params.logfile_autoremove);
        } else if (! strcmp(key, "cachesize")) {
            params.cachesize = parse_int(val);
            LOG(log_info, logtype_cnid, "db_param: setting cachesize to %d", params.cachesize);
        } else if (! strcmp(key, "maxlocks")) {
            params.maxlocks = parse_int(val);
            LOG(log_info, logtype_cnid, "db_param: setting maxlocks to %d", params.maxlocks);
        } else if (! strcmp(key, "maxlockobjs")) {
            params.maxlockobjs = parse_int(val);
            LOG(log_info, logtype_cnid, "db_param: setting maxlockobjs to %d", params.maxlockobjs);
        } else if (! strcmp(key, "flush_frequency")) {
            params.flush_frequency = parse_int(val);
            LOG(log_info, logtype_cnid, "db_param: setting flush_frequency to %d", params.flush_frequency);
        } else if (! strcmp(key, "flush_interval")) {
            params.flush_interval = parse_int(val);
            LOG(log_info, logtype_cnid, "db_param: setting flush_interval to %d", params.flush_interval);
        } else if (! strcmp(key, "idle_timeout")) {
            params.idle_timeout = parse_int(val);
            LOG(log_info, logtype_cnid, "db_param: setting idle timeout to %d", params.idle_timeout);
        }

        if (parse_err)
            break;
    }
    
    if (strlen(params.usock_file) == 0) {
        LOG(log_error, logtype_cnid, "default usock filename too long");
        parse_err++;
    }

    fclose(fp);
    if (! parse_err) {
        /* sanity checks */
        if (params.flush_frequency <= 0) 
            params.flush_frequency = 86400;

        if (params.flush_interval <= 0)
            params.flush_interval = 1000000;

        if (params.fd_table_size <= 2)
            params.fd_table_size = 32;

        if (params.idle_timeout <= 0)
            params.idle_timeout = 86400;

        return &params;
    }
    else
        return NULL;
}



