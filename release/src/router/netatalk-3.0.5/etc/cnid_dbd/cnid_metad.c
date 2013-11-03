/*
 * Copyright (C) Joerg Lenneis 2003
 * Copyright (C) Frank Lahm 2009, 2010
 *
 * All Rights Reserved.  See COPYING.
 */

/* 
   cnid_dbd metadaemon to start up cnid_dbd upon request from afpd.
   Here is how it works:
   
                       via TCP socket
   1.       afpd          ------->        cnid_metad

                   via UNIX domain socket
   2.   cnid_metad        ------->         cnid_dbd

                    passes afpd client fd
   3.   cnid_metad        ------->         cnid_dbd

   Result:
                       via TCP socket
   4.       afpd          ------->         cnid_dbd

   cnid_metad and cnid_dbd have been converted to non-blocking IO in 2010.
 */


#ifdef HAVE_CONFIG_H
#include "config.h"
#endif /* HAVE_CONFIG_H */

#include <unistd.h>
#undef __USE_GNU

#include <stdlib.h>
#include <sys/param.h>
#include <errno.h>
#include <string.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/resource.h>
#include <sys/wait.h>
#include <sys/uio.h>
#include <sys/un.h>
// #define _XPG4_2 1
#include <sys/socket.h>
#include <stdio.h>
#include <time.h>

#ifndef WEXITSTATUS
#define WEXITSTATUS(stat_val) ((unsigned)(stat_val) >> 8)
#endif /* ! WEXITSTATUS */
#ifndef WIFEXITED
#define WIFEXITED(stat_val) (((stat_val) & 255) == 0)
#endif /* ! WIFEXITED */
#ifndef WIFSTOPPED
#define WIFSTOPPED(status) (((status) & 0xff) == 0x7f)
#endif

#ifndef WIFSIGNALED
#define WIFSIGNALED(status) (!WIFSTOPPED(status) && !WIFEXITED(status))
#endif
#ifndef WTERMSIG
#define WTERMSIG(status)      ((status) & 0x7f)
#endif

/* functions for username and group */
#include <pwd.h>
#include <grp.h>

/* FIXME */
#ifdef linux
#ifndef USE_SETRESUID
#define USE_SETRESUID 1
#define SWITCH_TO_GID(gid)  ((setresgid(gid,gid,gid) < 0 || setgid(gid) < 0) ? -1 : 0)
#define SWITCH_TO_UID(uid)  ((setresuid(uid,uid,uid) < 0 || setuid(uid) < 0) ? -1 : 0)
#endif  /* USE_SETRESUID */
#else   /* ! linux */
#ifndef USE_SETEUID
#define USE_SETEUID 1
#define SWITCH_TO_GID(gid)  ((setegid(gid) < 0 || setgid(gid) < 0) ? -1 : 0)
#define SWITCH_TO_UID(uid)  ((setuid(uid) < 0 || seteuid(uid) < 0 || setuid(uid) < 0) ? -1 : 0)
#endif  /* USE_SETEUID */
#endif  /* linux */

#include <atalk/util.h>
#include <atalk/logger.h>
#include <atalk/cnid_dbd_private.h>
#include <atalk/paths.h>
#include <atalk/compat.h>
#include <atalk/errchk.h>
#include <atalk/bstrlib.h>
#include <atalk/bstradd.h>
#include <atalk/netatalk_conf.h>
#include <atalk/volume.h>

#include "usockfd.h"

#define DBHOME        ".AppleDB"
#define DBHOMELEN    8

static int srvfd;
static int rqstfd;
static volatile sig_atomic_t sigchild = 0;
static uint maxvol;

#define MAXSPAWN   3                   /* Max times respawned in.. */
#define TESTTIME   10                  /* this much seconds apfd client tries to  *
                                        * to reconnect every 5 secondes, catch it */
#define MAXVOLS    4096
#define DEFAULTHOST  "localhost"
#define DEFAULTPORT  "4700"

//Change path by Edison in 20130923
#define T_PATH_CNID_DBD	"/usr/sbin/cnid_dbd"

struct server {
    char *v_path;
    pid_t pid;
    time_t tm;                    /* When respawned last */
    unsigned int count;           /* Times respawned in the last TESTTIME secondes */
    int control_fd;               /* file descriptor to child cnid_dbd process */
};

static struct server srv[MAXVOLS];

static void daemon_exit(int i)
{
    exit(i);
}

/* ------------------ */
static void sig_handler(int sig)
{
    switch( sig ) {
    case SIGTERM:
    case SIGQUIT:
        LOG(log_note, logtype_afpd, "shutting down on %s",
            sig == SIGTERM ? "SIGTERM" : "SIGQUIT");
        break;
    default :
        LOG(log_error, logtype_afpd, "unexpected signal: %d", sig);
    }
    daemon_exit(0);
}

static struct server *test_usockfn(const char *path)
{
    int i;

    for (i = 0; i < maxvol; i++) {
        if (srv[i].v_path && STRCMP(path, ==, srv[i].v_path))
            return &srv[i];
    }

    return NULL;
}

/* -------------------- */
static int maybe_start_dbd(const AFPObj *obj, char *dbdpn, const char *volpath)
{
    pid_t pid;
    struct server *up;
    int sv[2];
    int i;
    time_t t;
    char buf1[8];
    char buf2[8];

    LOG(log_debug, logtype_cnid, "maybe_start_dbd(\"%s\"): BEGIN", volpath);

    up = test_usockfn(volpath);
    if (up && up->pid) {
        /* we already have a process, send our fd */
        LOG(log_debug, logtype_cnid, "maybe_start_dbd: cnid_dbd[%d] already serving", up->pid);
        if (send_fd(up->control_fd, rqstfd) < 0) {
            /* FIXME */
            return -1;
        }
        return 0;
    }

    LOG(log_debug, logtype_cnid, "maybe_start_dbd: no cnid_dbd serving yet");

    time(&t);
    if (!up) {
        /* find an empty slot (i < maxvol) or the first free slot (i == maxvol)*/
        for (i = 0; i <= maxvol && i < MAXVOLS; i++) {
            if (srv[i].v_path == NULL) {
                up = &srv[i];
                if ((up->v_path = strdup(volpath)) == NULL)
                    return -1;
                up->tm = t;
                up->count = 0;
                if (i == maxvol)
                    maxvol++;
                break;
            }
        }
        if (!up) {
            LOG(log_error, logtype_cnid, "no free slot for cnid_dbd child. Configured maximum: %d. Do you have so many volumes?", MAXVOLS);
            return -1;
        }
    } else {
        /* we have a slot but no process */
        if (up->count > 0) {
            /* check for respawn too fast */
            if (t < (up->tm + TESTTIME)) {
                /* We're in the respawn time window */
                if (up->count > MAXSPAWN) {
                    /* ...and already tried to fork too often */
                    LOG(log_maxdebug, logtype_cnid, "maybe_start_dbd: respawning too fast");
                    return -1; /* just exit, dont sleep, because we might have work to do for another client  */
                }
            } else {
                /* out of respawn too fast windows reset the count */
                LOG(log_info, logtype_cnid, "maybe_start_dbd: respawn window ended");
                up->count = 0;
            }
        }
        up->count++;
        up->tm = t;
        LOG(log_maxdebug, logtype_cnid, "maybe_start_dbd: respawn count: %u", up->count);
        if (up->count > MAXSPAWN) {
            /* We spawned too fast. From now until the first time we tried + TESTTIME seconds
               we will just return -1 above */
            LOG(log_info, logtype_cnid, "maybe_start_dbd: reached MAXSPAWN threshhold");
       }
    }

    /* 
       Create socketpair for comm between parent and child.
       We use it to pass fds from connecting afpd processes to our
       cnid_dbd child via fd passing.
    */
    if (socketpair(PF_UNIX, SOCK_STREAM, 0, sv) < 0) {
        LOG(log_error, logtype_cnid, "error in socketpair: %s", strerror(errno));
        return -1;
    }

    if ((pid = fork()) < 0) {
        LOG(log_error, logtype_cnid, "error in fork: %s", strerror(errno));
        return -1;
    }
    if (pid == 0) {
        int ret;
        /*
         *  Child. Close descriptors and start the daemon. If it fails
         *  just log it. The client process will fail connecting
         *  afterwards anyway.
         */

        close(srvfd);
        close(sv[0]);

        for (i = 0; i < MAXVOLS; i++) {
            if (srv[i].pid && up != &srv[i]) {
                close(srv[i].control_fd);
            }
        }

        sprintf(buf1, "%i", sv[1]);
        sprintf(buf2, "%i", rqstfd);

        if (up->count == MAXSPAWN) {
            /* there's a pb with the db inform child, it will delete the db */
            LOG(log_warning, logtype_cnid,
                "Multiple attempts to start CNID db daemon for \"%s\" failed, wiping the slate clean...",
                up->v_path);
            ret = execlp(dbdpn, dbdpn, "-F", obj->options.configfile, "-p", volpath, "-t", buf1, "-l", buf2, "-d", NULL);
        } else {
            ret = execlp(dbdpn, dbdpn, "-F", obj->options.configfile, "-p", volpath, "-t", buf1, "-l", buf2, NULL);
        }
        /* Yikes! We're still here, so exec failed... */
        LOG(log_error, logtype_cnid, "Fatal error in exec: %s", strerror(errno));
        daemon_exit(0);
    }
    /*
     *  Parent.
     */
    up->pid = pid;
    close(sv[1]);
    up->control_fd = sv[0];
    return 0;
}

/* ------------------ */
static int set_dbdir(const char *dbdir, const char *vpath)
{
    EC_INIT;
    struct stat st;
    bstring oldpath, newpath;
    char *cmd_argv[4];

    LOG(log_debug, logtype_cnid, "set_dbdir: volume: %s, db path: %s", vpath, dbdir);

    EC_NULL_LOG( oldpath = bformat("%s/%s/", vpath, DBHOME) );
    EC_NULL_LOG( newpath = bformat("%s/%s/", dbdir, DBHOME) );

    if (lstat(dbdir, &st) < 0 && mkdir(dbdir, 0755) < 0) {
        LOG(log_error, logtype_cnid, "set_dbdir: mkdir failed for %s", dbdir);
        EC_FAIL;
    }

    if (lstat(cfrombstr(oldpath), &st) == 0 && lstat(cfrombstr(newpath), &st) != 0 && errno == ENOENT) {
        /* There's an .AppleDB in the volume root, we move it */
        cmd_argv[0] = "mv";
        cmd_argv[1] = bdata(oldpath);
        cmd_argv[2] = (char *)dbdir;
        cmd_argv[3] = NULL;
        if (run_cmd("mv", cmd_argv) != 0) {
            LOG(log_error, logtype_cnid, "set_dbdir: moving CNID db from \"%s\" to \"%s\" failed",
                bdata(oldpath), dbdir);
            EC_FAIL;
        }

    }

    if (lstat(cfrombstr(newpath), &st) < 0 && mkdir(cfrombstr(newpath), 0755 ) < 0) {
        LOG(log_error, logtype_cnid, "set_dbdir: mkdir failed for %s", bdata(newpath));
        EC_FAIL;
    }

EC_CLEANUP:
    bdestroy(oldpath);
    bdestroy(newpath);
    EC_EXIT;
}

/* ------------------ */
static void catch_child(int sig _U_) 
{
    sigchild = 1;
}

/* ----------------------- */
static void set_signal(void)
{
    struct sigaction sv;
    sigset_t set;

    memset(&sv, 0, sizeof(sv));

    /* Catch SIGCHLD */
    sv.sa_handler = catch_child;
    sv.sa_flags = SA_NOCLDSTOP;
    sigemptyset(&sv.sa_mask);
    if (sigaction(SIGCHLD, &sv, NULL) < 0) {
        LOG(log_error, logtype_cnid, "cnid_metad: sigaction: %s", strerror(errno));
        daemon_exit(EXITERR_SYS);
    }

    /* Catch SIGTERM and SIGQUIT */
    sv.sa_handler = sig_handler;
    sigfillset(&sv.sa_mask );
    if (sigaction(SIGTERM, &sv, NULL ) < 0 ) {
        LOG(log_error, logtype_afpd, "sigaction: %s", strerror(errno) );
        daemon_exit(EXITERR_SYS);
    }
    if (sigaction(SIGQUIT, &sv, NULL ) < 0 ) {
        LOG(log_error, logtype_afpd, "sigaction: %s", strerror(errno) );
        daemon_exit(EXITERR_SYS);
    }

    /* Ignore the rest */
    sv.sa_handler = SIG_IGN;
    sigemptyset(&sv.sa_mask );
    if (sigaction(SIGALRM, &sv, NULL ) < 0 ) {
        LOG(log_error, logtype_afpd, "sigaction: %s", strerror(errno) );
        daemon_exit(EXITERR_SYS);
    }
    sv.sa_handler = SIG_IGN;
    sigemptyset(&sv.sa_mask );
    if (sigaction(SIGHUP, &sv, NULL ) < 0 ) {
        LOG(log_error, logtype_afpd, "sigaction: %s", strerror(errno) );
        daemon_exit(EXITERR_SYS);
    }
    sv.sa_handler = SIG_IGN;
    sigemptyset(&sv.sa_mask );
    if (sigaction(SIGUSR1, &sv, NULL ) < 0 ) {
        LOG(log_error, logtype_afpd, "sigaction: %s", strerror(errno) );
        daemon_exit(EXITERR_SYS);
    }
    sv.sa_handler = SIG_IGN;
    sigemptyset(&sv.sa_mask );
    if (sigaction(SIGUSR2, &sv, NULL ) < 0 ) {
        LOG(log_error, logtype_afpd, "sigaction: %s", strerror(errno) );
        daemon_exit(EXITERR_SYS);
    }
    sv.sa_handler = SIG_IGN;
    sigemptyset(&sv.sa_mask );
    if (sigaction(SIGPIPE, &sv, NULL ) < 0 ) {
        LOG(log_error, logtype_afpd, "sigaction: %s", strerror(errno) );
        daemon_exit(EXITERR_SYS);
    }

    /* block everywhere but in pselect */
    sigemptyset(&set);
    sigaddset(&set, SIGCHLD);
    sigprocmask(SIG_SETMASK, &set, NULL);
}

static int setlimits(void)
{
    struct rlimit rlim;

    if (getrlimit(RLIMIT_NOFILE, &rlim) != 0) {
        LOG(log_error, logtype_afpd, "setlimits: %s", strerror(errno));
        exit(1);
    }
    if (rlim.rlim_cur != RLIM_INFINITY && rlim.rlim_cur < 65535) {
        rlim.rlim_cur = 65535;
        if (rlim.rlim_max != RLIM_INFINITY && rlim.rlim_max < 65535)
            rlim.rlim_max = 65535;
        if (setrlimit(RLIMIT_NOFILE, &rlim) != 0) {
            LOG(log_error, logtype_afpd, "setlimits: %s", strerror(errno));
            exit(1);
        }
    }
    return 0;
}

/* ------------------ */
int main(int argc, char *argv[])
{
    char  volpath[MAXPATHLEN + 1];
    int   len, actual_len;
    pid_t pid;
    int   status;
//    char  *dbdpn = _PATH_CNID_DBD;	//Edison marked 20130923
    char  *dbdpn = T_PATH_CNID_DBD;
    char  *host;
    char  *port;
    int    i;
    int    cc;
    uid_t  uid = 0;
    gid_t  gid = 0;
    int    debug = 0;
    int    ret;
    sigset_t set;
    AFPObj obj = { 0 };
    struct vol *vol;

    while (( cc = getopt( argc, argv, "dF:vV")) != -1 ) {
        switch (cc) {
        case 'd':
            debug = 1;
            break;
        case 'F':
            obj.cmdlineconfigfile = strdup(optarg);
            break;
        case 'v':
        case 'V':
            printf("cnid_metad (Netatalk %s)\n", VERSION);
            return -1;
        default:
            printf("cnid_metad [-dvV] [-F alternate configfile ]\n");
            return -1;
        }
    }

    if (!debug && daemonize(0, 0) != 0)
        exit(EXITERR_SYS);

    if (afp_config_parse(&obj, "cnid_metad") != 0)
        daemon_exit(1);

    if (load_volumes(&obj) != 0)
        daemon_exit(1);

    (void)setlimits();

    host = atalk_iniparser_getstrdup(obj.iniconfig, INISEC_GLOBAL, "cnid listen", "localhost:4700");
    if ((port = strrchr(host, ':')))
        *port++ = 0;
    else
        port = DEFAULTPORT;
    if ((srvfd = tsockfd_create(host, port, 10)) < 0)
        daemon_exit(1);

    LOG(log_note, logtype_afpd, "CNID Server listening on %s:%s", host, port);

    /* switch uid/gid */
    if (uid || gid) {
        LOG(log_debug, logtype_cnid, "Setting uid/gid to %i/%i", uid, gid);
        if (gid) {
            if (SWITCH_TO_GID(gid) < 0) {
                LOG(log_info, logtype_cnid, "unable to switch to group %d", gid);
                daemon_exit(1);
            }
        }
        if (uid) {
            if (SWITCH_TO_UID(uid) < 0) {
                LOG(log_info, logtype_cnid, "unable to switch to user %d", uid);
                daemon_exit(1);
            }
        }
    }

    set_signal();

    sigemptyset(&set);
    sigprocmask(SIG_SETMASK, NULL, &set);
    sigdelset(&set, SIGCHLD);

    while (1) {
        rqstfd = usockfd_check(srvfd, &set);
        /* Collect zombie processes and log what happened to them */
        if (sigchild) while ((pid = waitpid(-1, &status, WNOHANG)) > 0) {
            for (i = 0; i < maxvol; i++) {
                if (srv[i].pid == pid) {
                    srv[i].pid = 0;
                    close(srv[i].control_fd);
                    break;
                }
            }
            if (WIFEXITED(status)) {
                LOG(log_info, logtype_cnid, "cnid_dbd[%i] exited with exit code %i",
                    pid, WEXITSTATUS(status));
            } else {
                /* cnid_dbd did a clean exit probably on idle timeout, reset bookkeeping */
                srv[i].tm = 0;
                srv[i].count = 0;
            }
            if (WIFSIGNALED(status)) {
                LOG(log_info, logtype_cnid, "cnid_dbd[%i] got signal %i",
                    pid, WTERMSIG(status));
            }
            sigchild = 0;
        }
        if (rqstfd <= 0)
            continue;

        ret = readt(rqstfd, &len, sizeof(int), 1, 4);

        if (!ret) {
            /* already close */
            goto loop_end;
        }
        else if (ret < 0) {
            LOG(log_severe, logtype_cnid, "error read: %s", strerror(errno));
            goto loop_end;
        }
        else if (ret != sizeof(int)) {
            LOG(log_error, logtype_cnid, "short read: got %d", ret);
            goto loop_end;
        }
        /*
         *  checks for buffer overruns. The client libatalk side does it too
         *  before handing the dir path over but who trusts clients?
         */
        if (!len || len +DBHOMELEN +2 > MAXPATHLEN) {
            LOG(log_error, logtype_cnid, "wrong len parameter: %d", len);
            goto loop_end;
        }

        actual_len = readt(rqstfd, volpath, len, 1, 5);
        if (actual_len < 0) {
            LOG(log_severe, logtype_cnid, "Read(2) error : %s", strerror(errno));
            goto loop_end;
        }
        if (actual_len != len) {
            LOG(log_error, logtype_cnid, "error/short read (dir): %s", strerror(errno));
            goto loop_end;
        }
        volpath[len] = '\0';

        LOG(log_debug, logtype_cnid, "main: request for volume: %s", volpath);

        if (load_volumes(&obj) != 0) {
            LOG(log_severe, logtype_cnid, "main: error reloading config");
            goto loop_end;
        }

        if ((vol = getvolbypath(&obj, volpath)) == NULL) {
            LOG(log_severe, logtype_cnid, "main: no volume for path \"%s\"", volpath);
            goto loop_end;
        }

        LOG(log_maxdebug, logtype_cnid, "main: dbpath: %s", vol->v_dbpath);

        if (set_dbdir(vol->v_dbpath, volpath) < 0) {
            goto loop_end;
        }

        maybe_start_dbd(&obj, dbdpn, vol->v_path);
    loop_end:
        close(rqstfd);
    }
}
