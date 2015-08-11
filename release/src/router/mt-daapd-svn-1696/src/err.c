/*
 * $Id: err.c 1628 2007-08-06 05:18:01Z rpedde $
 * Generic error handling
 *
 * Copyright (C) 2003 Ron Pedde (ron@pedde.com)
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

/**
 * \file err.c
 * Error handling, logging, and memory leak checking.
 *
 * Most of these functions should not be used directly.  For the most
 * part, they are hidden in macros like #DPRINTF and #MEMNOTIFY.  The
 * only function here that is really directly useable is log_setdest()
 */

#ifdef HAVE_CONFIG_H
#  include "config.h"
#endif

#include <stdio.h>
#include <stdarg.h>
#include <time.h>
#include <errno.h>
#include <limits.h>
#include <pthread.h>
#ifdef HAVE_STDINT_H
#include <stdint.h>
#endif
#include <stdlib.h>
#include <string.h>

#include <sys/stat.h>
#include <sys/types.h>

#include "daapd.h"
#include "err.h"
#include "io.h"

#ifndef ERR_LEAN
# include "os.h"
# include "plugin.h"
#endif

#include "util.h"

#ifndef PACKAGE
# define PACKAGE "unknown daemon"
#endif

typedef struct err_threadlist_t {
    uint32_t id;
    struct err_threadlist_t *next;
} ERR_THREADLIST;

static int err_debuglevel=0; /**< current debuglevel, set from command line with -d */
static int err_logdest=0; /**< current log destination */
static char err_filename[PATH_MAX + 1];
static IOHANDLE err_file = NULL;
static unsigned int err_debugmask=0xFFFFFFFF; /**< modules to debug, see \ref log_categories */
static int err_truncate = 0;
static int err_syslog_open = 0;

/** text list of modules to match for setting debug mask */
static char *err_categorylist[] = {
    "config","webserver","database","scan","query","index","browse",
    "playlist","art","daap","main","rend","xml","parse","plugin","lock",NULL
};

static ERR_THREADLIST err_threadlist = { 0, NULL };

/*
 * Forwards
 */

static uint32_t __err_get_threadid(void);
static void __err_thread_add(void);
static void __err_thread_del(void);
static int __err_thread_check(void);

/**
 * add a thread id to the threadlist.  note that the threadlist semaphore must
 * be locked.
 */
void __err_thread_add(void) {
    ERR_THREADLIST *pnew;
    
    pnew = (ERR_THREADLIST *)malloc(sizeof(ERR_THREADLIST));
    if(!pnew) {
        fprintf(stderr,"malloc error in __err_thread_add\n");
        exit(-1);
    }
            
    pnew->id = __err_get_threadid();
    pnew->next = err_threadlist.next;
    err_threadlist.next = pnew;
}

/**
 * remove a thread id from the threadlist.  The threadlist semaphore must be locked
 */
void __err_thread_del(void) {
    uint32_t thread_id = __err_get_threadid();
    ERR_THREADLIST *last, *current;
    last = &err_threadlist;
    current = err_threadlist.next;
    
    while((current) && (current->id != thread_id)) {
        last = current;
        current = current->next;
    }
    
    if(current) {
        last->next = current->next;
        free(current);
    }
}

/**
 * see if the current thread is already in the thread list
 */
int __err_thread_check(void) {
    uint32_t thread_id = __err_get_threadid();
    ERR_THREADLIST *current = err_threadlist.next;
    
    while(current && (current->id != thread_id))
        current = current->next;
        
    if(current)
        return TRUE;
        
    return FALSE;
}

/**
 * get an integer representation of a thread id
 */
uint32_t __err_get_threadid(void) {
    pthread_t tid;
    int thread_id=0;

    memset((void*)&tid,0,sizeof(pthread_t));
    tid = pthread_self();

    if(sizeof(pthread_t) == sizeof(int)) {
        memcpy((void*)&thread_id,(void*)&tid,sizeof(thread_id));
    } else {
        thread_id = util_djb_hash_block((unsigned char *)&tid,sizeof(pthread_t));
    }

    return thread_id;
}

/**
 * if we are logging to a file, then re-open the file.  This
 * would help for log rotation
 */
void err_reopen(void) {
    if(!(err_logdest & LOGDEST_LOGFILE))
        return;

    io_close(err_file);
    if(!io_open("file://%U?mode=a&ascii=1",err_filename)) {
        /* what to do when you lose your logging mechanism?  Keep
         * going?
         */
        err_setdest(err_logdest & (~LOGDEST_LOGFILE));
        err_setdest(err_logdest | LOGDEST_SYSLOG);

        DPRINTF(E_LOG,L_MISC,"Could not rotate log file: %s\n",
            io_errstr(err_file));
        return;
    }

    DPRINTF(E_LOG,L_MISC,"Rotated logs\n");
}

/**
 * Write a printf-style formatted message to the log destination.
 * This can be stderr, syslog/eventviewer, or a logfile, as determined by
 * err_setdest().  Note that this function should not be directly
 * used, rather it should be used via the #DPRINTF macro.
 *
 * \param level Level at which to log \ref log_levels
 * \param cat the category to log \ref log_categories
 * \param fmt printf-style
 */
void err_log(int level, unsigned int cat, char *fmt, ...)
{
    va_list ap;
    char timebuf[256];
    char errbuf[4096];
    struct tm tm_now;
    time_t tt_now;
    int syslog_only = FALSE;

    if(level > 1) {
        if(level > err_debuglevel)
            return;

        if(!(cat & err_debugmask))
            return;
    } /* we'll *always* process a log level 0 or 1 */

    /* skip recursive calls to logging functions to avoid deadlocks (except for aborts) */
    util_mutex_lock(l_err_list);
    if(err_threadlist.next && __err_thread_check()) { /* skip logging */
        if(!level) {
            syslog_only = TRUE; /* syslog fatals even on recursive calls */
        } else {
            util_mutex_unlock(l_err_list);
            return;
        }
    }
    __err_thread_add();
    util_mutex_unlock(l_err_list);

    va_start(ap, fmt);
    vsnprintf(errbuf, sizeof(errbuf), fmt, ap);
    va_end(ap);

    /* always log fatals and level 1 to syslog */
    if(level <= 1) {
        if(!err_syslog_open)
            os_opensyslog();
        err_syslog_open=1;
        os_syslog(level,errbuf);

        if(syslog_only && !level) {
            fprintf(stderr,"Aborting\n");
            exit(-1);
        }
    }
    
    util_mutex_lock(l_err);

    if((err_logdest & LOGDEST_LOGFILE) && (err_file) && (!syslog_only)) {
        tt_now=time(NULL);
        localtime_r(&tt_now,&tm_now);
        snprintf(timebuf,sizeof(timebuf),"%04d-%02d-%02d %02d:%02d:%02d",
                 tm_now.tm_year + 1900, tm_now.tm_mon + 1, tm_now.tm_mday,
                 tm_now.tm_hour, tm_now.tm_min, tm_now.tm_sec);
        io_printf(err_file,"%s (%08x): %s",timebuf,__err_get_threadid(),errbuf);
        if(!level) io_printf(err_file,"%s: Aborting\n",timebuf);
    }

    /* always log to stderr on fatal error */
    if((err_logdest & LOGDEST_STDERR) || (!level)) {
        fprintf(stderr, "%s",errbuf);
        if(!level) fprintf(stderr,"Aborting\n");
    }

    util_mutex_unlock(l_err);

#ifndef ERR_LEAN
    if(level < 2) { /* only event level fatals and log level */
        plugin_event_dispatch(PLUGIN_EVENT_LOG, level, errbuf, (int)strlen(errbuf)+1);
    }
#endif

    util_mutex_lock(l_err_list);
    __err_thread_del();
    util_mutex_unlock(l_err_list);

    if(!level) {
        exit(EXIT_FAILURE);      /* this should go to an OS-specific exit routine */
    }
}

/**
 * simple get/set interface to debuglevel to avoid global
 */
void err_setlevel(int level) {
    util_mutex_lock(l_err);
    err_debuglevel = level;
    util_mutex_unlock(l_err);
}

/**
 * get current debug level
 */
int err_getlevel(void) {
    int level;

    util_mutex_lock(l_err);
    level = err_debuglevel;
    util_mutex_unlock(l_err);

    return level;
}


/**
 * get the logfile destination
 */
int err_getdest(void) {
    int dest;

    util_mutex_lock(l_err);
    dest=err_logdest;
    util_mutex_unlock(l_err);

    return dest;
}


int err_settruncate(int truncate) {
    char *file;

    if(err_truncate == truncate)
        return TRUE;

    err_truncate = truncate;
    if((err_truncate) && (err_file)) {
        file=strdup(err_filename);
        err_setlogfile(err_filename);
        if(file) free(file);
    }

    return TRUE;
}

int err_setlogfile(char *file) {
    char *mode;
    int result=TRUE;

/*
    if(strcmp(file,err_filename) == 0)
        return TRUE;
*/

    if(err_file) {
        io_close(err_file);
    } else {
        err_file = io_new();
        if(!err_file) {
            err_logdest &= ~LOGDEST_LOGFILE;
            if(!err_syslog_open)
                os_opensyslog();
            os_syslog(1,"Error initializing logfile");
        }
    }

    mode = "a";
    if(err_truncate) mode = "w";

    strncpy(err_filename,file,sizeof(err_filename)-1);

    if(!io_open(err_file,"file://%U?mode=%s&ascii=1",err_filename,mode)) {
        fprintf(stderr,"Error opening logfile: %s",io_errstr(err_file));
        err_logdest &= ~LOGDEST_LOGFILE;

        if(!err_syslog_open)
            os_opensyslog();
        os_syslog(1,"Error opening logfile");

        result=FALSE;
    }

    return result;
}

/**
 * Sets the log destination.  (stderr, syslog, or logfile)
 *
 * \param app appname (used only for syslog destination)
 * \param destination where to log to \ref log_dests "as defined in err.h"
 */
void err_setdest(int destination) {
    if(err_logdest == destination)
        return;

    util_mutex_lock(l_err);
    if((err_logdest & LOGDEST_LOGFILE) &&
       (!(destination & LOGDEST_LOGFILE))) {
        /* used to be logging to file, not any more */
        io_close(err_file);
    }

    err_logdest=destination;
    util_mutex_unlock(l_err);
}
/**
 * Set the debug mask.  Given a comma separated list, this walks
 * through the err_categorylist and sets the bitfields for the
 * requested log modules.
 *
 * \param list comma separated list of modules to debug.
 */
extern int err_setdebugmask(char *list) {
    unsigned int rack;
    char *token, *str, *last;
    char *tmpstr;
    int index;

    err_debugmask=0x80000000; /* always log L_MISC! */
    str=tmpstr=strdup(list);
    if(!str)
        return 0;

    util_mutex_lock(l_err);
    while(1) {
        token=strtok_r(str,",",&last);
        str=NULL;

        if(token) {
            rack=1;
            index=0;
            while((err_categorylist[index]) &&
                  (strcasecmp(err_categorylist[index],token))) {
                rack <<= 1;
                index++;
            }

            if(!err_categorylist[index]) {
                util_mutex_unlock(l_err);
                DPRINTF(E_LOG,L_MISC,"Unknown module: %s\n",token);
                free(tmpstr);
                return 1;
            } else {
                err_debugmask |= rack;
            }
        } else break; /* !token */
    }

    util_mutex_unlock(l_err);
    DPRINTF(E_INF,L_MISC,"Debug mask is 0x%08x\n",err_debugmask);
    free(tmpstr);

    return 0;
}


