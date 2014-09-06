/*	$OpenBSD: main.c,v 1.52 2005/02/10 14:25:08 itojun Exp $	*/
/*	$NetBSD: main.c,v 1.9 1996/05/07 02:55:02 thorpej Exp $	*/

/*
 * Copyright (c) 1983, 1988, 1993
 *	Regents of the University of California.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#ifndef lint
char copyright[] =
"@(#) Copyright (c) 1983, 1988, 1993\n\
	Regents of the University of California.  All rights reserved.\n";
#endif /* not lint */

#ifdef  INHERITED_CODE
#ifndef lint
#if 0
static char sccsid[] = "from: @(#)main.c	8.4 (Berkeley) 3/1/94";
#else
static char *rcsid = "$OpenBSD: main.c,v 1.52 2005/02/10 14:25:08 itojun Exp $";
#endif
#endif /* not lint */
#endif

#include <net-snmp/net-snmp-config.h>
#include <net-snmp/net-snmp-includes.h>
#include <net-snmp/utilities.h>

#if HAVE_NETDB_H
#include <netdb.h>
#endif

#include "main.h"
#include "netstat.h"

#if HAVE_WINSOCK_H
#include "winstub.h"
#endif

int	Aflag;		/* show addresses of protocol control block */
int	aflag;		/* show all sockets (including servers) */
int	bflag;		/* show bytes instead of packets */
int	dflag;		/* show i/f dropped packets */
int	gflag;		/* show group (multicast) routing or stats */
int	iflag;		/* show interfaces */
int	lflag;		/* show routing table with use and ref */
int	mflag;		/* show memory stats */
int	nflag;		/* show addresses numerically */
int	oflag;		/* Open/Net-BSD style octet output */
int	pflag;		/* show given protocol */
int	qflag;		/* only display non-zero values for output */
int	rflag;		/* show routing tables (or routing stats) */
int	Sflag;		/* show source address in routing table */
int	sflag;		/* show protocol statistics */
int	tflag;		/* show i/f watchdog timers */
int	vflag;		/* be verbose */


int	interval;	/* repeat interval for i/f stats */
char	*intrface;	/* desired i/f for stats, or NULL for all i/fs */
int	af;		/* address family */
int     max_getbulk = 32;  /* specifies the max-repeaters value to use with GETBULK requests */

char    *progname = NULL;

    /*
     * struct nlist nl[] - Omitted
     */

typedef void (stringfun)(const char*);

struct protox {
        /* pr_index/pr_sindex - Omitted */ 
	int		pr_wanted;	/* 1 if wanted, 0 otherwise */
	stringfun	*pr_cblocks;	/* control blocks printing routine */
	stringfun	*pr_stats;	/* statistics printing routine */
	const char	*pr_name;	/* well-known name */
} protox[] = {
	{ 1,	tcpprotopr,	tcp_stats,	"tcp" },	
	{ 1,	udpprotopr,	udp_stats,	"udp" },	

	{ 1,	(stringfun*)0,	ip_stats,	"ip" },	/* protopr Omitted */
	{ 1,	(stringfun*)0,	icmp_stats,	"icmp" },
	/* igmp/ah/esp/ipencap/etherip/ipcomp/carp/pfsync/pim - Omitted */
	{ 0,	(stringfun*)0,	(stringfun*)0,	NULL }
};

struct protox ip6protox[] = {
	{ 1,	tcp6protopr,	(stringfun*)0,	"tcp6" },
	{ 1,	udp6protopr,	(stringfun*)0,	"udp6" },

	{ 1,	(stringfun*)0,	ip6_stats,	"ip6" },/* ip6protopr Omitted */
	{ 1,	(stringfun*)0,	icmp6_stats,	"icmp6" },
	/* pim6/rip6 - Omitted */
	{ 0,	(stringfun*)0,	(stringfun*)0,	NULL }
};

	/* {ipx,ns,atalk}protox Omitted */

struct protox *protoprotox[] = {
	protox, ip6protox, NULL
};

static void printproto(struct protox *, const char *);
static void usage(void);
static struct protox *name2protox(const char *);
static struct protox *knownname(const char *);

netsnmp_session *ss;
struct protox *tp = NULL; /* for printing cblocks & stats */

static void
optProc( int argc, char *const *argv, int opt )
{
    switch (opt) {
    case 'C':
        while (*optarg) {
            switch (*optarg++) {
	    /*	case 'A':		*BSD:  display PCB addresses
					Linux: protocol family
			Aflag = 1;
			break;
	     */
		case 'a':
			aflag = 1;
			break;
		case 'b':
			bflag = 1;
			break;
		case 'd':
			dflag = 1;
			break;
		case 'f':
                        if (!*optarg)
                            optarg = argv[optind++];
			if (strcmp(optarg, "inet") == 0)
				af = AF_INET;
			else if (strcmp(optarg, "inet6") == 0)
				af = AF_INET6;
			/*
			else if (strcmp(optarg, "local") == 0)
				af = AF_LOCAL;
			else if (strcmp(optarg, "unix") == 0)
				af = AF_UNIX;
			else if (strcmp(optarg, "ipx") == 0)
				af = AF_IPX;
			else if (strcmp(optarg, "ns") == 0)
				af = AF_NS;
			else if (strcmp(optarg, "encap") == 0)
				af = PF_KEY;
			else if (strcmp(optarg, "atalk") == 0)
				af = AF_APPLETALK;
			*/
			else {
				(void)fprintf(stderr,
				    "%s: %s: unknown address family\n",
				    progname, optarg);
				exit(1);
			}
			return;
		case 'g':
			gflag = 1;
			break;
		case 'I':
			iflag = 1;
                        if (!*optarg)
                            optarg = argv[optind++];
			intrface = optarg;
			return;
		case 'i':
			iflag = 1;
			break;
	    /*  case 'L':		FreeBSD: Display listen queue lengths
					NetBSD:  Suppress link-level routes */
	    /*	case 'l':		OpenBSD: Wider IPv6 display
					Linux:   Listening sockets only
			lflag = 1;
			break;
		case 'M':		*BSD:    Memory image
					Linux:   Masqueraded connections
			memf = optarg;
			break;
	     */
		case 'm':
			mflag = 1;
			break;
	    /*	case 'N':		*BSD:    Kernel image
			nlistf = optarg;
			break;
	     */
		case 'n':
			nflag = 1;
			break;
		case 'o':
			oflag = 1;
			break;
	    /*  case 'P':		NetBSD:
					OpenBSD: dump PCB block */
		case 'p':
                        if (!*optarg)
                            optarg = argv[optind++];
			if ((tp = name2protox(optarg)) == NULL) {
				(void)fprintf(stderr,
				    "%s: %s: unknown protocol\n",
				    progname, optarg);
				exit(1);
			}
			pflag = 1;
			return;
	    /*	case 'q':		NetBSD:  IRQ information
					OpenBSD: Suppress inactive I/Fs
			qflag = 1;
			break;
	     */
		case 'r':
			rflag = 1;
			break;
		case 'R':
                        if (optind < argc) {
                            if (argv[optind]) {
                                max_getbulk = atoi(argv[optind]);
                                if (max_getbulk == 0) {
                                    usage();
                                    fprintf(stderr, "Bad -CR option: %s\n", 
                                            argv[optind]);
                                    exit(1);
                                }
                            }
                        } else {
                            usage();
                            fprintf(stderr, "Bad -CR option: no argument given\n");
                            exit(1);
                        }
                        optind++;
                        break;
		case 'S':	     /* FreeBSD:
					NetBSD:  Semi-numeric display
					OpenBSD: Show route source selector */
			Sflag = 1;
			break;
		case 's':
			++sflag;
			break;
	     /*	case 't':		FreeBSD:
					OpenBSD: Display watchdog timers
			tflag = 1;
			break;
		case 'u':		OpenBSD: unix sockets only
			af = AF_UNIX;
			break;
	      */
		case 'v':
			vflag = 1;
			break;
		case 'w':
                        if (!*optarg)
                            optarg = argv[optind++];
			interval = atoi(optarg);
			iflag = 1;
			return;
		case '?':
		default:
			usage();
            }
        }
        break;   /* End of '-Cx' switch */

    /*
     *  Backward compatability for the main display modes
     *    (where this doesn't clash with standard SNMP flags)
     */
    case 'i':
	iflag = 1;
	break;
    case 'R':
	rflag = 1;    /* -r sets the retry count */
	break;
    case 's':
	++sflag;
	break;
    }
}

int
main(int argc, char *argv[])
{
	netsnmp_session session;
	struct protoent *p;
        char *cp;

	af = AF_UNSPEC;
        cp = strrchr( argv[0], '/' );
        if (cp)
            progname = cp+1;
        else
            progname = argv[0];

	switch (snmp_parse_args( argc, argv, &session, "C:iRs", optProc)) {
	case NETSNMP_PARSE_ARGS_ERROR:
	    exit(1);
	case NETSNMP_PARSE_ARGS_SUCCESS_EXIT:
	    exit(0);
	case NETSNMP_PARSE_ARGS_ERROR_USAGE:
	    usage();
	    exit(1);
	default:
	    break;
	}

	    /*
	     * Check argc vs optind ??
	     */
	argv += optind;
	argc -= optind;

    /*
     * Open an SNMP session.
     */
    SOCK_STARTUP;
    ss = snmp_open(&session);
    if (ss == NULL) {
        /*
         * diagnose snmp_open errors with the input netsnmp_session pointer 
         */
        snmp_sess_perror("snmpnetstat", &session);
        SOCK_CLEANUP;
        exit(1);
    }

	/*
	 * Omitted:
	 *     Privilege handling
	 *    "Backward Compatibility"
	 *     Kernel namelis handling
	 */

	if (mflag) {
            /*
		mbpr(nl[N_MBSTAT].n_value, nl[N_MBPOOL].n_value,
		    nl[N_MCLPOOL].n_value);
             */
		exit(0);
	}
	if (pflag) {
		printproto(tp, tp->pr_name);
		exit(0);
	}
	/*
	 * Keep file descriptors open to avoid overhead
	 * of open/close on each call to get* routines.
	 */
	sethostent(1);
	setnetent(1);
	if (iflag) {
		intpr(interval);
		exit(0);
	}
	if (rflag) {
             /*
		if (sflag)
			rt_stats();
		else
              */
			routepr();
		exit(0);
	}
     /*
	if (gflag) {
		if (sflag) {
			if (af == AF_INET || af == AF_UNSPEC)
				mrt_stats(nl[N_MRTPROTO].n_value,
				    nl[N_MRTSTAT].n_value);
#ifdef NETSNMP_ENABLE_IPV6
			if (af == AF_INET6 || af == AF_UNSPEC)
				mrt6_stats(nl[N_MRT6PROTO].n_value,
				    nl[N_MRT6STAT].n_value);
#endif
		}
		else {
			if (af == AF_INET || af == AF_UNSPEC)
				mroutepr(nl[N_MRTPROTO].n_value,
				    nl[N_MFCHASHTBL].n_value,
				    nl[N_MFCHASH].n_value,
				    nl[N_VIFTABLE].n_value);
#ifdef NETSNMP_ENABLE_IPV6
			if (af == AF_INET6 || af == AF_UNSPEC)
				mroute6pr(nl[N_MRT6PROTO].n_value,
				    nl[N_MF6CTABLE].n_value,
				    nl[N_MIF6TABLE].n_value);
#endif
		}
		exit(0);
	}
     */
	if (af == AF_INET || af == AF_UNSPEC) {
		setprotoent(1);
		setservent(1);
		/* ugh, this is O(MN) ... why do we do this? */
		while ((p = getprotoent())) {
			for (tp = protox; tp->pr_name; tp++)
				if (strcmp(tp->pr_name, p->p_name) == 0)
					break;
			if (tp->pr_name == NULL || tp->pr_wanted == 0)
				continue;
			printproto(tp, p->p_name);
		}
		endprotoent();
	}
	if (af == AF_INET6 || af == AF_UNSPEC)
		for (tp = ip6protox; tp->pr_name; tp++)
			printproto(tp, tp->pr_name);
    /*
	if (af == AF_IPX || af == AF_UNSPEC)
		for (tp = ipxprotox; tp->pr_name; tp++)
			printproto(tp, tp->pr_name);
	if (af == AF_NS || af == AF_UNSPEC)
		for (tp = nsprotox; tp->pr_name; tp++)
			printproto(tp, tp->pr_name);
	if ((af == AF_UNIX || af == AF_UNSPEC) && !sflag)
		unixpr(nl[N_UNIXSW].n_value);
	if (af == AF_APPLETALK || af == AF_UNSPEC)
		for (tp = atalkprotox; tp->pr_name; tp++)
			printproto(tp, tp->pr_name);
     */
	exit(0);
}

/*
 * Print out protocol statistics or control blocks (per sflag).
 * Namelist checks - Omitted
 */
static void
printproto(struct protox *tp, const char *name)
{
	void (*pr)(const char *);

	if (sflag) {
		pr = tp->pr_stats;
	} else {
		pr = tp->pr_cblocks;
	}
	if (pr != NULL)
		(*pr)(name);
}

/*
 * Read kernel memory - Omitted
 */

const char *
plural(int n)
{
	return (n != 1 ? "s" : "");
}

/*
 * Find the protox for the given "well-known" name.
 */
static struct protox *
knownname(const char *name)
{
	struct protox **tpp, *tp;

	for (tpp = protoprotox; *tpp; tpp++)
		for (tp = *tpp; tp->pr_name; tp++)
			if (strcmp(tp->pr_name, name) == 0)
				return (tp);
	return (NULL);
}

/*
 * Find the protox corresponding to name.
 */
static struct protox *
name2protox(const char *name)
{
	struct protox *tp;
	char **alias;			/* alias from p->aliases */
	struct protoent *p;

	/*
	 * Try to find the name in the list of "well-known" names. If that
	 * fails, check if name is an alias for an Internet protocol.
	 */
	if ((tp = knownname(name)))
		return (tp);

	setprotoent(1);			/* make protocol lookup cheaper */
	while ((p = getprotoent())) {
		/* netsnmp_assert: name not same as p->name */
		for (alias = p->p_aliases; *alias; alias++)
			if (strcmp(name, *alias) == 0) {
				endprotoent();
				return (knownname(p->p_name));
			}
	}
	endprotoent();
	return (NULL);
}

static void
usage(void)
{
	(void)fprintf(stderr,
"usage: %s [snmp_opts] [-Can] [-Cf address_family]\n", progname);
	(void)fprintf(stderr,
"       %s [snmp_opts] [-CbdgimnrSs] [-Cf address_family]\n", progname);
	(void)fprintf(stderr,
"       %s [snmp_opts] [-Cbdn] [-CI interface] [-Cw wait]\n", progname);
	(void)fprintf(stderr,
"       %s [snmp_opts] [-Cs] [-Cp protocol]\n", progname);
	(void)fprintf(stderr,
"       %s [snmp_opts] [-Ca] [-Cf address_family] [-Ci | -CI interface]\n", progname);
	exit(1);
}
