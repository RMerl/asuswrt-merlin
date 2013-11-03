/* 
   Copyright (c) 2012 Frank Lahm <franklahm@gmail.com>
   
   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.
 
   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.
*/
#ifdef HAVE_CONFIG_H
#include "config.h"
#endif /* HAVE_CONFIG_H */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <sys/param.h>
#include <sys/uio.h>
#include <sys/time.h>
#include <sys/socket.h>
#include <sys/poll.h>
#include <errno.h>
#include <sys/wait.h>
#include <sys/resource.h>

#include <atalk/logger.h>
#include <atalk/adouble.h>
#include <atalk/compat.h>
#include <atalk/dsi.h>
#include <atalk/afp.h>
#include <atalk/paths.h>
#include <atalk/util.h>
#include <atalk/server_child.h>
#include <atalk/server_ipc.h>
#include <atalk/errchk.h>
#include <atalk/globals.h>
#include <atalk/netatalk_conf.h>

#include <event2/event.h>

/* how many seconds we wait to shutdown from SIGTERM before we send SIGKILL */
#define KILL_GRACETIME 5

/* forward declaration */
static pid_t run_process(const char *path, ...);
static void kill_childs(int sig, ...);

/* static variables */
static AFPObj obj;
static pid_t afpd_pid = -1,  cnid_metad_pid = -1;
static uint afpd_restarts, cnid_metad_restarts;
static struct event_base *base;
struct event *sigterm_ev, *sigquit_ev, *sigchld_ev, *timer_ev;
static int in_shutdown;

/******************************************************************
 * libevent helper functions
 ******************************************************************/

/* libevent logging callback */
static void libevent_logmsg_cb(int severity, const char *msg)
{
    switch (severity) {
    case _EVENT_LOG_DEBUG:
        LOG(log_debug, logtype_default, "libevent: %s", msg);
        break;
    case _EVENT_LOG_MSG:
        LOG(log_info, logtype_default, "libevent: %s", msg);
        break;
    case _EVENT_LOG_WARN:
        LOG(log_warning, logtype_default, "libevent: %s", msg);
        break;
    case _EVENT_LOG_ERR:
        LOG(log_error, logtype_default, "libevent: %s", msg);
        break;
    default:
        LOG(log_error, logtype_default, "libevent: %s", msg);
        break; /* never reached */
    }
}

/******************************************************************
 * libevent event callbacks
 ******************************************************************/

/* SIGTERM callback */
static void sigterm_cb(evutil_socket_t fd, short what, void *arg)
{
    sigset_t sigs;
    struct timeval tv;

    LOG(log_info, logtype_afpd, "Exiting on SIGTERM");

    if (in_shutdown)
        return;
    in_shutdown = 1;

    /* block any signal but SIGCHLD */
    sigfillset(&sigs);
    sigdelset(&sigs, SIGCHLD);
    sigprocmask(SIG_SETMASK, &sigs, NULL);

    /* add 10 sec timeout timer, remove all events but SIGCHLD */
    tv.tv_sec = KILL_GRACETIME;
    tv.tv_usec = 0;
    event_base_loopexit(base, &tv);
    event_del(sigterm_ev);
    event_del(sigquit_ev);
    event_del(timer_ev);

    kill_childs(SIGTERM, &afpd_pid, &cnid_metad_pid, NULL);
}

/* SIGQUIT callback */
static void sigquit_cb(evutil_socket_t fd, short what, void *arg)
{
    LOG(log_note, logtype_afpd, "Exiting on SIGQUIT");
    kill_childs(SIGQUIT, &afpd_pid, &cnid_metad_pid, NULL);
}

/* SIGQUIT callback */
static void sighup_cb(evutil_socket_t fd, short what, void *arg)
{
    LOG(log_note, logtype_afpd, "Received SIGHUP, sending all processes signal to reload config");
    kill_childs(SIGHUP, &afpd_pid, &cnid_metad_pid, NULL);
}

/* SIGCHLD callback */
static void sigchld_cb(evutil_socket_t fd, short what, void *arg)
{
    int status;
    pid_t pid;

    LOG(log_debug, logtype_afpd, "Got SIGCHLD event");
  
    while ((pid = waitpid(-1, &status, WNOHANG)) > 0) {
        if (WIFEXITED(status)) {
            if (WEXITSTATUS(status))
                LOG(log_info, logtype_afpd, "child[%d]: exited %d", pid, WEXITSTATUS(status));
            else
                LOG(log_info, logtype_afpd, "child[%d]: done", pid);
        } else {
            if (WIFSIGNALED(status))
                LOG(log_info, logtype_afpd, "child[%d]: killed by signal %d", pid, WTERMSIG(status));
            else
                LOG(log_info, logtype_afpd, "child[%d]: died", pid);
        }

        if (pid == afpd_pid)
            afpd_pid = -1;
        else if (pid == cnid_metad_pid)
            cnid_metad_pid = -1;
        else
            LOG(log_error, logtype_afpd, "Bad pid: %d", pid);
    }

    if (in_shutdown && afpd_pid == -1 && cnid_metad_pid == -1)
        event_base_loopbreak(base);
}

/* timer callback */
static void timer_cb(evutil_socket_t fd, short what, void *arg)
{
    if (in_shutdown)
        return;

    if (afpd_pid == -1) {
        afpd_restarts++;
        LOG(log_note, logtype_afpd, "Restarting 'afpd' (restarts: %u)", afpd_restarts);
        if ((afpd_pid = run_process(_PATH_AFPD, "-d", "-F", obj.options.configfile, NULL)) == -1) {
            LOG(log_error, logtype_afpd, "Error starting 'afpd'");
        }
    }

    if (cnid_metad_pid == -1) {
        cnid_metad_restarts++;
        LOG(log_note, logtype_afpd, "Restarting 'cnid_metad' (restarts: %u)", cnid_metad_restarts);
        if ((cnid_metad_pid = run_process(_PATH_CNID_METAD, "-d", "-F", obj.options.configfile, NULL)) == -1) {
            LOG(log_error, logtype_afpd, "Error starting 'cnid_metad'");
        }
    }
}

/******************************************************************
 * helper functions
 ******************************************************************/

/* kill processes passed as varargs of type "pid_t *", terminate list with NULL */
static void kill_childs(int sig, ...)
{
    va_list args;
    pid_t *pid;

    va_start(args, sig);

    while ((pid = va_arg(args, pid_t *)) != NULL) {
        if (*pid == -1)
            continue;
        kill(*pid, sig);
    }
    va_end(args);
}

/* this get called when error conditions are met that require us to exit gracefully */
static void netatalk_exit(int ret)
{
    server_unlock(PATH_NETATALK_LOCK);
    exit(ret);
}

/* this forks() and exec() "path" with varags as argc[] */
static pid_t run_process(const char *path, ...)
{
    int ret, i = 0;
    char *myargv[10];
    va_list args;
    pid_t pid;

    if ((pid = fork()) < 0) {
        LOG(log_error, logtype_cnid, "error in fork: %s", strerror(errno));
        return -1;
    }

    if (pid == 0) {
        myargv[i++] = (char *)path;
        va_start(args, path);
        while ((myargv[i++] = va_arg(args, char *)) != NULL)
            ;
        va_end(args);

        ret = execv(path, myargv);

        /* Yikes! We're still here, so exec failed... */
        LOG(log_error, logtype_cnid, "Fatal error in exec: %s", strerror(errno));
        exit(1);
    }
    return pid;
}

static void usage(void)
{
    printf("usage: netatalk [-F configfile] \n");
}

int main(int argc, char **argv)
{
    int c, ret, debug = 0;
    sigset_t blocksigs;
    struct timeval tv;

    /* Log SIGBUS/SIGSEGV SBT */
    fault_setup(NULL);

    while ((c = getopt(argc, argv, ":dF:")) != -1) {
        switch(c) {
        case 'd':
            debug = 1;
            break;
        case 'F':
            obj.cmdlineconfigfile = strdup(optarg);
            break;
        default:
            usage();
            exit(EXIT_FAILURE);
        }
    }

    if (check_lockfile("netatalk", PATH_NETATALK_LOCK) != 0)
        exit(EXITERR_SYS);

    if (!debug && daemonize(0, 0) != 0)
        exit(EXITERR_SYS);

    if (create_lockfile("netatalk", PATH_NETATALK_LOCK) != 0)
        exit(EXITERR_SYS);

    sigfillset(&blocksigs);
    sigprocmask(SIG_SETMASK, &blocksigs, NULL);
    
    if (afp_config_parse(&obj, "netatalk") != 0)
        netatalk_exit(EXITERR_CONF);

    event_set_log_callback(libevent_logmsg_cb);
    event_set_fatal_callback(netatalk_exit);

    LOG(log_note, logtype_default, "Netatalk AFP server starting");

    if ((afpd_pid = run_process(_PATH_AFPD, "-d", "-F", obj.options.configfile, NULL)) == -1) {
        LOG(log_error, logtype_afpd, "Error starting 'afpd'");
        netatalk_exit(EXITERR_CONF);
    }

    if ((cnid_metad_pid = run_process(_PATH_CNID_METAD, "-d", "-F", obj.options.configfile, NULL)) == -1) {
        LOG(log_error, logtype_afpd, "Error starting 'cnid_metad'");
        netatalk_exit(EXITERR_CONF);
    }

    if ((base = event_base_new()) == NULL) {
        LOG(log_error, logtype_afpd, "Error starting event loop");
        netatalk_exit(EXITERR_CONF);
    }

    sigterm_ev = event_new(base, SIGTERM, EV_SIGNAL, sigterm_cb, NULL);
    sigquit_ev = event_new(base, SIGQUIT, EV_SIGNAL | EV_PERSIST, sigquit_cb, NULL);
    sigquit_ev = event_new(base, SIGHUP,  EV_SIGNAL | EV_PERSIST, sighup_cb, NULL);
    sigchld_ev = event_new(base, SIGCHLD, EV_SIGNAL | EV_PERSIST, sigchld_cb, NULL);
    timer_ev = event_new(base, -1, EV_PERSIST, timer_cb, NULL);

    tv.tv_sec = 1;
    tv.tv_usec = 0;

    event_add(sigterm_ev, NULL);
    event_add(sigquit_ev, NULL);
    event_add(sigchld_ev, NULL);
    event_add(timer_ev, &tv);

    sigfillset(&blocksigs);
    sigdelset(&blocksigs, SIGTERM);
    sigdelset(&blocksigs, SIGQUIT);
    sigdelset(&blocksigs, SIGCHLD);
    sigdelset(&blocksigs, SIGHUP);
    sigprocmask(SIG_SETMASK, &blocksigs, NULL);

    /* run the event loop */
    ret = event_base_dispatch(base);

    if (afpd_pid != -1 || cnid_metad_pid != -1) {
        if (afpd_pid != -1)
            LOG(log_error, logtype_afpd, "AFP service did not shutdown, killing it");
        if (cnid_metad_pid != -1)
            LOG(log_error, logtype_afpd, "CNID database service did not shutdown, killing it");
        kill_childs(SIGKILL, &afpd_pid, &cnid_metad_pid, NULL);
    }

    LOG(log_note, logtype_afpd, "Netatalk AFP server exiting");

    netatalk_exit(ret);
}
