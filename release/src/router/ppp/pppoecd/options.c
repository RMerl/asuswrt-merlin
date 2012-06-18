/*
 * options.c - handles option processing for PPP.
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

#define RCSID	"$Id: options.c,v 1.11 2004/10/08 07:34:34 tallest Exp $"

#include <getopt.h>
#include <stdlib.h>
#include <string.h>
#include <syslog.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include "pppd.h"
#include "fsm.h"
#include "lcp.h"
#include "ipcp.h"
#ifdef INET6
#include "ipv6cp.h"
#endif

int	debug = 0;		/* Debug flag */
int	kdebugflag = 0;		/* Tell kernel to print debug messages */
int	default_device = 1;	/* Using /dev/tty or equivalent */
char	devnam[MAXPATHLEN];	/* Device name */
bool	nodetach = 0;		/* Don't detach from controlling tty */
bool	updetach = 0;		/* Detach once link is up */
int	maxconnect = 0;		/* Maximum connect time */
char	user[MAXNAMELEN];	/* Username for PAP */
char	passwd[MAXSECRETLEN];	/* Password for PAP */
//bool	persist = 0;
bool	persist = 1;		/* Reopen link after it goes down */
char	our_name[MAXNAMELEN];	/* Our name for authentication purposes */
bool	demand = 0;		/* do dial-on-demand */
char	*ipparam = NULL;	/* Extra parameter for ip up/down scripts */
int	idle_time_limit = 0;	/* Disconnect if idle for this many seconds */
int	holdoff = 30;		/* # seconds to pause before reconnecting */
bool	holdoff_specified;	/* true if a holdoff value has been given */
int	log_to_fd = 1;		/* send log messages to this fd too */
bool	log_default = 1;	/* log_to_fd is default (stdout) */
int	maxfail = 0;		/* max # of unsuccessful connection attempts */
char	linkname[MAXPATHLEN];	/* logical name for link */
bool	tune_kernel = 1;	/* may alter kernel settings */
int	connect_delay = 1000;	/* wait this many ms after connect script */
int	req_unit = -1;		/* requested interface unit */
char	*bundle_name = NULL;	/* bundle name for multilink */
bool	dump_options;		/* print out option values */
bool	dryrun;			/* print out option values and exit */
char	*domain;		/* domain name set by domain option */
int	baud_rate;		/* Actual bits/second for serial device */
#ifdef UNNUMBERIP_SUPPORT
char	is_unnumber_ip = 0;	/* This parameter use for unnumber IP. by tallest */
#endif
char	ppp_disconnect_func[MAXFUNCLEN]; /* This is a function for dial on demand disconnection using. by tallest 0407 */

int 	logmask = LOG_NOTICE;	// more logging	-- zzz

char *current_option;		/* the name of the option being parsed */
int  privileged_option;		/* set iff the current option came from root */
char *option_source;		/* string saying where the option came from */
int  option_priority = OPRIO_CFGFILE; /* priority of the current options */
bool devnam_fixed;		/* can no longer change device name */

extern char *pppoe_ac_name;
extern char *pppoe_srv_name;
extern int lcp_echo_interval;  /* Interval between LCP echo-requests */
extern int lcp_echo_fails;     /* Tolerance to unanswered echo-requests */
extern int retry_num;	       /* interval of send disc */
extern int retransmit_time;

extern int setdevname_pppoe(const char *cp);

static char *usage_string = "usage: %s interface -d -k [-i idle] [-u username] [-p passwd] [-a acname] [-s srvname] [-r mru] [-t mtu] [-I lcp_echo_interval] [-T lcp_echo_fails] [-P ipparam] [-L Local IP] [-N retry_num] [-R set default route]"
#ifdef INET6
			    " [-6 enable PPP IPv6CP]"
#endif
			    " [-n use unnumber ip] [-C disconnected function]\n";

/*
 * parse_args - parse a string of arguments from the command line.
 */
int
parse_args(argc, argv)
    int argc;
    char **argv;
{
    int opt;
    struct in_addr Laddr;	//, Naddr;

    while ((opt = getopt(argc, argv, "dki:u:p:a:s:r:t:U:I:T:P:L:N:R6nC:v:x")) != -1) {
	    switch (opt) {
	    case 'd':
		    debug = nodetach = 1;
		    break;
	    case 'k':
		    persist = 1;
		    break;
	    case 'i':
		    idle_time_limit = atoi(optarg);
		    if (idle_time_limit > 0) {
			    demand = 1;
				maxfail = 0;
			}
			else {
				maxfail = 10;
			}
		    break;
	    case 'u':
		    strncpy(user, optarg, MAXNAMELEN);
		    strncpy(our_name, optarg, MAXNAMELEN);
			memset(optarg, 'X', strlen(optarg));
		    break;
	    case 'p':
		    strncpy(passwd, optarg, MAXSECRETLEN);
			memset(optarg, 'X', strlen(optarg));
		    break;
	    case 'a':
		    pppoe_ac_name = optarg;
		    break;
	    case 's':
		    pppoe_srv_name = optarg;
		    break;
	    case 'r':
		    lcp_wantoptions[0].neg_mru = 1;
		    lcp_wantoptions[0].mru = atoi(optarg);
		    break;
	    case 't':
		    lcp_allowoptions[0].mru = atoi(optarg);
		    break;
	    case 'I':	// by honor
		    lcp_echo_interval = atoi(optarg);
		    break;
	    case 'T':	// by honor
		    lcp_echo_fails = atoi(optarg);
		    break;
	    case 'P':	// by honor
		    ipparam = optarg;
		    break;
	    case 'L':	// by honor, support static IP Address
		    inet_aton(optarg, &Laddr);
		    ipcp_wantoptions[0].ouraddr = Laddr.s_addr;
		    break;
	    case 'N':	// by honor
		    retry_num = atoi(optarg);
		    break;
	    case 'U':
		    req_unit = atoi(optarg);
		    break;
	    case 'R':	//by tallest set default route
		    	ipcp_wantoptions[0].default_route = 1;
		    break;
	    case '6':   //by Polaris75 - enable IPv6CP
#ifdef INET6
			ipv6cp_protent.enabled_flag = 1;
#endif
			break;
	    case 'n':   //by tallest for unnumber ip use. 
#ifdef UNNUMBERIP_SUPPORT
                        is_unnumber_ip = 1;
#endif
		    break;
	    case 'C': //by tallest 0407
			strncpy(ppp_disconnect_func, optarg, MAXFUNCLEN);
                    break;
	    case 'v': //by tallest 0407
			retransmit_time = atoi(optarg);
                    break;
		case 'x':	// zzz
			logmask = LOG_INFO;
			break;
	    default:
		    fprintf(stderr, usage_string, argv[0]);
		    return 0;
	    }
    }

    if (optind < argc)
	    setdevname_pppoe(argv[optind]);
    else {
	    fprintf(stderr, usage_string, argv[0]);
	    return 0;
    }

    return 1;
}
