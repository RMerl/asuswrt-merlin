/*-
 * Copyright (c) 1990 The Regents of the University of California.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
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
"@(#) Copyright (c) 1990 The Regents of the University of California.\n\
 All rights reserved.\n";
#endif /* not lint */

/*
@(#)portmap.c	2.3 88/08/11 4.0 RPCSRC
static char sccsid[] = "@(#)portmap.c 1.32 87/08/06 Copyr 1984 Sun Micro";
*/

/*
 * portmap.c, Implements the program,version to port number mapping for
 * rpc.
 */

/*
 * Copyright (c) 2009, Sun Microsystems, Inc.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 * - Redistributions of source code must retain the above copyright notice,
 *   this list of conditions and the following disclaimer.
 * - Redistributions in binary form must reproduce the above copyright notice,
 *   this list of conditions and the following disclaimer in the documentation
 *   and/or other materials provided with the distribution.
 * - Neither the name of Sun Microsystems, Inc. nor the names of its
 *   contributors may be used to endorse or promote products derived
 *   from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#include <rpc/rpc.h>
#include <rpc/pmap_prot.h>
#include <stdio.h>
#include <unistd.h>
#include <syslog.h>
#include <netdb.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <sys/wait.h>
#include <sys/signal.h>
#include <sys/time.h>
#include <sys/resource.h>
#include <sys/types.h>
#ifdef SYSV40
#include <netinet/in.h>
#endif
#include <arpa/inet.h>

#include <stdlib.h>
#include <pwd.h>

#ifndef LOG_PERROR
#define LOG_PERROR 0
#endif

#ifndef LOG_DAEMON
#define LOG_DAEMON 0
#endif

/* Older SYSV. */
#if !defined(SIGCHLD) && defined(SIGCLD)
#define SIGCHLD      SIGCLD
#endif

#ifndef svc_getcaller		/* SYSV4 */
#  define svc_getcaller svc_getrpccaller
#endif

static void reg_service(struct svc_req *rqstp, SVCXPRT *xprt);
#ifndef IGNORE_SIGCHLD			/* Lionel Cons <cons@dxcern.cern.ch> */
static void reap(int);
#endif
static void callit(struct svc_req *rqstp, SVCXPRT *xprt);
struct pmaplist *pmaplist;
int debugging = 0;
int store_fd = -1;
static void dump_table(void);
static void load_table(void);

#include "pmap_check.h"

 /*
  * How desperate can one be. It is possible to prevent an attacker from
  * manipulating your portmapper tables from outside with requests that
  * contain spoofed source address information. The countermeasure is to
  * force all rpc servers to register and unregister with the portmapper via
  * the loopback network interface, instead of via the primary network
  * interface that every host can talk to. For this countermeasure to work it
  * is necessary to #define LOOPBACK_SETUNSET, to disable source routing in
  * the kernel, and to modify libc so that get_myaddress() chooses the
  * loopback interface address.
  */

#ifdef LOOPBACK_SETUNSET
static SVCXPRT *ludpxprt, *ltcpxprt;
static int on = 1;
#ifndef INADDR_LOOPBACK
#define INADDR_LOOPBACK ntohl(inet_addr("127.0.0.1"))
#endif
#endif

#ifdef DAEMON_UID
int daemon_uid = DAEMON_UID;
int daemon_gid = DAEMON_GID;
#else
int daemon_uid = 1;
int daemon_gid = 1;
#endif

/*
 * We record with each registration a flag telling whether it was
 * registered with a privilege port or not.
 * If it was, it can only be unregistered with a privileged port.
 * So that we can still use standard pmap xdr routines, we store
 * this flag in a structure wrapped around a pmaplist.
 */
struct flagged_pml {
	struct pmaplist pml;
	int priv;
};

int
main(int argc, char **argv)
{
	SVCXPRT *xprt;
	int sock, c;
	struct sockaddr_in addr;
	int len = sizeof(struct sockaddr_in);
	struct pmaplist *pml;
	struct flagged_pml *fpml;
	char *chroot_path = NULL;
	struct in_addr bindaddr;
	int have_bindaddr = 0;
	int foreground = 0;
	int have_uid = 0;

	while ((c = getopt(argc, argv, "Vdflt:vi:u:g:")) != EOF) {
		switch (c) {

		case 'V':
			printf("portmap version 6.0 - 2007-May-11\n");
			exit(1);

		case 'u':
			daemon_uid = atoi(optarg);
			if (daemon_uid <= 0) {
				fprintf(stderr,
					"portmap: illegal uid: %s\n", optarg);
				exit(1);
			}
			have_uid = 1;
			break;
		case 'g':
			daemon_gid = atoi(optarg);
			if (daemon_gid <= 0) {
				fprintf(stderr,
					"portmap: illegal gid: %s\n", optarg);
				exit(1);
			}
			have_uid = 1;
			break;
		case 'd':
			debugging = 1;
		case 'f':
			foreground = 1;
			break;

		case 't':
			chroot_path = optarg;
			break;

		case 'v':
			verboselog = 1;
			break;

		case 'l':
			optarg = "127.0.0.1";
			/* FALL THROUGH */
		case 'i':
			have_bindaddr = inet_aton(optarg, &bindaddr);
			break;
		default:
			fprintf(stderr,
				"usage: %s [-dflv] [-t dir] [-i address] "
				"[-u uid] [-g gid]\n",
				argv[0]);
			fprintf(stderr, "-d: debugging mode\n");
			fprintf(stderr,
				"-f: don't daemonize, log to standard error\n");
			fprintf(stderr, "-t dir: chroot into dir\n");
			fprintf(stderr, "-v: verbose logging\n");
			fprintf(stderr, "-i address: bind to address\n");
			fprintf(stderr, "-l: same as -i 127.0.0.1\n");
			fprintf(stderr, "-u uid : setuid to this uid\n");
			fprintf(stderr, "-g uid : setgid to this gid\n");
			exit(1);
		}
	}

	if (!foreground && daemon(0, 0)) {
		(void) fprintf(stderr, "portmap: fork: %s", strerror(errno));
		exit(1);
	}

#ifdef LOG_DAEMON
	openlog("portmap", LOG_PID|LOG_NDELAY | ( foreground ? LOG_PERROR : 0),
	    FACILITY);
#else
	openlog("portmap", LOG_PID|LOG_NDELAY | ( foreground ? LOG_PERROR : 0));
#endif

#ifdef RPCUSER
	if (!have_uid) {
		struct passwd *pwent;
		pwent = getpwnam(RPCUSER);
		if (pwent) {
			daemon_uid = pwent->pw_uid;
			daemon_gid = pwent->pw_gid;
		} else
			syslog(LOG_WARNING, "user '" RPCUSER
			       "' not found, reverting to default uid");
	}
#endif

	if ((sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP)) < 0) {
		syslog(LOG_ERR, "cannot create udp socket: %m");
		exit(1);
	}
#ifdef LOOPBACK_SETUNSET
	setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &on, sizeof on);
#endif

	memset((char *) &addr, 0, sizeof(addr));
	addr.sin_addr.s_addr = 0;
	addr.sin_family = AF_INET;
	addr.sin_port = htons(PMAPPORT);
	if (have_bindaddr)
		memcpy(&addr.sin_addr, &bindaddr, sizeof(bindaddr));

	if (bind(sock, (struct sockaddr *)&addr, len) != 0) {
		syslog(LOG_ERR, "cannot bind udp: %m");
		exit(1);
	}

	if ((xprt = svcudp_create(sock)) == (SVCXPRT *)NULL) {
		syslog(LOG_ERR, "couldn't do udp_create");
		exit(1);
	}
	/* make an entry for ourself */
	fpml = malloc(sizeof(struct flagged_pml));
	pml = &fpml->pml;
	fpml->priv = 1;
	pml->pml_next = NULL;
	pml->pml_map.pm_prog = PMAPPROG;
	pml->pml_map.pm_vers = PMAPVERS;
	pml->pml_map.pm_prot = IPPROTO_UDP;
	pml->pml_map.pm_port = PMAPPORT;
	pmaplist = pml;

	if ((sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP)) < 0) {
		syslog(LOG_ERR, "cannot create tcp socket: %m");
		exit(1);
	}
#ifdef LOOPBACK_SETUNSET
	setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &on, sizeof on);
#endif
	if (bind(sock, (struct sockaddr *)&addr, len) != 0) {
		syslog(LOG_ERR, "cannot bind tcp: %m");
		exit(1);
	}
	if ((xprt = svctcp_create(sock, RPCSMALLMSGSIZE, RPCSMALLMSGSIZE))
	    == (SVCXPRT *)NULL) {
		syslog(LOG_ERR, "couldn't do tcp_create");
		exit(1);
	}
	/* make an entry for ourself */
	fpml = malloc(sizeof(struct flagged_pml));
	pml = &fpml->pml;
	fpml->priv = 1;
	pml->pml_map.pm_prog = PMAPPROG;
	pml->pml_map.pm_vers = PMAPVERS;
	pml->pml_map.pm_prot = IPPROTO_TCP;
	pml->pml_map.pm_port = PMAPPORT;
	pml->pml_next = pmaplist;
	pmaplist = pml;

#ifdef LOOPBACK_SETUNSET
	if ((sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP)) < 0) {
		syslog(LOG_ERR, "cannot create udp socket: %m");
		exit(1);
	}
	setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &on, sizeof on);

	addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
	if (bind(sock, (struct sockaddr *)&addr, len) != 0) {
		syslog(LOG_ERR, "cannot bind udp: %m");
		exit(1);
	}

	if ((ludpxprt = svcudp_create(sock)) == (SVCXPRT *)NULL) {
		syslog(LOG_ERR, "couldn't do udp_create");
		exit(1);
	}
	if ((sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP)) < 0) {
		syslog(LOG_ERR, "cannot create tcp socket: %m");
		exit(1);
	}
	setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &on, sizeof on);
	if (bind(sock, (struct sockaddr *)&addr, len) != 0) {
		syslog(LOG_ERR, "cannot bind tcp: %m");
		exit(1);
	}
	if ((ltcpxprt = svctcp_create(sock, RPCSMALLMSGSIZE, RPCSMALLMSGSIZE))
	    == (SVCXPRT *)NULL) {
		syslog(LOG_ERR, "couldn't do tcp_create");
		exit(1);
	}
#endif

	(void)svc_register(xprt, PMAPPROG, PMAPVERS, reg_service, FALSE);

	store_fd = open("/var/run/portmap_mapping", O_RDWR|O_CREAT, 0600);
	load_table();

	/* additional initializations */
	if (chroot_path) {
		if (chroot(chroot_path) < 0) {
			syslog(LOG_ERR, "couldn't do chroot");
			exit(1);
		} else {
			chdir(chroot_path);
		}
	}
	check_startup();
#ifdef IGNORE_SIGCHLD			/* Lionel Cons <cons@dxcern.cern.ch> */
	(void)signal(SIGCHLD, SIG_IGN);
#else
	(void)signal(SIGCHLD, reap);
#endif
	(void)signal(SIGPIPE, SIG_IGN);
	svc_run();
	syslog(LOG_ERR, "run_svc returned unexpectedly");
	abort();
}

/* need to override perror calls in rpc library */
void perror(const char *what)
{

	syslog(LOG_ERR, "%s: %m", what);
}

static struct pmaplist *
find_service(u_long prog, u_long vers, u_long prot)
{
	struct pmaplist *hit = NULL;
	struct pmaplist *pml;

	for (pml = pmaplist; pml != NULL; pml = pml->pml_next) {
		if ((pml->pml_map.pm_prog != prog) ||
			(pml->pml_map.pm_prot != prot))
			continue;
		hit = pml;
		if (pml->pml_map.pm_vers == vers)
		    break;
	}
	return (hit);
}

/* 
 * 1 OK, 0 not
 */
static void reg_service(struct svc_req *rqstp, SVCXPRT *xprt)
{
	struct pmap reg;
	struct pmaplist *pml, *prevpml, *fnd;
	struct flagged_pml *fpml;
	int ans, port;
	caddr_t t;
	
	/*
	 * Later wrappers change the logging severity on the fly. Reset to
	 * defaults before handling the next request.
	 */
	allow_severity = LOG_INFO;
	deny_severity = LOG_WARNING;

	if (debugging)
		(void) fprintf(stderr, "server: about do a switch\n");
	switch (rqstp->rq_proc) {

	case PMAPPROC_NULL:
		/*
		 * Null proc call
		 */
		/* remote host authorization check */
		check_default(svc_getcaller(xprt), rqstp->rq_proc, (u_long) 0);
		if (!svc_sendreply(xprt, (xdrproc_t) xdr_void, (caddr_t)0)
		    && debugging) {
			abort();
		}
		break;

	case PMAPPROC_SET:
		/*
		 * Set a program,version to port mapping
		 */
		if (!svc_getargs(xprt, (xdrproc_t) xdr_pmap, (caddr_t)&reg))
			svcerr_decode(xprt);
		else {
			/* reject non-local requests, protect priv. ports */
			if (!CHECK_SETUNSET(xprt, ludpxprt, ltcpxprt,
			    rqstp->rq_proc, reg.pm_prog, reg.pm_port)) {
				ans = 0;
				goto done;
			} 
			/*
			 * check to see if already used
			 * find_service returns a hit even if
			 * the versions don't match, so check for it
			 */
			fnd = find_service(reg.pm_prog, reg.pm_vers, reg.pm_prot);
			if (fnd && fnd->pml_map.pm_vers == reg.pm_vers) {
				if (fnd->pml_map.pm_port == reg.pm_port) {
					ans = 1;
					goto done;
				}
				else {
					ans = 0;
					goto done;
				}
			} else {
				/* 
				 * add to END of list
				 */
				fpml = (struct flagged_pml *)
				    malloc((u_int)sizeof(struct flagged_pml));
				pml = &fpml->pml;
				fpml->priv =
					(ntohs(svc_getcaller(xprt)->sin_port)
					 < IPPORT_RESERVED);
				pml->pml_map = reg;
				pml->pml_next = 0;

				if (pmaplist == 0) {
					pmaplist = pml;
				} else {
					for (fnd= pmaplist; fnd->pml_next != 0;
					    fnd = fnd->pml_next);
					fnd->pml_next = pml;
				}
				ans = 1;
				dump_table();
			}
		done:
			if ((!svc_sendreply(xprt, (xdrproc_t)xdr_int,
					    (caddr_t)&ans)) &&
			    debugging) {
				(void) fprintf(stderr, "svc_sendreply\n");
				abort();
			}
		}
		break;

	case PMAPPROC_UNSET:
		/*
		 * Remove a program,version to port mapping.
		 */
		if (!svc_getargs(xprt, (xdrproc_t)xdr_pmap, (caddr_t)&reg))
			svcerr_decode(xprt);
		else {
			ans = 0;
			/* reject non-local requests */
			if (!CHECK_SETUNSET(xprt, ludpxprt, ltcpxprt,
			    rqstp->rq_proc, reg.pm_prog, (u_long) 0))
				goto done;
			for (prevpml = NULL, pml = pmaplist; pml != NULL; ) {
				if ((pml->pml_map.pm_prog != reg.pm_prog) ||
					(pml->pml_map.pm_vers != reg.pm_vers)) {
					/* both pml & prevpml move forwards */
					prevpml = pml;
					pml = pml->pml_next;
					continue;
				}
				/* found it; pml moves forward, prevpml stays */
				/* privileged port check */
				if (!check_privileged_port(svc_getcaller(xprt), 
				    rqstp->rq_proc, 
				    reg.pm_prog, 
				    pml->pml_map.pm_port)) {
					ans = 0;
					break;
				}
				fpml = (struct flagged_pml*)pml;
				if (fpml->priv &&
				    (ntohs(svc_getcaller(xprt)->sin_port)
				     >= IPPORT_RESERVED)) {
					ans = 0;
					break;
				}

				ans = 1;
				t = (caddr_t)pml;
				pml = pml->pml_next;
				if (prevpml == NULL)
					pmaplist = pml;
				else
					prevpml->pml_next = pml;
				free(t);
				dump_table();
			}
			if ((!svc_sendreply(xprt, (xdrproc_t)xdr_int,
					    (caddr_t)&ans)) &&
			    debugging) {
				(void) fprintf(stderr, "svc_sendreply\n");
				abort();
			}
		}
		break;

	case PMAPPROC_GETPORT:
		/*
		 * Lookup the mapping for a program,version and return its port
		 */
		if (!svc_getargs(xprt, (xdrproc_t)xdr_pmap, (caddr_t)&reg))
			svcerr_decode(xprt);
		else {
			/* remote host authorization check */
			if (!check_default(svc_getcaller(xprt), 
			    rqstp->rq_proc, 
			    reg.pm_prog)) {
				ans = 0;
				goto done;
			}
			fnd = find_service(reg.pm_prog, reg.pm_vers, reg.pm_prot);
			if (fnd)
				port = fnd->pml_map.pm_port;
			else
				port = 0;
			if ((!svc_sendreply(xprt, (xdrproc_t)xdr_int,
					    (caddr_t)&port)) &&
			    debugging) {
				(void) fprintf(stderr, "svc_sendreply\n");
				abort();
			}
		}
		break;

	case PMAPPROC_DUMP:
		/*
		 * Return the current set of mapped program,version
		 */
		if (!svc_getargs(xprt, (xdrproc_t)xdr_void, NULL))
			svcerr_decode(xprt);
		else {
			/* remote host authorization check */
			struct pmaplist *p;
			if (!check_default(svc_getcaller(xprt), 
			    rqstp->rq_proc, (u_long) 0)) {
				p = 0;	/* send empty list */
			} else {
				p = pmaplist;
			}
			if ((!svc_sendreply(xprt, (xdrproc_t)xdr_pmaplist,
			    (caddr_t)&p)) && debugging) {
				(void) fprintf(stderr, "svc_sendreply\n");
				abort();
			}
		}
		break;

	case PMAPPROC_CALLIT:
		/*
		 * Calls a procedure on the local machine.  If the requested
		 * procedure is not registered this procedure does not return
		 * error information!!
		 * This procedure is only supported on rpc/udp and calls via 
		 * rpc/udp.  It passes null authentication parameters.
		 */
		callit(rqstp, xprt);
		break;

	default:
		/* remote host authorization check */
		check_default(svc_getcaller(xprt), rqstp->rq_proc, (u_long) 0);
		svcerr_noproc(xprt);
		break;
	}
}


/*
 * Stuff for the rmtcall service
 */
#define ARGSIZE 9000

struct encap_parms {
	u_int arglen;
	char *args;
};

static bool_t
xdr_encap_parms(XDR *xdrs, struct encap_parms *epp)
{

	return (xdr_bytes(xdrs, &(epp->args), &(epp->arglen), ARGSIZE));
}

struct rmtcallargs {
	u_long	rmt_prog;
	u_long	rmt_vers;
	u_long	rmt_port;
	u_long	rmt_proc;
	struct encap_parms rmt_args;
};

static bool_t
xdr_rmtcall_args(XDR *xdrs, struct rmtcallargs *cap)
{

	/* does not get a port number */
	if (xdr_u_long(xdrs, &(cap->rmt_prog)) &&
	    xdr_u_long(xdrs, &(cap->rmt_vers)) &&
	    xdr_u_long(xdrs, &(cap->rmt_proc))) {
		return (xdr_encap_parms(xdrs, &(cap->rmt_args)));
	}
	return (FALSE);
}

static bool_t
xdr_rmtcall_result(XDR *xdrs, struct rmtcallargs *cap)
{
	if (xdr_u_long(xdrs, &(cap->rmt_port)))
		return (xdr_encap_parms(xdrs, &(cap->rmt_args)));
	return (FALSE);
}

/*
 * only worries about the struct encap_parms part of struct rmtcallargs.
 * The arglen must already be set!!
 */
static bool_t
xdr_opaque_parms(XDR *xdrs, struct rmtcallargs *cap)
{

	return (xdr_opaque(xdrs, cap->rmt_args.args, cap->rmt_args.arglen));
}

/*
 * This routine finds and sets the length of incoming opaque paraters
 * and then calls xdr_opaque_parms.
 */
static bool_t
xdr_len_opaque_parms(XDR *xdrs, struct rmtcallargs *cap)
{
	u_int beginpos, lowpos, highpos, currpos, pos;

	beginpos = lowpos = pos = xdr_getpos(xdrs);
	highpos = lowpos + ARGSIZE;
	while ((int)(highpos - lowpos) >= 0) {
		currpos = (lowpos + highpos) / 2;
		if (xdr_setpos(xdrs, currpos)) {
			pos = currpos;
			lowpos = currpos + 1;
		} else {
			highpos = currpos - 1;
		}
	}
	xdr_setpos(xdrs, beginpos);
	cap->rmt_args.arglen = pos - beginpos;
	return (xdr_opaque_parms(xdrs, cap));
}

/*
 * Call a remote procedure service
 * This procedure is very quiet when things go wrong.
 * The proc is written to support broadcast rpc.  In the broadcast case,
 * a machine should shut-up instead of complain, less the requestor be
 * overrun with complaints at the expense of not hearing a valid reply ...
 *
 * This now forks so that the program & process that it calls can call 
 * back to the portmapper.
 */
static void callit(struct svc_req *rqstp, SVCXPRT *xprt)
{
	struct rmtcallargs a;
	struct pmaplist *pml;
	u_short port;
	struct sockaddr_in me;
	int pid, so = -1;
	CLIENT *client;
	struct authunix_parms *au = (struct authunix_parms *)rqstp->rq_clntcred;
	struct timeval timeout;
	char buf[ARGSIZE];

	timeout.tv_sec = 5;
	timeout.tv_usec = 0;
	a.rmt_args.args = buf;
	if (!svc_getargs(xprt, (xdrproc_t)xdr_rmtcall_args, (caddr_t)&a))
		return;
	/* host and service access control */
	if (!check_callit(svc_getcaller(xprt), 
	    rqstp->rq_proc, a.rmt_prog, a.rmt_proc))
		return;
	if ((pml = find_service(a.rmt_prog, a.rmt_vers,
	    (u_long)IPPROTO_UDP)) == NULL)
		return;
	/*
	 * fork a child to do the work.  Parent immediately returns.
	 * Child exits upon completion.
	 */
	if ((pid = fork()) != 0) {
		if (pid < 0)
			syslog(LOG_ERR, "CALLIT (prog %lu): fork: %m",
			    a.rmt_prog);
		return;
	}
	port = pml->pml_map.pm_port;
	get_myaddress(&me);
	me.sin_port = htons(port);
	client = clntudp_create(&me, a.rmt_prog, a.rmt_vers, timeout, &so);
	if (client != (CLIENT *)NULL) {
		if (rqstp->rq_cred.oa_flavor == AUTH_UNIX) {
			client->cl_auth = authunix_create(au->aup_machname,
			   au->aup_uid, au->aup_gid, au->aup_len, au->aup_gids);
		}
		a.rmt_port = (u_long)port;
		if (clnt_call(client, a.rmt_proc, (xdrproc_t)xdr_opaque_parms,
			      (caddr_t)&a, (xdrproc_t)xdr_len_opaque_parms,
			      (caddr_t)&a, timeout) == RPC_SUCCESS) {
			svc_sendreply(xprt, (xdrproc_t)xdr_rmtcall_result,
				      (caddr_t)&a);
		}
		AUTH_DESTROY(client->cl_auth);
		clnt_destroy(client);
	}
	(void)close(so);
	exit(0);
}

#ifndef IGNORE_SIGCHLD			/* Lionel Cons <cons@dxcern.cern.ch> */
static void reap(int ignore)
{
	int save_errno = errno;
	while (wait3((int *)NULL, WNOHANG, (struct rusage *)NULL) > 0);
	errno = save_errno;
}
#endif

/* Dump and restore mapping table so that we can survive kill/restart.
 * To cope with chroot, an fd is opened early and we just write to that.
 * If we are killed while writing the file, we lose, but that isn't
 * very likely...
 */

static void dump_table(void)
{
	FILE *f;
	struct pmaplist *pml;

	if (store_fd < 0)
		return;
	ftruncate(store_fd, 0);
	lseek(store_fd, 0, 0);
	f = fdopen(dup(store_fd), "w");
	if (!f)
		return;

	for (pml = pmaplist ; pml ; pml = pml->pml_next) {
		struct flagged_pml *fpml = (struct flagged_pml*)pml;

		fprintf(f, "%lu %lu %lu %lu %d\n",
			pml->pml_map.pm_prog,
			pml->pml_map.pm_vers,
			pml->pml_map.pm_prot,
			pml->pml_map.pm_port,
			fpml->priv);
	}
	fclose(f);
}

static void load_table(void)
{
	FILE *f;
	struct pmaplist **ep;
	struct flagged_pml fpml, *fpmlp;

	ep = &pmaplist;
	while ((*ep)->pml_next)
		ep = & (*ep)->pml_next;

	if (store_fd < 0)
		return;
	lseek(store_fd, 0, 0);
	f = fdopen(dup(store_fd), "r");
	if (f == NULL)
		return;

	while (fscanf(f, "%lu %lu %lu %lu %d\n",
		      &fpml.pml.pml_map.pm_prog,
		      &fpml.pml.pml_map.pm_vers,
		      &fpml.pml.pml_map.pm_prot,
		      &fpml.pml.pml_map.pm_port,
		      &fpml.priv) == 5) {
		if (fpml.pml.pml_map.pm_port == PMAPPORT)
			continue;
		fpmlp = malloc(sizeof(struct flagged_pml));
		if (!fpmlp)
			break;
		*fpmlp = fpml;
		*ep = &fpmlp->pml;
		ep = &fpmlp->pml.pml_next;
		*ep = NULL;
	}
	fclose(f);
}
