/*
 * main.c - Point-to-Point Protocol main module
 *
 * Copyright (c) 1989 Carnegie Mellon University.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms are permitted
 * provided that the above copyright notice and this paragraph are
 * duplicated in all such forms and that any documentation,
 * advertising materials, and other materials related to such
 * distribution and use acknowledge that the software was developed
 * by Carnegie Mellon University.  The name of the
 * University may not be used to endorse or promote products derived
 * from this software without specific prior written permission.
 * THIS SOFTWARE IS PROVIDED ``AS IS'' AND WITHOUT ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, WITHOUT LIMITATION, THE IMPLIED
 * WARRANTIES OF MERCHANTIBILITY AND FITNESS FOR A PARTICULAR PURPOSE.
 */

#define RCSID	"$Id: main.c,v 1.7 2004/08/12 02:56:55 tallest Exp $"

#include <stdio.h>
#include <ctype.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <errno.h>
#include <fcntl.h>
#include <syslog.h>
#include <netdb.h>
#include <utmp.h>
#include <pwd.h>
#include <setjmp.h>
#include <sys/param.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/time.h>
#include <sys/resource.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include "pppd.h"
#include "magic.h"
#include "fsm.h"
#include "lcp.h"
#include "ipcp.h"
#ifdef INET6
#include "ipv6cp.h"
#endif
#include "upap.h"
#include "chap.h"
#include "ccp.h"
#include "pathnames.h"

#ifdef CBCP_SUPPORT
#include "cbcp.h"
#endif

#ifdef IPX_CHANGE
#include "ipxcp.h"
#endif /* IPX_CHANGE */
#ifdef AT_CHANGE
#include "atcp.h"
#endif

static const char rcsid[] = RCSID;

/* interface vars */
char ifname[32];		/* Interface name */
int ifunit;			/* Interface unit number */
int dial_cnt;

struct channel *the_channel;

char *progname;			/* Name of this program */
char hostname[MAXNAMELEN];	/* Our hostname */
static char pidfilename[MAXPATHLEN];	/* name of pid file */
static char linkpidfile[MAXPATHLEN];	/* name of linkname pid file */
char ppp_devnam[MAXPATHLEN];	/* name of PPP tty (maybe ttypx) */
uid_t uid;			/* Our real user-id */

int hungup;			/* terminal has been hung up */
int privileged;			/* we're running as real uid root */
int need_holdoff;		/* need holdoff period before restarting */
int detached;			/* have detached from terminal */
volatile int status;		/* exit status for pppd */
int unsuccess;			/* # unsuccessful connection attempts */
int do_callback;		/* != 0 if we should do callback next */
int doing_callback;		/* != 0 if we are doing callback */
#define pppdb NULL

//	int (*holdoff_hook) __P((void)) = NULL;
int (*new_phase_hook) __P((int)) = NULL;

static int conn_running;	/* we have a [dis]connector running */
static int devfd;		/* fd of underlying device */
static int fd_ppp = -1;		/* fd for talking PPP */
static int fd_loop;		/* fd for getting demand-dial packets */

int phase;			/* where the link is at */
int kill_link;
int open_ccp_flag;
int listen_time;
int got_sigusr2;
int got_sigterm;
int got_sighup;

static int waiting;
static sigjmp_buf sigjmp;

char **script_env;		/* Env. variable values for scripts */
int s_env_nalloc;		/* # words avail at script_env */

u_char outpacket_buf[PPP_MRU+PPP_HDRLEN]; /* buffer for outgoing packet */
u_char inpacket_buf[PPP_MRU+PPP_HDRLEN]; /* buffer for incoming packet */

static int n_children;		/* # child processes still running */
static int got_sigchld;		/* set if we have received a SIGCHLD */

int privopen;			/* don't lock, open device as root */

char *no_ppp_msg = "Sorry - this system lacks PPP kernel support\n";

GIDSET_TYPE groups[NGROUPS_MAX];/* groups the user is in */
int ngroups;			/* How many groups valid in groups */

static struct timeval start_time;	/* Time when link was started. */

struct pppd_stats link_stats;
int link_connect_time;
int link_stats_valid;

const char pppoe_disc_file[] = "/var/lib/misc/pppoe-disc";


/*
 * We maintain a list of child process pids and
 * functions to call when they exit.
 */
struct subprocess {
    pid_t	pid;
    char	*prog;
    void	(*done) __P((void *));
    void	*arg;
    struct subprocess *next;
};

static struct subprocess *children;

/* Prototypes for procedures local to this file. */

static void setup_signals __P((void));
static void create_pidfile __P((void));
static void create_linkpidfile __P((void));
static void cleanup __P((void));
static void get_input __P((void));
static void calltimeout __P((void));
static struct timeval *timeleft __P((struct timeval *));
static void kill_my_pg __P((int));
static void hup __P((int));
static void term __P((int));
static void chld __P((int));
static void toggle_debug __P((int));
static void open_ccp __P((int));
static void bad_signal __P((int));
static void holdoff_end __P((void *));
static int reap_kids __P((int waitfor));
#define update_db_entry()
#define add_db_key(a)
#define delete_db_key(a)
#define cleanup_db()
static void handle_events __P((void));

extern	char	*ttyname __P((int));
extern	char	*getlogin __P((void));
int main __P((int, char *[]));

extern void prng_init();	// random.c


#ifdef ultrix
#undef	O_NONBLOCK
#define	O_NONBLOCK	O_NDELAY
#endif

#ifdef ULTRIX
#define setlogmask(x)
#endif

/*
 * PPP Data Link Layer "protocol" table.
 * One entry per supported protocol.
 * The last entry must be NULL.
 */
struct protent *protocols[] = {
    &lcp_protent,
    &pap_protent,
#ifdef CHAP_SUPPORT
    &chap_protent,
#endif
    &ipcp_protent,
#ifdef INET6
    &ipv6cp_protent,
#endif
#ifdef CCP_SUPPORT
    &ccp_protent,
#endif
    NULL
};

/*
 * If PPP_DRV_NAME is not defined, use the default "ppp" as the device name.
 */
#if !defined(PPP_DRV_NAME)
#define PPP_DRV_NAME	"ppp"
#endif /* !defined(PPP_DRV_NAME) */

extern int retransmit_time;
extern int redial_immediately;

/*
int debug_exist(const char *name)
{
	struct stat st;
	return (stat(name, &st) == 0);
}
*/

int
main(argc, argv)
    int argc;
    char *argv[];
{
    int i, t;
    char *p;
    struct passwd *pw;
    struct protent *protp;
    char numbuf[16];

    new_phase(PHASE_INITIALIZE);

    /*
     * Ensure that fds 0, 1, 2 are open, to /dev/null if nowhere else.
     * This way we can close 0, 1, 2 in detach() without clobbering
     * a fd that we are using.
     */
    if ((i = open("/dev/null", O_RDWR)) >= 0) {
		while (0 <= i && i <= 2)
		    i = dup(i);
		if (i >= 0)
		    close(i);
    }

    script_env = NULL;

    /* Initialize syslog facilities */
    //reopen_log();

    if (gethostname(hostname, MAXNAMELEN) < 0 ) {
		LOGX_ERROR("Couldn't get hostname");
		option_error("Couldn't get hostname: %m");
		exit(1);
    }
    hostname[MAXNAMELEN-1] = 0;

    /* make sure we don't create world or group writable files. */
    umask(umask(0777) | 022);

    uid = getuid();
    privileged = uid == 0;
    slprintf(numbuf, sizeof(numbuf), "%d", uid);
    script_setenv("ORIG_UID", numbuf, 0);

    ngroups = getgroups(NGROUPS_MAX, groups);

    /*
     * Initialize magic number generator now so that protocols may
     * use magic numbers in initialization.
     */
    magic_init();

    /*
     * Initialize each protocol.
     */
    for (i = 0; (protp = protocols[i]) != NULL; ++i)
        (*protp->init)(0);

    progname = *argv;

    /*
     * Parse, in order, the system options file, the user's options file,
     * and the command line arguments.
     */

    if (!parse_args(argc, argv))
		exit(EXIT_OPTION_ERROR);
    devnam_fixed = 1;		/* can no longer change device name */

    reopen_log();	// According to ipparam value, decide log name

    /*
     * Work out the device name, if it hasn't already been specified,
     * and parse the tty's options file.
     */
    if (the_channel->process_extra_options)
		(*the_channel->process_extra_options)();

    if (debug)
		setlogmask(LOG_UPTO(LOG_DEBUG));

    /*
     * Check that we are running as root.
     */
    if (geteuid() != 0) {
		option_error("must be root to run %s, since it is not setuid-root",
			     argv[0]);
		exit(EXIT_NOT_ROOT);
    }

    if (!ppp_available()) {
		option_error("%s", no_ppp_msg);
		exit(EXIT_NO_KERNEL_SUPPORT);
    }

    /*
     * Check that the options given are valid and consistent.
     */
    check_options();
    if (!sys_check_options())
		exit(EXIT_OPTION_ERROR);
    auth_check_options();
    for (i = 0; (protp = protocols[i]) != NULL; ++i)
		if (protp->check_options != NULL)
		    (*protp->check_options)();
	    if (the_channel->check_options)
			(*the_channel->check_options)();


    if (dump_options || dryrun) {
		init_pr_log(NULL, LOG_INFO);
		print_options(pr_log, NULL);
		end_pr_log();
		if (dryrun)
		    die(0);
    }

    /*
     * Initialize system-dependent stuff.
     */
    sys_init();

	/*
	 * Initialize random seed.
	 */
	prng_init();

    /*
     * Detach ourselves from the terminal, if required,
     * and identify who is running us.
     */
    if (!nodetach && !updetach)
		detach();
    p = getlogin();
    if (p == NULL) {
	pw = getpwuid(uid);
	if (pw != NULL && pw->pw_name != NULL)
	    p = pw->pw_name;
	else
	    p = "(unknown)";
    }

	LOGX_INFO("Starting");
	LOGX_DEBUG("demand=%d", demand);

    script_setenv("PPPLOGNAME", p, 0);

    if (devnam[0])
		script_setenv("DEVICE", devnam, 1);
    slprintf(numbuf, sizeof(numbuf), "%d", getpid());
    script_setenv("PPPD_PID", numbuf, 1);

    setup_signals();

    waiting = 0;

    create_linkpidfile();

    /*
     * If we're doing dial-on-demand, set up the interface now.
     */
    if (demand) {
		/*
		 * Open the loopback channel and set it up to be the ppp interface.
		 */
		fd_loop = open_ppp_loopback();
		set_ifunit(1);

		/*
		 * Configure the interface and mark it up, etc.
		 */
		demand_conf();
    }

    do_callback = 0;

	dial_cnt = 0;
    for (;;) {	// ##1

		listen_time = 0;
		need_holdoff = 1;
		devfd = -1;
		status = EXIT_OK;
		++unsuccess;
		doing_callback = do_callback;
		do_callback = 0;

		if (demand && !doing_callback) {
		    /*
		     * Don't do anything until we see some activity.
		     */
			LOGX_DEBUG("%s: PHASE_DORMANT", __FUNCTION__);
		    new_phase(PHASE_DORMANT);
		    demand_unblock();
		    add_fd(fd_loop);
		    for (;;) {	// ##2
				handle_events();
				if (kill_link && !persist)
				    break;
				if (get_loop_output()) {
				    break;
			    }
		    }	// for (;;) ##2
		    remove_fd(fd_loop);
		    if (kill_link && !persist) {
				LOGX_DEBUG("%s: (kill_link && !persist)", __FUNCTION__);
				break;
			}

		    /*
		     * Now we want to bring up the link.
		     */
		    demand_block();
		    info("Starting link");
		}	// if (demand && !doing_callback)

		new_phase(PHASE_SERIALCONN);

		devfd = the_channel->connect();
		LOGX_DEBUG("%s: the_channel->connect()=%d", __FUNCTION__, devfd);
		if (devfd < 0) {
			redial_immediately = 0;
		    continue;
		}

		/* set up the serial device as a ppp interface */
		fd_ppp = the_channel->establish_ppp(devfd);
		LOGX_DEBUG("%s: the_channel->establish_ppp()=%d", __FUNCTION__, fd_ppp);
		if (fd_ppp < 0) {
		    status = EXIT_FATAL_ERROR;
		    goto disconnect;
		}

		if (!demand && ifunit >= 0)
		    set_ifunit(1);

		/*
		 * Start opening the connection and wait for
		 * incoming events (reply, timeout, etc.).
		 */
		notice("Connect: %s <--> %s", ifname, ppp_devnam);
		my_gettimeofday(&start_time, NULL);
		link_stats_valid = 0;
		script_unsetenv("CONNECT_TIME");
		script_unsetenv("BYTES_SENT");
		script_unsetenv("BYTES_RCVD");
		lcp_lowerup(0);

		add_fd(fd_ppp);
		lcp_open(0);		/* Start protocol */
		status = EXIT_NEGOTIATION_FAILED;
		new_phase(PHASE_ESTABLISH);
		while (phase != PHASE_DEAD) {
		    handle_events();
		    get_input();
		    if (kill_link)
				lcp_close(0, "User request");
#ifdef CCP_SUPPORT
			if (open_ccp_flag) {
				if (phase == PHASE_NETWORK || phase == PHASE_RUNNING) {
					ccp_fsm[0].flags = OPT_RESTART; /* clears OPT_SILENT */
					(*ccp_protent.open)(0);
				}
			}
#endif
		}	// while (phase != PHASE_DEAD)

		LOGX_DEBUG("%s: phase == PHASE_DEAD", __FUNCTION__);

		/*
		 * Print connect time and statistics.
		 */
		LOGX_NOTICE("Disconnected.");
		if (link_stats_valid) {
		    int tt = (link_connect_time + 5) / 6;    /* 1/10ths of minutes */
		    LOGX_NOTICE("Connect time %d.%d minutes.", tt / 10, tt % 10);
		    LOGX_NOTICE("Sent %u bytes, received %u bytes.", link_stats.bytes_out, link_stats.bytes_in);
		}

		/*
		 * Delete pid file before disestablishing ppp.  Otherwise it
		 * can happen that another pppd gets the same unit and then
		 * we delete its pid file.
		 */
		if (!demand) {
		    if (pidfilename[0] != 0
				&& unlink(pidfilename) < 0 && errno != ENOENT)
					warn("unable to delete pid file %s: %m", pidfilename);
		    pidfilename[0] = 0;
		}

		/*
		 * If we may want to bring the link up again, transfer
		 * the ppp unit back to the loopback.  Set the
		 * real serial device back to its normal mode of operation.
		 */
		remove_fd(fd_ppp);
		clean_check();
		the_channel->disestablish_ppp(devfd);
		fd_ppp = -1;
		if (!hungup)
		    lcp_lowerdown(0);
		if (!demand)
		    script_unsetenv("IFNAME");

disconnect:
		LOGX_DEBUG("%s: disconnect", __FUNCTION__);

		if (!demand) {
			sys_cleanup();
		}

		new_phase(PHASE_DISCONNECT);
		the_channel->disconnect();

		if (the_channel->cleanup)
		    (*the_channel->cleanup)();

		if (!demand) {
		    if (pidfilename[0] != 0
				&& unlink(pidfilename) < 0 && errno != ENOENT)
					warn("unable to delete pid file %s: %m", pidfilename);
		    pidfilename[0] = 0;
		}

		if (!persist || (maxfail > 0 && unsuccess >= maxfail))
		    break;
		if (demand)
		    demand_discard();

		if (need_holdoff) {
			LOGX_DEBUG("%s: redial_immediately=%d", __FUNCTION__, redial_immediately);

			/***************************************
			 * modify by tanghui 2006-03-28
			 * first 3 time set to 15 sec
			 * after that, should try to redial randomly
			 * FIXME: has send a PADI before HOLDOFF so, (3 - 1)
			 ***************************************/
			if (!redial_immediately)
			{
				if(!idle_time_limit)
				{
					if(dial_cnt < 3)
					{
						holdoff = 10;
					}
					else
					{
						holdoff = (int)(3 + (((retransmit_time - 3.0) * rand())/ (RAND_MAX + 1.0)));
						if((holdoff > 93) && (holdoff > retransmit_time - 90))
						{
							holdoff -= 90;
						}
						//holdoff = 3 + gen_random_int(retransmit_time - 3);
					}
				}
				else
				{
					holdoff = 10;
				}
				t = holdoff;
			}
			else
			{
				t = 10; //modified from 0 to 10 //by crazy 20070508
				redial_immediately = 0;
			}

			LOGX_DEBUG("%s: t=%d", __FUNCTION__, t);

			if (t > 0) {
			    new_phase(PHASE_HOLDOFF);
			    TIMEOUT(holdoff_end, NULL, t);
			    do {
					handle_events();
					if (kill_link)
					    new_phase(PHASE_DORMANT); /* allow signal to end holdoff */
				} while (phase == PHASE_HOLDOFF);
			    if (!persist)
					break;
			}
		}	// need_holdoff
		else {
			LOGX_DEBUG("%s: !need_holdoff", __FUNCTION__);
		}
    }	// for(;;)	##1

    /* Wait for scripts to finish */
    while (n_children > 0) {
	if (debug) {
	    struct subprocess *chp;
	    dbglog("Waiting for %d child processes...", n_children);
	    for (chp = children; chp != NULL; chp = chp->next)
		dbglog("  script %s, pid %d", chp->prog, chp->pid);
	}
	if (reap_kids(1) < 0)
	    break;
    }

    die(status);
    return 0;
}

/*
 * handle_events - wait for something to happen and respond to it.
 */
static void
handle_events()
{
    struct timeval timo;
    sigset_t mask;

    kill_link = open_ccp_flag = 0;
    if (sigsetjmp(sigjmp, 1) == 0) {
	sigprocmask(SIG_BLOCK, &mask, NULL);
	if (got_sighup || got_sigterm || got_sigusr2 || got_sigchld) {
	    sigprocmask(SIG_UNBLOCK, &mask, NULL);
	} else {
	    waiting = 1;
	    sigprocmask(SIG_UNBLOCK, &mask, NULL);
	    wait_input(timeleft(&timo));
	}
    }
    waiting = 0;
    calltimeout();
    if (got_sighup) {
	kill_link = 1;
	got_sighup = 0;
	if (status != EXIT_HANGUP)
	    status = EXIT_USER_REQUEST;
    }
    if (got_sigterm) {
	kill_link = 1;
#if 1	// zzz
	persist = 0;
#else
	/*
	 * remove by tanghui @ 2006-03-31
	 * don't terminate the process
	 */
	//persist = 0;
	/*****************************/
#endif
	status = EXIT_USER_REQUEST;
	got_sigterm = 0;
    }
    if (got_sigchld) {
	reap_kids(0);	/* Don't leave dead kids lying around */
	got_sigchld = 0;
    }
    if (got_sigusr2) {
	open_ccp_flag = 1;
	got_sigusr2 = 0;
    }
}

/*
 * setup_signals - initialize signal handling.
 */
static void
setup_signals()
{
    struct sigaction sa;
    sigset_t mask;

    /*
     * Compute mask of all interesting signals and install signal handlers
     * for each.  Only one signal handler may be active at a time.  Therefore,
     * all other signals should be masked when any handler is executing.
     */
    sigemptyset(&mask);
    sigaddset(&mask, SIGHUP);
    sigaddset(&mask, SIGINT);
    sigaddset(&mask, SIGTERM);
    sigaddset(&mask, SIGCHLD);
    sigaddset(&mask, SIGUSR2);

#define SIGNAL(s, handler)	do { \
	sa.sa_handler = handler; \
	if (sigaction(s, &sa, NULL) < 0) \
	    fatal("Couldn't establish signal handler (%d): %m", s); \
    } while (0)

    sa.sa_mask = mask;
    sa.sa_flags = 0;
    SIGNAL(SIGHUP, hup);		/* Hangup */
    SIGNAL(SIGINT, term);		/* Interrupt */
    SIGNAL(SIGTERM, term);		/* Terminate */
    SIGNAL(SIGCHLD, chld);

    SIGNAL(SIGUSR1, toggle_debug);	/* Toggle debug flag */
    SIGNAL(SIGUSR2, open_ccp);		/* Reopen CCP */

    /*
     * Install a handler for other signals which would otherwise
     * cause pppd to exit without cleaning up.
     */
    SIGNAL(SIGABRT, bad_signal);
    SIGNAL(SIGALRM, bad_signal);
    SIGNAL(SIGFPE, bad_signal);
    SIGNAL(SIGILL, bad_signal);
    SIGNAL(SIGPIPE, bad_signal);
    SIGNAL(SIGQUIT, bad_signal);
    SIGNAL(SIGSEGV, bad_signal);
#ifdef SIGBUS
    SIGNAL(SIGBUS, bad_signal);
#endif
#ifdef SIGEMT
    SIGNAL(SIGEMT, bad_signal);
#endif
#ifdef SIGPOLL
    SIGNAL(SIGPOLL, bad_signal);
#endif
#ifdef SIGPROF
    SIGNAL(SIGPROF, bad_signal);
#endif
#ifdef SIGSYS
    SIGNAL(SIGSYS, bad_signal);
#endif
#ifdef SIGTRAP
    SIGNAL(SIGTRAP, bad_signal);
#endif
#ifdef SIGVTALRM
    SIGNAL(SIGVTALRM, bad_signal);
#endif
#ifdef SIGXCPU
    SIGNAL(SIGXCPU, bad_signal);
#endif
#ifdef SIGXFSZ
    SIGNAL(SIGXFSZ, bad_signal);
#endif

    /*
     * Apparently we can get a SIGPIPE when we call syslog, if
     * syslogd has died and been restarted.  Ignoring it seems
     * be sufficient.
     */
    signal(SIGPIPE, SIG_IGN);
}

/*
 * set_ifunit - do things we need to do once we know which ppp
 * unit we are using.
 */
void
set_ifunit(iskey)
    int iskey;
{
    info("Using interface %s%d", PPP_DRV_NAME, ifunit);
    slprintf(ifname, sizeof(ifname), "%s%d", PPP_DRV_NAME, ifunit);
    script_setenv("IFNAME", ifname, iskey);

    char *arg[3]={_PATH_SETPPPOEPID, ipparam, NULL};  // tallest 1219
    run_program(_PATH_SETPPPOEPID,arg,0,0,0);

    if (iskey) {
	create_pidfile();	/* write pid to file */
	create_linkpidfile();
    }
}

/*
 * detach - detach us from the controlling terminal.
 */
void
detach()
{
    int pid;
    char numbuf[16];

    if (detached)
	return;
    if ((pid = fork()) < 0) {
	error("Couldn't detach (fork failed: %m)");
	die(1);			/* or just return? */
    }
    if (pid != 0) {
	/* parent */
	exit(0);		/* parent dies */
    }
    setsid();
    chdir("/");
    close(0);
    close(1);
    close(2);
    detached = 1;
    if (log_default)
	log_to_fd = -1;
    /* update pid files if they have been written already */
    if (pidfilename[0])
	create_pidfile();
    if (linkpidfile[0])
	create_linkpidfile();
    slprintf(numbuf, sizeof(numbuf), "%d", getpid());
    script_setenv("PPPD_PID", numbuf, 1);
}

/*
 * reopen_log - (re)open our connection to syslog.
 */
void
reopen_log()
{
#ifdef ULTRIX
    openlog("pppd", LOG_PID);
#else
#ifdef MPPPOE_SUPPORT
    if(!strncmp(ipparam, "0", 1))
        openlog("pppoe", LOG_PID | LOG_NDELAY, LOG_PPP);
    else
        openlog("pppoe-1", LOG_PID | LOG_NDELAY, LOG_PPP);
#else
    openlog("pppoe", LOG_PID | LOG_NDELAY, LOG_PPP);
#endif
    setlogmask(LOG_UPTO(logmask));
#endif
}

/*
 * Create a file containing our process ID.
 */
static void
create_pidfile()
{
    FILE *pidfile;

    slprintf(pidfilename, sizeof(pidfilename), "%s%s.pid",
	     _PATH_VARRUN, ifname);
    if ((pidfile = fopen(pidfilename, "w")) != NULL) {
	fprintf(pidfile, "%d\n", getpid());
	(void) fclose(pidfile);
    } else {
	error("Failed to create pid file %s: %m", pidfilename);
	pidfilename[0] = 0;
    }
}

static void
create_linkpidfile()
{
    FILE *pidfile;

    if (linkname[0] == 0)
	return;
    script_setenv("LINKNAME", linkname, 1);
    slprintf(linkpidfile, sizeof(linkpidfile), "%sppp-%s.pid",
	     _PATH_VARRUN, linkname);
    if ((pidfile = fopen(linkpidfile, "w")) != NULL) {
	fprintf(pidfile, "%d\n", getpid());
	if (ifname[0])
	    fprintf(pidfile, "%s\n", ifname);
	(void) fclose(pidfile);
    } else {
	error("Failed to create pid file %s: %m", linkpidfile);
	linkpidfile[0] = 0;
    }
}

/*
 * holdoff_end - called via a timeout when the holdoff period ends.
 */
static void
holdoff_end(arg)
    void *arg;
{
    new_phase(PHASE_DORMANT);
}

/*
 * get_input - called when incoming data is available.
 */
static void
get_input()
{
    int len, i;
    u_char *p;
    u_short protocol;
    struct protent *protp;

    p = inpacket_buf;	/* point to beginning of packet buffer */

    len = read_packet(inpacket_buf);
    if (len < 0)
		return;

    if (len == 0) {
		notice("Modem hangup");
		hungup = 1;
		status = EXIT_HANGUP;
		lcp_lowerdown(0);	/* serial link is no longer available */
		link_terminated(0);
		return;
    }

    if (debug /*&& (debugflags & DBG_INPACKET)*/)
		dbglog("rcvd %P", p, len);

    if (len < PPP_HDRLEN) {
		MAINDEBUG(("io(): Received short packet."));
		return;
    }

    p += 2;				/* Skip address and control */
    GETSHORT(protocol, p);
    len -= PPP_HDRLEN;

    /*
     * Toss all non-LCP packets unless LCP is OPEN.
     */
    if (protocol != PPP_LCP && lcp_fsm[0].state != OPENED) {
		MAINDEBUG(("get_input: Received non-LCP packet when LCP not open."));
		return;
    }

    /*
     * Until we get past the authentication phase, toss all packets
     * except LCP, LQR and authentication packets.
     */
    if (phase <= PHASE_AUTHENTICATE
		&& !(protocol == PPP_LCP || protocol == PPP_LQR
		     || protocol == PPP_PAP || protocol == PPP_CHAP)) {
		MAINDEBUG(("get_input: discarding proto 0x%x in phase %d",
			   protocol, phase));
		return;
    }

    /*
     * Upcall the proper protocol input routine.
     */
    for (i = 0; (protp = protocols[i]) != NULL; ++i) {
		if (protp->protocol == protocol && protp->enabled_flag) {
		    (*protp->input)(0, p, len);
		    return;
		}
		if (protocol == (protp->protocol & ~0x8000) && protp->enabled_flag
			&& protp->datainput != NULL) {
			(*protp->datainput)(0, p, len);
			return;
		}
    }

    if (debug) {
		warn("Unsupported protocol 0x%x received", protocol);
    }
    lcp_sprotrej(0, p - PPP_HDRLEN, len + PPP_HDRLEN);
}

/*
 * new_phase - signal the start of a new phase of pppd's operation.
 */
void
new_phase(p)
    int p;
{
    phase = p;
    if (new_phase_hook)
	(*new_phase_hook)(p);
}

/*
 * die - clean up state and exit with the specified status.
 */
void
die(status)
    int status;
{
    cleanup();

	LOGX_INFO("Exiting");
    exit(status);
}

/*
 * cleanup - restore anything which needs to be restored before we exit
 */
/* ARGSUSED */
static void
cleanup()
{
    sys_cleanup();

    if (fd_ppp >= 0)
		the_channel->disestablish_ppp(devfd);
    if (the_channel->cleanup)
		(*the_channel->cleanup)();

    if (pidfilename[0] != 0 && unlink(pidfilename) < 0 && errno != ENOENT)
		warn("unable to delete pid file %s: %m", pidfilename);
    pidfilename[0] = 0;
    if (linkpidfile[0] != 0 && unlink(linkpidfile) < 0 && errno != ENOENT)
		warn("unable to delete pid file %s: %m", linkpidfile);
    linkpidfile[0] = 0;

	unlink(pppoe_disc_file);
}

/*
 * update_link_stats - get stats at link termination.
 */
void
update_link_stats(u)
    int u;
{
    struct timeval now;
    char numbuf[32];

    if (!get_ppp_stats(u, &link_stats)
	|| my_gettimeofday(&now, NULL) < 0)
	return;
    link_connect_time = now.tv_sec - start_time.tv_sec;
    link_stats_valid = 1;

    slprintf(numbuf, sizeof(numbuf), "%d", link_connect_time);
    script_setenv("CONNECT_TIME", numbuf, 0);
    slprintf(numbuf, sizeof(numbuf), "%d", link_stats.bytes_out);
    script_setenv("BYTES_SENT", numbuf, 0);
    slprintf(numbuf, sizeof(numbuf), "%d", link_stats.bytes_in);
    script_setenv("BYTES_RCVD", numbuf, 0);
}


struct	callout {
    struct timeval	c_time;		/* time at which to call routine */
    void		*c_arg;		/* argument to routine */
    void		(*c_func) __P((void *)); /* routine */
    struct		callout *c_next;
};

static struct callout *callout = NULL;	/* Callout list */
static struct timeval timenow;		/* Current time */

/*
 * timeout - Schedule a timeout.
 *
 * Note that this timeout takes the number of milliseconds, NOT hz (as in
 * the kernel).
 */
void
timeout(func, arg, secs, usecs)
    void (*func) __P((void *));
    void *arg;
    int secs, usecs;
{
    struct callout *newp, *p, **pp;

    // Compiler error
    //MAINDEBUG(("Timeout %p:%p in %d.%03d seconds.", func, arg,
    //	       time / 1000, time % 1000));

    /*
     * Allocate timeout.
     */
    if ((newp = (struct callout *) malloc(sizeof(struct callout))) == NULL)
	fatal("Out of memory in timeout()!");
    newp->c_arg = arg;
    newp->c_func = func;
    my_gettimeofday(&timenow, NULL);
    newp->c_time.tv_sec = timenow.tv_sec + secs;
    newp->c_time.tv_usec = timenow.tv_usec + usecs;
    if (newp->c_time.tv_usec >= 1000000) {
	newp->c_time.tv_sec += newp->c_time.tv_usec / 1000000;
	newp->c_time.tv_usec %= 1000000;
    }

    /*
     * Find correct place and link it in.
     */
    for (pp = &callout; (p = *pp); pp = &p->c_next)
	if (newp->c_time.tv_sec < p->c_time.tv_sec
	    || (newp->c_time.tv_sec == p->c_time.tv_sec
		&& newp->c_time.tv_usec < p->c_time.tv_usec))
	    break;
    newp->c_next = p;
    *pp = newp;
}


/*
 * untimeout - Unschedule a timeout.
 */
void
untimeout(func, arg)
    void (*func) __P((void *));
    void *arg;
{
    struct callout **copp, *freep;

    MAINDEBUG(("Untimeout %p:%p.", func, arg));

    /*
     * Find first matching timeout and remove it from the list.
     */
    for (copp = &callout; (freep = *copp); copp = &freep->c_next)
	if (freep->c_func == func && freep->c_arg == arg) {
	    *copp = freep->c_next;
	    free((char *) freep);
	    break;
	}
}


/*
 * calltimeout - Call any timeout routines which are now due.
 */
static void
calltimeout()
{
    struct callout *p;

    while (callout != NULL) {
	p = callout;

	if (my_gettimeofday(&timenow, NULL) < 0)
	    fatal("Failed to get time of day: %m");
	if (!(p->c_time.tv_sec < timenow.tv_sec
	      || (p->c_time.tv_sec == timenow.tv_sec
		  && p->c_time.tv_usec <= timenow.tv_usec)))
	    break;		/* no, it's not time yet */

	callout = p->c_next;
	(*p->c_func)(p->c_arg);

	free((char *) p);
    }
}


/*
 * timeleft - return the length of time until the next timeout is due.
 */
static struct timeval *
timeleft(tvp)
    struct timeval *tvp;
{
    if (callout == NULL)
	return NULL;

    my_gettimeofday(&timenow, NULL);
    tvp->tv_sec = callout->c_time.tv_sec - timenow.tv_sec;
    tvp->tv_usec = callout->c_time.tv_usec - timenow.tv_usec;
    if (tvp->tv_usec < 0) {
	tvp->tv_usec += 1000000;
	tvp->tv_sec -= 1;
    }
    if (tvp->tv_sec < 0)
	tvp->tv_sec = tvp->tv_usec = 0;

    return tvp;
}


/*
 * kill_my_pg - send a signal to our process group, and ignore it ourselves.
 */
static void
kill_my_pg(sig)
    int sig;
{
    struct sigaction act, oldact;

    act.sa_handler = SIG_IGN;
    act.sa_flags = 0;
    kill(0, sig);
    sigaction(sig, &act, &oldact);
    sigaction(sig, &oldact, NULL);
}


/*
 * hup - Catch SIGHUP signal.
 *
 * Indicates that the physical layer has been disconnected.
 * We don't rely on this indication; if the user has sent this
 * signal, we just take the link down.
 */
static void
hup(sig)
    int sig;
{
    info("Hangup (SIGHUP)");
    got_sighup = 1;
    if (conn_running)
	/* Send the signal to the [dis]connector process(es) also */
	kill_my_pg(sig);
    if (waiting)
	siglongjmp(sigjmp, 1);
}


/*
 * term - Catch SIGTERM signal and SIGINT signal (^C/del).
 *
 * Indicates that we should initiate a graceful disconnect and exit.
 */
/*ARGSUSED*/
static void
term(sig)
    int sig;
{
    info("Terminating on signal %d.", sig);
    got_sigterm = 1;
    if (conn_running)
	/* Send the signal to the [dis]connector process(es) also */
	kill_my_pg(sig);
    if (waiting)
	siglongjmp(sigjmp, 1);
}


/*
 * chld - Catch SIGCHLD signal.
 * Sets a flag so we will call reap_kids in the mainline.
 */
static void
chld(sig)
    int sig;
{
    got_sigchld = 1;
    if (waiting)
	siglongjmp(sigjmp, 1);
}


/*
 * toggle_debug - Catch SIGUSR1 signal.
 *
 * Toggle debug flag.
 */
static void toggle_debug(int sig)
{
    debug = !debug;
	setlogmask(LOG_UPTO(debug ? LOG_DEBUG : logmask));
}


/*
 * open_ccp - Catch SIGUSR2 signal.
 *
 * Try to (re)negotiate compression.
 */
/*ARGSUSED*/
static void
open_ccp(sig)
    int sig;
{
    got_sigusr2 = 1;
    if (waiting)
	siglongjmp(sigjmp, 1);
}


/*
 * bad_signal - We've caught a fatal signal.  Clean up state and exit.
 */
static void
bad_signal(sig)
    int sig;
{
    static int crashed = 0;

    if (crashed)
	_exit(127);
    crashed = 1;

	LOGX_ERROR("Fatal signal %d.", sig);

    if (conn_running)
	kill_my_pg(SIGTERM);
    die(127);
}

/*
 * run-program - execute a program with given arguments,
 * but don't wait for it.
 * If the program can't be executed, logs an error unless
 * must_exist is 0 and the program file doesn't exist.
 * Returns -1 if it couldn't fork, 0 if the file doesn't exist
 * or isn't an executable plain file, or the process ID of the child.
 * If done != NULL, (*done)(arg) will be called later (within
 * reap_kids) iff the return value is > 0.
 */
pid_t
run_program(prog, args, must_exist, done, arg)
    char *prog;
    char **args;
    int must_exist;
    void (*done) __P((void *));
    void *arg;
{
    int pid;
    struct stat sbuf;

    /*
     * First check if the file exists and is executable.
     * We don't use access() because that would use the
     * real user-id, which might not be root, and the script
     * might be accessible only to root.
     */
    errno = EINVAL;
    if (stat(prog, &sbuf) < 0 || !S_ISREG(sbuf.st_mode)
	|| (sbuf.st_mode & (S_IXUSR|S_IXGRP|S_IXOTH)) == 0) {
	if (must_exist || errno != ENOENT)
	    warn("Can't execute %s: %m", prog);
	return 0;
    }

    pid = fork();
    if (pid == -1) {
	error("Failed to create child process for %s: %m", prog);
	return -1;
    }
    if (pid == 0) {
	int new_fd;

	/* Leave the current location */
	(void) setsid();	/* No controlling tty. */
	(void) umask (S_IRWXG|S_IRWXO);
	(void) chdir ("/");	/* no current directory. */
	setuid(0);		/* set real UID = root */
	setgid(getegid());

	/* Ensure that nothing of our device environment is inherited. */
	sys_close();
	closelog();
	close (0);
	close (1);
	close (2);
	if (the_channel->close)
	    (*the_channel->close)();

        /* Don't pass handles to the PPP device, even by accident. */
	new_fd = open (_PATH_DEVNULL, O_RDWR);
	if (new_fd >= 0) {
	    if (new_fd != 0) {
	        dup2  (new_fd, 0); /* stdin <- /dev/null */
		close (new_fd);
	    }
	    dup2 (0, 1); /* stdout -> /dev/null */
	    dup2 (0, 2); /* stderr -> /dev/null */
	}

#ifdef BSD
	/* Force the priority back to zero if pppd is running higher. */
	if (setpriority (PRIO_PROCESS, 0, 0) < 0)
	    warn("can't reset priority to 0: %m");
#endif

	/* SysV recommends a second fork at this point. */

	/* run the program */
	execve(prog, args, script_env);
	if (must_exist || errno != ENOENT) {
	    /* have to reopen the log, there's nowhere else
	       for the message to go. */
	    reopen_log();
	    syslog(LOG_ERR, "Can't execute %s: %m", prog);
	    closelog();
	}
	_exit(-1);
    }

    if (debug)
	dbglog("Script %s started (pid %d)", prog, pid);
    record_child(pid, prog, done, arg);
    return pid;
}


/*
 * record_child - add a child process to the list for reap_kids
 * to use.
 */
void
record_child(pid, prog, done, arg)
    int pid;
    char *prog;
    void (*done) __P((void *));
    void *arg;
{
    struct subprocess *chp;

    ++n_children;

    chp = (struct subprocess *) malloc(sizeof(struct subprocess));
    if (chp == NULL) {
	warn("losing track of %s process", prog);
    } else {
	chp->pid = pid;
	chp->prog = prog;
	chp->done = done;
	chp->arg = arg;
	chp->next = children;
	children = chp;
    }
}


/*
 * reap_kids - get status from any dead child processes,
 * and log a message for abnormal terminations.
 */
static int
reap_kids(waitfor)
    int waitfor;
{
    int pid, status;
    struct subprocess *chp, **prevp;

    if (n_children == 0)
	return 0;
    while ((pid = waitpid(-1, &status, (waitfor? 0: WNOHANG))) != -1
	   && pid != 0) {
	for (prevp = &children; (chp = *prevp) != NULL; prevp = &chp->next) {
	    if (chp->pid == pid) {
		--n_children;
		*prevp = chp->next;
		break;
	    }
	}
	if (WIFSIGNALED(status)) {
	    warn("Child process %s (pid %d) terminated with signal %d",
		 (chp? chp->prog: "??"), pid, WTERMSIG(status));
	} else if (debug)
	    dbglog("Script %s finished (pid %d), status = 0x%x",
		   (chp? chp->prog: "??"), pid, status);
	if (chp && chp->done)
	    (*chp->done)(chp->arg);
	if (chp)
	    free(chp);
    }
    if (pid == -1) {
	if (errno == ECHILD)
	    return -1;
	if (errno != EINTR)
	    error("Error waiting for child process: %m");
    }
    return 0;
}

/*
 * script_setenv - set an environment variable value to be used
 * for scripts that we run (e.g. ip-up, auth-up, etc.)
 */
void
script_setenv(var, value, iskey)
    char *var, *value;
    int iskey;
{
    size_t varl = strlen(var);
    size_t vl = varl + strlen(value) + 2;
    int i;
    char *p, *newstring;

    newstring = (char *) malloc(vl+1);
    if (newstring == 0)
	return;
    *newstring++ = iskey;
    slprintf(newstring, vl, "%s=%s", var, value);

    /* check if this variable is already set */
    if (script_env != 0) {
	for (i = 0; (p = script_env[i]) != 0; ++i) {
	    if (strncmp(p, var, varl) == 0 && p[varl] == '=') {
		if (p[-1] && pppdb != NULL)
		    delete_db_key(p);
		free(p-1);
		script_env[i] = newstring;
		if (iskey && pppdb != NULL)
		    add_db_key(newstring);
		update_db_entry();
		return;
	    }
	}
    } else {
	/* no space allocated for script env. ptrs. yet */
	i = 0;
	script_env = (char **) malloc(16 * sizeof(char *));
	if (script_env == 0)
	    return;
	s_env_nalloc = 16;
    }

    /* reallocate script_env with more space if needed */
    if (i + 1 >= s_env_nalloc) {
	int new_n = i + 17;
	char **newenv = (char **) realloc((void *)script_env,
					  new_n * sizeof(char *));
	if (newenv == 0)
	    return;
	script_env = newenv;
	s_env_nalloc = new_n;
    }

    script_env[i] = newstring;
    script_env[i+1] = 0;

    if (pppdb != NULL) {
	if (iskey)
	    add_db_key(newstring);
	update_db_entry();
    }
}

/*
 * script_unsetenv - remove a variable from the environment
 * for scripts.
 */
void
script_unsetenv(var)
    char *var;
{
    int vl = strlen(var);
    int i;
    char *p;

    if (script_env == 0)
	return;
    for (i = 0; (p = script_env[i]) != 0; ++i) {
	if (strncmp(p, var, vl) == 0 && p[vl] == '=') {
	    if (p[-1] && pppdb != NULL)
		delete_db_key(p);
	    free(p-1);
	    while ((script_env[i] = script_env[i+1]) != 0)
		++i;
	    break;
	}
    }
    if (pppdb != NULL)
	update_db_entry();
}
