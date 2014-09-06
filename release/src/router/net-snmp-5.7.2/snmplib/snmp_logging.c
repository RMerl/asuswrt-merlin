/*
 * logging.c - generic logging for snmp-agent
 * * Contributed by Ragnar Kjørstad, ucd@ragnark.vestdata.no 1999-06-26 
 */
/* Portions of this file are subject to the following copyright(s).  See
 * the Net-SNMP's COPYING file for more details and other copyrights
 * that may apply:
 */
/*
 * Portions of this file are copyrighted by:
 * Copyright © 2003 Sun Microsystems, Inc. All rights reserved.
 * Use is subject to license terms specified in the COPYING file
 * distributed with the Net-SNMP package.
 */
/** @defgroup snmp_logging generic logging for net-snmp 
 *  @ingroup library
 * 
 *  @{
 */
#include <net-snmp/net-snmp-config.h>
#include <net-snmp/net-snmp-features.h>
#include <stdio.h>
#if HAVE_MALLOC_H
#include <malloc.h>
#endif
#if HAVE_STRING_H
#include <string.h>
#else
#include <strings.h>
#endif
#include <ctype.h>
#if HAVE_STDLIB_H
#include <stdlib.h>
#endif
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#if HAVE_SYSLOG_H
#include <syslog.h>
#ifndef LOG_CONS                /* Interesting Ultrix feature */
#include <sys/syslog.h>
#endif
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
#if HAVE_NETINET_IN_H
#include <netinet/in.h>
#endif

#include <stdarg.h>

#if HAVE_UNISTD_H
#include <unistd.h>
#endif
#if HAVE_DMALLOC_H
#include <dmalloc.h>
#endif

#include <net-snmp/types.h>
#include <net-snmp/output_api.h>
#include <net-snmp/library/snmp_logging.h>      /* For this file's "internal" definitions */
#include <net-snmp/config_api.h>
#include <net-snmp/utilities.h>

#include <net-snmp/library/callback.h>
#define LOGLENGTH 1024

#ifdef va_copy
#define NEED_VA_END_AFTER_VA_COPY
#else
#ifdef __vacopy
#define vacopy __vacopy
#define NEED_VA_END_AFTER_VA_COPY
#else
#define va_copy(dest, src) memcpy (&dest, &src, sizeof (va_list))
#endif
#endif

netsnmp_feature_child_of(logging_all, libnetsnmp)

netsnmp_feature_child_of(logging_outputs, logging_all)
netsnmp_feature_child_of(logging_file, logging_outputs)
netsnmp_feature_child_of(logging_stdio, logging_outputs)
netsnmp_feature_child_of(logging_syslog, logging_outputs)
netsnmp_feature_child_of(logging_external, logging_all)

netsnmp_feature_child_of(enable_stderrlog, logging_all)

netsnmp_feature_child_of(logging_enable_calllog, netsnmp_unused)
netsnmp_feature_child_of(logging_enable_loghandler, netsnmp_unused)

/* default to the file/stdio/syslog set */
netsnmp_feature_want(logging_outputs)

/*
 * logh_head:  A list of all log handlers, in increasing order of priority
 * logh_priorities:  'Indexes' into this list, by priority
 */
netsnmp_log_handler *logh_head = NULL;
netsnmp_log_handler *logh_priorities[LOG_DEBUG+1];
static int  logh_enabled = 0;

#ifndef NETSNMP_FEATURE_REMOVE_LOGGING_SYSLOG
static char syslogname[64] = DEFAULT_LOG_ID;
#endif /* NETSNMP_FEATURE_REMOVE_LOGGING_SYSLOG */

void
netsnmp_disable_this_loghandler(netsnmp_log_handler *logh)
{
    if (!logh || (0 == logh->enabled))
        return;
    logh->enabled = 0;
    --logh_enabled;
    netsnmp_assert(logh_enabled >= 0);
}

void
netsnmp_enable_this_loghandler(netsnmp_log_handler *logh)
{
    if (!logh || (0 != logh->enabled))
        return;
    logh->enabled = 1;
    ++logh_enabled;
}

#ifndef NETSNMP_FEATURE_REMOVE_LOGGING_FILE
void
netsnmp_enable_filelog(netsnmp_log_handler *logh, int dont_zero_log);
#endif /* NETSNMP_FEATURE_REMOVE_LOGGING_FILE */

#ifndef HAVE_VSNPRINTF
                /*
                 * Need to use the UCD-provided one 
                 */
int             vsnprintf(char *str, size_t count, const char *fmt,
                          va_list arg);
#endif

void
parse_config_logOption(const char *token, char *cptr)
{
  int my_argc = 0 ;
  char **my_argv = NULL;

  snmp_log_options( cptr, my_argc, my_argv );
}

void
init_snmp_logging(void)
{
    netsnmp_ds_register_premib(ASN_BOOLEAN, "snmp", "logTimestamp", 
			 NETSNMP_DS_LIBRARY_ID, NETSNMP_DS_LIB_LOG_TIMESTAMP);
    register_prenetsnmp_mib_handler("snmp", "logOption",
                                    parse_config_logOption, NULL, "string");

}

void
shutdown_snmp_logging(void)
{
   snmp_disable_log();
   while(NULL != logh_head)
      netsnmp_remove_loghandler( logh_head );
}

/*
 * These definitions handle 4.2 systems without additional syslog facilities.
 */
#ifndef NETSNMP_FEATURE_REMOVE_LOGGING_SYSLOG
#ifndef LOG_CONS
#define LOG_CONS	0       /* Don't bother if not defined... */
#endif
#ifndef LOG_PID
#define LOG_PID		0       /* Don't bother if not defined... */
#endif
#ifndef LOG_LOCAL0
#define LOG_LOCAL0	0
#endif
#ifndef LOG_LOCAL1
#define LOG_LOCAL1	0
#endif
#ifndef LOG_LOCAL2
#define LOG_LOCAL2	0
#endif
#ifndef LOG_LOCAL3
#define LOG_LOCAL3	0
#endif
#ifndef LOG_LOCAL4
#define LOG_LOCAL4	0
#endif
#ifndef LOG_LOCAL5
#define LOG_LOCAL5	0
#endif
#ifndef LOG_LOCAL6
#define LOG_LOCAL6	0
#endif
#ifndef LOG_LOCAL7
#define LOG_LOCAL7	0
#endif
#ifndef LOG_DAEMON
#define LOG_DAEMON	0
#endif
#ifndef LOG_USER
#define LOG_USER	0
#endif
#endif /* NETSNMP_FEATURE_REMOVE_LOGGING_SYSLOG */

/* Set line buffering mode for a stream. */
void
netsnmp_set_line_buffering(FILE *stream)
{
#if defined(WIN32)
    /*
     * According to MSDN, the Microsoft Visual Studio C runtime library does
     * not support line buffering, so turn off buffering completely.
     * See also http://msdn.microsoft.com/en-us/library/86cebhfs(VS.71).aspx.
     */
    setvbuf(stream, NULL, _IONBF, BUFSIZ);
#elif defined(HAVE_SETLINEBUF)
    /* setlinefunction() is a function from the BSD Unix API. */
    setlinebuf(stream);
#else
    /* See also the C89 or C99 standard for more information about setvbuf(). */
    setvbuf(stream, NULL, _IOLBF, BUFSIZ);
#endif
}

/*
 * Decodes log priority.
 * @param optarg - IN - priority to decode, "0" or "0-7"
 *                 OUT - points to last character after the decoded priority
 * @param pri_max - OUT - maximum priority (i.e. 0x7 from "0-7")
 */
int
decode_priority( char **optarg, int *pri_max )
{
    int pri_low = LOG_DEBUG;

    if (*optarg == NULL)
        return -1;

    switch (**optarg) {
        case '0': 
        case '!': 
            pri_low = LOG_EMERG;
            break;
        case '1': 
        case 'a': 
        case 'A': 
            pri_low = LOG_ALERT;
            break;
        case '2': 
        case 'c': 
        case 'C': 
            pri_low = LOG_CRIT;
            break;
        case '3': 
        case 'e': 
        case 'E': 
            pri_low = LOG_ERR;
            break;
        case '4': 
        case 'w': 
        case 'W': 
            pri_low = LOG_WARNING;
            break;
        case '5': 
        case 'n': 
        case 'N': 
            pri_low = LOG_NOTICE;
            break;
        case '6': 
        case 'i': 
        case 'I': 
            pri_low = LOG_INFO;
            break;
        case '7': 
        case 'd': 
        case 'D': 
            pri_low = LOG_DEBUG;
            break;
        default: 
            fprintf(stderr, "invalid priority: %c\n",**optarg);
            return -1;
    }
    *optarg = *optarg+1;

    if (pri_max && **optarg=='-') {
        *optarg = *optarg + 1; /* skip '-' */
        *pri_max = decode_priority( optarg, NULL );
        if (*pri_max == -1) return -1;
        if (pri_low < *pri_max) { 
            int tmp = pri_low; 
            pri_low = *pri_max; 
            *pri_max = tmp; 
        }

    }
    return pri_low;
}

#ifndef NETSNMP_FEATURE_REMOVE_LOGGING_SYSLOG
int
decode_facility( char *optarg )
{
    if (optarg == NULL)
        return -1;

    switch (*optarg) {
    case 'd':
    case 'D':
        return LOG_DAEMON;
    case 'u':
    case 'U':
        return LOG_USER;
    case '0':
        return LOG_LOCAL0;
    case '1':
        return LOG_LOCAL1;
    case '2':
        return LOG_LOCAL2;
    case '3':
        return LOG_LOCAL3;
    case '4':
        return LOG_LOCAL4;
    case '5':
        return LOG_LOCAL5;
    case '6':
        return LOG_LOCAL6;
    case '7':
        return LOG_LOCAL7;
    default:
        fprintf(stderr, "invalid syslog facility: %c\n",*optarg);
        return -1;
    }
}
#endif /* NETSNMP_FEATURE_REMOVE_LOGGING_SYSLOG */

int
snmp_log_options(char *optarg, int argc, char *const *argv)
{
    char           *cp = optarg;
        /*
	 * Hmmm... this doesn't seem to work.
	 * The main agent 'getopt' handling assumes
	 *   that the -L option takes an argument,
	 *   and objects if this is missing.
	 * Trying to differentiate between
	 *   new-style "-Lx", and old-style "-L xx"
	 *   is likely to be a major headache.
	 */
    char            missing_opt = 'e';	/* old -L is new -Le */
    int             priority = LOG_DEBUG;
    int             pri_max  = LOG_EMERG;
    int             inc_optind = 0;
    netsnmp_log_handler *logh;

    DEBUGMSGT(("logging:options", "optarg: '%s', argc %d, argv '%s'\n",
               optarg, argc, argv ? argv[0] : "NULL"));
    optarg++;
    if (!*cp)
        cp = &missing_opt;

    /*
     * Support '... -Lx=value ....' syntax
     */
    if (*optarg == '=') {
        optarg++;
    }
    /*
     * and '.... "-Lx value" ....'  (*with* the quotes)
     */
    while (*optarg && isspace((unsigned char)(*optarg))) {
        optarg++;
    }
    /*
     * Finally, handle ".... -Lx value ...." syntax
     *   (*without* surrounding quotes)
     */
    if ((!*optarg) && (NULL != argv)) {
        /*
         * We've run off the end of the argument
         *  so move on to the next.
         * But we might not actually need it, so don't
	 *  increment optind just yet!
         */
        optarg = argv[optind];
        inc_optind = 1;
    }

    DEBUGMSGT(("logging:options", "*cp: '%c'\n", *cp));
    switch (*cp) {

#ifndef NETSNMP_FEATURE_REMOVE_LOGGING_STDIO
    /*
     * Log to Standard Error
     */
    case 'E':
        priority = decode_priority( &optarg, &pri_max );
        if (priority == -1)  return -1;
        if (inc_optind)
            optind++;
        /* Fallthrough */
    case 'e':
        logh = netsnmp_register_loghandler(NETSNMP_LOGHANDLER_STDERR, priority);
        if (logh) {
            netsnmp_set_line_buffering(stderr);
            logh->pri_max = pri_max;
            logh->token   = strdup("stderr");
	}
        break;

    /*
     * Log to Standard Output
     */
    case 'O':
        priority = decode_priority( &optarg, &pri_max );
        if (priority == -1)  return -1;
        if (inc_optind)
            optind++;
        /* Fallthrough */
    case 'o':
        logh = netsnmp_register_loghandler(NETSNMP_LOGHANDLER_STDERR, priority);
        if (logh) {
            netsnmp_set_line_buffering(stdout);
            logh->pri_max = pri_max;
            logh->token   = strdup("stdout");
            logh->imagic  = 1;	    /* stdout, not stderr */
	}
        break;
#endif /* NETSNMP_FEATURE_REMOVE_LOGGING_STDIO */

    /*
     * Log to a named file
     */
    case 'F':
        priority = decode_priority( &optarg, &pri_max );
        if (priority == -1 || !argv)  return -1;
        optarg = argv[++optind];
        /* Fallthrough */
#ifndef NETSNMP_FEATURE_REMOVE_LOGGING_FILE
    case 'f':
        if (inc_optind)
            optind++;
        if (!optarg) {
            fprintf(stderr, "Missing log file\n");
            return -1;
        }
        logh = netsnmp_register_loghandler(NETSNMP_LOGHANDLER_FILE, priority);
        if (logh) {
            logh->pri_max = pri_max;
            logh->token   = strdup(optarg);
            netsnmp_enable_filelog(logh,
                                   netsnmp_ds_get_boolean(NETSNMP_DS_LIBRARY_ID,
                                                          NETSNMP_DS_LIB_APPEND_LOGFILES));
	}
        break;
#endif /* NETSNMP_FEATURE_REMOVE_LOGGING_FILE */

#ifndef NETSNMP_FEATURE_REMOVE_LOGGING_SYSLOG
    /*
     * Log to syslog
     */
    case 'S':
        priority = decode_priority( &optarg, &pri_max );
        if (priority == -1 || !argv)  return -1;
        if (!optarg[0]) {
            /* The command line argument with priority does not contain log
             * facility. The facility must be in next argument then. */
            optind++;
            if (optind < argc)
                optarg = argv[optind];
        }
        /* Fallthrough */
    case 's':
        if (inc_optind)
            optind++;
        if (!optarg) {
            fprintf(stderr, "Missing syslog facility\n");
            return -1;
        }
        logh = netsnmp_register_loghandler(NETSNMP_LOGHANDLER_SYSLOG, priority);
        if (logh) {
            int facility = decode_facility(optarg);
            if (facility == -1)  return -1;
            logh->pri_max = pri_max;
            logh->token   = strdup(snmp_log_syslogname(NULL));
            logh->magic   = (void *)(intptr_t)facility;
	    snmp_enable_syslog_ident(snmp_log_syslogname(NULL), facility);
	}
        break;
#endif /* NETSNMP_FEATURE_REMOVE_LOGGING_SYSLOG */

    /*
     * Don't log 
     */
    case 'N':
        priority = decode_priority( &optarg, &pri_max );
        if (priority == -1)  return -1;
        if (inc_optind)
            optind++;
        /* Fallthrough */
    case 'n':
        /*
         * disable all logs to clean them up (close files, etc),
         * remove all log handlers, then register a null handler.
         */
        snmp_disable_log();
        while(NULL != logh_head)
            netsnmp_remove_loghandler( logh_head );
        logh = netsnmp_register_loghandler(NETSNMP_LOGHANDLER_NONE, priority);
        if (logh) {
            logh->pri_max = pri_max;
	}
        break;

    default:
        fprintf(stderr, "Unknown logging option passed to -L: %c.\n", *cp);
        return -1;
    }
    return 0;
}

#ifndef NETSNMP_FEATURE_REMOVE_LOGGING_SYSLOG
char *
snmp_log_syslogname(const char *pstr)
{
  if (pstr)
    strlcpy (syslogname, pstr, sizeof(syslogname));

  return syslogname;
}
#endif /* NETSNMP_FEATURE_REMOVE_LOGGING_SYSLOG */

void
snmp_log_options_usage(const char *lead, FILE * outf)
{
    const char *pri1_msg = " for level 'pri' and above";
    const char *pri2_msg = " for levels 'p1' to 'p2'";
#ifndef NETSNMP_FEATURE_REMOVE_LOGGING_STDIO
    fprintf(outf, "%se:           log to standard error\n", lead);
    fprintf(outf, "%so:           log to standard output\n", lead);
#endif /* NETSNMP_FEATURE_REMOVE_LOGGING_STDIO */
    fprintf(outf, "%sn:           don't log at all\n", lead);
#ifndef NETSNMP_FEATURE_REMOVE_LOGGING_FILE
    fprintf(outf, "%sf file:      log to the specified file\n", lead);
#endif /* NETSNMP_FEATURE_REMOVE_LOGGING_FILE */
#ifndef NETSNMP_FEATURE_REMOVE_LOGGING_SYSLOG
    fprintf(outf, "%ss facility:  log to syslog (via the specified facility)\n", lead);
#endif /* NETSNMP_FEATURE_REMOVE_LOGGING_SYSLOG */
    fprintf(outf, "\n%s(variants)\n", lead);
#ifndef NETSNMP_FEATURE_REMOVE_LOGGING_FILE
    fprintf(outf, "%s[EON] pri:   log to standard error, output or /dev/null%s\n", lead, pri1_msg);
    fprintf(outf, "%s[EON] p1-p2: log to standard error, output or /dev/null%s\n", lead, pri2_msg);
#endif /* NETSNMP_FEATURE_REMOVE_LOGGING_FILE */
    fprintf(outf, "%s[FS] pri token:    log to file/syslog%s\n", lead, pri1_msg);
    fprintf(outf, "%s[FS] p1-p2 token:  log to file/syslog%s\n", lead, pri2_msg);
}

/**
 * Is logging done?
 *
 * @return Returns 0 if logging is off, 1 when it is done.
 *
 */
int
snmp_get_do_logging(void)
{
    return (logh_enabled > 0);
}


static char    *
sprintf_stamp(time_t * now, char *sbuf)
{
    time_t          Now;
    struct tm      *tm;

    if (now == NULL) {
        now = &Now;
        time(now);
    }
    tm = localtime(now);
    sprintf(sbuf, "%.4d-%.2d-%.2d %.2d:%.2d:%.2d ",
            tm->tm_year + 1900, tm->tm_mon + 1, tm->tm_mday,
            tm->tm_hour, tm->tm_min, tm->tm_sec);
    return sbuf;
}

#ifndef NETSNMP_FEATURE_REMOVE_LOGGING_SYSLOG
void
snmp_disable_syslog_entry(netsnmp_log_handler *logh)
{
    if (!logh || !logh->enabled || logh->type != NETSNMP_LOGHANDLER_SYSLOG)
        return;

#ifdef WIN32
    if (logh->magic) {
        HANDLE eventlog_h = (HANDLE)logh->magic;
        CloseEventLog(eventlog_h);
        logh->magic = NULL;
    }
#else
    closelog();
    logh->imagic  = 0;
#endif

    netsnmp_disable_this_loghandler(logh);
}

void
snmp_disable_syslog(void)
{
    netsnmp_log_handler *logh;

    for (logh = logh_head; logh; logh = logh->next)
        if (logh->enabled && logh->type == NETSNMP_LOGHANDLER_SYSLOG)
            snmp_disable_syslog_entry(logh);
}
#endif /* NETSNMP_FEATURE_REMOVE_LOGGING_SYSLOG */

#ifndef NETSNMP_FEATURE_REMOVE_LOGGING_FILE
void
snmp_disable_filelog_entry(netsnmp_log_handler *logh)
{
    if (!logh /* || !logh->enabled */ || logh->type != NETSNMP_LOGHANDLER_FILE)
        return;

    if (logh->magic) {
        fputs("\n", (FILE*)logh->magic);	/* XXX - why? */
        fclose((FILE*)logh->magic);
        logh->magic   = NULL;
    }
    netsnmp_disable_this_loghandler(logh);
}
#endif /* NETSNMP_FEATURE_REMOVE_LOGGING_FILE */

#ifndef NETSNMP_FEATURE_REMOVE_LOGGING_FILE
void
snmp_disable_filelog(void)
{
    netsnmp_log_handler *logh;

    for (logh = logh_head; logh; logh = logh->next)
        if (logh->enabled && logh->type == NETSNMP_LOGHANDLER_FILE)
            snmp_disable_filelog_entry(logh);
}
#endif /* NETSNMP_FEATURE_REMOVE_LOGGING_FILE */

#ifndef NETSNMP_FEATURE_REMOVE_LOGGING_STDIO
/*
 * returns that status of stderr logging
 *
 * @retval 0 : stderr logging disabled
 * @retval 1 : stderr logging enabled
 */
int
snmp_stderrlog_status(void)
{
    netsnmp_log_handler *logh;

    for (logh = logh_head; logh; logh = logh->next)
        if (logh->enabled && (logh->type == NETSNMP_LOGHANDLER_STDOUT ||
                              logh->type == NETSNMP_LOGHANDLER_STDERR)) {
            return 1;
       }

    return 0;
}

void
snmp_disable_stderrlog(void)
{
    netsnmp_log_handler *logh;

    for (logh = logh_head; logh; logh = logh->next)
        if (logh->enabled && (logh->type == NETSNMP_LOGHANDLER_STDOUT ||
                              logh->type == NETSNMP_LOGHANDLER_STDERR)) {
            netsnmp_disable_this_loghandler(logh);
	}
}
#endif /* NETSNMP_FEATURE_REMOVE_LOGGING_STDIO */

#ifndef NETSNMP_FEATURE_REMOVE_LOGGING_ENABLE_CALLLOG
void
snmp_disable_calllog(void)
{
    netsnmp_log_handler *logh;

    for (logh = logh_head; logh; logh = logh->next)
        if (logh->enabled && logh->type == NETSNMP_LOGHANDLER_CALLBACK) {
            netsnmp_disable_this_loghandler(logh);
	}
}
#endif /* NETSNMP_FEATURE_REMOVE_LOGGING_ENABLE_CALLLOG */

void
snmp_disable_log(void)
{
    netsnmp_log_handler *logh;

    for (logh = logh_head; logh; logh = logh->next) {
#ifndef NETSNMP_FEATURE_REMOVE_LOGGING_SYSLOG
        if (logh->type == NETSNMP_LOGHANDLER_SYSLOG)
            snmp_disable_syslog_entry(logh);
#endif /* NETSNMP_FEATURE_REMOVE_LOGGING_SYSLOG */
#ifndef NETSNMP_FEATURE_REMOVE_LOGGING_FILE
        if (logh->type == NETSNMP_LOGHANDLER_FILE)
            snmp_disable_filelog_entry(logh);
#endif /* NETSNMP_FEATURE_REMOVE_LOGGING_FILE */
        netsnmp_disable_this_loghandler(logh);
    }
}

/*
 * close and reopen all file based logs, to allow logfile
 * rotation.
 */
void
netsnmp_logging_restart(void)
{
    netsnmp_log_handler *logh;
    int doneone = 0;

    for (logh = logh_head; logh; logh = logh->next) {
        if (0 == logh->enabled)
            continue;
#ifndef NETSNMP_FEATURE_REMOVE_LOGGING_SYSLOG
        if (logh->type == NETSNMP_LOGHANDLER_SYSLOG) {
            snmp_disable_syslog_entry(logh);
            snmp_enable_syslog_ident(logh->token,(int)(intptr_t)logh->magic);
            doneone = 1;
        }
#endif /* NETSNMP_FEATURE_REMOVE_LOGGING_SYSLOG */
#ifndef NETSNMP_FEATURE_REMOVE_LOGGING_FILE
        if (logh->type == NETSNMP_LOGHANDLER_FILE && !doneone) {
            snmp_disable_filelog_entry(logh);
            /** hmm, don't zero status isn't saved.. i think it's
             * safer not to overwrite, in case a hup is just to
             * re-read config files...
             */
            netsnmp_enable_filelog(logh, 1);
        }
#endif /* NETSNMP_FEATURE_REMOVE_LOGGING_FILE */
    }
}

/* ================================================== */

#ifndef NETSNMP_FEATURE_REMOVE_LOGGING_SYSLOG
void
snmp_enable_syslog(void)
{
    snmp_enable_syslog_ident(snmp_log_syslogname(NULL), LOG_DAEMON);
}

void
snmp_enable_syslog_ident(const char *ident, const int facility)
{
    netsnmp_log_handler *logh;
    int                  found = 0;
    int                  enable = 1;
#ifdef WIN32
    HANDLE               eventlog_h;
#else
    void                *eventlog_h = NULL;
#endif

    snmp_disable_syslog();	/* ??? */
#ifdef WIN32
    eventlog_h = OpenEventLog(NULL, ident);
    if (eventlog_h == NULL) {
	    /*
	     * Hmmm.....
	     * Maybe disable this handler, and log the error ?
	     */
        fprintf(stderr, "Could not open event log for %s. "
                "Last error: 0x%x\n", ident, GetLastError());
        enable = 0;
    }
#else
    openlog(snmp_log_syslogname(ident), LOG_CONS | LOG_PID, facility);
#endif

    for (logh = logh_head; logh; logh = logh->next)
        if (logh->type == NETSNMP_LOGHANDLER_SYSLOG) {
            logh->magic   = (void*)eventlog_h;
            logh->imagic  = enable;	/* syslog open */
            if (logh->enabled && (0 == enable))
                netsnmp_disable_this_loghandler(logh);
            else if ((0 == logh->enabled) && enable)
                netsnmp_enable_this_loghandler(logh);
            found         = 1;
	}

    if (!found) {
        logh = netsnmp_register_loghandler(NETSNMP_LOGHANDLER_SYSLOG,
                                           LOG_DEBUG );
        if (logh) {
            logh->magic    = (void*)eventlog_h;
            logh->token    = strdup(ident);
            logh->imagic   = enable;	/* syslog open */
            if (logh->enabled && (0 == enable))
                netsnmp_disable_this_loghandler(logh);
            else if ((0 == logh->enabled) && enable)
                netsnmp_enable_this_loghandler(logh);
        }
    }
}
#endif /* NETSNMP_FEATURE_REMOVE_LOGGING_SYSLOG */

#ifndef NETSNMP_FEATURE_REMOVE_LOGGING_FILE
void
netsnmp_enable_filelog(netsnmp_log_handler *logh, int dont_zero_log)
{
    FILE *logfile;

    if (!logh)
        return;

    if (!logh->magic) {
        logfile = fopen(logh->token, dont_zero_log ? "a" : "w");
        if (!logfile) {
	    snmp_log_perror(logh->token);
            return;
	}
        logh->magic = (void*)logfile;
        netsnmp_set_line_buffering(logfile);
    }
    netsnmp_enable_this_loghandler(logh);
}

void
snmp_enable_filelog(const char *logfilename, int dont_zero_log)
{
    netsnmp_log_handler *logh;

    /*
     * don't disable ALL filelogs whenever a new one is enabled.
     * this prevents '-Lf file' from working in snmpd, as the
     * call to set up /var/log/snmpd.log will disable the previous
     * log setup.
     * snmp_disable_filelog();
     */

    if (logfilename) {
        logh = netsnmp_find_loghandler( logfilename );
        if (!logh) {
            logh = netsnmp_register_loghandler(NETSNMP_LOGHANDLER_FILE,
                                               LOG_DEBUG );
            if (logh)
                logh->token = strdup(logfilename);
	}
        if (logh)
            netsnmp_enable_filelog(logh, dont_zero_log);
    } else {
        for (logh = logh_head; logh; logh = logh->next)
            if (logh->type == NETSNMP_LOGHANDLER_FILE)
                netsnmp_enable_filelog(logh, dont_zero_log);
    }
}
#endif /* NETSNMP_FEATURE_REMOVE_LOGGING_FILE */


#ifndef NETSNMP_FEATURE_REMOVE_ENABLE_STDERRLOG
/* used in the perl modules and ip-mib/ipv4InterfaceTable/ipv4InterfaceTable_subagent.c */
void
snmp_enable_stderrlog(void)
{
    netsnmp_log_handler *logh;
    int                  found = 0;

    for (logh = logh_head; logh; logh = logh->next)
        if (logh->type == NETSNMP_LOGHANDLER_STDOUT ||
            logh->type == NETSNMP_LOGHANDLER_STDERR) {
            netsnmp_enable_this_loghandler(logh);
            found         = 1;
        }

    if (!found) {
        logh = netsnmp_register_loghandler(NETSNMP_LOGHANDLER_STDERR,
                                           LOG_DEBUG );
        if (logh)
            logh->token    = strdup("stderr");
    }
}
#endif /* NETSNMP_FEATURE_REMOVE_ENABLE_STDERRLOG */

#ifndef NETSNMP_FEATURE_REMOVE_LOGGING_ENABLE_CALLLOG
void
snmp_enable_calllog(void)	/* XXX - or take a callback routine ??? */
{
    netsnmp_log_handler *logh;
    int                  found = 0;

    for (logh = logh_head; logh; logh = logh->next)
        if (logh->type == NETSNMP_LOGHANDLER_CALLBACK) {
            netsnmp_enable_this_loghandler(logh);
            found         = 1;
	}

    if (!found) {
        logh = netsnmp_register_loghandler(NETSNMP_LOGHANDLER_CALLBACK,
                                           LOG_DEBUG );
        if (logh)
            logh->token    = strdup("callback");
    }
}
#endif /* NETSNMP_FEATURE_REMOVE_LOGGING_ENABLE_CALLLOG */


/* ==================================================== */


netsnmp_log_handler *
netsnmp_find_loghandler( const char *token )
{
    netsnmp_log_handler *logh;
    if (!token)
        return NULL;

    for (logh = logh_head; logh; logh = logh->next)
        if (logh->token && !strcmp( token, logh->token ))
            break;

    return logh;
}

int
netsnmp_add_loghandler( netsnmp_log_handler *logh )
{
    int i;
    netsnmp_log_handler *logh2;

    if (!logh)
        return 0;

    /*
     * Find the appropriate point for the new entry...
     *   (logh2 will point to the entry immediately following)
     */
    for (logh2 = logh_head; logh2; logh2 = logh2->next)
        if ( logh2->priority >= logh->priority )
            break;

    /*
     * ... and link it into the main list.
     */
    if (logh2) {
        if (logh2->prev)
            logh2->prev->next = logh;
        else
            logh_head = logh;
        logh->next  = logh2;
        logh2->prev = logh;
    } else if (logh_head ) {
        /*
         * If logh2 is NULL, we're tagging on to the end
         */
        for (logh2 = logh_head; logh2->next; logh2 = logh2->next)
            ;
        logh2->next = logh;
    } else {
        logh_head = logh;
    }

    /*
     * Also tweak the relevant priority-'index' array.
     */
    for (i=LOG_EMERG; i<=logh->priority; i++)
        if (!logh_priorities[i] ||
             logh_priorities[i]->priority >= logh->priority)
             logh_priorities[i] = logh;

    return 1;
}

netsnmp_log_handler *
netsnmp_register_loghandler( int type, int priority )
{
    netsnmp_log_handler *logh;

    logh = SNMP_MALLOC_TYPEDEF(netsnmp_log_handler);
    if (!logh)
        return NULL;

    DEBUGMSGT(("logging:register", "registering log type %d with pri %d\n",
               type, priority));

    logh->type     = type;
    switch ( type ) {
    case NETSNMP_LOGHANDLER_STDOUT:
        logh->imagic  = 1;
        /* fallthrough */
#ifndef NETSNMP_FEATURE_REMOVE_LOGGING_STDIO
    case NETSNMP_LOGHANDLER_STDERR:
        logh->handler = log_handler_stdouterr;
        break;
#endif /* NETSNMP_FEATURE_REMOVE_LOGGING_STDIO */

#ifndef NETSNMP_FEATURE_REMOVE_LOGGING_FILE
    case NETSNMP_LOGHANDLER_FILE:
        logh->handler = log_handler_file;
        logh->imagic  = 1;
        break;
#endif /* NETSNMP_FEATURE_REMOVE_LOGGING_FILE */
#ifndef NETSNMP_FEATURE_REMOVE_LOGGING_SYSLOG
    case NETSNMP_LOGHANDLER_SYSLOG:
        logh->handler = log_handler_syslog;
        break;
#endif /* NETSNMP_FEATURE_REMOVE_LOGGING_SYSLOG */
    case NETSNMP_LOGHANDLER_CALLBACK:
        logh->handler = log_handler_callback;
        break;
    case NETSNMP_LOGHANDLER_NONE:
        logh->handler = log_handler_null;
        break;
    default:
        free(logh);
        return NULL;
    }
    logh->priority = priority;
    netsnmp_enable_this_loghandler(logh);
    netsnmp_add_loghandler( logh );
    return logh;
}


#ifndef NETSNMP_FEATURE_REMOVE_LOGGING_ENABLE_LOGHANDLER
int
netsnmp_enable_loghandler( const char *token )
{
    netsnmp_log_handler *logh;

    logh = netsnmp_find_loghandler( token );
    if (!logh)
        return 0;
    netsnmp_enable_this_loghandler(logh);
    return 1;
}


int
netsnmp_disable_loghandler( const char *token )
{
    netsnmp_log_handler *logh;

    logh = netsnmp_find_loghandler( token );
    if (!logh)
        return 0;
    netsnmp_disable_this_loghandler(logh);
    return 1;
}
#endif /* NETSNMP_FEATURE_REMOVE_LOGGING_ENABLE_LOGHANDLER */

int
netsnmp_remove_loghandler( netsnmp_log_handler *logh )
{
    int i;
    if (!logh)
        return 0;

    if (logh->prev)
        logh->prev->next = logh->next;
    else
        logh_head = logh->next;

    if (logh->next)
        logh->next->prev = logh->prev;

    for (i=LOG_EMERG; i<=logh->priority; i++)
        logh_priorities[i] = NULL;
    free(NETSNMP_REMOVE_CONST(char*, logh->token));
    SNMP_FREE(logh);

    return 1;
}

/* ==================================================== */

#ifndef NETSNMP_FEATURE_REMOVE_LOGGING_STDIO
int
log_handler_stdouterr(  netsnmp_log_handler* logh, int pri, const char *str)
{
    static int      newline = 1;	 /* MTCRITICAL_RESOURCE */
    const char     *newline_ptr;
    char            sbuf[40];

    if (netsnmp_ds_get_boolean(NETSNMP_DS_LIBRARY_ID, 
                               NETSNMP_DS_LIB_LOG_TIMESTAMP) && newline) {
        sprintf_stamp(NULL, sbuf);
    } else {
        strcpy(sbuf, "");
    }
    /*
     * Remember whether or not the current line ends with a newline for the
     * next call of log_handler_stdouterr().
     */
    newline_ptr = strrchr(str, '\n');
    newline = newline_ptr && newline_ptr[1] == 0;

    if (logh->imagic)
       printf(         "%s%s", sbuf, str);
    else
       fprintf(stderr, "%s%s", sbuf, str);

    return 1;
}
#endif /* NETSNMP_FEATURE_REMOVE_LOGGING_STDIO */


#ifndef NETSNMP_FEATURE_REMOVE_LOGGING_SYSLOG
#ifdef WIN32
int
log_handler_syslog(  netsnmp_log_handler* logh, int pri, const char *str)
{
    WORD            etype;
    LPCTSTR         event_msg[2];
    HANDLE          eventlog_h = logh->magic;

        /*
         **  EVENT TYPES:
         **
         **  Information (EVENTLOG_INFORMATION_TYPE)
         **      Information events indicate infrequent but significant
         **      successful operations.
         **  Warning (EVENTLOG_WARNING_TYPE)
         **      Warning events indicate problems that are not immediately
         **      significant, but that may indicate conditions that could
         **      cause future problems. Resource consumption is a good
         **      candidate for a warning event.
         **  Error (EVENTLOG_ERROR_TYPE)
         **      Error events indicate significant problems that the user
         **      should know about. Error events usually indicate a loss of
         **      functionality or data.
         */
    switch (pri) {
        case LOG_EMERG:
        case LOG_ALERT:
        case LOG_CRIT:
        case LOG_ERR:
            etype = EVENTLOG_ERROR_TYPE;
            break;
        case LOG_WARNING:
            etype = EVENTLOG_WARNING_TYPE;
            break;
        case LOG_NOTICE:
        case LOG_INFO:
        case LOG_DEBUG:
            etype = EVENTLOG_INFORMATION_TYPE;
            break;
        default:
            etype = EVENTLOG_INFORMATION_TYPE;
            break;
    }
    event_msg[0] = str;
    event_msg[1] = NULL;
    /* NOTE: 4th parameter must match winservice.mc:MessageId value */
    if (!ReportEvent(eventlog_h, etype, 0, 100, NULL, 1, 0, event_msg, NULL)) {
	    /*
	     * Hmmm.....
	     * Maybe disable this handler, and log the error ?
	     */
        fprintf(stderr, "Could not report event.  Last error: 0x%x\n",
			GetLastError());
        return 0;
    }
    return 1;
}
#else
int
log_handler_syslog(  netsnmp_log_handler* logh, int pri, const char *str)
{
	/*
	 * XXX
	 * We've got three items of information to work with:
	 *     Is the syslog currently open?
	 *     What ident string to use?
	 *     What facility to log to?
	 *
	 * We've got two "magic" locations (imagic & magic) plus the token
	 */
    if (!(logh->imagic)) {
        const char *ident    = logh->token;
        int   facility = (int)(intptr_t)logh->magic;
        if (!ident)
            ident = netsnmp_ds_get_string(NETSNMP_DS_LIBRARY_ID,
                                          NETSNMP_DS_LIB_APPTYPE);
        openlog(ident, LOG_CONS | LOG_PID, facility);
        logh->imagic = 1;
    }
    syslog( pri, "%s", str );
    return 1;
}
#endif /* !WIN32 */
#endif /* NETSNMP_FEATURE_REMOVE_LOGGING_SYSLOG */


#ifndef NETSNMP_FEATURE_REMOVE_LOGGING_FILE
int
log_handler_file(    netsnmp_log_handler* logh, int pri, const char *str)
{
    FILE           *fhandle;
    char            sbuf[40];

    /*
     * We use imagic to save information about whether the next output
     * will start a new line, and thus might need a timestamp
     */
    if (netsnmp_ds_get_boolean(NETSNMP_DS_LIBRARY_ID, 
                               NETSNMP_DS_LIB_LOG_TIMESTAMP) && logh->imagic) {
        sprintf_stamp(NULL, sbuf);
    } else {
        strcpy(sbuf, "");
    }

    /*
     * If we haven't already opened the file, then do so.
     * Save the filehandle pointer for next time.
     *
     * Note that this should still work, even if the file
     * is closed in the meantime (e.g. a regular "cleanup" sweep)
     */
    fhandle = (FILE*)logh->magic;
    if (!logh->magic) {
        fhandle = fopen(logh->token, "a+");
        if (!fhandle)
            return 0;
        logh->magic = (void*)fhandle;
    }
    fprintf(fhandle, "%s%s", sbuf, str);
    fflush(fhandle);
    logh->imagic = str[strlen(str) - 1] == '\n';
    return 1;
}
#endif /* NETSNMP_FEATURE_REMOVE_LOGGING_FILE */

int
log_handler_callback(netsnmp_log_handler* logh, int pri, const char *str)
{
	/*
	 * XXX - perhaps replace 'snmp_call_callbacks' processing
	 *       with individual callback log_handlers ??
	 */
    struct snmp_log_message slm;
    int             dodebug = snmp_get_do_debugging();

    slm.priority = pri;
    slm.msg = str;
    if (dodebug)            /* turn off debugging inside the callbacks else will loop */
        snmp_set_do_debugging(0);
    snmp_call_callbacks(SNMP_CALLBACK_LIBRARY, SNMP_CALLBACK_LOGGING, &slm);
    if (dodebug)
        snmp_set_do_debugging(dodebug);
    return 1;
}

int
log_handler_null(    netsnmp_log_handler* logh, int pri, const char *str)
{
    /*
     * Dummy log handler - just throw away the error completely
     * You probably don't really want to do this!
     */
    return 1;
}

void
snmp_log_string(int priority, const char *str)
{
#ifndef NETSNMP_FEATURE_REMOVE_LOGGING_STDIO
    static int stderr_enabled = 0;
    static netsnmp_log_handler lh = { 1, 0, 0, 0, "stderr",
                                      log_handler_stdouterr, 0, NULL,  NULL,
                                      NULL };
#endif /* NETSNMP_FEATURE_REMOVE_LOGGING_STDIO */
    netsnmp_log_handler *logh;

    /*
     * We've got to be able to log messages *somewhere*!
     * If you don't want stderr logging, then enable something else.
     */
    if (0 == logh_enabled) {
#ifndef NETSNMP_FEATURE_REMOVE_LOGGING_STDIO
        if (!stderr_enabled) {
            ++stderr_enabled;
            netsnmp_set_line_buffering(stderr);
        }
        log_handler_stdouterr( &lh, priority, str );
#endif /* NETSNMP_FEATURE_REMOVE_LOGGING_STDIO */

        return;
    }
#ifndef NETSNMP_FEATURE_REMOVE_LOGGING_STDIO
    else if (stderr_enabled) {
        stderr_enabled = 0;
        log_handler_stdouterr( &lh, LOG_INFO,
                               "Log handling defined - disabling stderr\n" );
    }
#endif /* NETSNMP_FEATURE_REMOVE_LOGGING_STDIO */
        

    /*
     * Start at the given priority, and work "upwards"....
     */
    logh = logh_priorities[priority];
    for ( ; logh; logh = logh->next ) {
        /*
         * ... but skipping any handlers with a "maximum priority"
         *     that we have already exceeded. And don't forget to
         *     ensure this logging is turned on (see snmp_disable_stderrlog
         *     and its cohorts).
         */
        if (logh->enabled && (priority >= logh->pri_max))
            logh->handler( logh, priority, str );
    }
}

/* ==================================================== */


/**
 * This snmp logging function allows variable argument list given the
 * specified priority, format and a populated va_list structure.
 * The default logfile this function writes to is /var/log/snmpd.log.
 *
 * @param priority is an integer representing the type of message to be written
 *	to the snmp log file.  The types are errors, warning, and information.
 *      - The error types are:
 *        - LOG_EMERG       system is unusable 
 *        - LOG_ALERT       action must be taken immediately 
 *        - LOG_CRIT        critical conditions 
 *        - LOG_ERR         error conditions
 *      - The warning type is:
 *        - LOG_WARNING     warning conditions 
 *      - The information types are:
 *        - LOG_NOTICE      normal but significant condition
 *        - LOG_INFO        informational
 *        - LOG_DEBUG       debug-level messages
 *
 * @param format is a pointer to a char representing the variable argument list
 *	format used.
 *
 * @param ap is a va_list type used to traverse the list of arguments.
 *
 * @return Returns 0 on success, -1 when the code could not format the log-
 *         string, -2 when dynamic memory could not be allocated if the length
 *         of the log buffer is greater then 1024 bytes.  For each of these
 *         errors a LOG_ERR messgae is written to the logfile.
 *
 * @see snmp_log
 */
int
snmp_vlog(int priority, const char *format, va_list ap)
{
    char            buffer[LOGLENGTH];
    int             length;
    char           *dynamic;
    va_list         aq;

    va_copy(aq, ap);
    length = vsnprintf(buffer, LOGLENGTH, format, ap);
    va_end(ap);

    if (length == 0) {
#ifdef NEED_VA_END_AFTER_VA_COPY
        va_end(aq);
#endif
        return (0);             /* Empty string */
    }

    if (length == -1) {
        snmp_log_string(LOG_ERR, "Could not format log-string\n");
#ifdef NEED_VA_END_AFTER_VA_COPY
        va_end(aq);
#endif
        return (-1);
    }

    if (length < LOGLENGTH) {
        snmp_log_string(priority, buffer);
#ifdef NEED_VA_END_AFTER_VA_COPY
        va_end(aq);
#endif
        return (0);
    }

    dynamic = (char *) malloc(length + 1);
    if (dynamic == NULL) {
        snmp_log_string(LOG_ERR,
                        "Could not allocate memory for log-message\n");
        snmp_log_string(priority, buffer);
#ifdef NEED_VA_END_AFTER_VA_COPY
        va_end(aq);
#endif
        return (-2);
    }

    vsnprintf(dynamic, length + 1, format, aq);
    snmp_log_string(priority, dynamic);
    free(dynamic);
    va_end(aq);
    return 0;
}

/**
 * This snmp logging function allows variable argument list given the
 * specified format and priority.  Calls the snmp_vlog function.
 * The default logfile this function writes to is /var/log/snmpd.log.
 *
 * @see snmp_vlog
 */
int
snmp_log(int priority, const char *format, ...)
{
    va_list         ap;
    int             ret;
    va_start(ap, format);
    ret = snmp_vlog(priority, format, ap);
    va_end(ap);
    return (ret);
}

/*
 * log a critical error.
 */
void
snmp_log_perror(const char *s)
{
    char           *error = strerror(errno);
    if (s) {
        if (error)
            snmp_log(LOG_ERR, "%s: %s\n", s, error);
        else
            snmp_log(LOG_ERR, "%s: Error %d out-of-range\n", s, errno);
    } else {
        if (error)
            snmp_log(LOG_ERR, "%s\n", error);
        else
            snmp_log(LOG_ERR, "Error %d out-of-range\n", errno);
    }
}

#ifndef NETSNMP_FEATURE_REMOVE_LOGGING_EXTERNAL
/* external access to logh_head variable */
netsnmp_log_handler  *
get_logh_head(void)
{
	return logh_head;
}
#endif /* NETSNMP_FEATURE_REMOVE_LOGGING_EXTERNAL */

/**  @} */
