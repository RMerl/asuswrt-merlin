/*
 *  MTA-MIB implementation for sendmail - mibII/mta_sendmail.c
 *  Christoph Mammitzsch <Christoph.Mammitzsch@tu-clausthal.de>
 *
 * todo: put queue directory into description?
 *
 *  13.02.2002:
 *    - support sendmail 8.12 queue groups
 *
 *
 *  05.04.2000:
 *
 *    - supports sendmail 8.10.0 statistics files now
 *    - function read_option has been removed
 *
 *  12.04.2000:
 *
 *    - renamed configuration tokens:
 *        sendmail config        -> sendmail_config
 *        sendmail stats         -> sendmail_stats
 *        sendmail queue         -> sendmail_queue
 *        sendmail index         -> sendmail_index
 *        sendmail statcachetime -> sendmail_stats_t
 *        sendmail dircacetime   -> sendmail_queue_t
 *
 *    - now using snmpd_register_config_handler instead of config_parse_dot_conf
 *
 *  15.04.2000:
 *
 *    - introduced new function print_error
 *    - changed open_sendmailst and read_sendmailcf to use the new function
 *    - changed calls to open_sendmailst and read_sendmailcf
 *    - added some error handling to calls to chdir(), close() and closedir()
 *
 */


/** "include files" */
#ifdef __lint
# define NETSNMP_NO_DEBUGGING 1    /* keeps lint from complaining about the DEBUGMSG* macros */
#endif

#include <net-snmp/net-snmp-config.h>

#include <net-snmp/net-snmp-includes.h>
#include <net-snmp/agent/net-snmp-agent-includes.h>

#include "mta_sendmail.h"

#include <sys/types.h>

#include <stdio.h>

#include <ctype.h>

#ifdef HAVE_STRING_H
# include <string.h>
#else
# include <strings.h>
#endif

#if HAVE_STDLIB_H
#include <stdlib.h>
#endif

#ifdef HAVE_UNISTD_H
# include <unistd.h>
#endif

#ifdef HAVE_FCNTL_H
# include <fcntl.h>
#endif

#if HAVE_DIRENT_H
#include <dirent.h>
#else
# define dirent direct
# if HAVE_SYS_NDIR_H
#  include <sys/ndir.h>
# endif
# if HAVE_SYS_DIR_H
#  include <sys/dir.h>
# endif
# if HAVE_NDIR_H
#  include <ndir.h>
# endif
#endif

#ifdef HAVE_SYS_STAT_H
# include <sys/stat.h>
#endif

#if TIME_WITH_SYS_TIME
# include <sys/time.h>
# include <time.h>
#else
# if HAVE_SYS_TIME_H
#  include <sys/time.h>
# else
#  include <time.h>
# endif
#endif

#include <errno.h>
#include <stdarg.h>

 /**/
/** "macros and variables for registering the OID tree" */
    /*
     * prefix for all OIDs 
     */

static FindVarMethod var_mtaEntry;
static FindVarMethod var_mtaGroupEntry;

static oid      mta_variables_oid[] = { 1, 3, 6, 1, 2, 1, 28 };

/*
 * bits that indicate what's needed to compute the value 
 */
#define   NEEDS_STATS   (1 << 6)
#define   NEEDS_DIR     (1 << 7)
#define   NEEDS         (NEEDS_STATS | NEEDS_DIR)

/*
 * symbolic names for the magic values 
 */
enum {
    MTARECEIVEDMESSAGES = 3 | NEEDS_STATS,
    MTASTOREDMESSAGES = 4 | NEEDS_DIR,
    MTATRANSMITTEDMESSAGES = 5 | NEEDS_STATS,
    MTARECEIVEDVOLUME = 6 | NEEDS_STATS,
    MTASTOREDVOLUME = 7 | NEEDS_DIR,
    MTATRANSMITTEDVOLUME = 8 | NEEDS_STATS,
    MTAGROUPSTOREDMESSAGES = 17 | NEEDS_DIR,
    MTAGROUPSTOREDVOLUME = 18 | NEEDS_DIR,
    MTAGROUPRECEIVEDMESSAGES = 19 | NEEDS_STATS,
    MTAGROUPREJECTEDMESSAGES = 20 | NEEDS_STATS,
    MTAGROUPTRANSMITTEDMESSAGES = 22 | NEEDS_STATS,
    MTAGROUPRECEIVEDVOLUME = 23 | NEEDS_STATS,
    MTAGROUPTRANSMITTEDVOLUME = 25 | NEEDS_STATS,
    MTAGROUPNAME = 43,
    MTAGROUPHIERARCHY = 49
};

/*
 * structure that tells the agent, which function returns what values 
 */
static struct variable3 mta_variables[] = {
    {MTARECEIVEDMESSAGES, ASN_COUNTER, NETSNMP_OLDAPI_RONLY,
     var_mtaEntry, 3, {1, 1, 1}},
    {MTASTOREDMESSAGES, ASN_GAUGE, NETSNMP_OLDAPI_RONLY,
     var_mtaEntry, 3, {1, 1, 2}},
    {MTATRANSMITTEDMESSAGES, ASN_COUNTER, NETSNMP_OLDAPI_RONLY,
     var_mtaEntry, 3, {1, 1, 3}},
    {MTARECEIVEDVOLUME, ASN_COUNTER, NETSNMP_OLDAPI_RONLY,
     var_mtaEntry, 3, {1, 1, 4}},
    {MTASTOREDVOLUME, ASN_GAUGE, NETSNMP_OLDAPI_RONLY,
     var_mtaEntry, 3, {1, 1, 5}},
    {MTATRANSMITTEDVOLUME, ASN_COUNTER, NETSNMP_OLDAPI_RONLY,
     var_mtaEntry, 3, {1, 1, 6}},

    {MTAGROUPRECEIVEDMESSAGES, ASN_COUNTER, NETSNMP_OLDAPI_RONLY,
     var_mtaGroupEntry, 3, {2, 1, 2}},
    {MTAGROUPREJECTEDMESSAGES, ASN_COUNTER, NETSNMP_OLDAPI_RONLY,
     var_mtaGroupEntry, 3, {2, 1, 3}},
    {MTAGROUPSTOREDMESSAGES, ASN_GAUGE, NETSNMP_OLDAPI_RONLY,
     var_mtaGroupEntry, 3, {2, 1, 4}},
    {MTAGROUPTRANSMITTEDMESSAGES, ASN_COUNTER, NETSNMP_OLDAPI_RONLY,
     var_mtaGroupEntry, 3, {2, 1, 5}},
    {MTAGROUPRECEIVEDVOLUME, ASN_COUNTER, NETSNMP_OLDAPI_RONLY,
     var_mtaGroupEntry, 3, {2, 1, 6}},
    {MTAGROUPSTOREDVOLUME, ASN_GAUGE, NETSNMP_OLDAPI_RONLY,
     var_mtaGroupEntry, 3, {2, 1, 7}},
    {MTAGROUPTRANSMITTEDVOLUME, ASN_COUNTER, NETSNMP_OLDAPI_RONLY,
     var_mtaGroupEntry, 3, {2, 1, 8}},
    {MTAGROUPNAME, ASN_OCTET_STR, NETSNMP_OLDAPI_RONLY,
     var_mtaGroupEntry, 3, {2, 1, 25}},
    {MTAGROUPHIERARCHY, ASN_INTEGER, NETSNMP_OLDAPI_RONLY,
     var_mtaGroupEntry, 3, {2, 1, 31}}
};
 /**/
/** "other macros and structures" */
    /*
     * for boolean values 
     */
#ifndef FALSE
#define FALSE 0
#endif
#ifndef TRUE
#define TRUE  1
#endif
#ifndef BOOL
#define BOOL  short
#endif
    /*
     * important constants 
     */
#define FILENAMELEN     200     /* maximum length for filenames */
#define MAXMAILERS       25     /* maximum number of mailers (copied from the sendmail sources) */
#define MAXQUEUEGROUPS   50     /* maximum # of queue groups (copied from sendmail) */
#define MNAMELEN         20     /* maximum length of mailernames (copied from the sendmail sources) */
#define STAT_VERSION_8_9  2     /* version of sendmail V8.9.x statistics files (copied from the sendmail sources) */
#define STAT_VERSION_8_10 3     /* version of sendmail V8.10.x statistics files (copied from the sendmail sources) */
#define STAT_VERSION_8_12_QUAR 4     /* version of sendmail V8.12.x statistics files using -D_FFR_QUARANTINE (commercial and edge-living opensource*/
#define STAT_MAGIC  0x1B1DE     /* magic value to identify statistics files from sendmail V8.9.x or higher (copied from the sendmail sources) */
    /*
     * structure of sendmail.st file from sendmail V8.10.x (copied from the sendmail sources) 
     */

struct statisticsV8_12_QUAR {
    int             stat_magic; /* magic number */
    int             stat_version;       /* stat file version */
    time_t          stat_itime; /* file initialization time */
    short           stat_size;  /* size of this structure */
    long            stat_cf;    /* # from connections */
    long            stat_ct;    /* # to connections */
    long            stat_cr;    /* # rejected connections */
    long            stat_nf[MAXMAILERS];        /* # msgs from each mailer */
    long            stat_bf[MAXMAILERS];        /* kbytes from each mailer */
    long            stat_nt[MAXMAILERS];        /* # msgs to each mailer */
    long            stat_bt[MAXMAILERS];        /* kbytes to each mailer */
    long            stat_nr[MAXMAILERS];        /* # rejects by each mailer */
    long            stat_nd[MAXMAILERS];        /* # discards by each mailer */
    long            stat_nq[MAXMAILERS];        /* # quarantines by each mailer*/
};

    struct statisticsV8_10 {
    int             stat_magic; /* magic number */
    int             stat_version;       /* stat file version */
    time_t          stat_itime; /* file initialization time */
    short           stat_size;  /* size of this structure */
    long            stat_cf;    /* # from connections */
    long            stat_ct;    /* # to connections */
    long            stat_cr;    /* # rejected connections */
    long            stat_nf[MAXMAILERS];        /* # msgs from each mailer */
    long            stat_bf[MAXMAILERS];        /* kbytes from each mailer */
    long            stat_nt[MAXMAILERS];        /* # msgs to each mailer */
    long            stat_bt[MAXMAILERS];        /* kbytes to each mailer */
    long            stat_nr[MAXMAILERS];        /* # rejects by each mailer */
    long            stat_nd[MAXMAILERS];        /* # discards by each mailer */

};

/*
 * structure of sendmail.st file from sendmail V8.9.x (copied from the sendmail sources) 
 */
struct statisticsV8_9 {
    int             stat_magic; /* magic number */
    int             stat_version;       /* stat file version */
    time_t          stat_itime; /* file initialization time */
    short           stat_size;  /* size of this structure */
    long            stat_nf[MAXMAILERS];        /* # msgs from each mailer */
    long            stat_bf[MAXMAILERS];        /* kbytes from each mailer */
    long            stat_nt[MAXMAILERS];        /* # msgs to each mailer */
    long            stat_bt[MAXMAILERS];        /* kbytes to each mailer */
    long            stat_nr[MAXMAILERS];        /* # rejects by each mailer */
    long            stat_nd[MAXMAILERS];        /* # discards by each mailer */
};

/*
 * structure of sendmail.st file from sendmail V8.8.x (copied from the sendmail sources) 
 */
struct statisticsV8_8 {
    time_t          stat_itime; /* file initialization time */
    short           stat_size;  /* size of this structure */
    long            stat_nf[MAXMAILERS];        /* # msgs from each mailer */
    long            stat_bf[MAXMAILERS];        /* kbytes from each mailer */
    long            stat_nt[MAXMAILERS];        /* # msgs to each mailer */
    long            stat_bt[MAXMAILERS];        /* kbytes to each mailer */
};
 /**/
    /*
     * queue groups (strictly a sendmail 8.12+ thing 
     */
    struct QDir {
    char            dir[FILENAMELEN + 1];
    struct QDir    *next;
};

struct QGrp {
    char           *name;       /* name of queuegroup */

    time_t          last;       /* last time we counted */
    int             count;      /* # of files */
    int             size;       /* size of files */

    struct QDir    *dirs;       /* directories in queue group */
};

/** "static variables" */

/*
 * a list of all the queue groups, NULL terminated 
 */
static struct QGrp qgrps[MAXQUEUEGROUPS];
static int      nqgrps = 0;

static char     sendmailst_fn[FILENAMELEN + 1]; /* name of statistics file */
static int      sendmailst_fh = -1;     /* filehandle for statistics file */
static char     sendmailcf_fn[FILENAMELEN + 1]; /* name of sendmails config file */
static char     mailernames[MAXMAILERS][MNAMELEN + 1];  /* array of mailer names */
static int      mailers = MAXMAILERS;   /* number of mailer names in array */

static long    *stat_nf;        /* pointer to stat_nf array within the statistics structure */
static long    *stat_bf;        /* pointer to stat_bf array within the statistics structure */
static long    *stat_nt;        /* pointer to stat_nt array within the statistics structure */
static long    *stat_bt;        /* pointer to stat_bt array within the statistics structure */
static long    *stat_nr;        /* pointer to stat_nr array within the statistics structure,
                                 * only valid for statistics files from sendmail >=V8.9.0    */
static long    *stat_nd;        /* pointer to stat_nd array within the statistics structure,
                                 * only valid for statistics files from sendmail >=V8.9.0    */
static int      stats_size;     /* size of statistics structure */
static long     stats[sizeof(struct statisticsV8_12_QUAR) / sizeof(long) + 1];       /* buffer for statistics structure */
static time_t   lastreadstats;  /* time stats file has been read */
static long     applindex = 1;  /* ApplIndex value for OIDs */
static long     stat_cache_time = 5;    /* time (in seconds) to wait before reading stats file again */
static long     dir_cache_time = 10;    /* time (in seconds) to wait before scanning queue directoy again */

 /**/
/** static void print_error(int priority, BOOL config, BOOL config_only, char *function, char *format, ...)
 *
 *  Description:
 *
 *    Called to print errors. It uses the config_perror or the snmp_log function
 *    depending on whether the config parameter is TRUE or FALSE.
 *
 *  Parameters:
 *
 *    priority:    priority to be used when calling the snmp_log function
 *
 *    config:      indicates whether this function has been called during the
 *                 configuration process or not. If set to TRUE, the function
 *                 config_perror will be used to report the error.
 *
 *    config_only: if set to TRUE, the error will only be printed when function
 *                 has been called during the configuration process.
 *
 *    function:    name of the calling function. Used when printing via snmp_log.
 *
 *    format:      format string for the error message
 *
 *    ...:         additional parameters to insert into the error message string
 *
 */
static void
print_error(int priority, BOOL config, BOOL config_only,
            const char *function, const char *format, ...)
{
    va_list         ap;
    char            buffer[2 * FILENAMELEN + 200];      /* I know, that's not perfectly safe, but since I don't use more
                                                         * than two filenames in one error message, that should be enough */

    va_start(ap, format);
    vsnprintf(buffer, sizeof(buffer), format, ap);

    if (config) {
        config_perror(buffer);
    } else if (!config_only) {
        snmp_log(priority, "%s: %s\n", function, buffer);
    }
    va_end(ap);
}

 /**/
/** static void open_sendmailst(BOOL config)
 *
 *  Description:
 *
 *    Closes old sendmail.st file, then tries to open the new sendmail.st file
 *    and guess it's version. If it succeeds, it initializes the stat_*
 *    pointers and the stats_size variable.
 *
 *  Parameters:
 *
 *    config: TRUE if function has been called during the configuration process
 *
 *  Returns:
 *
 *    nothing
 *
 */
    static void
open_sendmailst(BOOL config)
{
    int             filelen;

    if (sendmailst_fh != -1) {
        while (close(sendmailst_fh) == -1 && errno == EINTR) {
            /*
             * do nothing 
             */
        }
    }

    sendmailst_fh = open(sendmailst_fn, O_RDONLY);

    if (sendmailst_fh == -1) {
        print_error(LOG_ERR, config, TRUE,
                    "mibII/mta_sendmail.c:open_sendmailst",
                    "could not open file \"%s\"", sendmailst_fn);
        return;
    }

    filelen = read(sendmailst_fh, (void *) &stats, sizeof stats);

    if (((struct statisticsV8_10 *) stats)->stat_magic == STAT_MAGIC) {

        if (((struct statisticsV8_12_QUAR *) stats)->stat_version ==
            STAT_VERSION_8_12_QUAR
            && ((struct statisticsV8_12_QUAR *) stats)->stat_size ==
            sizeof(struct statisticsV8_12_QUAR)
            && filelen == sizeof(struct statisticsV8_12_QUAR)) {
            DEBUGMSGTL(("mibII/mta_sendmail.c:open_sendmailst",
                        "looks like file \"%s\" has been created by sendmail V8.10.0 or newer\n",
                        sendmailst_fn));
            stat_nf = (((struct statisticsV8_12_QUAR *) stats)->stat_nf);
            stat_bf = (((struct statisticsV8_12_QUAR *) stats)->stat_bf);
            stat_nt = (((struct statisticsV8_12_QUAR *) stats)->stat_nt);
            stat_bt = (((struct statisticsV8_12_QUAR *) stats)->stat_bt);
            stat_nr = (((struct statisticsV8_12_QUAR *) stats)->stat_nr);
            stat_nd = (((struct statisticsV8_12_QUAR *) stats)->stat_nd);
            stats_size = sizeof(struct statisticsV8_12_QUAR);
        } else

		 if (((struct statisticsV8_10 *) stats)->stat_version ==
            STAT_VERSION_8_10
            && ((struct statisticsV8_10 *) stats)->stat_size ==
            sizeof(struct statisticsV8_10)
            && filelen == sizeof(struct statisticsV8_10)) {
            DEBUGMSGTL(("mibII/mta_sendmail.c:open_sendmailst",
                        "looks like file \"%s\" has been created by sendmail V8.10.0 or newer\n",
                        sendmailst_fn));
            stat_nf = (((struct statisticsV8_10 *) stats)->stat_nf);
            stat_bf = (((struct statisticsV8_10 *) stats)->stat_bf);
            stat_nt = (((struct statisticsV8_10 *) stats)->stat_nt);
            stat_bt = (((struct statisticsV8_10 *) stats)->stat_bt);
            stat_nr = (((struct statisticsV8_10 *) stats)->stat_nr);
            stat_nd = (((struct statisticsV8_10 *) stats)->stat_nd);
            stats_size = sizeof(struct statisticsV8_10);

        } else if (((struct statisticsV8_9 *) stats)->stat_version ==
                   STAT_VERSION_8_9
                   && ((struct statisticsV8_9 *) stats)->stat_size ==
                   sizeof(struct statisticsV8_9)
                   && filelen == sizeof(struct statisticsV8_9)) {
            DEBUGMSGTL(("mibII/mta_sendmail.c:open_sendmailst",
                        "looks like file \"%s\" has been created by sendmail V8.9.x\n",
                        sendmailst_fn));
            stat_nf = (((struct statisticsV8_9 *) stats)->stat_nf);
            stat_bf = (((struct statisticsV8_9 *) stats)->stat_bf);
            stat_nt = (((struct statisticsV8_9 *) stats)->stat_nt);
            stat_bt = (((struct statisticsV8_9 *) stats)->stat_bt);
            stat_nr = (((struct statisticsV8_9 *) stats)->stat_nr);
            stat_nd = (((struct statisticsV8_9 *) stats)->stat_nd);
            stats_size = sizeof(struct statisticsV8_9);
        } else {
            print_error(LOG_WARNING, config, FALSE,
                        "mibII/mta_sendmail.c:open_sendmailst",
                        "could not guess version of statistics file \"%s\"",
                        sendmailst_fn);
            while (close(sendmailst_fh) == -1 && errno == EINTR) {
                /*
                 * do nothing 
                 */
            }
            sendmailst_fh = -1;
        }
    } else {
        if (((struct statisticsV8_8 *) stats)->stat_size ==
            sizeof(struct statisticsV8_8)
            && filelen == sizeof(struct statisticsV8_8)) {
            DEBUGMSGTL(("mibII/mta_sendmail.c:open_sendmailst",
                        "looks like file \"%s\" has been created by sendmail V8.8.x\n",
                        sendmailst_fn));
            stat_nf = (((struct statisticsV8_8 *) stats)->stat_nf);
            stat_bf = (((struct statisticsV8_8 *) stats)->stat_bf);
            stat_nt = (((struct statisticsV8_8 *) stats)->stat_nt);
            stat_bt = (((struct statisticsV8_8 *) stats)->stat_bt);
            stat_nr = (long *) NULL;
            stat_nd = (long *) NULL;
            stats_size = sizeof(struct statisticsV8_8);
        } else {
            print_error(LOG_WARNING, config, FALSE,
                        "mibII/mta_sendmail.c:open_sendmailst",
                        "could not guess version of statistics file \"%s\"",
                        sendmailst_fn);
            while (close(sendmailst_fh) == -1 && errno == EINTR) {
                /*
                 * do nothing 
                 */
            }
            sendmailst_fh = -1;
        }
    }
}

 /**/ static void
count_queuegroup(struct QGrp *qg)
{
    struct QDir    *d;
    char            cwd[SNMP_MAXPATH];
    time_t          current_time = time(NULL);

    if (current_time <= (qg->last + dir_cache_time)) {
        return;
    }

    if (getcwd(cwd, sizeof cwd) == NULL) {
        snmp_log(LOG_ERR,
                 "mibII/mta_sendmail.c:count_queuegroup: could not get current working directory\n");
        return;
    }

    qg->count = 0;
    qg->size = 0;

    for (d = qg->dirs; d != NULL; d = d->next) {
        DIR            *dp;
        struct dirent  *dirp;
        struct stat     filestat;

        if (chdir(d->dir) != 0)
            continue;
        dp = opendir(".");
        if (dp == NULL)
            continue;
        while ((dirp = readdir(dp)) != NULL) {
            if (dirp->d_name[0] == 'd' && dirp->d_name[1] == 'f') {
                if (stat(dirp->d_name, &filestat) == 0) {
                    qg->size += (filestat.st_size + 999) / 1000;
                }
            } else if (dirp->d_name[0] == 'q' && dirp->d_name[1] == 'f') {
                qg->count++;
            }
        }
        closedir(dp);
    }

    qg->last = current_time;

    chdir(cwd);
}

/** static void add_queuegroup(const char *name, const char *path)
 *
 * Description:
 *
 *   Adds a queuegroup of 'name' with root path 'path' to the static
 *   list of queue groups.  if 'path' ends in a *, we expand it out to
 *   all matching subdirs.  also look for 'qf' subdirectories.
 *
 * Parameters:
 *
 *   qgname: name of the queuegroup discovered
 *   path: path of queuegroup discovered
 */
static void
add_queuegroup(const char *name, char *path)
{
    char            parentdir[FILENAMELEN];
    char           *p;
    struct QDir    *new = NULL;
    struct QDir    *subdir = NULL;
    DIR            *dp;
    struct dirent  *dirp;

    if (nqgrps == MAXQUEUEGROUPS) {
        /*
         * xxx error 
         */
        return;
    }

    if (strlen(path) > FILENAMELEN - 10) {
        /*
         * xxx error 
         */
        return;
    }

    p = path + strlen(path) - 1;
    if (*p == '*') {            /* multiple queue dirs */
        /*
         * remove * 
         */
        *p = '\0';

        strcpy(parentdir, path);
        /*
         * remove last directory component from parentdir 
         */
        for (p = parentdir + strlen(parentdir) - 1; p >= parentdir; p--) {
            if (*p == '/') {
                *p = '\0';
                break;
            }
        }

        if (p < parentdir) {
            /*
             * no trailing / ?!? 
             */

            /*
             * xxx error 
             */
            return;
        }
        p++;

        /*
         * p is now the prefix we need to match 
         */
        if ((dp = opendir(parentdir)) == NULL) {
            /*
             * xxx can't open parentdir 
             */
            return;
        }

        while ((dirp = readdir(dp)) != NULL) {
            if (!strncmp(dirp->d_name, p, strlen(p)) &&
                dirp->d_name[0] != '.') {
                /*
                 * match, add it to the list 
                 */

                /*
                 * single queue directory 
                 */
                subdir = (struct QDir *) malloc(sizeof(struct QDir));
                snprintf(subdir->dir, FILENAMELEN - 5, "%s/%s", parentdir,
                         dirp->d_name);
                subdir->next = new;
                new = subdir;
            }
        }

        closedir(dp);
    } else {
        /*
         * single queue directory 
         */
        new = (struct QDir *) malloc(sizeof(struct QDir));
        strcpy(new->dir, path);
        new->next = NULL;
    }

    /*
     * check 'new' for /qf directories 
     */
    for (subdir = new; subdir != NULL; subdir = subdir->next) {
        char            qf[FILENAMELEN + 1];

        snprintf(qf, FILENAMELEN, "%s/qf", subdir->dir);
        if ((dp = opendir(qf)) != NULL) {
            /*
             * it exists ! 
             */
            strcpy(subdir->dir, qf);
            closedir(dp);
        }
    }

    /*
     * we now have the list of directories in 'new'; create the queuegroup
     * object 
     */
    qgrps[nqgrps].name = strdup(name);
    qgrps[nqgrps].last = 0;
    qgrps[nqgrps].count = 0;
    qgrps[nqgrps].size = 0;
    qgrps[nqgrps].dirs = new;

    nqgrps++;
}

/** static BOOL read_sendmailcf(BOOL config)
 *
 *  Description:
 *
 *    Tries to open the file named in sendmailcf_fn and to get the names of
 *    the mailers, the status file and the mailqueue directories.
 *
 *  Parameters:
 *
 *    config: TRUE if function has been called during the configuration process
 *
 *  Returns:
 *
 *    TRUE  : config file has been successfully opened
 *
 *    FALSE : could not open config file
 *
 */

static BOOL
read_sendmailcf(BOOL config)
{
    FILE           *sendmailcf_fp;
    char            line[500];
    char           *filename;
    char           *qgname, *p;
    int             linenr;
    int             linelen;
    int             found_sendmailst = FALSE;
    int             i;


    sendmailcf_fp = fopen(sendmailcf_fn, "r");
    if (sendmailcf_fp == NULL) {
        print_error(LOG_ERR, config, TRUE,
                    "mibII/mta_sendmail.c:read_sendmailcf",
                    "could not open file \"%s\"", sendmailcf_fn);
        return FALSE;
    }

    /*
     * initializes the standard mailers, which aren't necessarily mentioned in the sendmail.cf file 
     */
    strcpy(mailernames[0], "prog");
    strcpy(mailernames[1], "*file*");
    strcpy(mailernames[2], "*include*");
    mailers = 3;

    /*
     * reset queuegroups 
     */

    linenr = 1;
    while (fgets(line, sizeof line, sendmailcf_fp) != NULL) {
        linelen = strlen(line);

        if (line[linelen - 1] != '\n') {
            print_error(LOG_WARNING, config, FALSE,
                        "mibII/mta_sendmail.c:read_sendmailcf",
                        "line %d in config file \"%s is too long\n",
                        linenr, sendmailcf_fn);
            while (fgets(line, sizeof line, sendmailcf_fp) != NULL && line[strlen(line) - 1] != '\n') { /* skip rest of the line */
                /*
                 * nothing to do 
                 */
            }
            linenr++;
            continue;
        }

        line[--linelen] = '\0';

        switch (line[0]) {

        case 'M':

            if (mailers < MAXMAILERS) {
                for (i = 1;
                     line[i] != ',' && !isspace(line[i]) && line[i] != '\0'
                     && i <= MNAMELEN; i++) {
                    mailernames[mailers][i - 1] = line[i];
                }
                mailernames[mailers][i - 1] = '\0';

                DEBUGMSGTL(("mibII/mta_sendmail.c:read_sendmailcf",
                            "found mailer \"%s\"\n",
                            mailernames[mailers]));

                for (i = 0;
                     i < mailers
                     && strcmp(mailernames[mailers], mailernames[i]) != 0;
                     i++) {
                    /*
                     * nothing to do 
                     */
                }

                if (i == mailers) {
                    mailers++;
                } else {
                    if (i < 3) {
                        DEBUGMSGTL(("mibII/mta_sendmail.c:read_sendmailcf",
                                    "mailer \"%s\" already existed, but since it's one of the predefined mailers, that's probably nothing to worry about\n",
                                    mailernames[mailers]));
                    } else {
                        DEBUGMSGTL(("mibII/mta_sendmail.c:read_sendmailcf",
                                    "mailer \"%s\" already existed\n",
                                    mailernames[mailers]));
                    }
                    mailernames[mailers][0] = '\0';
                }
            } else {
                print_error(LOG_WARNING, config, FALSE,
                            "mibII/mta_sendmail.c:read_sendmailcf",
                            "found too many mailers in config file \"%s\"",
                            sendmailcf_fn);
            }


            break;

        case 'O':
            switch (line[1]) {
            case ' ':
                /*
                 * long option 
                 */
                if (strncasecmp(line + 2, "StatusFile", 10) == 0) {
                    filename = line + 12;
                } else if (strncasecmp(line + 2, "QueueDirectory", 14) ==
                           0) {
                    filename = line + 16;
                } else {
                    /*
                     * not an option we care about 
                     */
                    break;
                }

                /*
                 * make sure it's the end of the option 
                 */
                if (*filename != ' ' && *filename != '=')
                    break;

                /*
                 * skip WS 
                 */
                while (*filename == ' ')
                    filename++;

                /*
                 * must be O <option> = <file> 
                 */
                if (*filename++ != '=') {
                    print_error(LOG_WARNING, config, FALSE,
                                "mibII/mta_sendmail.c:read_sendmailcf",
                                "line %d in config file \"%s\" ist missing an '='",
                                linenr, sendmailcf_fn);
                    break;
                }

                /*
                 * skip WS 
                 */
                while (*filename == ' ')
                    filename++;

                if (strlen(filename) > FILENAMELEN) {
                    print_error(LOG_WARNING, config, FALSE,
                                "mibII/mta_sendmail.c:read_sendmailcf",
                                "line %d config file \"%s\" contains a filename that's too long",
                                linenr, sendmailcf_fn);
                    break;
                }

                if (strncasecmp(line + 2, "StatusFile", 10) == 0) {
                    strlcpy(sendmailst_fn, filename, sizeof(sendmailst_fn));
                    found_sendmailst = TRUE;
                    DEBUGMSGTL(("mibII/mta_sendmail.c:read_sendmailcf",
                                "found statatistics file \"%s\"\n",
                                sendmailst_fn));
                } else if (strncasecmp(line + 2, "QueueDirectory", 14) ==
                           0) {
                    add_queuegroup("mqueue", filename);
                } else {
                    print_error(LOG_CRIT, config, FALSE,
                                "mibII/mta_sendmail.c:read_sendmailcf",
                                "This shouldn't happen.");
                    abort();
                }
                break;

            case 'S':
                if (strlen(line + 2) > FILENAMELEN) {
                    print_error(LOG_WARNING, config, FALSE,
                                "mibII/mta_sendmail.c:read_sendmailcf",
                                "line %d config file \"%s\" contains a filename that's too long",
                                linenr, sendmailcf_fn);
                    break;
                }
                strcpy(sendmailst_fn, line + 2);
                found_sendmailst = TRUE;
                DEBUGMSGTL(("mibII/mta_sendmail.c:read_sendmailcf",
                            "found statatistics file \"%s\"\n",
                            sendmailst_fn));
                break;

            case 'Q':
                if (strlen(line + 2) > FILENAMELEN) {
                    print_error(LOG_WARNING, config, FALSE,
                                "mibII/mta_sendmail.c:read_sendmailcf",
                                "line %d config file \"%s\" contains a filename that's too long",
                                linenr, sendmailcf_fn);
                    break;
                }

                add_queuegroup("mqueue", line + 2);
                break;
            }
            break;

        case 'Q':
            /*
             * found a queue group 
             */
            p = qgname = line + 1;
            while (*p && *p != ',') {
                p++;
            }
            if (*p == '\0') {
                print_error(LOG_WARNING, config, FALSE,
                            "mibII/mta_sendmail.c:read_sendmailcf",
                            "line %d config file \"%s\" contains a weird queuegroup",
                            linenr, sendmailcf_fn);
                break;
            }

            /*
             * look for the directory 
             */
            filename = NULL;
            *p++ = '\0';
            while (*p != '\0') {
                /*
                 * skip WS 
                 */
                while (*p && *p == ' ')
                    p++;

                if (*p == 'P') {        /* found path */
                    while (*p && *p != '=')
                        p++;
                    if (*p++ != '=') {
                        print_error(LOG_WARNING, config, FALSE,
                                    "mibII/mta_sendmail.c:read_sendmailcf",
                                    "line %d config file \"%s\" contains a weird queuegroup",
                                    linenr, sendmailcf_fn);
                        break;
                    }
                    filename = p;

                    /*
                     * find next ',', turn into \0 
                     */
                    while (*p && *p != ',')
                        p++;
                    *p = '\0';
                }

                /*
                 * skip to one past the next , 
                 */
                while (*p && *p != ',') {
                    p++;
                }
		if (*p)
		    p++;
            }

            /*
             * we found a directory 
             */
            if (filename) {
                add_queuegroup(qgname, filename);
            } else {
                print_error(LOG_WARNING, config, FALSE,
                            "mibII/mta_sendmail.c:read_sendmailcf",
                            "line %d config file \"%s\" contains a weird queuegroup: no directory",
                            linenr, sendmailcf_fn);
            }

            break;
        }

        linenr++;
    }

    fclose(sendmailcf_fp);

    for (i = mailers; i < MAXMAILERS; i++) {
        mailernames[i][0] = '\0';
    }

    if (found_sendmailst) {
        open_sendmailst(config);
    }

    return TRUE;
}

 /**/
/** static void mta_sendmail_parse_config(const char* token, char *line)
 *
 *  Description:
 *
 *    Called by the agent for each configuration line that belongs to this module.
 *    The possible tokens are:
 *
 *    sendmail_config  - filename of the sendmail configutarion file
 *    sendmail_stats   - filename of the sendmail statistics file
 *    sendmail_queue   - name of the sendmail mailqueue directory
 *    sendmail_index   - the ApplIndex to use for the table
 *    sendmail_stats_t - the time (in seconds) to cache statistics
 *    sendmail_queue_t - the time (in seconds) to cache the directory scanning results
 *
 *    For "sendmail_config", "sendmail_stats" and "sendmail_queue", the copy_nword
 *    function is used to copy the filename.
 *
 *  Parameters:
 *
 *    token: first word of the line
 *
 *    line:  rest of the line
 *
 *  Returns:
 *
 *    nothing
 *
 */
    static void
mta_sendmail_parse_config(const char *token, char *line)
{
    if (strlen(line) > FILENAMELEN) {   /* Might give some false alarm, but better to be safe than sorry */
        config_perror("line too long");
        return;
    }

    if (strcasecmp(token, "sendmail_stats") == 0) {
        while (isspace(*line)) {
            line++;
        }
        copy_nword(line, sendmailst_fn, sizeof(sendmailst_fn));

        open_sendmailst(TRUE);

        if (sendmailst_fh == -1) {
	    netsnmp_config_error("couldn't open file \"%s\"", sendmailst_fn);
            return;
        }

        DEBUGMSGTL(("mibII/mta_sendmail.c:mta_sendmail_parse_config",
                    "opened statistics file \"%s\"\n", sendmailst_fn));
        return;
    } else if (strcasecmp(token, "sendmail_config") == 0) {
        while (isspace(*line)) {
            line++;
        }
        copy_nword(line, sendmailcf_fn, sizeof(sendmailcf_fn));

        read_sendmailcf(TRUE);

        DEBUGMSGTL(("mibII/mta_sendmail.c:mta_sendmail_parse_config",
                    "read config file \"%s\"\n", sendmailcf_fn));
        return;
    } else if (strcasecmp(token, "sendmail_queue") == 0) {
        while (isspace(*line)) {
            line++;
        }
        add_queuegroup("mqueue", line);

        return;
    } else if (strcasecmp(token, "sendmail_index") == 0) {
        while (isspace(*line)) {
            line++;
        }
        applindex = atol(line);
        if (applindex < 1) {
            config_perror("invalid index number");
            applindex = 1;
        }
    } else if (strcasecmp(token, "sendmail_stats_t") == 0) {
        while (isspace(*line)) {
            line++;
        }
        stat_cache_time = atol(line);
        if (stat_cache_time < 1) {
            config_perror("invalid cache time");
            applindex = 5;
        }
    } else if (strcasecmp(token, "sendmail_queue_t") == 0) {
        while (isspace(*line)) {
            line++;
        }
        dir_cache_time = atol(line);
        if (dir_cache_time < 1) {
            config_perror("invalid cache time");
            applindex = 10;
        }
    } else {
        config_perror
            ("mibII/mta_sendmail.c says: What should I do with that token? Did you ./configure the agent properly?");
    }

    return;
}

 /**/
/** void init_mta_sendmail(void)
 *
 *  Description:
 *
 *    Called by the agent to initialize the module. The function will register
 *    the OID tree and the config handler and try some default values for the
 *    sendmail.cf and sendmail.st files and for the mailqueue directory.
 *
 *  Parameters:
 *
 *    none
 *
 *  Returns:
 *
 *    nothing
 *
 */
    void
init_mta_sendmail(void)
{
    REGISTER_MIB("mibII/mta_sendmail", mta_variables, variable3,
                 mta_variables_oid);

    snmpd_register_config_handler("sendmail_config",
                                  mta_sendmail_parse_config, NULL, "file");
    snmpd_register_config_handler("sendmail_stats",
                                  mta_sendmail_parse_config, NULL, "file");
    snmpd_register_config_handler("sendmail_queue",
                                  mta_sendmail_parse_config, NULL,
                                  "directory");
    snmpd_register_config_handler("sendmail_index",
                                  mta_sendmail_parse_config, NULL,
                                  "integer");
    snmpd_register_config_handler("sendmail_stats_t",
                                  mta_sendmail_parse_config, NULL,
                                  "cachetime/sec");
    snmpd_register_config_handler("sendmail_queue_t",
                                  mta_sendmail_parse_config, NULL,
                                  "cachetime/sec");

    strcpy(sendmailcf_fn, "/etc/mail/sendmail.cf");
    if (read_sendmailcf(FALSE) == FALSE) {
        strcpy(sendmailcf_fn, "/etc/sendmail.cf");
        read_sendmailcf(FALSE);
    }

    if (sendmailst_fh == -1) {
        strcpy(sendmailst_fn, "/etc/mail/statistics");
        open_sendmailst(FALSE);
        if (sendmailst_fh == -1) {
            strcpy(sendmailst_fn, "/etc/mail/sendmail.st");
            open_sendmailst(FALSE);
        }
    }

}

 /**/
/** unsigned char *var_mtaEntry(struct variable *vp, oid *name, size_t *length, int exact, size_t *var_len, WriteMethod **write_method)
 *
 *  Description:
 *
 *    Called by the agent in order to get the values for the mtaTable.
 *
 *  Parameters:
 *
 *    see agent documentation
 *
 *  Returns:
 *
 *    see agent documentation
 *
 */
unsigned char  *
var_mtaEntry(struct variable *vp,
             oid * name,
             size_t * length,
             int exact, size_t * var_len, WriteMethod ** write_method)
{


    static long     long_ret;
    auto int        i;
    auto int        result;
    auto time_t     current_time;
    int             global_count = 0;
    int             global_size = 0;

    if (exact) {
        if (*length != vp->namelen + 1) {
            return NULL;
        }
        result =
            snmp_oid_compare(name, *length - 1, vp->name, vp->namelen);
        if (result != 0 || name[*length - 1] != applindex) {
            return NULL;
        }
    } else {
        if (*length <= vp->namelen) {
            result = -1;
        } else {
            result =
                snmp_oid_compare(name, *length - 1, vp->name, vp->namelen);
        }
        if (result > 0) {
            return NULL;
        }
        if (result == 0 && name[*length - 1] >= applindex) {
            return NULL;
        }
        if (result < 0) {
            memcpy(name, vp->name, (int) vp->namelen * (int) sizeof *name);
            *length = vp->namelen + 1;
        }
        name[vp->namelen] = applindex;
    }

    *write_method = (WriteMethod *) NULL;
    *var_len = sizeof(long);    /* default to 'long' results */

    if (vp->magic & NEEDS_STATS) {
        if (sendmailst_fh == -1)
            return NULL;
        current_time = time(NULL);
        if (current_time == (time_t) - 1
            || current_time > lastreadstats + stat_cache_time) {
            if (lseek(sendmailst_fh, 0, SEEK_SET) == -1) {
                snmp_log(LOG_ERR,
                         "mibII/mta_sendmail.c:var_mtaEntry: could not rewind to the beginning of file \"%s\"\n",
                         sendmailst_fn);
                return NULL;
            }
            if (read(sendmailst_fh, (void *) &stats, stats_size) !=
                stats_size) {
                snmp_log(LOG_ERR,
                         "mibII/mta_sendmail.c:var_mtaEntry: could not read from statistics file \"%s\"\n",
                         sendmailst_fn);
                return NULL;
            }
            if (current_time != (time_t) - 1) {
                lastreadstats = current_time;
            }
        }
    }

    if (vp->magic & NEEDS_DIR) {
        global_count = 0;
        global_size = 0;
        /*
         * count all queue group messages 
         */
        for (i = 0; i < nqgrps; i++) {
            count_queuegroup(&qgrps[i]);
            global_count += qgrps[i].count;
            global_size += qgrps[i].size;
        }
    }

    switch (vp->magic) {

    case MTARECEIVEDMESSAGES:

        long_ret = 0;
        for (i = 0; i < MAXMAILERS; i++) {
            long_ret += stat_nf[i];
        }
        return (unsigned char *) &long_ret;

    case MTASTOREDMESSAGES:

        long_ret = global_count;
        return (unsigned char *) &long_ret;

    case MTATRANSMITTEDMESSAGES:

        long_ret = 0;
        for (i = 0; i < MAXMAILERS; i++) {
            long_ret += stat_nt[i];
        }
        return (unsigned char *) &long_ret;

    case MTARECEIVEDVOLUME:

        long_ret = 0;
        for (i = 0; i < MAXMAILERS; i++) {
            long_ret += stat_bf[i];
        }
        return (unsigned char *) &long_ret;

    case MTASTOREDVOLUME:

        long_ret = global_size;
        return (unsigned char *) &long_ret;

    case MTATRANSMITTEDVOLUME:

        long_ret = 0;
        for (i = 0; i < MAXMAILERS; i++) {
            long_ret += stat_bt[i];
        }
        return (unsigned char *) &long_ret;

    default:
        snmp_log(LOG_ERR,
                 "mibII/mta_sendmail.c:mtaEntry: unknown magic value\n");
    }
    return NULL;
}

 /**/
/** unsigned char *var_mtaGroupEntry(struct variable *vp, oid *name, size_t *length, int exact, size_t *var_len, WriteMethod **write_method)
 *
 *  Description:
 *
 *    Called by the agent in order to get the values for the mtaGroupTable.
 *
 *  Parameters:
 *
 *    see agent documentation
 *
 *  Returns:
 *
 *    see agent documentation
 *
 */
unsigned char  *
var_mtaGroupEntry(struct variable *vp,
                  oid * name,
                  size_t * length,
                  int exact, size_t * var_len, WriteMethod ** write_method)
{
    static long     long_ret;
    auto long       row;
    auto int        result;
    auto time_t     current_time;


    if (exact) {
        if (*length != vp->namelen + 2) {
            return NULL;
        }
        result =
            snmp_oid_compare(name, *length - 2, vp->name, vp->namelen);
        if (result != 0 || name[*length - 2] != applindex
            || name[*length - 1] <= 0
            || name[*length - 1] > mailers + nqgrps) {
            return NULL;
        }
    } else {
        if (*length < vp->namelen) {
            result = -1;
        } else {
            result =
                snmp_oid_compare(name, vp->namelen, vp->name, vp->namelen);
        }

        if (result > 0) {
            /*
             * OID prefix too large 
             */
            return NULL;
        }

        if (result == 0) {
            /*
             * OID prefix matches exactly,... 
             */
            if (*length > vp->namelen && name[vp->namelen] > applindex) {
                /*
                 * ... but ApplIndex too large 
                 */
                return NULL;
            }
            if (*length > vp->namelen && name[vp->namelen] == applindex) {
                /*
                 * ... ApplIndex ok,... 
                 */
                if (*length > vp->namelen + 1
                    && name[vp->namelen + 1] >= 1) {
                    if (name[vp->namelen + 1] >= mailers + nqgrps) {
                        /*
                         * ... but mailernr too large 
                         */
                        return NULL;
                    } else {
                        name[vp->namelen + 1]++;
                    }
                } else {
                    name[vp->namelen + 1] = 1;
                }
            } else {
                name[vp->namelen] = applindex;
                name[vp->namelen + 1] = 1;
            }
        } else {                /* OID prefix too small */
            memcpy(name, vp->name, (int) vp->namelen * (int) sizeof *name);
            name[vp->namelen] = applindex;
            name[vp->namelen + 1] = 1;
        }
        *length = vp->namelen + 2;
    }

    *write_method = 0;
    *var_len = sizeof(long);    /* default to 'long' results */

    if (vp->magic & NEEDS_STATS) {
        if (sendmailst_fh == -1)
            return NULL;
        current_time = time(NULL);
        if (current_time == (time_t) - 1 ||
            current_time > lastreadstats + stat_cache_time) {
            if (lseek(sendmailst_fh, 0, SEEK_SET) == -1) {
                snmp_log(LOG_ERR,
                         "mibII/mta_sendmail.c:var_mtaGroupEntry: could not rewind to beginning of file \"%s\"\n",
                         sendmailst_fn);
                return NULL;
            }
            if (read(sendmailst_fh, (void *) &stats, stats_size) !=
                stats_size) {
                snmp_log(LOG_ERR,
                         "mibII/mta_sendmail.c:var_mtaGroupEntry: could not read statistics file \"%s\"\n",
                         sendmailst_fn);
                return NULL;
            }
            if (current_time != (time_t) - 1) {
                lastreadstats = current_time;
            }
        }
    }

    row = name[*length - 1] - 1;

    /*
     * if this is a mailer but we're asking for queue-group only info, 
     * bump there 
     */
    if (!exact && row < mailers && (vp->magic == MTAGROUPSTOREDMESSAGES ||
                                    vp->magic == MTAGROUPSTOREDVOLUME)) {
        row = mailers;
        name[*length - 1] = row + 1;
    }

    if (row < mailers) {
        switch (vp->magic) {
        case MTAGROUPRECEIVEDMESSAGES:
            long_ret = (long) stat_nf[row];
            return (unsigned char *) &long_ret;

        case MTAGROUPREJECTEDMESSAGES:
            if (stat_nr != NULL && stat_nd != NULL) {
                long_ret = (long) (stat_nr[row] + stat_nd[row]);        /* Number of rejected plus number of discarded messages */
                return (unsigned char *) &long_ret;
            } else {
                return NULL;
            }

        case MTAGROUPTRANSMITTEDMESSAGES:
            long_ret = (long) stat_nt[row];
            return (unsigned char *) &long_ret;

        case MTAGROUPRECEIVEDVOLUME:
            long_ret = (long) stat_bf[row];
            return (unsigned char *) &long_ret;

        case MTAGROUPTRANSMITTEDVOLUME:
            long_ret = (long) stat_bt[row];
            return (unsigned char *) &long_ret;

        case MTAGROUPNAME:
            *var_len = strlen(mailernames[row]);
            return (unsigned char *) (*var_len >
                                      0 ? mailernames[row] : NULL);

        case MTAGROUPHIERARCHY:
            long_ret = (long) -1;
            return (unsigned char *) &long_ret;

        default:
            return NULL;
        }
    } else {
        /*
         * this is the queue group part of the table 
         */
        row -= mailers;
        switch (vp->magic) {
        case MTAGROUPSTOREDMESSAGES:
            count_queuegroup(&qgrps[row]);
            long_ret = (long) qgrps[row].count;
            return (unsigned char *) &long_ret;

        case MTAGROUPSTOREDVOLUME:
            count_queuegroup(&qgrps[row]);
            long_ret = (long) qgrps[row].size;
            return (unsigned char *) &long_ret;

        case MTAGROUPNAME:
            *var_len = strlen(qgrps[row].name);
            return (unsigned char *) (*var_len >
                                      0 ? qgrps[row].name : NULL);

        case MTAGROUPHIERARCHY:
            long_ret = (long) -2;
            return (unsigned char *) &long_ret;

        default:
            return NULL;
        }

    }
    return NULL;
}

 /**/
