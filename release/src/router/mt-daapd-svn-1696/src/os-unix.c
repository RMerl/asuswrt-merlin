/*
 * $Id: os-unix.c 1666 2007-09-16 02:03:25Z rpedde $
 * Abstracts os interface
 *
 * Copyright (c) 2006 Ron Pedde (rpedde@users.sourceforge.net)
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <dlfcn.h>
#include <errno.h>
#include <fcntl.h>
#include <grp.h>
#include <limits.h>
#include <pthread.h>
#include <pwd.h>
#include <signal.h>
#include <stdarg.h>
#include <stdlib.h>
#include <stdio.h>
#include <syslog.h>
#include <string.h>

#ifdef HAVE_UNISTD_H
# include <unistd.h>
#endif
#include <sys/types.h>
#include <sys/stat.h>
#ifdef HAVE_SYS_WAIT_H
# include <sys/wait.h>
#endif
#include <sys/time.h>
#include <sys/resource.h>

#ifdef MAC
#include "CoreFoundation/CoreFoundation.h"
#endif

#include "daapd.h"
#include "conf.h"
#include "err.h"
#include "os.h"

/** You say po-tay-to, I say po-tat-o */
#ifndef SIGCLD
# define SIGCLD SIGCHLD
#endif

/** Where to dump the pidfile */
#ifndef PIDFILE
#define PIDFILE "/var/run/mt-daapd.pid"
#endif

/* Forwards */
static int _os_daemon_start(void);
static int _os_start_signal_handler(void);

/* Globals */
char *_os_pidfile = PIDFILE;

/**
 * this initializes the platform... sets up signal handlers, forks to the
 * background, etc
 *
 * @param foreground whether to run in fg or fork to bg
 * @returns TRUE on success, FALSE otherwise
 */
int os_init(int foreground, char *runas) {
    int pid_fd;
    FILE *pid_fp=NULL;

    /* block signals and set up the signal handling thread */
    DPRINTF(E_LOG,L_MAIN,"Starting signal handler\n");
    if(_os_start_signal_handler()) {
        DPRINTF(E_FATAL,L_MAIN,"Error starting signal handler %s\n",strerror(errno));
    }

    /* open the pidfile, so it can be written once we detach */
    if(!foreground) {
        if(-1 == (pid_fd = open(_os_pidfile,O_CREAT | O_WRONLY | O_TRUNC, 0644))) {
            DPRINTF(E_LOG,L_MAIN,"Error opening pidfile (%s): %s\n",
                    _os_pidfile,strerror(errno));
        } else {
            if(0 == (pid_fp = fdopen(pid_fd, "w")))
                DPRINTF(E_LOG,L_MAIN,"fdopen: %s\n",strerror(errno));
        }
        _os_daemon_start();
        fprintf(pid_fp,"%d\n",getpid());
        fclose(pid_fp);
        DPRINTF(E_DBG,L_MAIN,"Pid: %d\n",getpid());
    }

    // Drop privs here
    if(os_drop_privs(runas)) {
        DPRINTF(E_FATAL,L_MAIN,"Error in drop_privs: %s\n",strerror(errno));
    }

    return TRUE;
}

/**
 * do any deinitialization necessary for the platform
 */
void os_deinit(void) {
}

/**
 * start syslogging
 *
 * @returns TRUE on success, FALSE otherwise
 */
int os_opensyslog(void) {
    openlog(PACKAGE,LOG_PID,LOG_DAEMON);
    return TRUE;
}


/**
 * stop syslogging
 *
 * @returns TRUE on success, FALSE otherwise
 */
int os_closesyslog(void) {
    closelog();
    return TRUE;
}

/**
 * log a syslog message
 *
 * @param level log level (1-9: 1=fatal, 9=debug)
 * @param msg message to log to the syslog
 * @returns TRUE on success, FALSE otherwise
 */
int os_syslog(int level, char *msg) {
    int priority;

    switch(level) {
    case 0:
    case 1:
        priority = LOG_ALERT;
        break;
    case 2:
    case 3:
    case 4:
        priority = LOG_NOTICE;
        break;
    case 5:
    case 6:
    case 7:
    case 8:
        priority = LOG_INFO;
        break;

    case 9:
    default:
        priority = LOG_DEBUG;
        break;
    }

    syslog(priority,"%s",msg);
    return TRUE;
}


/**
 * os-specific chown
 *
 *
 */
extern int os_chown(char *path, char *user) {
    struct passwd *pw=NULL;

    DPRINTF(E_DBG,L_MISC,"Chowning %s to %s\n",path,user);

    /* drop privs */
    if(getuid() == (uid_t)0) {
        if(atoi(user)) {
            pw=getpwuid((uid_t)atoi(user)); /* doh! */
        } else {
            pw=getpwnam(user);
        }

        if(pw) {
            if(initgroups(user,pw->pw_gid) != 0 ||
               chown(path, pw->pw_uid, pw->pw_gid) != 0) {
                DPRINTF(E_LOG,L_MISC,"Couldn't chown %s, gid=%d, uid=%d\n",
                        user,pw->pw_gid, pw->pw_uid);
                return FALSE;
            }
        } else {
            DPRINTF(E_LOG,L_MISC,"Couldn't lookup user %s for chown\n",user);
            return FALSE;
        }
    }

    DPRINTF(E_DBG,L_MISC,"Success!\n");
    return TRUE;
}

/**
 * find the old service pid and send it a SIGTERM
 */
int os_signal_server(int what) {
    FILE *pid_fp;
    int pid;
    int result = TRUE;
    int signal = 0;

    if(NULL == (pid_fp = fopen(_os_pidfile, "r"))) {
        DPRINTF(E_LOG,L_MAIN,"fdopen: %s\n",strerror(errno));
        return FALSE;
    }

    if(fscanf(pid_fp,"%d\n",&pid)) {
        kill(pid,SIGTERM);
    } else {
        DPRINTF(E_LOG,L_MAIN,"os_service_kill: can't get pid from pidfile\n");
        result = FALSE;
    }

    fclose(pid_fp);

    switch(what) {
    case S_SCAN:
        signal=SIGUSR1;
        break;
    case S_FULL:
        signal=SIGUSR2;
        break;
    case S_STOP:
        signal=SIGTERM;
    default:
        break;

    }

    if(kill(pid,signal)) {
        perror("kill");
        result = FALSE;
    }
    return result;
}


/**
 * Fork and exit.  Stolen pretty much straight from Stevens.
 *
 * @returns 0 on success, -1 with errno set on error
 */
int _os_daemon_start(void) {
    int childpid, fd;

    signal(SIGTTOU, SIG_IGN);
    signal(SIGTTIN, SIG_IGN);
    signal(SIGTSTP, SIG_IGN);

    // Fork and exit
    if ((childpid = fork()) < 0) {
        fprintf(stderr, "Can't fork!\n");
        return -1;
    } else if (childpid > 0)
        exit(0);

#ifdef SETPGRP_VOID
    setpgrp();
#else
    setpgrp(0,0);
#endif

#ifdef TIOCNOTTY
    if ((fd = open("/dev/tty", O_RDWR)) >= 0) {
        ioctl(fd, TIOCNOTTY, (char *) NULL);
        close(fd);
    }
#endif

    if((fd = open("/dev/null", O_RDWR, 0)) != -1) {
        dup2(fd, STDIN_FILENO);
        dup2(fd, STDOUT_FILENO);
        dup2(fd, STDERR_FILENO);
        if (fd > 2)
            close(fd);
    }

    /*
    for (fd = 0; fd < FOPEN_MAX; fd++)
        close(fd);
    */

    errno = 0;

    chdir("/");
    umask(0);

    return 0;
}


/**
 * Drop privs.  This allows mt-daapd to run as a non-privileged user.
 * Hopefully this will limit the damage it could do if exploited
 * remotely.  Note that only the user need be specified.  GID
 * is set to the primary group of the user.
 *
 * \param user user to run as (or UID)
 */
int os_drop_privs(char *user) {
    int err;
    struct passwd *pw=NULL;

    /* drop privs */
    if(getuid() == (uid_t)0) {
        if(atoi(user)) {
            pw=getpwuid((uid_t)atoi(user)); /* doh! */
        } else {
            pw=getpwnam(user);
        }

        if(pw) {
            if(initgroups(user,pw->pw_gid) != 0 ||
               setgid(pw->pw_gid) != 0 ||
               setuid(pw->pw_uid) != 0) {
                err=errno;
                fprintf(stderr,"Couldn't change to %s, gid=%d, uid=%d\n",
                        user,pw->pw_gid, pw->pw_uid);
                errno=err;
                return -1;
            }
        } else {
            err=errno;
            fprintf(stderr,"Couldn't lookup user %s\n",user);
            errno=err;
            return -1;
        }
    }

    return 0;
}

/**
 * wait for specified time, while setting signal flags
 * (if necessary)
 */
void os_wait(int seconds) {
    sigset_t intmask;
    int status;
    struct sigaction sa_ign;
    struct sigaction sa_dfl;
    sleep(seconds);

    sigpending(&intmask);

    sa_ign.sa_handler=SIG_IGN;
    sa_ign.sa_flags=0;
    sigemptyset(&sa_ign.sa_mask);

    sa_dfl.sa_handler=SIG_DFL;
    sa_dfl.sa_flags=0;
    sigemptyset(&sa_dfl.sa_mask);


    if(sigismember(&intmask, SIGCLD)) {
        DPRINTF(E_LOG,L_MAIN,"Got CLD signal.  Reaping\n");
        while (wait3(&status, WNOHANG, NULL) > 0) {};

        sigaction(SIGCLD,&sa_ign,NULL);
        sigaction(SIGCLD,&sa_dfl,NULL);
    }

    if((sigismember(&intmask, SIGTERM)) ||
       (sigismember(&intmask, SIGINT))) {
        DPRINTF(E_LOG,L_MAIN,"Got shutdown signal.\n");
        config.stop=1;

        sigaction(SIGTERM,&sa_ign,NULL);
        sigaction(SIGTERM,&sa_dfl,NULL);

        sigaction(SIGINT,&sa_ign,NULL);
        sigaction(SIGINT,&sa_dfl,NULL);
    }

    if(sigismember(&intmask, SIGHUP)) {
        DPRINTF(E_LOG,L_MAIN,"Got HUP signal.\n");
        /* if we can't reload, it keeps the old config file,
         * so no real damage */
        conf_reload();
        err_reopen();

        config.reload=1;

        sigaction(SIGHUP,&sa_ign,NULL);
        sigaction(SIGHUP,&sa_dfl,NULL);
    }
}

/**
 * Wait for signals and flag the main process.  This is
 * a thread handler for the signal processing thread.  It
 * does absolutely nothing except wait for signals.  The rest
 * of the threads are running with signals blocked, so this thread
 * is guaranteed to catch all the signals.  It sets flags in
 * the config structure that the main thread looks for.  Specifically,
 * the stop flag (from an INT signal), and the reload flag (from HUP).
 * \param arg NULL, but required of a thread procedure
 */
/**
 * Block signals, then start the signal handler.  The
 * signal handler started by spawning a new thread on
 * signal_handler().
 *
 * \returns 0 on success, -1 on failure with errno set
 */
int _os_start_signal_handler(void) {
    sigset_t set;

    if((sigemptyset(&set) == -1) ||
       (sigaddset(&set,SIGINT) == -1) ||
       (sigaddset(&set,SIGHUP) == -1) ||
       (sigaddset(&set,SIGCLD) == -1) ||
       (sigaddset(&set,SIGTERM) == -1) ||
       (sigaddset(&set,SIGPIPE) == -1) ||
       (pthread_sigmask(SIG_BLOCK, &set, NULL) == -1)) {
        DPRINTF(E_LOG,L_MAIN,"Error setting signal set\n");
        return -1;
    }

    return 0;
}

/**
 * set the pidfile to a non-default value
 *
 * @param file file to use as pidfile
 */
 void os_set_pidfile(char *file) {
    _os_pidfile = file;
 }

/**
 * load a shared library
 *
 * @param
 */
void *os_loadlib(char **pe, char *path) {
    void *retval;

    if(!(retval = dlopen(path,RTLD_NOW)))
        *pe = strdup(dlerror());

    return retval;
}

void *os_libfunc(char **pe, void *handle, char *function) {
    void *retval;

    if((!(retval = dlsym(handle,function))) && (pe))
        *pe = strdup(dlerror());

    return retval;
}

int os_unload(void *handle) {
    return dlclose(handle);
}

/**
 * Determine if an address is local or not
 *
 * @param hostaddr the address to test for locality
 */
int os_islocaladdr(char *hostaddr) {
    /* how can we check interfaces without something like libnet? */

    if(strncmp(hostaddr,"127.",4) == 0)
        return TRUE;

    return FALSE;
}

#ifdef MAC
char *os_apppath(char *parm) {
    CFURLRef pluginRef = CFBundleCopyBundleURL(CFBundleGetMainBundle());
    CFStringRef macPath = CFURLCopyFileSystemPath(pluginRef,
                                                  kCFURLPOSIXPathStyle);
    const char *pathPtr = CFStringGetCStringPtr(macPath,
                                                CFStringGetSystemEncoding());

    return strdup(pathPtr);
}
#else
char *os_apppath(char *parm) {
    char path[PATH_MAX];

    realpath(parm,path);
    if(strrchr(path,'/')) {
        *strrchr(path,'/') = '\0';
    }

    return strdup(path);
}
#endif

/**
 * stat wrapper
 */
int os_stat(const char *path, struct stat *sb) {
    return stat(path, sb);
}

int os_lstat(const char *path, struct stat *sb) {
    return lstat(path, sb);
}
