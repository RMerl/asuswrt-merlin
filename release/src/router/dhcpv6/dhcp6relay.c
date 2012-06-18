/*	$KAME: dhcp6relay.c,v 1.62 2006/01/19 04:25:16 jinmei Exp $	*/
/*
 * Copyright (C) 2000 WIDE Project.
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
 * 3. Neither the name of the project nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE PROJECT AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE PROJECT OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/queue.h>
#include <sys/uio.h>
#include <sys/signal.h>

#include <net/if.h>
#ifdef __FreeBSD__
#include <net/if_var.h>
#endif

#include <netinet/in.h>

#ifdef __KAME__
#include <netinet6/in6_var.h>
#endif

#include <netdb.h>
#include <arpa/inet.h>

#include <stdio.h>
#include <stdarg.h>
#include <syslog.h>
#include <unistd.h>
#include <stdlib.h>		/* XXX: freebsd2 needs this for opt{arg,ind} */
#include <errno.h>
#include <err.h>
#include <string.h>

#include <dhcp6.h>
#include <config.h>
#include <common.h>

#define DHCP6RELAY_PIDFILE "/var/run/dhcp6relay.pid"
static char *pid_file = DHCP6RELAY_PIDFILE;

static int ssock;		/* socket for relaying to servers */
static int csock;		/* socket for clients */
static int maxfd;		/* maxi file descriptor for select(2) */

static int debug = 0;
static sig_atomic_t sig_flags = 0;
#define SIGF_TERM 0x1

static char *relaydevice;
static char *boundaddr;
static char *serveraddr = DH6ADDR_ALLSERVER;
static char *scriptpath;

static char *rmsgctlbuf;
static socklen_t rmsgctllen;
static struct msghdr rmh;
static char rdatabuf[BUFSIZ];
static int relayifid;

static int mhops = DHCP6_RELAY_MULTICAST_HOPS;

static struct sockaddr_in6 sa6_server, sa6_client;

struct ifid_list {
	TAILQ_ENTRY(ifid_list) ilink;
	unsigned int ifid;
};
TAILQ_HEAD(, ifid_list) ifid_list;
struct prefix_list {
	TAILQ_ENTRY(prefix_list) plink;
	struct sockaddr_in6 paddr; /* contains meaningless but enough members */
	int plen;
};
TAILQ_HEAD(, prefix_list) global_prefixes; /* list of non-link-local prefixes */
static char *global_strings[] = {
	/* "fec0::/10",	site-local unicast addresses were deprecated */
	"2000::/3",
	"FC00::/7",  /* Unique Local Address (RFC4193) */
	NULL
};

static void usage __P((void));
static struct prefix_list *make_prefix __P((char *));
static void relay6_init __P((int, char *[]));
static void relay6_loop __P((void));
static void relay6_recv __P((int, int));
static void process_signals __P((void));
static void relay6_signal __P((int));
static int make_msgcontrol __P((struct msghdr *, void *, socklen_t,
    struct in6_pktinfo *, int));
static void relay_to_server __P((struct dhcp6 *, ssize_t,
    struct sockaddr_in6 *, char *, unsigned int));
static void relay_to_client __P((struct dhcp6_relay *, ssize_t,
    struct sockaddr *));
extern int relay6_script __P((char *, struct sockaddr_in6 *,
    struct dhcp6 *, int));


static void
usage()
{
	fprintf(stderr,
	    "usage: dhcp6relay [-dDf] [-b boundaddr] [-H hoplim] "
	    "[-r relay-IF] [-s serveraddr] [-p pidfile] [-S script] IF ...\n");
	exit(0);
}

int
main(argc, argv)
	int argc;
	char *argv[];
{
	int ch, pid;
	char *progname;
	char *p;
	FILE *pidfp;

	if ((progname = strrchr(*argv, '/')) == NULL)
		progname = *argv;
	else
		progname++;

	while((ch = getopt(argc, argv, "b:dDfH:r:s:S:p:")) != -1) {
		switch(ch) {
		case 'b':
			boundaddr = optarg;
			break;
		case 'd':
			debug = 1;
			break;
		case 'D':
			debug = 2;
			break;
		case 'f':
			foreground++;
			break;
		case 'H':
			p = NULL;
			mhops = (int)strtoul(optarg, &p, 10);
			if (!*optarg || *p) {
				errx(1, "illegal hop limit: %s", optarg);
				/* NOTREACHED */
			}
			if (mhops <= 0 || mhops > 255) {
				errx(1, "illegal hop limit: %d", mhops);
				/* NOTREACHED */
			}
			break;
		case 'r':
			relaydevice = optarg;
			break;
		case 's':
			serveraddr = optarg;
			break;
		case 'S':
			scriptpath = optarg;
			break;
		case 'p':
			pid_file = optarg;
			break;
		default:
			usage();
			exit(0);
		}
	}
	argc -= optind;
	argv += optind;

	if (argc < 1) {
		usage();
		/* NOTREACHED */
	}
	if (relaydevice == NULL) {
		if (argc != 1) {
			fprintf(stderr, "you should explicitly specify a "
			    "relaying interface, when you are to "
			    "listen on multiple interfaces");
			exit(0);
		}
		relaydevice = argv[0];
	}

	if (foreground == 0) {
		int fd;

		if (daemon(0, 0) < 0)
			err(1, "daemon");

		for (fd = 3; fd < 1024; fd++)
			close(fd);

		openlog(progname, LOG_NDELAY|LOG_PID, LOG_DAEMON);
	}
	setloglevel(debug);

	/* dump current PID */
	pid = getpid();
	if ((pidfp = fopen(pid_file, "w")) != NULL) {
		fprintf(pidfp, "%d\n", pid);
		fclose(pidfp);
	}

	relay6_init(argc, argv);

	dprintf(LOG_INFO, FNAME, "dhcp6relay started");
	relay6_loop();

	exit(0);
}

static struct prefix_list *
make_prefix(pstr0)
	char *pstr0;
{
	struct prefix_list *pent;
	char *p, *ep;
	int plen;
	char pstr[BUFSIZ];
	struct in6_addr paddr;

	/* make a local copy for safety */
	if (strlcpy(pstr, pstr0, sizeof (pstr)) >= sizeof (pstr)) {
		dprintf(LOG_WARNING, FNAME,
		    "prefix string too long (maybe bogus): %s", pstr0);
		return (NULL);
	}

	/* parse the string */
	if ((p = strchr(pstr, '/')) == NULL)
		plen = 128; /* assumes it as a host prefix */
	else {
		if (p[1] == '\0') {
			dprintf(LOG_WARNING, FNAME,
			    "no prefix length (ignored): %s", p + 1);
			return (NULL);
		}
		plen = (int)strtoul(p + 1, &ep, 10);
		if (*ep != '\0') {
			dprintf(LOG_WARNING, FNAME,
			    "illegal prefix length (ignored): %s", p + 1);
			return (NULL);
		}
		*p = '\0';
	}
	if (inet_pton(AF_INET6, pstr, &paddr) != 1) {
		dprintf(LOG_ERR, FNAME,
		    "inet_pton failed for %s", pstr);
		return (NULL);
	}

	/* allocate a new entry */
	if ((pent = (struct prefix_list *)malloc(sizeof (*pent))) == NULL) {
		dprintf(LOG_WARNING, FNAME, "memory allocation failed");
		return (NULL);	/* or abort? */
	}

	/* fill in each member of the entry */
	memset(pent, 0, sizeof (*pent));
	pent->paddr.sin6_family = AF_INET6;
#ifdef HAVE_SA_LEN
	pent->paddr.sin6_len = sizeof (struct sockaddr_in6);
#endif
	pent->paddr.sin6_addr = paddr;
	pent->plen = plen;

	return (pent);
}

static void
relay6_init(int ifnum, char *iflist[])
{
	struct addrinfo hints;
	struct addrinfo *res, *res2;
	int i, error, on;
	struct ipv6_mreq mreq6;
	static struct iovec iov[2];

	/* initialize non-link-local prefixes list */
	TAILQ_INIT(&global_prefixes);
	for (i = 0; global_strings[i]; i++) {
		struct prefix_list *p;

		if ((p = make_prefix(global_strings[i])) != NULL)
			TAILQ_INSERT_TAIL(&global_prefixes, p, plink);
	}

	/* initialize special socket addresses */
	memset(&hints, 0, sizeof (hints));
	hints.ai_family = PF_INET6;
	hints.ai_socktype = SOCK_DGRAM;
	hints.ai_protocol = IPPROTO_UDP;
	hints.ai_flags = AI_PASSIVE;
	error = getaddrinfo(serveraddr, DH6PORT_UPSTREAM, &hints, &res);
	if (error) {
		dprintf(LOG_ERR, FNAME, "getaddrinfo: %s",
		    gai_strerror(error));
		goto failexit;
	}
	if (res->ai_family != PF_INET6 ||
	    res->ai_addrlen < sizeof (sa6_server)) {
		/* this should be impossible, but check for safety */
		dprintf(LOG_ERR, FNAME,
		    "getaddrinfo returned a bogus address: %s",
		    strerror(errno));
		goto failexit;
	}
	/* XXX: assume only one DHCPv6 server address */
	memcpy(&sa6_server, res->ai_addr, sizeof (sa6_server));
	freeaddrinfo(res);

	/* initialize send/receive buffer */
	iov[0].iov_base = (caddr_t)rdatabuf;
	iov[0].iov_len = sizeof (rdatabuf);
	rmh.msg_iov = iov;
	rmh.msg_iovlen = 1;
	rmsgctllen = CMSG_SPACE(sizeof (struct in6_pktinfo));
	if ((rmsgctlbuf = (char *)malloc(rmsgctllen)) == NULL) {
		dprintf(LOG_ERR, FNAME, "memory allocation failed");
		goto failexit;
	}

	/*
	 * Setup a socket to communicate with clients.
	 */
	memset(&hints, 0, sizeof (hints));
	hints.ai_family = PF_INET6;
	hints.ai_socktype = SOCK_DGRAM;
	hints.ai_protocol = IPPROTO_UDP;
	hints.ai_flags = AI_PASSIVE;
	error = getaddrinfo(NULL, DH6PORT_UPSTREAM, &hints, &res);
	if (error) {
		dprintf(LOG_ERR, FNAME, "getaddrinfo: %s",
		    gai_strerror(error));
		goto failexit;
	}
	csock = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
	if (csock < 0) {
		dprintf(LOG_ERR, FNAME, "socket(csock): %s", strerror(errno));
		goto failexit;
	}
	if (csock > maxfd)
		maxfd = csock;
	on = 1;
	if (setsockopt(csock, SOL_SOCKET, SO_REUSEPORT,
	    &on, sizeof(on)) < 0) {
		dprintf(LOG_ERR, FNAME, "setsockopt(csock, SO_REUSEPORT): %s",
		    strerror(errno));
		goto failexit;
	}
#ifdef IPV6_V6ONLY
	if (setsockopt(csock, IPPROTO_IPV6, IPV6_V6ONLY,
	    &on, sizeof (on)) < 0) {
		dprintf(LOG_ERR, FNAME, "setsockopt(csock, IPV6_V6ONLY): %s",
		    strerror(errno));
		goto failexit;
	}
#endif
	if (bind(csock, res->ai_addr, res->ai_addrlen) < 0) {
		dprintf(LOG_ERR, FNAME, "bind(csock): %s", strerror(errno));
		goto failexit;
	}
	freeaddrinfo(res);
	on = 1;
#ifdef IPV6_RECVPKTINFO
	if (setsockopt(csock, IPPROTO_IPV6, IPV6_RECVPKTINFO,
	    &on, sizeof (on)) < 0) {
		dprintf(LOG_ERR, FNAME, "setsockopt(IPV6_RECVPKTINFO): %s",
		    strerror(errno));
		goto failexit;
	}
#else
	if (setsockopt(csock, IPPROTO_IPV6, IPV6_PKTINFO,
	    &on, sizeof (on)) < 0) {
		dprintf(LOG_ERR, FNAME, "setsockopt(IPV6_PKTINFO): %s",
		    strerror(errno));
		goto failexit;
	}
#endif

	hints.ai_flags = 0;
	error = getaddrinfo(DH6ADDR_ALLAGENT, 0, &hints, &res2);
	if (error) {
		dprintf(LOG_ERR, FNAME, "getaddrinfo: %s",
		    gai_strerror(error));
		goto failexit;
	}
	memset(&mreq6, 0, sizeof (mreq6));
	memcpy(&mreq6.ipv6mr_multiaddr,
	    &((struct sockaddr_in6 *)res2->ai_addr)->sin6_addr,
	    sizeof (mreq6.ipv6mr_multiaddr));

	TAILQ_INIT(&ifid_list);
	while (ifnum-- > 0) {
		char *ifp = iflist[0];
		struct ifid_list *ifd;

		ifd = (struct ifid_list *)malloc(sizeof (*ifd));
		if (ifd == NULL) {
			dprintf(LOG_WARNING, FNAME,
			    "memory allocation failed");
			goto failexit;
		}
		memset(ifd, 0, sizeof (*ifd));
		ifd->ifid = if_nametoindex(ifp);
		if (ifd->ifid == 0) {
			dprintf(LOG_ERR, FNAME, "invalid interface %s", ifp);
			goto failexit;
		}
		mreq6.ipv6mr_interface = ifd->ifid;

		if (setsockopt(csock, IPPROTO_IPV6, IPV6_JOIN_GROUP,
		    &mreq6, sizeof (mreq6))) {
			dprintf(LOG_ERR, FNAME,
			    "setsockopt(csock, IPV6_JOIN_GROUP): %s",
			     strerror(errno));
			goto failexit;
		}
		TAILQ_INSERT_TAIL(&ifid_list, ifd, ilink);
		iflist++;
	}
	freeaddrinfo(res2);

	/*
	 * Setup a socket to relay to servers.
	 */
	relayifid = if_nametoindex(relaydevice);
	if (relayifid == 0)
		dprintf(LOG_ERR, FNAME, "invalid interface %s", relaydevice);
	/*
	 * We are not really sure if we need to listen on the downstream
	 * port to receive packets from servers.  We'll need to clarify the
	 * specification, but we do for now.
	 */
	hints.ai_flags = AI_PASSIVE;
	error = getaddrinfo(boundaddr, DH6PORT_DOWNSTREAM, &hints, &res);
	if (error) {
		dprintf(LOG_ERR, FNAME, "getaddrinfo: %s",
		    gai_strerror(error));
		goto failexit;
	}
	memcpy(&sa6_client, res->ai_addr, sizeof (sa6_client));
	ssock = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
	if (ssock < 0) {
		dprintf(LOG_ERR, FNAME, "socket(outsock): %s",
		    strerror(error));
		goto failexit;
	}
	if (ssock > maxfd)
		maxfd = ssock;
	on = 1;
	/*
	 * Both a relay and a client may run on a single node.  If we need to
	 * listen on the downstream port, we need REUSEPORT to avoid conflict.
	 */
	if (setsockopt(ssock, SOL_SOCKET, SO_REUSEPORT,
	    &on, sizeof (on)) < 0) {
		dprintf(LOG_ERR, FNAME, "setsockopt(ssock, SO_REUSEPORT): %s",
		    strerror(errno));
		goto failexit;
	}
	on = 1;
#ifdef IPV6_V6ONLY
	if (setsockopt(ssock, IPPROTO_IPV6, IPV6_V6ONLY,
	    &on, sizeof (on)) < 0) {
		dprintf(LOG_ERR, FNAME, "setsockopt(ssock, IPV6_V6ONLY): %s",
		    strerror(errno));
		goto failexit;
	}
#endif
	if (bind(ssock, res->ai_addr, res->ai_addrlen) < 0) {
		dprintf(LOG_ERR, FNAME, "bind(ssock): %s", strerror(errno));
		goto failexit;
	}
	freeaddrinfo(res);

	on = 1;
#ifdef IPV6_RECVPKTINFO
	if (setsockopt(ssock, IPPROTO_IPV6, IPV6_RECVPKTINFO,
	    &on, sizeof (on)) < 0) {
		dprintf(LOG_ERR, FNAME, "setsockopt(IPV6_RECVPKTINFO): %s",
		    strerror(errno));
		goto failexit;
	}
#else
	if (setsockopt(ssock, IPPROTO_IPV6, IPV6_PKTINFO,
	    &on, sizeof (on)) < 0) {
		dprintf(LOG_ERR, FNAME, "setsockopt(IPV6_PKTINFO): %s",
		    strerror(errno));
		goto failexit;
	}
#endif

	if (signal(SIGTERM, relay6_signal) == SIG_ERR) {
		dprintf(LOG_WARNING, FNAME, "failed to set signal: %s",
		    strerror(errno));
		exit(1);
	}
	return;

  failexit:
	exit(1);
}

static void
relay6_signal(sig)
	int sig;
{

	switch (sig) {
	case SIGTERM:
		sig_flags |= SIGF_TERM;
		break;
	}
}

static void
process_signals()
{
	if ((sig_flags & SIGF_TERM)) {
		unlink(pid_file);
		exit(0);
	}
}

static void
relay6_loop()
{
	fd_set readfds;
	int e;

	while(1) {
		if (sig_flags)
			process_signals();

		/* we'd rather use FD_COPY here, but it's not POSIX friendly */
		FD_ZERO(&readfds);
		FD_SET(csock, &readfds);
		FD_SET(ssock, &readfds);

		e = select(maxfd + 1, &readfds, NULL, NULL, NULL);
		switch(e) {
		case 0:		/* impossible in our situation */
			errx(1, "select returned 0");
				/* NOTREACHED */
		case -1:
			if (errno != EINTR) {
				err(1, "select");
				/* NOTREACHED */
			}
			continue;
		default:
			break;
		}

		if (FD_ISSET(csock, &readfds))
			relay6_recv(csock, 1);

		if (FD_ISSET(ssock, &readfds))
			relay6_recv(ssock, 0);
	}
}

static void
relay6_recv(s, fromclient)
	int s, fromclient;
{
	ssize_t len;
	struct sockaddr_storage from;
	struct in6_pktinfo *pi = NULL;
	struct cmsghdr *cm;
	struct dhcp6 *dh6;
	struct ifid_list *ifd;
	char ifname[IF_NAMESIZE];

	rmh.msg_control = (caddr_t)rmsgctlbuf;
	rmh.msg_controllen = rmsgctllen;

	rmh.msg_name = &from;
	rmh.msg_namelen = sizeof (from);

	if ((len = recvmsg(s, &rmh, 0)) < 0) {
		dprintf(LOG_WARNING, FNAME, "recvmsg: %s", strerror(errno));
		return;
	}

	dprintf(LOG_DEBUG, FNAME, "from %s, size %d",
	    addr2str((struct sockaddr *)&from), len);

	if (((struct sockaddr *)&from)->sa_family != AF_INET6) {
		dprintf(LOG_WARNING, FNAME,
		    "non-IPv6 packet is received (AF %d) ",
		    ((struct sockaddr *)&from)->sa_family);
		return;
	}

	/* get optional information as ancillary data (if available) */
	for (cm = (struct cmsghdr *)CMSG_FIRSTHDR(&rmh); cm;
	     cm = (struct cmsghdr *)CMSG_NXTHDR(&rmh, cm)) {
		if (cm->cmsg_level != IPPROTO_IPV6)
			continue;

		switch(cm->cmsg_type) {
		case IPV6_PKTINFO:
			pi = (struct in6_pktinfo *)CMSG_DATA(cm);
			break;
		}
	}
	if (pi == NULL) {
		dprintf(LOG_WARNING, FNAME,
		    "failed to get the arrival interface");
		return;
	}
	for (ifd = TAILQ_FIRST(&ifid_list); ifd;
	     ifd = TAILQ_NEXT(ifd, ilink)) {
		if (pi->ipi6_ifindex == ifd->ifid)
			break;
	}
	/*
	 * DHCPv6 relay may receive a DHCPv6 packet from a non-listening 
	 * interface, when a DHCPv6 server is running on that interface.
	 * This check prevents such reception.
	 */
	if (ifd == NULL && pi->ipi6_ifindex != relayifid)
		return;
	if (if_indextoname(pi->ipi6_ifindex, ifname) == NULL) {
		dprintf(LOG_WARNING, FNAME,
		    "if_indextoname(id = %d): %s",
		    pi->ipi6_ifindex, strerror(errno));
		return;
	}

	/* packet validation */
	if (len < sizeof (*dh6)) {
		dprintf(LOG_INFO, FNAME, "short packet (%d bytes)", len);
		return;
	}

	dh6 = (struct dhcp6 *)rdatabuf;
	dprintf(LOG_DEBUG, FNAME, "received %s from %s",
	    dhcp6msgstr(dh6->dh6_msgtype), addr2str((struct sockaddr *)&from));

	/*
	 * Relay the packet according to the type.  A client message or
	 * a relay forward message is forwarded to servers (or other relays),
	 * and a relay reply message is forwarded to the intended client.
	 */
	if (fromclient) {
		switch (dh6->dh6_msgtype) {
		case DH6_SOLICIT:
		case DH6_REQUEST:
		case DH6_CONFIRM:
		case DH6_RENEW:
		case DH6_REBIND:
		case DH6_RELEASE:
		case DH6_DECLINE:
		case DH6_INFORM_REQ:
		case DH6_RELAY_FORW:
			relay_to_server(dh6, len, (struct sockaddr_in6 *)&from,
			    ifname, htonl(pi->ipi6_ifindex));
			break;
		case DH6_RELAY_REPLY:
			/*
			 * The server may send a relay reply to the client
			 * port.
			 * XXX: need to clarify the port issue
			 */
			relay_to_client((struct dhcp6_relay *)dh6, len,
			    (struct sockaddr *)&from);
			break;
		default:
			dprintf(LOG_INFO, FNAME,
			    "unexpected message (%s) on the client side "
			    "from %s", dhcp6msgstr(dh6->dh6_msgtype),
			    addr2str((struct sockaddr *)&from));
			break;
		}
	} else {
		if (dh6->dh6_msgtype != DH6_RELAY_REPLY) {
			dprintf(LOG_INFO, FNAME,
			    "unexpected message (%s) on the server side"
			    "from %s", dhcp6msgstr(dh6->dh6_msgtype),
			    addr2str((struct sockaddr *)&from));
			return;
		}
		relay_to_client((struct dhcp6_relay *)dh6, len,
		    (struct sockaddr *)&from);
	}
}

static int
make_msgcontrol(mh, ctlbuf, buflen, pktinfo, hlim)
	struct msghdr *mh;
	void *ctlbuf;
	socklen_t buflen;
	struct in6_pktinfo *pktinfo;
	int hlim;
{
	struct cmsghdr *cm;
	socklen_t controllen;

	controllen = 0;
	if (pktinfo)
		controllen += CMSG_SPACE(sizeof (*pktinfo));
	if (hlim > 0)
		controllen += CMSG_SPACE(sizeof (hlim));
	if (buflen < controllen)
		return (-1);

	memset(ctlbuf, 0, buflen);
	mh->msg_controllen = controllen;
	mh->msg_control = ctlbuf;

	cm = (struct cmsghdr *)CMSG_FIRSTHDR(mh);
	if (pktinfo) {
		cm->cmsg_len = CMSG_LEN(sizeof (*pktinfo));
		cm->cmsg_level = IPPROTO_IPV6;
		cm->cmsg_type = IPV6_PKTINFO;
		memcpy(CMSG_DATA((struct cmsghdr *)cm), pktinfo,
		    sizeof (*pktinfo));

		cm = CMSG_NXTHDR(mh, cm);
	}

	if (hlim > 0) {
		cm->cmsg_len = CMSG_LEN(sizeof (hlim));
		cm->cmsg_level = IPPROTO_IPV6;
		cm->cmsg_type = IPV6_HOPLIMIT;
		*(int *)CMSG_DATA((struct cmsghdr *)cm) = hlim;

		cm = CMSG_NXTHDR(mh, cm); /* just in case */
	}

	return (0);
}

static void
relay_to_server(dh6, len, from, ifname, ifid)
	struct dhcp6 *dh6;
	ssize_t len;
	struct sockaddr_in6 *from;
	char *ifname;
	unsigned int ifid;
{
	struct dhcp6_optinfo optinfo;
	struct dhcp6_relay *dh6relay;
	struct in6_addr linkaddr;
	struct prefix_list *p;
	int optlen, relaylen;
	int cc;
	struct msghdr mh;
	static struct iovec iov[2];
	u_char relaybuf[sizeof (*dh6relay) + BUFSIZ];
	struct in6_pktinfo pktinfo;
	char ctlbuf[CMSG_SPACE(sizeof (struct in6_pktinfo))
	    + CMSG_SPACE(sizeof (int))];

	/*
	 * Prepare a relay forward option.
	 */
	dhcp6_init_options(&optinfo);

	/* Relay message */
	if ((optinfo.relaymsg_msg = malloc(len)) == NULL) {
		dprintf(LOG_WARNING, FNAME,
		    "failed to allocate memory to copy the original packet: "
		    "%s", strerror(errno));
		goto out;
	}
	optinfo.relaymsg_len = len;
	memcpy(optinfo.relaymsg_msg, dh6, len);

	/* Interface-id.  We always use this option. */
	if ((optinfo.ifidopt_id = malloc(sizeof (ifid))) == NULL) {
		dprintf(LOG_WARNING, FNAME,
		    "failed to allocate memory for IFID: %s", strerror(errno));
		goto out;
	}
	optinfo.ifidopt_len = sizeof (ifid);
	memcpy(optinfo.ifidopt_id, &ifid, sizeof (ifid));

	/*
	 * Construct a relay forward message.
	 */
	memset(relaybuf, 0, sizeof (relaybuf));

	dh6relay = (struct dhcp6_relay *)relaybuf;
	memset(dh6relay, 0, sizeof (*dh6relay));
	dh6relay->dh6relay_msgtype = DH6_RELAY_FORW;
	memcpy(&dh6relay->dh6relay_peeraddr, &from->sin6_addr,
	    sizeof (dh6relay->dh6relay_peeraddr));

	/* find a global address to fill in the link address field */
	memset(&linkaddr, 0, sizeof (linkaddr));
	for (p = TAILQ_FIRST(&global_prefixes); p; p = TAILQ_NEXT(p, plink)) {
		if (getifaddr(&linkaddr, ifname, &p->paddr.sin6_addr,
			      p->plen, 1, IN6_IFF_INVALID) == 0) /* found */
			break;
	}
	if (p == NULL) {
		dprintf(LOG_NOTICE, FNAME,
		    "failed to find a global address on %s", ifname);

		/*
		 * When relaying a message from a client, we need a global
		 * link address.
		 * XXX: this may be too strong for the stateless case, but
		 * the DHCPv6 specification seems to require the behavior. 
		 */
		if (dh6->dh6_msgtype != DH6_RELAY_FORW)
			goto out;
	}

	if (dh6->dh6_msgtype == DH6_RELAY_FORW) {
		struct dhcp6_relay *dh6relay0 = (struct dhcp6_relay *)dh6;

		/* Relaying a Message from a Relay Agent */

		/*
		 * If the hop-count in the message is greater than or equal to
		 * HOP_COUNT_LIMIT, the relay agent discards the received
		 * message.
		 * [RFC3315 Section 20.1.2]
		 */
		if (dh6relay0->dh6relay_hcnt >= DHCP6_RELAY_HOP_COUNT_LIMIT) {
			dprintf(LOG_INFO, FNAME, "too many relay forwardings");
			goto out;
		}

		dh6relay->dh6relay_hcnt = dh6relay0->dh6relay_hcnt + 1;

		/*
		 * We can keep the link-address field 0, regardless of the
		 * scope of the source address, since we always include
		 * interface-ID option.
		 */
	} else {
		/* Relaying a Message from a Client */
		memcpy(&dh6relay->dh6relay_linkaddr, &linkaddr,
		    sizeof (dh6relay->dh6relay_linkaddr));
		dh6relay->dh6relay_hcnt = 0;
	}

	relaylen = sizeof (*dh6relay);
	if ((optlen = dhcp6_set_options(DH6_RELAY_FORW,
	    (struct dhcp6opt *)(dh6relay + 1),
	    (struct dhcp6opt *)(relaybuf + sizeof (relaybuf)),
	    &optinfo)) < 0) {
		dprintf(LOG_INFO, FNAME,
		    "failed to construct relay options");
		goto out;
	}
	relaylen += optlen;

	/*
	 * Forward the message.
	 */
	memset(&mh, 0, sizeof (mh));
	iov[0].iov_base = relaybuf;
	iov[0].iov_len = relaylen;
	mh.msg_iov = iov;
	mh.msg_iovlen = 1;
	mh.msg_name = &sa6_server;
	mh.msg_namelen = sizeof (sa6_server);
	if (IN6_IS_ADDR_MULTICAST(&sa6_server.sin6_addr)) {
		memset(&pktinfo, 0, sizeof (pktinfo));
		pktinfo.ipi6_ifindex = relayifid;
		if (make_msgcontrol(&mh, ctlbuf, sizeof (ctlbuf),
		    &pktinfo, mhops)) {
			dprintf(LOG_WARNING, FNAME,
			    "failed to make message control data");
			goto out;
		}
	}

	if ((cc = sendmsg(ssock, &mh, 0)) < 0) {
		dprintf(LOG_WARNING, FNAME,
		    "sendmsg %s failed: %s",
		    addr2str((struct sockaddr *)&sa6_server), strerror(errno));
	} else if (cc != relaylen) {
		dprintf(LOG_WARNING, FNAME,
		    "failed to send a complete packet to %s",
		    addr2str((struct sockaddr *)&sa6_server));
	} else {
		dprintf(LOG_DEBUG, FNAME,
		    "relay a message to a server %s",
		    addr2str((struct sockaddr *)&sa6_server));
	}

  out:
	dhcp6_clear_options(&optinfo);
}

static void
relay_to_client(dh6relay, len, from)
	struct dhcp6_relay *dh6relay;
	ssize_t len;
	struct sockaddr *from;
{
	struct dhcp6_optinfo optinfo;
	struct sockaddr_in6 peer;
	unsigned int ifid;
	char ifnamebuf[IFNAMSIZ];
	int cc;
	int relayed = 0;
	struct dhcp6 *dh6;
	struct msghdr mh;
	struct in6_pktinfo pktinfo;
	static struct iovec iov[2];
	char ctlbuf[CMSG_SPACE(sizeof (struct in6_pktinfo))];

	dprintf(LOG_DEBUG, FNAME,
	    "dhcp6 relay reply: hop=%d, linkaddr=%s, peeraddr=%s",
	    dh6relay->dh6relay_hcnt,
	    in6addr2str(&dh6relay->dh6relay_linkaddr, 0),
	    in6addr2str(&dh6relay->dh6relay_peeraddr, 0));

	/*
	 * parse and validate options in the relay reply message.
	 */
	dhcp6_init_options(&optinfo);
	if (dhcp6_get_options((struct dhcp6opt *)(dh6relay + 1),
	    (struct dhcp6opt *)((char *)dh6relay + len), &optinfo) < 0) {
		dprintf(LOG_INFO, FNAME, "failed to parse options");
		return;
	}

	/* A relay reply message must include a relay message option */
	if (optinfo.relaymsg_msg == NULL) {
		dprintf(LOG_INFO, FNAME, "relay reply message from %s "
		    "without a relay message", addr2str(from));
		goto out;
	}

	/* minimum validation for the inner message */
	if (optinfo.relaymsg_len < sizeof (struct dhcp6)) {
		dprintf(LOG_INFO, FNAME, "short relay message from %s",
		    addr2str(from));
		goto out;
	}

	/*
	 * Extract interface ID which should be included in relay reply
	 * messages to us.
	 */
	ifid = 0;
	if (optinfo.ifidopt_id) {
		if (optinfo.ifidopt_len != sizeof (ifid)) {
			dprintf(LOG_INFO, FNAME,
			    "unexpected length (%d) for Interface ID from %s",
			    optinfo.ifidopt_len, addr2str(from));
			goto out;
		} else {
			memcpy(&ifid, optinfo.ifidopt_id, sizeof (ifid));
			ifid = ntohl(ifid);

			/* validation for ID */
			if ((if_indextoname(ifid, ifnamebuf)) == NULL) {
				dprintf(LOG_INFO, FNAME,
				    "invalid interface ID: %x", ifid);
				goto out;
			}
		}
	} else {
		dprintf(LOG_INFO, FNAME,
		    "Interface ID is not included from %s", addr2str(from));
		/*
		 * the responding server should be buggy, but we deal with it.
		 */
	}

	/*
	 * If we fail, try to get the interface from the link address.
	 */
	if (ifid == 0 &&
	    !IN6_IS_ADDR_UNSPECIFIED(&dh6relay->dh6relay_linkaddr) &&
	    !IN6_IS_ADDR_LINKLOCAL(&dh6relay->dh6relay_linkaddr)) {
		if (getifidfromaddr(&dh6relay->dh6relay_linkaddr, &ifid))
			ifid = 0;
	}

	if (ifid == 0) {
		dprintf(LOG_INFO, FNAME, "failed to determine relay link");
		goto out;
	}

	peer = sa6_client;
	dh6 = (struct dhcp6 *) optinfo.relaymsg_msg;
	if (dh6->dh6_msgtype != DH6_RELAY_REPLY) {
		relayed++;
	} else {
		/* 
		 * change dst port to server/relay port, since it's a
		 * reply to relay, not to a client
		 */
		peer.sin6_port = htons(547);	/* DH6PORT_UPSTREAM */
	}
	memcpy(&peer.sin6_addr, &dh6relay->dh6relay_peeraddr,
	    sizeof (peer.sin6_addr));
	if (IN6_IS_ADDR_LINKLOCAL(&peer.sin6_addr))
		peer.sin6_scope_id = ifid; /* XXX: we assume a 1to1 map */

	/* construct a message structure specifying the outgoing interface */
	memset(&mh, 0, sizeof (mh));
	iov[0].iov_base = optinfo.relaymsg_msg;
	iov[0].iov_len = optinfo.relaymsg_len;
	mh.msg_iov = iov;
	mh.msg_iovlen = 1;
	mh.msg_name = &peer;
	mh.msg_namelen = sizeof (peer);
	memset(&pktinfo, 0, sizeof (pktinfo));
	pktinfo.ipi6_ifindex = ifid;
	if (make_msgcontrol(&mh, ctlbuf, sizeof (ctlbuf), &pktinfo, 0)) {
		dprintf(LOG_WARNING, FNAME,
		    "failed to make message control data");
		goto out;
	}

	/* send packet */
	if ((cc = sendmsg(csock, &mh, 0)) < 0) {
		dprintf(LOG_WARNING, FNAME,
		    "sendmsg to %s failed: %s",
		    addr2str((struct sockaddr *)&peer), strerror(errno));
	} else if (cc != optinfo.relaymsg_len) {
		dprintf(LOG_WARNING, FNAME,
		    "failed to send a complete packet to %s",
		    addr2str((struct sockaddr *)&peer));
	} else {
		dprintf(LOG_DEBUG, FNAME,
		    "relay a message to a client %s",
		    addr2str((struct sockaddr *)&peer));
	}

	if (relayed && scriptpath != NULL)
		relay6_script(scriptpath, &peer, dh6, optinfo.relaymsg_len);

  out:
	dhcp6_clear_options(&optinfo);
	return;
}
