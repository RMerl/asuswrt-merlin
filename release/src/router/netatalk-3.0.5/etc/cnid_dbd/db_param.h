/*
 * Copyright (C) Joerg Lenneis 2003
 * Copyright (C) Frank Lahm 2010
 * All Rights Reserved.  See COPYING.
 */

#ifndef CNID_DBD_DB_PARAM_H
#define CNID_DBD_DB_PARAM_H 1

#include <sys/param.h>

#define DEFAULT_LOGFILE_AUTOREMOVE 1
#define DEFAULT_CACHESIZE          (8 * 1024) /* KB, so 8 MB */
#define DEFAULT_MAXLOCKS           20000
#define DEFAULT_MAXLOCKOBJS        20000
#define DEFAULT_FLUSH_FREQUENCY    1000
#define DEFAULT_FLUSH_INTERVAL     1800
#define DEFAULT_USOCK_FILE         "usock"
#define DEFAULT_FD_TABLE_SIZE      512
#define DEFAULT_IDLE_TIMEOUT       (10 * 60)

struct db_param {
    char *dir;
    int logfile_autoremove;
    int cachesize;              /* in KB */
    int maxlocks;
    int maxlockobjs;
    int flush_interval;
    int flush_frequency;
    char usock_file[MAXPATHLEN + 1];    
    int fd_table_size;
    int idle_timeout;
    int max_vols;
};

extern struct db_param *db_param_read  (char *);

#endif /* CNID_DBD_DB_PARAM_H */

