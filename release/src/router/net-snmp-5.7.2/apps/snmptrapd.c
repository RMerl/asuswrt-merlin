/*
 * snmptrapd.c - receive and log snmp traps
 *
 */
/*****************************************************************
	Copyright 1989, 1991, 1992 by Carnegie Mellon University

                      All Rights Reserved

Permission to use, copy, modify, and distribute this software and its
documentation for any purpose and without fee is hereby granted,
provided that the above copyright notice appear in all copies and that
both that copyright notice and this permission notice appear in
supporting documentation, and that the name of CMU not be
used in advertising or publicity pertaining to distribution of the
software without specific, written prior permission.

CMU DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE, INCLUDING
ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS, IN NO EVENT SHALL
CMU BE LIABLE FOR ANY SPECIAL, INDIRECT OR CONSEQUENTIAL DAMAGES OR
ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS,
WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION,
ARISING OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS
SOFTWARE.
******************************************************************/
#include <net-snmp/net-snmp-config.h>

#if HAVE_STDLIB_H
#include <stdlib.h>
#endif
#if HAVE_UNISTD_H
#include <unistd.h>
#endif
#if HAVE_STRING_H
#include <string.h>
#else
#include <strings.h>
#endif
#include <sys/types.h>
#if HAVE_SYS_WAIT_H
#include <sys/wait.h>
#endif
#if HAVE_SYS_SOCKET_H
#include <sys/socket.h>
#endif
#if HAVE_SYS_SOCKIO_H
#include <sys/sockio.h>
#endif
#if HAVE_NETINET_IN_H
#include <netinet/in.h>
#endif
#include <stdio.h>
#if !defined(mingw32) && defined(HAVE_SYS_TIME_H)
# include <sys/time.h>
# if TIME_WITH_SYS_TIME
#  include <time.h>
# endif
#else
# include <time.h>
#endif
#if HAVE_SYS_SELECT_H
#include <sys/select.h>
#endif
#if HAVE_SYS_PARAM_H
#include <sys/param.h>
#endif
#if HAVE_SYSLOG_H
#include <syslog.h>
#endif
#if HAVE_SYS_IOCTL_H
#include <sys/ioctl.h>
#endif
#if HAVE_NET_IF_H
#include <net/if.h>
#endif
#if HAVE_NETDB_H
#include <netdb.h>
#endif
#if HAVE_ARPA_INET_H
#include <arpa/inet.h>
#endif
#if HAVE_FCNTL_H
#include <fcntl.h>
#endif
#if HAVE_PROCESS_H              /* Win32-getpid */
#include <process.h>
#endif
#if HAVE_PWD_H
#include <pwd.h>
#endif
#if HAVE_GRP_H
#include <grp.h>
#endif
#include <signal.h>
#include <errno.h>

#include <net-snmp/net-snmp-includes.h>
#include <net-snmp/agent/net-snmp-agent-includes.h>
#include <net-snmp/library/fd_event_manager.h>
#include "snmptrapd_handlers.h"
#include "snmptrapd_log.h"
#include "snmptrapd_auth.h"
#include "notification-log-mib/notification_log.h"
#include "tlstm-mib/snmpTlstmCertToTSNTable/snmpTlstmCertToTSNTable.h"
#include "mibII/vacm_conf.h"
#ifdef NETSNMP_EMBEDDED_PERL
#include "snmp_perl.h"
#endif

/*
 * Include winservice.h to support Windows Service
 */
#ifdef WIN32
#include <windows.h>
#include <tchar.h>
#include <net-snmp/library/winservice.h>

#define WIN32SERVICE

#endif

#if NETSNMP_USE_LIBWRAP
#include <tcpd.h>
#endif

#include <net-snmp/net-snmp-features.h>

#ifndef BSD4_3
#define BSD4_2
#endif

#ifndef FD_SET

typedef long    fd_mask;
#define NFDBITS	(sizeof(fd_mask) * NBBY)        /* bits per mask */

#define	FD_SET(n, p)	((p)->fds_bits[(n)/NFDBITS] |= (1 << ((n) % NFDBITS)))
#define	FD_CLR(n, p)	((p)->fds_bits[(n)/NFDBITS] &= ~(1 << ((n) % NFDBITS)))
#define	FD_ISSET(n, p)	((p)->fds_bits[(n)/NFDBITS] & (1 << ((n) % NFDBITS)))
#define FD_ZERO(p)      memset((p), 0, sizeof(*(p)))
#endif

char           *logfile = NULL;
extern int      SyslogTrap;
extern int      dropauth;
static int      reconfig = 0;
char            ddefault_port[] = "udp:162";	/* Default default port */
char           *default_port = ddefault_port;
#if HAVE_GETPID
    FILE           *PID;
    char           *pid_file = NULL;
#endif
extern void parse_format(const char *token, char *line);
char           *trap1_fmt_str_remember = NULL;
int             dofork = 1;

extern int      netsnmp_running;

#ifdef NETSNMP_USE_MYSQL
extern int      netsnmp_mysql_init(void);
extern void     snmptrapd_register_sql_configs( void );
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

/*
 * Include an extra Facility variable to allow command line adjustment of
 * syslog destination 
 */
int Facility = LOG_DAEMON;

#ifdef WIN32SERVICE
/*
 * SNMP Trap Receiver Status 
 */
#define SNMPTRAPD_RUNNING 1
#define SNMPTRAPD_STOPPED 0
int             trapd_status = SNMPTRAPD_STOPPED;
/* app_name_long used for SCM, registry etc */
LPCTSTR         app_name_long = _T("Net-SNMP Trap Handler");     /* Application Name */
#endif

const char     *app_name = "snmptrapd";

void            trapd_update_config(void);

#ifdef WIN32SERVICE
void            StopSnmpTrapd(void);
int             SnmpTrapdMain(int argc, TCHAR * argv[]);
int __cdecl     _tmain(int argc, TCHAR * argv[]);
#else
int             main(int, char **);
#endif

#if defined(USING_AGENTX_SUBAGENT_MODULE) && !defined(NETSNMP_SNMPTRAPD_DISABLE_AGENTX)
extern void            subagent_init(void);
#endif


void
usage(void)
{
#ifdef WIN32SERVICE
    fprintf(stderr, "\nUsage:  snmptrapd [-register] [-quiet] [OPTIONS] [LISTENING ADDRESSES]");
    fprintf(stderr, "\n        snmptrapd [-unregister] [-quiet]");
#else
    fprintf(stderr, "Usage: snmptrapd [OPTIONS] [LISTENING ADDRESSES]\n");
#endif
    fprintf(stderr, "\n\tNET-SNMP Version:  %s\n", netsnmp_get_version());
    fprintf(stderr, "\tWeb:      http://www.net-snmp.org/\n");
    fprintf(stderr, "\tEmail:    net-snmp-coders@lists.sourceforge.net\n");
    fprintf(stderr, "\n");
    fprintf(stderr, "  -a\t\t\tignore authentication failure traps\n");
    fprintf(stderr, "  -A\t\t\tappend to log file rather than truncating it\n");
    fprintf(stderr, "  -c FILE\t\tread FILE as a configuration file\n");
    fprintf(stderr,
            "  -C\t\t\tdo not read the default configuration files\n");
    fprintf(stderr, "  -d\t\t\tdump sent and received SNMP packets\n");
    fprintf(stderr, "  -D[TOKEN[,...]]\t\tturn on debugging output for the specified TOKENs\n\t\t\t   (ALL gives extremely verbose debugging output)\n");
    fprintf(stderr, "  -f\t\t\tdo not fork from the shell\n");
    fprintf(stderr,
            "  -F FORMAT\t\tuse specified format for logging to standard error\n");
#if HAVE_UNISTD_H
    fprintf(stderr, "  -g GID\t\tchange to this numeric gid after opening\n"
	   "\t\t\t  transport endpoints\n");
#endif
    fprintf(stderr, "  -h, --help\t\tdisplay this usage message\n");
    fprintf(stderr,
            "  -H\t\t\tdisplay configuration file directives understood\n");
    fprintf(stderr,
            "  -m MIBLIST\t\tuse MIBLIST instead of the default MIB list\n");
    fprintf(stderr,
            "  -M DIRLIST\t\tuse DIRLIST as the list of locations\n\t\t\t  to look for MIBs\n");
    fprintf(stderr,
            "  -n\t\t\tuse numeric addresses instead of attempting\n\t\t\t  hostname lookups (no DNS)\n");
#if HAVE_GETPID
    fprintf(stderr, "  -p FILE\t\tstore process id in FILE\n");
#endif
#ifdef WIN32SERVICE
    fprintf(stderr, "  -register\t\tregister as a Windows service\n");
    fprintf(stderr, "  \t\t\t  (followed by -quiet to prevent message popups)\n");
    fprintf(stderr, "  \t\t\t  (followed by the startup parameter list)\n");
    fprintf(stderr, "  \t\t\t  Note that some parameters are not relevant when running as a service\n");
#endif
    fprintf(stderr, "  -t\t\t\tPrevent traps from being logged to syslog\n");
#if HAVE_UNISTD_H
    fprintf(stderr, "  -u UID\t\tchange to this uid (numeric or textual) after\n"
	   "\t\t\t  opening transport endpoints\n");
#endif
#ifdef WIN32SERVICE
    fprintf(stderr, "  -unregister\t\tunregister as a Windows service\n");
    fprintf(stderr, "  \t\t\t  (followed -quiet to prevent message popups)\n");
#endif
    fprintf(stderr, "  -v, --version\t\tdisplay version information\n");
#if defined(USING_AGENTX_SUBAGENT_MODULE) && !defined(NETSNMP_SNMPTRAPD_DISABLE_AGENTX)
    fprintf(stderr, "  -x ADDRESS\t\tuse ADDRESS as AgentX address\n");
#endif
    fprintf(stderr,
            "  -O <OUTOPTS>\t\ttoggle options controlling output display\n");
    snmp_out_toggle_options_usage("\t\t\t", stderr);
    fprintf(stderr,
            "  -L <LOGOPTS>\t\ttoggle options controlling where to log to\n");
    snmp_log_options_usage("\t\t\t", stderr);
}

static void
version(void)
{
    printf("\nNET-SNMP Version:  %s\n", netsnmp_get_version());
    printf("Web:               http://www.net-snmp.org/\n");
    printf("Email:             net-snmp-coders@lists.sourceforge.net\n\n");
    exit(0);
}

RETSIGTYPE
term_handler(int sig)
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
hup_handler(int sig)
{
    reconfig = 1;
    signal(SIGHUP, hup_handler);
}
#endif

static int
pre_parse(netsnmp_session * session, netsnmp_transport *transport,
          void *transport_data, int transport_data_length)
{
#if NETSNMP_USE_LIBWRAP
    char *addr_string = NULL;

    if (transport != NULL && transport->f_fmtaddr != NULL) {
        /*
         * Okay I do know how to format this address for logging.  
         */
        addr_string = transport->f_fmtaddr(transport, transport_data,
                                           transport_data_length);
        /*
         * Don't forget to free() it.  
         */
    }

    if (addr_string != NULL) {
      /* Catch udp,udp6,tcp,tcp6 transports using "[" */
      char *tcpudpaddr = strstr(addr_string, "[");
      if ( tcpudpaddr != 0 ) {
	char sbuf[64];
	char *xp;

	strlcpy(sbuf, tcpudpaddr + 1, sizeof(sbuf));
        xp = strstr(sbuf, "]");
        if (xp)
            *xp = '\0';

        if (hosts_ctl("snmptrapd", STRING_UNKNOWN, 
		      sbuf, STRING_UNKNOWN) == 0) {
            DEBUGMSGTL(("snmptrapd:libwrap", "%s rejected", addr_string));
            SNMP_FREE(addr_string);
            return 0;
        }
      }
      SNMP_FREE(addr_string);
    } else {
        if (hosts_ctl("snmptrapd", STRING_UNKNOWN,
                      STRING_UNKNOWN, STRING_UNKNOWN) == 0) {
            DEBUGMSGTL(("snmptrapd:libwrap", "[unknown] rejected"));
            return 0;
        }
    }
#endif/*  NETSNMP_USE_LIBWRAP  */
    return 1;
}

static netsnmp_session *
snmptrapd_add_session(netsnmp_transport *t)
{
    netsnmp_session sess, *session = &sess, *rc = NULL;

    snmp_sess_init(session);
    session->peername = SNMP_DEFAULT_PEERNAME;  /* Original code had NULL here */
    session->version = SNMP_DEFAULT_VERSION;
    session->community_len = SNMP_DEFAULT_COMMUNITY_LEN;
    session->retries = SNMP_DEFAULT_RETRIES;
    session->timeout = SNMP_DEFAULT_TIMEOUT;
    session->callback = snmp_input;
    session->callback_magic = (void *) t;
    session->authenticator = NULL;
    sess.isAuthoritative = SNMP_SESS_UNKNOWNAUTH;

    rc = snmp_add(session, t, pre_parse, NULL);
    if (rc == NULL) {
        snmp_sess_perror("snmptrapd", session);
    }
    return rc;
}

static void
snmptrapd_close_sessions(netsnmp_session * sess_list)
{
    netsnmp_session *s = NULL, *next = NULL;

    for (s = sess_list; s != NULL; s = next) {
        next = s->next;
        snmp_close(s);
    }
}

void
parse_trapd_address(const char *token, char *cptr)
{
    char buf[BUFSIZ];
    char *p;
    cptr = copy_nword(cptr, buf, sizeof(buf));

    if (default_port == ddefault_port) {
        default_port = strdup(buf);
    } else {
        p = malloc(strlen(buf) + 1 + strlen(default_port) + 1);
        if (p) {
            strcat(p, buf);
            strcat(p, ",");
            strcat(p, default_port );
        }
        free(default_port);
        default_port = p;
    }
}

void
free_trapd_address(void)
{
    if (default_port != ddefault_port) {
        free(default_port);
    }
}

void
parse_config_doNotLogTraps(const char *token, char *cptr)
{
  if (atoi(cptr) > 0)
    SyslogTrap++;
}

void
free_config_pidFile(void)
{
  if (pid_file)
    free(pid_file);
  pid_file = NULL;
}

void
parse_config_pidFile(const char *token, char *cptr)
{
  free_config_pidFile();
  pid_file = strdup (cptr);
}

#ifdef HAVE_UNISTD_H
void
parse_config_agentuser(const char *token, char *cptr)
{
    if (cptr[0] == '#') {
        char           *ecp;
        int             uid;

        uid = strtoul(cptr + 1, &ecp, 10);
        if (*ecp != 0) {
            config_perror("Bad number");
	} else {
	    netsnmp_ds_set_int(NETSNMP_DS_APPLICATION_ID, 
			       NETSNMP_DS_AGENT_USERID, uid);
	}
#if defined(HAVE_GETPWNAM) && defined(HAVE_PWD_H)
    } else {
        struct passwd *info;

        info = getpwnam(cptr);
        if (info)
            netsnmp_ds_set_int(NETSNMP_DS_APPLICATION_ID, 
                               NETSNMP_DS_AGENT_USERID, info->pw_uid);
        else
            config_perror("User not found in passwd database");
        endpwent();
#endif
    }
}

void
parse_config_agentgroup(const char *token, char *cptr)
{
    if (cptr[0] == '#') {
        char           *ecp;
        int             gid = strtoul(cptr + 1, &ecp, 10);

        if (*ecp != 0) {
            config_perror("Bad number");
	} else {
            netsnmp_ds_set_int(NETSNMP_DS_APPLICATION_ID, 
			       NETSNMP_DS_AGENT_GROUPID, gid);
	}
#if defined(HAVE_GETGRNAM) && defined(HAVE_GRP_H)
    } else {
        struct group   *info;

        info = getgrnam(cptr);
        if (info)
            netsnmp_ds_set_int(NETSNMP_DS_APPLICATION_ID, 
                               NETSNMP_DS_AGENT_GROUPID, info->gr_gid);
        else
            config_perror("Group not found in group database");
        endgrent();
#endif
    }
}
#endif

void
parse_config_doNotFork(const char *token, char *cptr)
{
  if (netsnmp_ds_parse_boolean(cptr) == 1)
    dofork = 0;
}

void
parse_config_ignoreAuthFailure(const char *token, char *cptr)
{
  if (netsnmp_ds_parse_boolean(cptr) == 1)
    dropauth = 1;
}

void
parse_config_outputOption(const char *token, char *cptr)
{
  char *cp;

  cp = snmp_out_toggle_options(cptr);
  if (cp != NULL) {
    fprintf(stderr, "Unknown output option passed to -O: %c\n",
        *cp);
  }
}

static void
snmptrapd_main_loop(void)
{
    int             count, numfds, block;
    fd_set          readfds,writefds,exceptfds;
    struct timeval  timeout, *tvp;

    while (netsnmp_running) {
        if (reconfig) {
                /*
                 * If we are logging to a file, receipt of SIGHUP also
                 * indicates that the log file should be closed and
                 * re-opened.  This is useful for users that want to
                 * rotate logs in a more predictable manner.
                 */
                netsnmp_logging_restart();
                snmp_log(LOG_INFO, "NET-SNMP version %s restarted\n",
                         netsnmp_get_version());
            trapd_update_config();
            if (trap1_fmt_str_remember) {
                parse_format( NULL, trap1_fmt_str_remember );
            }
            reconfig = 0;
        }
        numfds = 0;
        FD_ZERO(&readfds);
        FD_ZERO(&writefds);
        FD_ZERO(&exceptfds);
        block = 0;
        tvp = &timeout;
        timerclear(tvp);
        tvp->tv_sec = 5;
        snmp_select_info(&numfds, &readfds, tvp, &block);
        if (block == 1)
            tvp = NULL;         /* block without timeout */
#ifndef NETSNMP_FEATURE_REMOVE_FD_EVENT_MANAGER
        netsnmp_external_event_info(&numfds, &readfds, &writefds, &exceptfds);
#endif /* NETSNMP_FEATURE_REMOVE_FD_EVENT_MANAGER */
        count = select(numfds, &readfds, &writefds, &exceptfds, tvp);
        if (count > 0) {
#ifndef NETSNMP_FEATURE_REMOVE_FD_EVENT_MANAGER
            netsnmp_dispatch_external_events(&count, &readfds, &writefds,
                                             &exceptfds);
#endif /* NETSNMP_FEATURE_REMOVE_FD_EVENT_MANAGER */
            /* If there are any more events after external events, then
             * try SNMP events. */
            if (count > 0) {
                snmp_read(&readfds);
            }
        } else {
            switch (count) {
            case 0:
                snmp_timeout();
                break;
            case -1:
                if (errno == EINTR)
                    continue;
                snmp_log_perror("select");
                netsnmp_running = 0;
                break;
            default:
                fprintf(stderr, "select returned %d\n", count);
                netsnmp_running = 0;
            }
	}
	run_alarms();
    }
}

/*******************************************************************-o-******
 * main - Non Windows
 * SnmpTrapdMain - Windows to support windows service
 *
 * Parameters:
 *	 argc
 *	*argv[]
 *      
 * Returns:
 *	0	Always succeeds.  (?)
 *
 *
 * Setup and start the trap receiver daemon.
 *
 * Also successfully EXITs with zero for some options.
 */
int
#ifdef WIN32SERVICE
SnmpTrapdMain(int argc, TCHAR * argv[])
#else
main(int argc, char *argv[])
#endif
{
    char            options[128] = "aAc:CdD::efF:g:hHI:L:m:M:no:O:PqsS:tu:vx:-:";
    netsnmp_session *sess_list = NULL, *ss = NULL;
    netsnmp_transport *transport = NULL;
    int             arg, i = 0;
    int             uid = 0, gid = 0;
    char           *cp, *listen_ports = NULL;
#if defined(USING_AGENTX_SUBAGENT_MODULE) && !defined(NETSNMP_SNMPTRAPD_DISABLE_AGENTX)
    int             agentx_subagent = 1;
#endif
    netsnmp_trapd_handler *traph;


#ifndef WIN32
    /*
     * close all non-standard file descriptors we may have
     * inherited from the shell.
     */
    for (i = getdtablesize() - 1; i > 2; --i) {
        (void) close(i);
    }
#endif /* #WIN32 */
    
#ifdef SIGTERM
    signal(SIGTERM, term_handler);
#endif
#ifdef SIGHUP
    signal(SIGHUP, SIG_IGN);   /* do not terminate on early SIGHUP */
#endif

#ifdef SIGINT
    signal(SIGINT, term_handler);
#endif
#ifdef SIGPIPE
    signal(SIGPIPE, SIG_IGN);   /* 'Inline' failure of wayward readers */
#endif

    /*
     * register our configuration handlers now so -H properly displays them 
     */
    snmptrapd_register_configs( );
#ifdef NETSNMP_USE_MYSQL
    snmptrapd_register_sql_configs( );
#endif
#ifdef NETSNMP_SECMOD_USM
    init_usm_conf( "snmptrapd" );
#endif /* NETSNMP_SECMOD_USM */
    register_config_handler("snmptrapd", "snmpTrapdAddr",
                            parse_trapd_address, free_trapd_address, "string");

    register_config_handler("snmptrapd", "doNotLogTraps",
                            parse_config_doNotLogTraps, NULL, "(1|yes|true|0|no|false)");
#if HAVE_GETPID
    register_config_handler("snmptrapd", "pidFile",
                            parse_config_pidFile, NULL, "string");
#endif
#ifdef HAVE_UNISTD_H
    register_config_handler("snmptrapd", "agentuser",
                            parse_config_agentuser, NULL, "userid");
    register_config_handler("snmptrapd", "agentgroup",
                            parse_config_agentgroup, NULL, "groupid");
#endif
    
    register_config_handler("snmptrapd", "doNotFork",
                            parse_config_doNotFork, NULL, "(1|yes|true|0|no|false)");

    register_config_handler("snmptrapd", "ignoreAuthFailure",
                            parse_config_ignoreAuthFailure, NULL, "(1|yes|true|0|no|false)");

    register_config_handler("snmptrapd", "outputOption",
                            parse_config_outputOption, NULL, "string");

    /*
     * Add some options if they are available.  
     */
#if HAVE_GETPID
    strcat(options, "p:");
#endif

#ifndef NETSNMP_FEATURE_REMOVE_LOGGING_SYSLOG
#ifdef WIN32
    snmp_log_syslogname(app_name_long);
#else
    snmp_log_syslogname(app_name);
#endif
#endif /* NETSNMP_FEATURE_REMOVE_LOGGING_SYSLOG */

    /*
     * Now process options normally.  
     */

    while ((arg = getopt(argc, argv, options)) != EOF) {
        switch (arg) {
        case '-':
            if (strcasecmp(optarg, "help") == 0) {
                usage();
                exit(0);
            }
            if (strcasecmp(optarg, "version") == 0) {
                version();
                exit(0);
            }

            handle_long_opt(optarg);
            break;

        case 'a':
            dropauth = 1;
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
                usage();
                exit(1);
            }
            break;

        case 'C':
            netsnmp_ds_set_boolean(NETSNMP_DS_LIBRARY_ID, 
				   NETSNMP_DS_LIB_DONT_READ_CONFIGS, 1);
            break;

        case 'd':
            netsnmp_ds_set_boolean(NETSNMP_DS_LIBRARY_ID, 
                                   NETSNMP_DS_LIB_DUMP_PACKET, 1);
            break;

        case 'D':
            debug_register_tokens(optarg);
            snmp_set_do_debugging(1);
            break;

        case 'f':
            dofork = 0;
            break;

        case 'F':
            if (optarg != NULL) {
                if (( strncmp( optarg, "print",   5 ) == 0 ) ||
                    ( strncmp( optarg, "syslog",  6 ) == 0 ) ||
                    ( strncmp( optarg, "execute", 7 ) == 0 )) {
                    /* New style: "type=format" */
                    trap1_fmt_str_remember = strdup(optarg);
                    cp = strchr( trap1_fmt_str_remember, '=' );
                    if (cp)
                        *cp = ' ';
                } else {
                    /* Old style: implicitly "print=format" */
                    trap1_fmt_str_remember = malloc(strlen(optarg) + 7);
                    sprintf( trap1_fmt_str_remember, "print %s", optarg );
                }
            } else {
                usage();
                exit(1);
            }
            break;

#if HAVE_UNISTD_H
        case 'g':
            if (optarg != NULL) {
                netsnmp_ds_set_int(NETSNMP_DS_APPLICATION_ID, 
				   NETSNMP_DS_AGENT_GROUPID, gid = atoi(optarg));
            } else {
                usage();
                exit(1);
            }
            break;
#endif

        case 'h':
            usage();
            exit(0);

        case 'H':
            init_agent("snmptrapd");
#ifdef USING_NOTIFICATION_LOG_MIB_NOTIFICATION_LOG_MODULE
            init_notification_log();
#endif
#ifdef NETSNMP_EMBEDDED_PERL
            init_perl();
#endif
            init_snmp("snmptrapd");
            fprintf(stderr, "Configuration directives understood:\n");
            read_config_print_usage("  ");
            exit(0);

        case 'I':
            if (optarg != NULL) {
                add_to_init_list(optarg);
            } else {
                usage();
            }
            break;

	case 'S':
            fprintf(stderr,
                    "Warning: -S option has been withdrawn; use -Ls <facility> instead\n");
            exit(1);
            break;

        case 'm':
            if (optarg != NULL) {
                setenv("MIBS", optarg, 1);
            } else {
                usage();
                exit(1);
            }
            break;

        case 'M':
            if (optarg != NULL) {
                setenv("MIBDIRS", optarg, 1);
            } else {
                usage();
                exit(1);
            }
            break;

        case 'n':
            netsnmp_ds_set_boolean(NETSNMP_DS_APPLICATION_ID, 
				   NETSNMP_DS_APP_NUMERIC_IP, 1);
            break;

        case 'o':
            fprintf(stderr,
                    "Warning: -o option has been withdrawn; use -Lf <file> instead\n");
            exit(1);
            break;

        case 'O':
            cp = snmp_out_toggle_options(optarg);
            if (cp != NULL) {
                fprintf(stderr, "Unknown output option passed to -O: %c\n",
			*cp);
                usage();
                exit(1);
            }
            break;

        case 'L':
	    if  (snmp_log_options( optarg, argc, argv ) < 0 ) {
                usage();
                exit(1);
            }
            break;

#if HAVE_GETPID
        case 'p':
            if (optarg != NULL) {
                parse_config_pidFile(NULL, optarg);
            } else {
                usage();
                exit(1);
            }
            break;
#endif

        case 'P':
            fprintf(stderr,
                    "Warning: -P option has been withdrawn; use -f -Le instead\n");
            exit(1);
            break;

        case 's':
            fprintf(stderr,
                    "Warning: -s option has been withdrawn; use -Lsd instead\n");
            exit(1);
            break;

        case 't':
            SyslogTrap++;
            break;

#if HAVE_UNISTD_H
        case 'u':
            if (optarg != NULL) {
                char           *ecp;

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
                usage();
                exit(1);
            }
            break;
#endif

        case 'v':
            version();
            exit(0);
            break;

        case 'x':
            if (optarg != NULL) {
                netsnmp_ds_set_string(NETSNMP_DS_APPLICATION_ID,
                                      NETSNMP_DS_AGENT_X_SOCKET, optarg);
            } else {
                usage();
                exit(1);
            }
            break;

        default:
            fprintf(stderr, "invalid option: -%c\n", arg);
            usage();
            exit(1);
            break;
        }
    }

    if (optind < argc) {
        /*
         * There are optional transport addresses on the command line.  
         */
        for (i = optind; i < argc; i++) {
            char *astring;
            if (listen_ports != NULL) {
                astring = malloc(strlen(listen_ports) + 2 + strlen(argv[i]));
                if (astring == NULL) {
                    fprintf(stderr, "malloc failure processing argv[%d]\n", i);
                    exit(1);
                }
                sprintf(astring, "%s,%s", listen_ports, argv[i]);
                free(listen_ports);
                listen_ports = astring;
            } else {
                listen_ports = strdup(argv[i]);
                if (listen_ports == NULL) {
                    fprintf(stderr, "malloc failure processing argv[%d]\n", i);
                    exit(1);
                }
            }
        }
    }

    SOCK_STARTUP;

    /*
     * I'm being lazy here, and not checking the
     * return value from these registration calls.
     * Don't try this at home, children!
     */
    if (0 == snmp_get_do_logging()) {
#ifndef NETSNMP_FEATURE_REMOVE_LOGGING_SYSLOG
        traph = netsnmp_add_global_traphandler(NETSNMPTRAPD_PRE_HANDLER,
                                               syslog_handler);
        traph->authtypes = TRAP_AUTH_LOG;
        snmp_enable_syslog();
#else /* NETSNMP_FEATURE_REMOVE_LOGGING_SYSLOG */
#ifndef NETSNMP_FEATURE_REMOVE_LOGGING_STDIO
        traph = netsnmp_add_global_traphandler(NETSNMPTRAPD_PRE_HANDLER,
                                               print_handler);
        traph->authtypes = TRAP_AUTH_LOG;
        snmp_enable_stderr();
#endif /* NETSNMP_FEATURE_REMOVE_LOGGING_STDIO */
#endif /* NETSNMP_FEATURE_REMOVE_LOGGING_SYSLOG */
    } else {
        traph = netsnmp_add_global_traphandler(NETSNMPTRAPD_PRE_HANDLER,
                                               print_handler);
        traph->authtypes = TRAP_AUTH_LOG;
    }

#if defined(USING_AGENTX_SUBAGENT_MODULE) && !defined(NETSNMP_SNMPTRAPD_DISABLE_AGENTX)
    /*
     * we're an agentx subagent? 
     */
    if (agentx_subagent) {
        /*
         * make us a agentx client. 
         */
        netsnmp_ds_set_boolean(NETSNMP_DS_APPLICATION_ID,
			       NETSNMP_DS_AGENT_ROLE, 1);
    }
#endif

    /*
     * don't fail if we can't do agentx (ie, socket not there, or not root) 
     */
    netsnmp_ds_toggle_boolean(NETSNMP_DS_APPLICATION_ID, 
			      NETSNMP_DS_AGENT_NO_ROOT_ACCESS);
    /*
     * ignore any warning messages.
     */
    netsnmp_ds_toggle_boolean(NETSNMP_DS_APPLICATION_ID, 
			      NETSNMP_DS_AGENT_NO_CONNECTION_WARNINGS);

    /*
     * initialize the agent library 
     */
    init_agent("snmptrapd");

#if defined(USING_AGENTX_SUBAGENT_MODULE) && !defined(NETSNMP_SNMPTRAPD_DISABLE_AGENTX)
#ifdef NETSNMP_FEATURE_CHECKING
    netsnmp_feature_require(register_snmpEngine_scalars_context)
#endif /* NETSNMP_FEATURE_CHECKING */
    /*
     * initialize local modules 
     */
    if (agentx_subagent) {
#ifdef USING_SNMPV3_SNMPENGINE_MODULE
        extern void register_snmpEngine_scalars_context(const char *);
#endif
        subagent_init();
#ifdef USING_NOTIFICATION_LOG_MIB_NOTIFICATION_LOG_MODULE
        /* register the notification log table */
        if (should_init("notificationLogMib")) {
            netsnmp_ds_set_string(NETSNMP_DS_APPLICATION_ID,
                              NETSNMP_DS_NOTIF_LOG_CTX,
                              "snmptrapd");
            traph = netsnmp_add_global_traphandler(NETSNMPTRAPD_POST_HANDLER,
                                                   notification_handler);
            traph->authtypes = TRAP_AUTH_LOG;
            init_notification_log();
        }
#endif
#ifdef USING_SNMPV3_SNMPENGINE_MODULE
        /*
         * register scalars from SNMP-FRAMEWORK-MIB::snmpEngineID group;
         * allows engineID probes via the master agent under the
         * snmptrapd context
         */
        register_snmpEngine_scalars_context("snmptrapd");
#endif
    }
#endif /* USING_AGENTX_SUBAGENT_MODULE && !NETSNMP_SNMPTRAPD_DISABLE_AGENTX */

    /* register our authorization handler */
    init_netsnmp_trapd_auth();

#if defined(USING_AGENTX_SUBAGENT_MODULE) && !defined(NETSNMP_SNMPTRAPD_DISABLE_AGENTX)
    if (agentx_subagent) {
#ifdef USING_AGENT_NSVACMACCESSTABLE_MODULE
        extern void init_register_nsVacm_context(const char *);
#endif
#ifdef USING_SNMPV3_USMUSER_MODULE
#ifdef NETSNMP_FEATURE_CHECKING
        netsnmp_feature_require(init_register_usmUser_context)
#endif /* NETSNMP_FEATURE_CHECKING */
        extern void init_register_usmUser_context(const char *);
        /* register ourselves as having a USM user database */
        init_register_usmUser_context("snmptrapd");
#endif
#ifdef USING_AGENT_NSVACMACCESSTABLE_MODULE
        /* register net-snmp vacm extensions */
        init_register_nsVacm_context("snmptrapd");
#endif
#ifdef USING_TLSTM_MIB_SNMPTLSTMCERTTOTSNTABLE_MODULE
        init_snmpTlstmCertToTSNTable_context("snmptrapd");
#endif
    }
#endif

#ifdef NETSNMP_EMBEDDED_PERL
    init_perl();
    {
        /* set the default path to load */
        char            init_file[SNMP_MAXBUF];
        snprintf(init_file, sizeof(init_file) - 1,
                 "%s/%s", SNMPSHAREPATH, "snmp_perl_trapd.pl");
        netsnmp_ds_set_string(NETSNMP_DS_APPLICATION_ID,
                              NETSNMP_DS_AGENT_PERL_INIT_FILE,
                              init_file);
    }
#endif

    /*
     * Initialize the world.
     */
    init_snmp("snmptrapd");

#ifdef SIGHUP
    signal(SIGHUP, hup_handler);
#endif

    if (trap1_fmt_str_remember) {
        parse_format( NULL, trap1_fmt_str_remember );
    }

    if (netsnmp_ds_get_boolean(NETSNMP_DS_APPLICATION_ID, 
			       NETSNMP_DS_AGENT_QUIT_IMMEDIATELY)) {
        /*
         * just starting up to process specific configuration and then
         * shutting down immediately. 
         */
        netsnmp_running = 0;
    }

    /*
     * if no logging options on command line or in conf files, use syslog
     */
    if (0 == snmp_get_do_logging()) {
#ifndef NETSNMP_FEATURE_REMOVE_LOGGING_SYSLOG
#ifdef WIN32
        snmp_enable_syslog_ident(app_name_long, Facility);
#else
        snmp_enable_syslog_ident(app_name, Facility);
#endif        
#endif /* NETSNMP_FEATURE_REMOVE_LOGGING_SYSLOG */
    }

    if (listen_ports)
        cp = listen_ports;
    else
        cp = default_port;

    while (cp != NULL) {
        char *sep = strchr(cp, ',');

        if (sep != NULL) {
            *sep = 0;
        }

        transport = netsnmp_transport_open_server("snmptrap", cp);
        if (transport == NULL) {
            snmp_log(LOG_ERR, "couldn't open %s -- errno %d (\"%s\")\n",
                     cp, errno, strerror(errno));
            snmptrapd_close_sessions(sess_list);
            SOCK_CLEANUP;
            exit(1);
        } else {
            ss = snmptrapd_add_session(transport);
            if (ss == NULL) {
                /*
                 * Shouldn't happen?  We have already opened the transport
                 * successfully so what could have gone wrong?  
                 */
                snmptrapd_close_sessions(sess_list);
                netsnmp_transport_free(transport);
                snmp_log(LOG_ERR, "couldn't open snmp - %s", strerror(errno));
                SOCK_CLEANUP;
                exit(1);
            } else {
                ss->next = sess_list;
                sess_list = ss;
            }
        }

        /*
         * Process next listen address, if there is one.  
         */

        if (sep != NULL) {
            *sep = ',';
            cp = sep + 1;
        } else {
            cp = NULL;
        }
    }
    SNMP_FREE(listen_ports); /* done with them */

#ifdef NETSNMP_USE_MYSQL
    if( netsnmp_mysql_init() ) {
        fprintf(stderr, "MySQL initialization failed\n");
        exit(1);
    }
#endif

#ifndef WIN32
    /*
     * fork the process to the background if we are not printing to stderr 
     */
    if (dofork && netsnmp_running) {
        int             fd;

        switch (fork()) {
        case -1:
            fprintf(stderr, "bad fork - %s\n", strerror(errno));
            _exit(1);

        case 0:
            /*
             * become process group leader 
             */
            if (setsid() == -1) {
                fprintf(stderr, "bad setsid - %s\n", strerror(errno));
                _exit(1);
            }

            /*
             * if we are forked, we don't want to print out to stdout or stderr 
             */
            fd = open("/dev/null", O_RDWR);
            dup2(fd, STDIN_FILENO);
            dup2(fd, STDOUT_FILENO);
            dup2(fd, STDERR_FILENO);
            close(fd);
            break;

        default:
            _exit(0);
        }
    }
#endif                          /* WIN32 */
#if HAVE_GETPID
    if (pid_file != NULL) {
        if ((PID = fopen(pid_file, "w")) == NULL) {
            snmp_log_perror("fopen");
            exit(1);
        }
        fprintf(PID, "%d\n", (int) getpid());
        fclose(PID);
        free_config_pidFile();
    }
#endif

    snmp_log(LOG_INFO, "NET-SNMP version %s\n", netsnmp_get_version());

    /*
     * ignore early sighup during startup
     */
    reconfig = 0;

#if HAVE_UNISTD_H
#ifdef HAVE_SETGID
    if ((gid = netsnmp_ds_get_int(NETSNMP_DS_APPLICATION_ID, 
				  NETSNMP_DS_AGENT_GROUPID)) != 0) {
        DEBUGMSGTL(("snmptrapd/main", "Changing gid to %d.\n", gid));
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
				  NETSNMP_DS_AGENT_USERID)) != 0) {
        DEBUGMSGTL(("snmptrapd/main", "Changing uid to %d.\n", uid));
        if (setuid(uid) == -1) {
            snmp_log_perror("setuid failed");
            if (!netsnmp_ds_get_boolean(NETSNMP_DS_APPLICATION_ID, 
					NETSNMP_DS_AGENT_NO_ROOT_ACCESS)) {
                exit(1);
            }
        }
    }
#endif
#endif

#ifdef WIN32SERVICE
    trapd_status = SNMPTRAPD_RUNNING;
#endif

    snmptrapd_main_loop();

    if (snmp_get_do_logging()) {
        struct tm      *tm;
        time_t          timer;
        time(&timer);
        tm = localtime(&timer);
        snmp_log(LOG_INFO,
                "%.4d-%.2d-%.2d %.2d:%.2d:%.2d NET-SNMP version %s Stopped.\n",
                 tm->tm_year + 1900, tm->tm_mon + 1, tm->tm_mday, tm->tm_hour,
                 tm->tm_min, tm->tm_sec, netsnmp_get_version());
    }
    snmp_log(LOG_INFO, "Stopping snmptrapd\n");
    
#ifdef NETSNMP_EMBEDDED_PERL
    shutdown_perl();
#endif
    snmptrapd_close_sessions(sess_list);
    snmp_shutdown("snmptrapd");
#ifdef WIN32SERVICE
    trapd_status = SNMPTRAPD_STOPPED;
#endif
    snmp_disable_log();
    SOCK_CLEANUP;
    return 0;
}

/*
 * Read the configuration files. Implemented as a signal handler so that
 * receipt of SIGHUP will cause configuration to be re-read when the
 * trap daemon is running detatched from the console.
 *
 */
void
trapd_update_config(void)
{
    free_config();
#ifdef USING_MIBII_VACM_CONF_MODULE
    vacm_standard_views(0,0,NULL,NULL);
#endif
    read_configs();
}


#if !defined(HAVE_GETDTABLESIZE) && !defined(WIN32)
#include <sys/resource.h>
int
getdtablesize(void)
{
    struct rlimit   rl;
    getrlimit(RLIMIT_NOFILE, &rl);
    return (rl.rlim_cur);
}
#endif

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
    LPCTSTR         lpszServiceDisplayName = _T("Net-SNMP Trap Handler");       /* Display Name */
    LPCTSTR         lpszServiceDescription =
#ifdef IFDESCR
        _T("SNMPv2c / SNMPv3 trap/inform receiver from Net-SNMP. Supports MIB objects for IP,ICMP,TCP,UDP, and network interface sub-layers.");
#else
        _T("SNMPv2c / SNMPv3 trap/inform receiver from Net-SNMP");
#endif
    InputParams     InputOptions;

    int             nRunType = RUN_AS_CONSOLE;
    int             quiet = 0;

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
        exit(0);
        break;
    case RUN_AS_SERVICE:
        /*
         * Run as service 
         */
        /*
         * Register Stop Function 
         */
        RegisterStopFunction(StopSnmpTrapd);
        return RunAsService(SnmpTrapdMain);
        break;
    default:
        /*
         * Run in console mode 
         */
        return SnmpTrapdMain(argc, argv);
        break;
    }
}

/*
 * To stop Snmp Trap Receiver daemon
 * This portion is still not working
 */
void
StopSnmpTrapd(void)
{
    /*
     * Shut Down Service
     */
    term_handler(1);

    /*
     * Wait till trap receiver is completely stopped 
     */

    while (trapd_status != SNMPTRAPD_STOPPED) {
        Sleep(100);
    }
}

#endif /*WIN32SERVICE*/
