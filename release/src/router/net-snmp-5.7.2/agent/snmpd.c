/*
 * snmpd.c
 */
/** @defgroup agent The Net-SNMP agent
 * The snmp agent responds to SNMP queries from management stations
 */
/* Portions of this file are subject to the following copyrights.  See
 * the Net-SNMP's COPYING file for more details and other copyrights
 * that may apply:
 */
/*
 * Copyright 1988, 1989 by Carnegie Mellon University
 * 
 * All Rights Reserved
 * 
 * Permission to use, copy, modify, and distribute this software and its 
 * documentation for any purpose and without fee is hereby granted, 
 * provided that the above copyright notice appear in all copies and that
 * both that copyright notice and this permission notice appear in 
 * supporting documentation, and that the name of CMU not be
 * used in advertising or publicity pertaining to distribution of the
 * software without specific, written prior permission.  
 * 
 * CMU DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE, INCLUDING
 * ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS, IN NO EVENT SHALL
 * CMU BE LIABLE FOR ANY SPECIAL, INDIRECT OR CONSEQUENTIAL DAMAGES OR
 * ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS,
 * WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION,
 * ARISING OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS
 * SOFTWARE.
 * *****************************************************************
 */
/*
 * Copyright © 2003 Sun Microsystems, Inc. All rights reserved.
 * Use is subject to license terms specified in the COPYING file
 * distributed with the Net-SNMP package.
 */
#include <net-snmp/net-snmp-config.h>
#include <net-snmp/net-snmp-features.h>
#include <net-snmp/types.h>

#if HAVE_IO_H
#include <io.h>
#endif
#include <stdio.h>
#include <errno.h>
#if HAVE_STRING_H
#include <string.h>
#else
#include <strings.h>
#endif
#if HAVE_STDLIB_H
#include <stdlib.h>
#endif
#if HAVE_UNISTD_H
#include <unistd.h>
#endif
#include <sys/types.h>
#if HAVE_NETINET_IN_H
#include <netinet/in.h>
#endif
#if HAVE_ARPA_INET_H
#include <arpa/inet.h>
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
#if HAVE_SYS_SELECT_H
#include <sys/select.h>
#endif
#if HAVE_SYS_SOCKET_H
#include <sys/socket.h>
#endif
#if HAVE_NET_IF_H
#include <net/if.h>
#endif
#if HAVE_INET_MIB2_H
#include <inet/mib2.h>
#endif
#if HAVE_SYS_IOCTL_H
#include <sys/ioctl.h>
#endif
#if HAVE_SYS_FILE_H
#include <sys/file.h>
#endif
#ifdef HAVE_FCNTL_H
#include <fcntl.h>
#endif
#if HAVE_SYS_WAIT_H
#include <sys/wait.h>
#endif
#include <signal.h>
#ifdef HAVE_SYS_PARAM_H
#include <sys/param.h>
#endif
#if HAVE_PROCESS_H              /* Win32-getpid */
#include <process.h>
#endif
#if HAVE_LIMITS_H
#include <limits.h>
#endif
#if HAVE_PWD_H
#include <pwd.h>
#endif
#if HAVE_GRP_H
#include <grp.h>
#endif
#ifdef HAVE_CRTDBG_H
#include <crtdbg.h>
#endif

#ifndef PATH_MAX
# ifdef _POSIX_PATH_MAX
#  define PATH_MAX _POSIX_PATH_MAX
# else
#  define PATH_MAX 255
# endif
#endif

#ifndef FD_SET
typedef long    fd_mask;
#define NFDBITS (sizeof(fd_mask) * NBBY)        /* bits per mask */
#define FD_SET(n, p)    ((p)->fds_bits[(n)/NFDBITS] |= (1 << ((n) % NFDBITS)))
#define FD_CLR(n, p)    ((p)->fds_bits[(n)/NFDBITS] &= ~(1 << ((n) % NFDBITS)))
#define FD_ISSET(n, p)  ((p)->fds_bits[(n)/NFDBITS] & (1 << ((n) % NFDBITS)))
#define FD_ZERO(p)      memset((p), 0, sizeof(*(p)))
#endif

#include <net-snmp/net-snmp-includes.h>
#include <net-snmp/agent/net-snmp-agent-includes.h>

#include <net-snmp/library/fd_event_manager.h>
#include <net-snmp/library/large_fd_set.h>

#include "m2m.h"
#include <net-snmp/agent/agent_module_config.h>
#include <net-snmp/agent/mib_module_config.h>

#include "snmpd.h"

#include <net-snmp/agent/mib_modules.h>

#include <net-snmp/agent/agent_trap.h>

#include <net-snmp/agent/table.h>
#include <net-snmp/agent/table_iterator.h>

/*
 * Include winservice.h to support Windows Service
 */
#ifdef WIN32
#include <windows.h>
#include <tchar.h>
#include <net-snmp/library/winservice.h>

#define WIN32SERVICE

#endif

netsnmp_feature_want(logging_file)
netsnmp_feature_want(logging_stdio)
netsnmp_feature_want(logging_syslog)

/*
 * Globals.
 */
#ifdef NETSNMP_USE_LIBWRAP
#include <tcpd.h>
#endif                          /* NETSNMP_USE_LIBWRAP */

#define TIMETICK         500000L

int             snmp_dump_packet;
static int      reconfig = 0;
int             Facility = LOG_DAEMON;

#ifdef WIN32SERVICE
/*
 * SNMP Agent Status 
 */
#define AGENT_RUNNING 1
#define AGENT_STOPPED 0
int             agent_status = AGENT_STOPPED;
/* app_name_long used for Event Log (syslog), SCM, registry etc */
LPCTSTR         app_name_long = _T("Net-SNMP Agent");     /* Application Name */
#endif

const char     *app_name = "snmpd";

extern int      netsnmp_running;
#ifdef USING_UTIL_FUNCS_RESTART_MODULE
extern char   **argvrestartp;
extern char    *argvrestart;
extern char    *argvrestartname;
#endif /* USING_UTIL_FUNCS_RESTART_MODULE */

#ifdef USING_SMUX_MODULE
#include <mibgroup/smux/smux.h>
#endif /* USING_SMUX_MODULE */

/*
 * Prototypes.
 */
int             snmp_read_packet(int);
int             snmp_input(int, netsnmp_session *, int, netsnmp_pdu *,
                           void *);
static void     usage(char *);
static void     SnmpTrapNodeDown(void);
static int      receive(void);
#ifdef WIN32SERVICE
void            StopSnmpAgent(void);
int             SnmpDaemonMain(int argc, TCHAR * argv[]);
int __cdecl     _tmain(int argc, TCHAR * argv[]);
#else
int             main(int, char **);
#endif

/*
 * These definitions handle 4.2 systems without additional syslog facilities.
 */
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


static void
usage(char *prog)
{
#ifdef WIN32SERVICE
    printf("\nUsage:  %s [-register] [-quiet] [OPTIONS] [LISTENING ADDRESSES]"
           "\n        %s [-unregister] [-quiet]", prog, prog);
#else
    printf("\nUsage:  %s [OPTIONS] [LISTENING ADDRESSES]", prog);
#endif
    printf("\n"
           "\n\tVersion:  %s\n%s"
           "\t\t\t  (config search path: %s)\n%s%s",
           netsnmp_get_version(),
           "\tWeb:      http://www.net-snmp.org/\n"
           "\tEmail:    net-snmp-coders@lists.sourceforge.net\n"
           "\n  -a\t\t\tlog addresses\n"
           "  -A\t\t\tappend to the logfile rather than truncating it\n"
           "  -c FILE[,...]\t\tread FILE(s) as configuration file(s)\n"
           "  -C\t\t\tdo not read the default configuration files\n",
           get_configuration_directory(),
           "  -d\t\t\tdump sent and received SNMP packets\n"
           "  -D[TOKEN[,...]]\tturn on debugging output for the given TOKEN(s)\n"
	   "\t\t\t  (try ALL for extremely verbose output)\n"
	   "\t\t\t  Don't put space(s) between -D and TOKEN(s).\n"
           "  -f\t\t\tdo not fork from the shell\n",
#if HAVE_UNISTD_H
           "  -g GID\t\tchange to this numeric gid after opening\n"
	   "\t\t\t  transport endpoints\n"
#endif
           "  -h, --help\t\tdisplay this usage message\n"
           "  -H\t\t\tdisplay configuration file directives understood\n"
           "  -I [-]INITLIST\tlist of mib modules to initialize (or not)\n"
           "\t\t\t  (run snmpd with -Dmib_init for a list)\n"
           "  -L <LOGOPTS>\t\ttoggle options controlling where to log to\n");
    snmp_log_options_usage("\t", stdout);
    printf("  -m MIBLIST\t\tuse MIBLIST instead of the default MIB list\n"
           "  -M DIRLIST\t\tuse DIRLIST as the list of locations to look for MIBs\n"
           "\t\t\t  (default %s)\n%s%s",
#ifndef NETSNMP_DISABLE_MIB_LOADING
           netsnmp_get_mib_directory(),
#else
           "MIBs not loaded",
#endif
           "  -p FILE\t\tstore process id in FILE\n"
           "  -q\t\t\tprint information in a more parsable format\n"
           "  -r\t\t\tdo not exit if files only accessible to root\n"
	   "\t\t\t  cannot be opened\n"
#ifdef WIN32SERVICE
           "  -register\t\tregister as a Windows service\n"
           "  \t\t\t  (followed by -quiet to prevent message popups)\n"
           "  \t\t\t  (followed by the startup parameter list)\n"
           "  \t\t\t  Note that some parameters are not relevant when running as a service\n"
#endif
#if HAVE_UNISTD_H
           "  -u UID\t\tchange to this uid (numeric or textual) after\n"
	   "\t\t\t  opening transport endpoints\n"
#endif
#ifdef WIN32SERVICE
           "  -unregister\t\tunregister as a Windows service\n"
           "  \t\t\t  (followed -quiet to prevent message popups)\n"
#endif
           "  -v, --version\t\tdisplay version information\n"
           "  -V\t\t\tverbose display\n"
#if defined(USING_AGENTX_SUBAGENT_MODULE)|| defined(USING_AGENTX_MASTER_MODULE)
           "  -x ADDRESS\t\tuse ADDRESS as AgentX address\n"
#endif
#ifdef USING_AGENTX_SUBAGENT_MODULE
           "  -X\t\t\trun as an AgentX subagent rather than as an\n"
	   "\t\t\t  SNMP master agent\n"
#endif
           ,
           "\nDeprecated options:\n"
           "  -l FILE\t\tuse -Lf <FILE> instead\n"
           "  -P\t\t\tuse -p instead\n"
           "  -s\t\t\tuse -Lsd instead\n"
           "  -S d|i|0-7\t\tuse -Ls <facility> instead\n"
           "\n"
           );
    exit(1);
}

static void
version(void)
{
    printf("\nNET-SNMP version:  %s\n"
           "Web:               http://www.net-snmp.org/\n"
           "Email:             net-snmp-coders@lists.sourceforge.net\n\n",
           netsnmp_get_version());
    exit(0);
}

RETSIGTYPE
SnmpdShutDown(int a)
{
#ifdef WIN32SERVICE
    extern netsnmp_session *main_session;
#endif
    netsnmp_running = 0;
#ifdef WIN32SERVICE
    /*
     * In case of windows, select() in receive() function will not return 
     * on signal. Thats why following function is called, which closes the 
     * socket descriptors and causes the select() to return
     */
    snmp_close(main_session);
#endif
}

#ifdef SIGHUP
RETSIGTYPE
SnmpdReconfig(int a)
{
    reconfig = 1;
    signal(SIGHUP, SnmpdReconfig);
}
#endif

#ifdef SIGUSR1
extern void     dump_registry(void);
RETSIGTYPE
SnmpdDump(int a)
{
    dump_registry();
    signal(SIGUSR1, SnmpdDump);
}
#endif

RETSIGTYPE
SnmpdCatchRandomSignal(int a)
{
    /* Disable all logs and log the error via syslog */
    snmp_disable_log();
#ifndef NETSNMP_FEATURE_REMOVE_LOGGING_SYSLOG
    snmp_enable_syslog();
#endif /* NETSNMP_FEATURE_REMOVE_LOGGING_SYSLOG */
    snmp_log(LOG_ERR, "Exiting on signal %d\n", a);
#ifndef NETSNMP_FEATURE_REMOVE_LOGGING_SYSLOG
    snmp_disable_syslog();
#endif /* NETSNMP_FEATURE_REMOVE_LOGGING_SYSLOG */
    exit(1);
}

static void
SnmpTrapNodeDown(void)
{
    send_easy_trap(SNMP_TRAP_ENTERPRISESPECIFIC, 2);
    /*
     * XXX  2 - Node Down #define it as NODE_DOWN_TRAP 
     */
}

/*******************************************************************-o-******
 * main - Non Windows
 * SnmpDaemonMain - Windows to support windows service
 *
 * Parameters:
 *	 argc
 *	*argv[]
 *      
 * Returns:
 *	0	Always succeeds.  (?)
 *
 *
 * Setup and start the agent daemon.
 *
 * Also successfully EXITs with zero for some options.
 */
int
#ifdef WIN32SERVICE
SnmpDaemonMain(int argc, TCHAR * argv[])
#else
main(int argc, char *argv[])
#endif
{
    char            options[128] = "aAc:CdD::fhHI:l:L:m:M:n:p:P:qrsS:UvV-:Y:";
    int             arg, i, ret;
    int             dont_fork = 0, do_help = 0;
    int             log_set = 0;
    int             agent_mode = -1;
    char           *pid_file = NULL;
    char            option_compatability[] = "-Le";
#if HAVE_GETPID
    int fd;
    FILE           *PID;
#endif

#ifndef WIN32
    /*
     * close all non-standard file descriptors we may have
     * inherited from the shell.
     */
    for (i = getdtablesize() - 1; i > 2; --i) {
        (void) close(i);
    }
#endif /* #WIN32 */
    
    /*
     * register signals ASAP to prevent default action (usually core)
     * for signals during startup...
     */
#ifdef SIGTERM
    DEBUGMSGTL(("signal", "registering SIGTERM signal handler\n"));
    signal(SIGTERM, SnmpdShutDown);
#endif
#ifdef SIGINT
    DEBUGMSGTL(("signal", "registering SIGINT signal handler\n"));
    signal(SIGINT, SnmpdShutDown);
#endif
#ifdef SIGHUP
    signal(SIGHUP, SIG_IGN);   /* do not terminate on early SIGHUP */
#endif
#ifdef SIGUSR1
    DEBUGMSGTL(("signal", "registering SIGUSR1 signal handler\n"));
    signal(SIGUSR1, SnmpdDump);
#endif
#ifdef SIGPIPE
    DEBUGMSGTL(("signal", "registering SIGPIPE signal handler\n"));
    signal(SIGPIPE, SIG_IGN);   /* 'Inline' failure of wayward readers */
#endif
#ifdef SIGXFSZ
    signal(SIGXFSZ, SnmpdCatchRandomSignal);
#endif

#ifdef NETSNMP_NO_ROOT_ACCESS
    /*
     * Default to no.  
     */
    netsnmp_ds_set_boolean(NETSNMP_DS_APPLICATION_ID, 
			   NETSNMP_DS_AGENT_NO_ROOT_ACCESS, 1);
#endif
    /*
     * Default to NOT running an AgentX master.  
     */
    netsnmp_ds_set_boolean(NETSNMP_DS_APPLICATION_ID, 
			   NETSNMP_DS_AGENT_AGENTX_MASTER, 0);
    netsnmp_ds_set_int(NETSNMP_DS_APPLICATION_ID,
                       NETSNMP_DS_AGENT_AGENTX_TIMEOUT, -1);
    netsnmp_ds_set_int(NETSNMP_DS_APPLICATION_ID,
                       NETSNMP_DS_AGENT_AGENTX_RETRIES, -1);

    netsnmp_ds_set_int(NETSNMP_DS_APPLICATION_ID,
                       NETSNMP_DS_AGENT_CACHE_TIMEOUT, 5);
    /*
     * Add some options if they are available.  
     */
#if HAVE_UNISTD_H
    strcat(options, "g:u:");
#endif
#if defined(USING_AGENTX_SUBAGENT_MODULE)|| defined(USING_AGENTX_MASTER_MODULE)
    strcat(options, "x:");
#endif
#ifdef USING_AGENTX_SUBAGENT_MODULE
    strcat(options, "X");
#endif

    /*
     * This is incredibly ugly, but it's probably the simplest way
     *  to handle the old '-L' option as well as the new '-Lx' style
     */
    for (i=0; i<argc; i++) {
        if (!strcmp(argv[i], "-L"))
            argv[i] = option_compatability;            
    }

#ifndef NETSNMP_FEATURE_REMOVE_LOGGING_SYSLOG
#ifdef WIN32
    snmp_log_syslogname(app_name_long);
#else
    snmp_log_syslogname(app_name);
#endif
#endif /* NETSNMP_FEATURE_REMOVE_LOGGING_SYSLOG */
    netsnmp_ds_set_string(NETSNMP_DS_LIBRARY_ID,
                          NETSNMP_DS_LIB_APPTYPE, app_name);

    /*
     * Now process options normally.  
     */
    while ((arg = getopt(argc, argv, options)) != EOF) {
        switch (arg) {
        case '-':
            if (strcasecmp(optarg, "help") == 0) {
                usage(argv[0]);
            }
            if (strcasecmp(optarg, "version") == 0) {
                version();
            }

            handle_long_opt(optarg);
            break;

        case 'a':
            log_addresses++;
            break;

        case 'A':
            netsnmp_ds_set_boolean(NETSNMP_DS_LIBRARY_ID,
                                   NETSNMP_DS_LIB_APPEND_LOGFILES, 1);
            break;

        case 'c':
            if (optarg != NULL) {
                netsnmp_ds_set_string(NETSNMP_DS_LIBRARY_ID, 
				      NETSNMP_DS_LIB_OPTIONALCONFIG, optarg);
            } else {
                usage(argv[0]);
            }
            break;

        case 'C':
            netsnmp_ds_set_boolean(NETSNMP_DS_LIBRARY_ID, 
				   NETSNMP_DS_LIB_DONT_READ_CONFIGS, 1);
            break;

        case 'd':
            netsnmp_ds_set_boolean(NETSNMP_DS_LIBRARY_ID,
                                   NETSNMP_DS_LIB_DUMP_PACKET,
                                   ++snmp_dump_packet);
            break;

        case 'D':
            debug_register_tokens(optarg);
            snmp_set_do_debugging(1);
            break;

        case 'f':
            dont_fork = 1;
            break;

#if HAVE_UNISTD_H
        case 'g':
            if (optarg != NULL) {
                char           *ecp;
                int             gid;

                gid = strtoul(optarg, &ecp, 10);
#if HAVE_GETGRNAM && HAVE_PWD_H
                if (*ecp) {
                    struct group  *info;

                    info = getgrnam(optarg);
                    gid = info ? info->gr_gid : -1;
                    endgrent();
                }
#endif
                if (gid < 0) {
                    fprintf(stderr, "Bad group id: %s\n", optarg);
                    exit(1);
                }
                netsnmp_ds_set_int(NETSNMP_DS_APPLICATION_ID, 
				   NETSNMP_DS_AGENT_GROUPID, gid);
            } else {
                usage(argv[0]);
            }
            break;
#endif

        case 'h':
            usage(argv[0]);
            break;

        case 'H':
            do_help = 1;
            break;

        case 'I':
            if (optarg != NULL) {
                add_to_init_list(optarg);
            } else {
                usage(argv[0]);
            }
            break;

#ifndef NETSNMP_FEATURE_REMOVE_LOGGING_FILE
        case 'l':
            printf("Warning: -l option is deprecated, use -Lf <file> instead\n");
            if (optarg != NULL) {
                if (strlen(optarg) > PATH_MAX) {
                    fprintf(stderr,
                            "%s: logfile path too long (limit %d chars)\n",
                            argv[0], PATH_MAX);
                    exit(1);
                }
                snmp_enable_filelog(optarg,
                                    netsnmp_ds_get_boolean(NETSNMP_DS_LIBRARY_ID,
                                                           NETSNMP_DS_LIB_APPEND_LOGFILES));
                log_set = 1;
            } else {
                usage(argv[0]);
            }
            break;
#endif /* NETSNMP_FEATURE_REMOVE_LOGGING_FILE */

        case 'L':
	    if  (snmp_log_options( optarg, argc, argv ) < 0 ) {
                usage(argv[0]);
            }
            log_set = 1;
            break;

        case 'm':
            if (optarg != NULL) {
                setenv("MIBS", optarg, 1);
            } else {
                usage(argv[0]);
            }
            break;

        case 'M':
            if (optarg != NULL) {
                setenv("MIBDIRS", optarg, 1);
            } else {
                usage(argv[0]);
            }
            break;

        case 'n':
            if (optarg != NULL) {
                app_name = optarg;
                netsnmp_ds_set_string(NETSNMP_DS_LIBRARY_ID,
                                      NETSNMP_DS_LIB_APPTYPE, app_name);
            } else {
                usage(argv[0]);
            }
            break;

        case 'P':
            printf("Warning: -P option is deprecated, use -p instead\n");
        case 'p':
            if (optarg != NULL) {
                pid_file = optarg;
            } else {
                usage(argv[0]);
            }
            break;

        case 'q':
            netsnmp_ds_set_boolean(NETSNMP_DS_LIBRARY_ID, 
                                   NETSNMP_DS_LIB_QUICK_PRINT, 1);
            break;

        case 'r':
            netsnmp_ds_toggle_boolean(NETSNMP_DS_APPLICATION_ID, 
				      NETSNMP_DS_AGENT_NO_ROOT_ACCESS);
            break;

#ifndef NETSNMP_FEATURE_REMOVE_LOGGING_SYSLOG
        case 's':
            printf("Warning: -s option is deprecated, use -Lsd instead\n");
            snmp_enable_syslog();
            log_set = 1;
            break;

        case 'S':
            printf("Warning: -S option is deprecated, use -Ls <facility> instead\n");
            if (optarg != NULL) {
                switch (*optarg) {
                case 'd':
                case 'D':
                    Facility = LOG_DAEMON;
                    break;
                case 'i':
                case 'I':
                    Facility = LOG_INFO;
                    break;
                case '0':
                    Facility = LOG_LOCAL0;
                    break;
                case '1':
                    Facility = LOG_LOCAL1;
                    break;
                case '2':
                    Facility = LOG_LOCAL2;
                    break;
                case '3':
                    Facility = LOG_LOCAL3;
                    break;
                case '4':
                    Facility = LOG_LOCAL4;
                    break;
                case '5':
                    Facility = LOG_LOCAL5;
                    break;
                case '6':
                    Facility = LOG_LOCAL6;
                    break;
                case '7':
                    Facility = LOG_LOCAL7;
                    break;
                default:
                    fprintf(stderr, "invalid syslog facility: -S%c\n",*optarg);
                    usage(argv[0]);
                }
                snmp_enable_syslog_ident(snmp_log_syslogname(NULL), Facility);
                log_set = 1;
            } else {
                fprintf(stderr, "no syslog facility specified\n");
                usage(argv[0]);
            }
            break;
#endif /* NETSNMP_FEATURE_REMOVE_LOGGING_SYSLOG */

        case 'U':
            netsnmp_ds_toggle_boolean(NETSNMP_DS_APPLICATION_ID, 
				      NETSNMP_DS_AGENT_LEAVE_PIDFILE);
            break;

#if HAVE_UNISTD_H
        case 'u':
            if (optarg != NULL) {
                char           *ecp;
                int             uid;

                uid = strtoul(optarg, &ecp, 10);
#if HAVE_GETPWNAM && HAVE_PWD_H
                if (*ecp) {
                    struct passwd  *info;

                    info = getpwnam(optarg);
                    uid = info ? info->pw_uid : -1;
                    endpwent();
                }
#endif
                if (uid < 0) {
                    fprintf(stderr, "Bad user id: %s\n", optarg);
                    exit(1);
                }
                netsnmp_ds_set_int(NETSNMP_DS_APPLICATION_ID, 
				   NETSNMP_DS_AGENT_USERID, uid);
            } else {
                usage(argv[0]);
            }
            break;
#endif

        case 'v':
            version();

        case 'V':
            netsnmp_ds_set_boolean(NETSNMP_DS_APPLICATION_ID, 
				   NETSNMP_DS_AGENT_VERBOSE, 1);
            break;

#if defined(USING_AGENTX_SUBAGENT_MODULE)|| defined(USING_AGENTX_MASTER_MODULE)
        case 'x':
            if (optarg != NULL) {
                netsnmp_ds_set_string(NETSNMP_DS_APPLICATION_ID, 
				      NETSNMP_DS_AGENT_X_SOCKET, optarg);
            } else {
                usage(argv[0]);
            }
            netsnmp_ds_set_boolean(NETSNMP_DS_APPLICATION_ID, 
				   NETSNMP_DS_AGENT_AGENTX_MASTER, 1);
            break;
#endif

        case 'X':
#if defined(USING_AGENTX_SUBAGENT_MODULE)
            agent_mode = SUB_AGENT;
#else
            fprintf(stderr, "%s: Illegal argument -X:"
		            "AgentX support not compiled in.\n", argv[0]);
            usage(argv[0]);
            exit(1);
#endif
            break;

        case 'Y':
            netsnmp_config_remember(optarg);
            break;

        default:
            usage(argv[0]);
            break;
        }
    }

    if (do_help) {
        netsnmp_ds_set_boolean(NETSNMP_DS_APPLICATION_ID, 
                               NETSNMP_DS_AGENT_NO_ROOT_ACCESS, 1);
        init_agent(app_name);        /* register our .conf handlers */
        init_mib_modules();
        init_snmp(app_name);
        fprintf(stderr, "Configuration directives understood:\n");
        read_config_print_usage("  ");
        exit(0);
    }

    if (optind < argc) {
#ifndef NETSNMP_NO_LISTEN_SUPPORT
        /*
         * There are optional transport addresses on the command line.  
         */
        DEBUGMSGTL(("snmpd/main", "optind %d, argc %d\n", optind, argc));
        for (i = optind; i < argc; i++) {
            char *c, *astring;
            if ((c = netsnmp_ds_get_string(NETSNMP_DS_APPLICATION_ID, 
					   NETSNMP_DS_AGENT_PORTS))) {
                astring = (char*)malloc(strlen(c) + 2 + strlen(argv[i]));
                if (astring == NULL) {
                    fprintf(stderr, "malloc failure processing argv[%d]\n", i);
                    exit(1);
                }
                sprintf(astring, "%s,%s", c, argv[i]);
                netsnmp_ds_set_string(NETSNMP_DS_APPLICATION_ID, 
				      NETSNMP_DS_AGENT_PORTS, astring);
                SNMP_FREE(astring);
            } else {
                netsnmp_ds_set_string(NETSNMP_DS_APPLICATION_ID, 
				      NETSNMP_DS_AGENT_PORTS, argv[i]);
            }
        }
        DEBUGMSGTL(("snmpd/main", "port spec: %s\n",
                    netsnmp_ds_get_string(NETSNMP_DS_APPLICATION_ID, 
					  NETSNMP_DS_AGENT_PORTS)));
#else /* NETSNMP_NO_LISTEN_SUPPORT */
        fprintf(stderr, "You specified ports to open; this agent was built to only send notifications\n");
        exit(1);
#endif /* NETSNMP_NO_LISTEN_SUPPORT */
    }

#ifdef NETSNMP_LOGFILE
#ifndef NETSNMP_FEATURE_REMOVE_LOGGING_FILE
    if (0 == log_set)
        snmp_enable_filelog(NETSNMP_LOGFILE,
                            netsnmp_ds_get_boolean(NETSNMP_DS_LIBRARY_ID,
                                                   NETSNMP_DS_LIB_APPEND_LOGFILES));
#endif /* NETSNMP_FEATURE_REMOVE_LOGGING_FILE */
#endif

#ifdef USING_UTIL_FUNCS_RESTART_MODULE
    {
        /*
         * Initialize a argv set to the current for restarting the agent.
         */
        char *cptr, **argvptr;

        argvrestartp = (char **)malloc((argc + 2) * sizeof(char *));
        argvptr = argvrestartp;
        for (i = 0, ret = 1; i < argc; i++) {
            ret += strlen(argv[i]) + 1;
        }
        argvrestart = (char *) malloc(ret);
        argvrestartname = (char *) malloc(strlen(argv[0]) + 1);
        if (!argvrestartp || !argvrestart || !argvrestartname) {
            fprintf(stderr, "malloc failure processing argvrestart\n");
            exit(1);
        }
        strcpy(argvrestartname, argv[0]);

        for (cptr = argvrestart, i = 0; i < argc; i++) {
            strcpy(cptr, argv[i]);
            *(argvptr++) = cptr;
            cptr += strlen(argv[i]) + 1;
        }
    }
#endif /* USING_UTIL_FUNCS_RESTART_MODULE */

    if (agent_mode == -1) {
        if (strstr(argv[0], "agentxd") != NULL) {
            netsnmp_ds_set_boolean(NETSNMP_DS_APPLICATION_ID,
				   NETSNMP_DS_AGENT_ROLE, SUB_AGENT);
        } else {
            netsnmp_ds_set_boolean(NETSNMP_DS_APPLICATION_ID,
				   NETSNMP_DS_AGENT_ROLE, MASTER_AGENT);
        }
    } else {
        netsnmp_ds_set_boolean(NETSNMP_DS_APPLICATION_ID,
			       NETSNMP_DS_AGENT_ROLE, agent_mode);
    }

    SOCK_STARTUP;
    if (init_agent(app_name) != 0) {
        snmp_log(LOG_ERR, "Agent initialization failed\n");
        exit(1);
    }
    init_mib_modules();

    /*
     * start library 
     */
    init_snmp(app_name);

    if ((ret = init_master_agent()) != 0) {
        /*
         * Some error opening one of the specified agent transports.  
         */
        snmp_log(LOG_ERR, "Server Exiting with code 1\n");
        exit(1);
    }

    /*
     * Initialize the world.  Detach from the shell.  Create initial user.  
     */
    if(!dont_fork) {
        int quit = ! netsnmp_ds_get_boolean(NETSNMP_DS_APPLICATION_ID,
                                            NETSNMP_DS_AGENT_QUIT_IMMEDIATELY);
        ret = netsnmp_daemonize(quit,
#ifndef NETSNMP_FEATURE_REMOVE_LOGGING_STDIO
                                snmp_stderrlog_status()
#else /* NETSNMP_FEATURE_REMOVE_LOGGING_STDIO */
                                0
#endif /* NETSNMP_FEATURE_REMOVE_LOGGING_STDIO */
            );
        /*
         * xxx-rks: do we care if fork fails? I think we should...
         */
        if(ret != 0) {
            snmp_log(LOG_ERR, "Server Exiting with code 1\n");
            exit(1);
        }
    }

#if HAVE_GETPID
    if (pid_file != NULL) {
        /*
         * unlink the pid_file, if it exists, prior to open.  Without
         * doing this the open will fail if the user specified pid_file
         * already exists.
         */
        unlink(pid_file);
        fd = open(pid_file, O_CREAT | O_EXCL | O_WRONLY, 0600);
        if (fd == -1) {
            snmp_log_perror(pid_file);
            if (!netsnmp_ds_get_boolean(NETSNMP_DS_APPLICATION_ID, 
                                        NETSNMP_DS_AGENT_NO_ROOT_ACCESS)) {
                exit(1);
            }
        } else {
            if ((PID = fdopen(fd, "w")) == NULL) {
                snmp_log_perror(pid_file);
                exit(1);
            } else {
                fprintf(PID, "%d\n", (int) getpid());
                fclose(PID);
            }
#ifndef _MSC_VER
            /* The sequence open()/fdopen()/fclose()/close() makes MSVC crash,
               hence skip the close() call when using the MSVC runtime. */
            close(fd);
#endif
        }
    }
#endif

#if defined(HAVE_UNISTD_H) && (defined(HAVE_CHOWN) || defined(HAVE_SETGID) || defined(HAVE_SETUID))
    {
    const char     *persistent_dir;
    int             uid, gid;

    persistent_dir = get_persistent_directory();
    mkdirhier( persistent_dir, NETSNMP_AGENT_DIRECTORY_MODE, 0 );
   
    uid = netsnmp_ds_get_int(NETSNMP_DS_APPLICATION_ID, 
			     NETSNMP_DS_AGENT_USERID);
    gid = netsnmp_ds_get_int(NETSNMP_DS_APPLICATION_ID, 
			     NETSNMP_DS_AGENT_GROUPID);
    
#ifdef HAVE_CHOWN
    if ( uid != 0 || gid != 0 )
        chown( persistent_dir, uid, gid );
#endif

#ifdef HAVE_SETGID
    if ((gid = netsnmp_ds_get_int(NETSNMP_DS_APPLICATION_ID, 
				  NETSNMP_DS_AGENT_GROUPID)) > 0) {
        DEBUGMSGTL(("snmpd/main", "Changing gid to %d.\n", gid));
        if (setgid(gid) == -1
#ifdef HAVE_SETGROUPS
            || setgroups(1, (gid_t *)&gid) == -1
#endif
            ) {
            snmp_log_perror("setgid failed");
            if (!netsnmp_ds_get_boolean(NETSNMP_DS_APPLICATION_ID, 
					NETSNMP_DS_AGENT_NO_ROOT_ACCESS)) {
                exit(1);
            }
        }
    }
#endif
#ifdef HAVE_SETUID
    if ((uid = netsnmp_ds_get_int(NETSNMP_DS_APPLICATION_ID, 
				  NETSNMP_DS_AGENT_USERID)) > 0) {
#if HAVE_GETPWNAM && HAVE_PWD_H && HAVE_INITGROUPS
        struct passwd *info;

        /*
         * Set supplementary groups before changing UID
         *   (which probably involves giving up privileges)
         */
        info = getpwuid(uid);
        if (info) {
            DEBUGMSGTL(("snmpd/main", "Supplementary groups for %s.\n", info->pw_name));
            if (initgroups(info->pw_name, (gid != 0 ? (gid_t)gid : info->pw_gid)) == -1) {
                snmp_log_perror("initgroups failed");
                if (!netsnmp_ds_get_boolean(NETSNMP_DS_APPLICATION_ID, 
                                            NETSNMP_DS_AGENT_NO_ROOT_ACCESS)) {
                    exit(1);
                }
            }
        }
        endpwent();
#endif
        DEBUGMSGTL(("snmpd/main", "Changing uid to %d.\n", uid));
        if (setuid(uid) == -1) {
            snmp_log_perror("setuid failed");
            if (!netsnmp_ds_get_boolean(NETSNMP_DS_APPLICATION_ID, 
					NETSNMP_DS_AGENT_NO_ROOT_ACCESS)) {
                exit(1);
            }
        }
    }
#endif
    }
#endif

    /*
     * Store persistent data immediately in case we crash later.  
     */
    snmp_store(app_name);

#ifdef SIGHUP
    DEBUGMSGTL(("signal", "registering SIGHUP signal handler\n"));
    signal(SIGHUP, SnmpdReconfig);
#endif

    /*
     * Send coldstart trap if possible.  
     */
    send_easy_trap(0, 0);

    /*
     * We're up, log our version number.  
     */
    snmp_log(LOG_INFO, "NET-SNMP version %s\n", netsnmp_get_version());
#ifdef WIN32SERVICE
    agent_status = AGENT_RUNNING;
#endif
    netsnmp_addrcache_initialise();

    /*
     * Forever monitor the dest_port for incoming PDUs.  
     */
    DEBUGMSGTL(("snmpd/main", "We're up.  Starting to process data.\n"));
    if (!netsnmp_ds_get_boolean(NETSNMP_DS_APPLICATION_ID, 
				NETSNMP_DS_AGENT_QUIT_IMMEDIATELY))
        receive();
    DEBUGMSGTL(("snmpd/main", "sending shutdown trap\n"));
    SnmpTrapNodeDown();
    DEBUGMSGTL(("snmpd/main", "Bye...\n"));
    snmp_shutdown(app_name);
    shutdown_master_agent();
    shutdown_agent();

    if (!netsnmp_ds_get_boolean(NETSNMP_DS_APPLICATION_ID, 
				NETSNMP_DS_AGENT_LEAVE_PIDFILE) &&
	(pid_file != NULL)) {
        unlink(pid_file);
    }
#ifdef WIN32SERVICE
    agent_status = AGENT_STOPPED;
#endif

#ifdef USING_UTIL_FUNCS_RESTART_MODULE
    SNMP_FREE(argvrestartname);
    SNMP_FREE(argvrestart);
    SNMP_FREE(argvrestartp);
#endif /* USING_UTIL_FUNCS_RESTART_MODULE */

    SOCK_CLEANUP;
    return 0;
}                               /* End main() -- snmpd */

#if defined(WIN32)

#include <process.h>
#include <net-snmp/library/snmp_assert.h>

static unsigned s_threadid;
HANDLE s_thread_handle;

static unsigned __stdcall wait_for_stdin(void* arg)
{
    if (getc(stdin) != EOF)
        netsnmp_running = 0;
    return 0;
}

static void create_stdin_waiter_thread(void)
{
    netsnmp_assert(s_thread_handle == 0);
    s_thread_handle = (HANDLE)_beginthreadex(0, 0, wait_for_stdin, 0, 0, &s_threadid);
    netsnmp_assert(s_thread_handle != 0);
}

static void join_stdin_waiter_thread(void)
{
    int result;

    netsnmp_assert(s_thread_handle != 0);
    result = WaitForSingleObject(s_thread_handle, 1000);
    netsnmp_assert(result != WAIT_TIMEOUT);
    CloseHandle(s_thread_handle);
    s_thread_handle = 0;
}
#endif

/*******************************************************************-o-******
 * receive
 *
 * Parameters:
 *      
 * Returns:
 *	0	On success.
 *	-1	System error.
 *
 * Infinite while-loop which monitors incoming messages for the agent.
 * Invoke the established message handlers for incoming messages on a per
 * port basis.  Handle timeouts.
 */
static int
receive(void)
{
    int             numfds;
    netsnmp_large_fd_set readfds, writefds, exceptfds;
    struct timeval  timeout, *tvp = &timeout;
    int             count, block, i;
#ifdef	USING_SMUX_MODULE
    int             sd;
#endif                          /* USING_SMUX_MODULE */

    netsnmp_large_fd_set_init(&readfds, FD_SETSIZE);
    netsnmp_large_fd_set_init(&writefds, FD_SETSIZE);
    netsnmp_large_fd_set_init(&exceptfds, FD_SETSIZE);

    /*
     * ignore early sighup during startup
     */
    reconfig = 0;

#if defined(WIN32)
    create_stdin_waiter_thread();
#endif

    /*
     * Loop-forever: execute message handlers for sockets with data
     */
    while (netsnmp_running) {
        if (reconfig) {
#if HAVE_SIGHOLD
            sighold(SIGHUP);
#endif
            reconfig = 0;
            snmp_log(LOG_INFO, "Reconfiguring daemon\n");
	    /*  Stop and restart logging.  This allows logfiles to be
		rotated etc.  */
	    netsnmp_logging_restart();
	    snmp_log(LOG_INFO, "NET-SNMP version %s restarted\n",
		     netsnmp_get_version());
            update_config();
            send_easy_trap(SNMP_TRAP_ENTERPRISESPECIFIC, 3);
#if HAVE_SIGHOLD
            sigrelse(SIGHUP);
#endif
        }

        /*
         * default to sleeping for a really long time. INT_MAX
         * should be sufficient (eg we don't care if time_t is
         * a long that's bigger than an int).
         */
        tvp = &timeout;
        tvp->tv_sec = INT_MAX;
        tvp->tv_usec = 0;

        numfds = 0;
        NETSNMP_LARGE_FD_ZERO(&readfds);
        NETSNMP_LARGE_FD_ZERO(&writefds);
        NETSNMP_LARGE_FD_ZERO(&exceptfds);
        block = 0;
        snmp_select_info2(&numfds, &readfds, tvp, &block);
        if (block == 1) {
            tvp = NULL;         /* block without timeout */
	}

#ifdef	USING_SMUX_MODULE
        if (smux_listen_sd >= 0) {
            NETSNMP_LARGE_FD_SET(smux_listen_sd, &readfds);
            numfds =
                smux_listen_sd >= numfds ? smux_listen_sd + 1 : numfds;

            for (i = 0; i < smux_snmp_select_list_get_length(); i++) {
                sd = smux_snmp_select_list_get_SD_from_List(i);
                if (sd != 0)
                {
                   NETSNMP_LARGE_FD_SET(sd, &readfds);
                   numfds = sd >= numfds ? sd + 1 : numfds;
                }
            }
        }
#endif                          /* USING_SMUX_MODULE */

#ifndef NETSNMP_FEATURE_REMOVE_FD_EVENT_MANAGER
        netsnmp_external_event_info2(&numfds, &readfds, &writefds, &exceptfds);
#endif /* NETSNMP_FEATURE_REMOVE_FD_EVENT_MANAGER */

    reselect:
        for (i = 0; i < NUM_EXTERNAL_SIGS; i++) {
            if (external_signal_scheduled[i]) {
                external_signal_scheduled[i]--;
                external_signal_handler[i](i);
            }
        }

        DEBUGMSGTL(("snmpd/select", "select( numfds=%d, ..., tvp=%p)\n",
                    numfds, tvp));
        if (tvp)
            DEBUGMSGTL(("timer", "tvp %ld.%ld\n", (long) tvp->tv_sec,
                        (long) tvp->tv_usec));
        count = netsnmp_large_fd_set_select(numfds, &readfds, &writefds, &exceptfds,
				     tvp);
        DEBUGMSGTL(("snmpd/select", "returned, count = %d\n", count));

        if (count > 0) {

#ifdef USING_SMUX_MODULE
            /*
             * handle the SMUX sd's 
             */
            if (smux_listen_sd >= 0) {
                for (i = 0; i < smux_snmp_select_list_get_length(); i++) {
                    sd = smux_snmp_select_list_get_SD_from_List(i);
                    if (NETSNMP_LARGE_FD_ISSET(sd, &readfds)) {
                        if (smux_process(sd) < 0) {
                            smux_snmp_select_list_del(sd);
                        }
                    }
                }
                /*
                 * new connection 
                 */
                if (NETSNMP_LARGE_FD_ISSET(smux_listen_sd, &readfds)) {
                    if ((sd = smux_accept(smux_listen_sd)) >= 0) {
                        smux_snmp_select_list_add(sd);
                    }
                }
            }

#endif                          /* USING_SMUX_MODULE */

#ifndef NETSNMP_FEATURE_REMOVE_FD_EVENT_MANAGER
            netsnmp_dispatch_external_events2(&count, &readfds,
                                              &writefds, &exceptfds);
#endif /* NETSNMP_FEATURE_REMOVE_FD_EVENT_MANAGER */

            /* If there are still events leftover, process them */
            if (count > 0) {
              snmp_read2(&readfds);
            }
        } else
            switch (count) {
            case 0:
                snmp_timeout();
                break;
            case -1:
                DEBUGMSGTL(("snmpd/select", "  errno = %d\n", errno));
                if (errno == EINTR) {
                    /*
                     * likely that we got a signal. Check our special signal
                     * flags before retrying select.
                     */
		    if (netsnmp_running && !reconfig) {
                        goto reselect;
		    }
                    continue;
                } else {
                    snmp_log_perror("select");
                }
                return -1;
            default:
                snmp_log(LOG_ERR, "select returned %d\n", count);
                return -1;
            }                   /* endif -- count>0 */

        /*
         * see if persistent store needs to be saved
         */
        snmp_store_if_needed();

        /*
         * run requested alarms 
         */
        run_alarms();

        netsnmp_check_outstanding_agent_requests();

    }                           /* endwhile */

    netsnmp_large_fd_set_cleanup(&readfds);
    netsnmp_large_fd_set_cleanup(&writefds);
    netsnmp_large_fd_set_cleanup(&exceptfds);

#if defined(WIN32)
    join_stdin_waiter_thread();
#endif

    snmp_log(LOG_INFO, "Received TERM or STOP signal...  shutting down...\n");
    return 0;

}                               /* end receive() */



/*******************************************************************-o-******
 * snmp_input
 *
 * Parameters:
 *	 op
 *	*session
 *	 requid
 *	*pdu
 *	*magic
 *      
 * Returns:
 *	1		On success	-OR-
 *	Passes through	Return from alarmGetResponse() when 
 *	  		  USING_V2PARTY_ALARM_MODULE is defined.
 *
 * Call-back function to manage responses to traps (informs) and alarms.
 * Not used by the agent to process other Response PDUs.
 */
int
snmp_input(int op,
           netsnmp_session * session,
           int reqid, netsnmp_pdu *pdu, void *magic)
{
    struct get_req_state *state = (struct get_req_state *) magic;

    if (op == NETSNMP_CALLBACK_OP_RECEIVED_MESSAGE) {
        if (pdu->command == SNMP_MSG_GET) {
            if (state->type == EVENT_GET_REQ) {
                /*
                 * this is just the ack to our inform pdu 
                 */
                return 1;
            }
        }
    } else if (op == NETSNMP_CALLBACK_OP_TIMED_OUT) {
        if (state->type == ALARM_GET_REQ) {
            /*
             * Need a mechanism to replace obsolete SNMPv2p alarm 
             */
        }
    }
    return 1;

}                               /* end snmp_input() */



/*
 * Windows Service Related functions 
 */
#ifdef WIN32SERVICE
/************************************************************
* main function for Windows
* Parse command line arguments for startup options,
* to start as service or console mode application in windows.
* Invokes appropriate startup functions depending on the 
* parameters passed
*************************************************************/
int
    __cdecl
_tmain(int argc, TCHAR * argv[])
{
    /*
     * Define Service Name and Description, which appears in windows SCM 
     */
    LPCTSTR         lpszServiceName = app_name_long;      /* Service Registry Name */
    LPCTSTR         lpszServiceDisplayName = _T("Net-SNMP Agent");       /* Display Name */
    LPCTSTR         lpszServiceDescription =
#ifdef IFDESCR
        _T("SNMPv2c / SNMPv3 command responder from Net-SNMP. Supports MIB objects for IP,ICMP,TCP,UDP, and network interface sub-layers.");
#else
        _T("SNMPv2c / SNMPv3 command responder from Net-SNMP");
#endif
    InputParams     InputOptions;


    int             nRunType = RUN_AS_CONSOLE;
    int             quiet = 0;
    
#if 0
    _CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF /*| _CRTDBG_CHECK_ALWAYS_DF*/);
#endif

    nRunType = ParseCmdLineForServiceOption(argc, argv, &quiet);

    switch (nRunType) {
    case REGISTER_SERVICE:
        /*
         * Register As service 
         */
        InputOptions.Argc = argc;
        InputOptions.Argv = argv;
        exit (RegisterService(lpszServiceName,
                        lpszServiceDisplayName,
                        lpszServiceDescription, &InputOptions, quiet));
        break;
    case UN_REGISTER_SERVICE:
        /*
         * Unregister service 
         */
        exit (UnregisterService(lpszServiceName, quiet));
        break;
    case RUN_AS_SERVICE:
        /*
         * Run as service 
         */
        /*
         * Register Stop Function 
         */
        RegisterStopFunction(StopSnmpAgent);
        return RunAsService(SnmpDaemonMain);
        break;
    default:
        /*
         * Run in console mode 
         */
        return SnmpDaemonMain(argc, argv);
        break;
    }
}

/*
 * To stop Snmp Agent daemon
 * This portion is still not working
 */
void
StopSnmpAgent(void)
{
    /*
     * Shut Down Agent 
     */
    SnmpdShutDown(1);

    /*
     * Wait till agent is completely stopped 
     */

    while (agent_status != AGENT_STOPPED) {
        Sleep(100);
    }
}

#endif /*WIN32SERVICE*/
