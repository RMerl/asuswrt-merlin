/*
 *      Modified for NRL 4.4BSD IPv6 release.
 *      07/31/96 bgp
 *
 *      Search for "#ifdef NRL" to find the changes.
 */

/*
 *	Modified for Linux IPv6 by Pedro Roque <roque@di.fc.ul.pt>
 *	31/07/1996
 *
 *	As ICMP error messages for IPv6 now include more than 8 bytes
 *	UDP datagrams are now sent via an UDP socket instead of magic
 *	RAW socket tricks.
 *
 *	Original copyright and comments left intact. They might not
 *	match the code anymore.
 */

/*-
 * Copyright (c) 1990, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Van Jacobson.
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
"@(#) Copyright (c) 1990, 1993\n\
	The Regents of the University of California.  All rights reserved.\n";
#endif /* not lint */

/*
 * traceroute host  - trace the route ip packets follow going to "host".
 *
 * Attempt to trace the route an ip packet would follow to some
 * internet host.  We find out intermediate hops by launching probe
 * packets with a small ttl (time to live) then listening for an
 * icmp "time exceeded" reply from a gateway.  We start our probes
 * with a ttl of one and increase by one until we get an icmp "port
 * unreachable" (which means we got to "host") or hit a max (which
 * defaults to 30 hops & can be changed with the -m flag).  Three
 * probes (change with -q flag) are sent at each ttl setting and a
 * line is printed showing the ttl, address of the gateway and
 * round trip time of each probe.  If the probe answers come from
 * different gateways, the address of each responding system will
 * be printed.  If there is no response within a 5 sec. timeout
 * interval (changed with the -w flag), a "*" is printed for that
 * probe.
 *
 * Probe packets are UDP format.  We don't want the destination
 * host to process them so the destination port is set to an
 * unlikely value (if some clod on the destination is using that
 * value, it can be changed with the -p flag).
 *
 * A sample use might be:
 *
 *     [yak 71]% traceroute nis.nsf.net.
 *     traceroute to nis.nsf.net (35.1.1.48), 30 hops max, 56 byte packet
 *      1  helios.ee.lbl.gov (128.3.112.1)  19 ms  19 ms  0 ms
 *      2  lilac-dmc.Berkeley.EDU (128.32.216.1)  39 ms  39 ms  19 ms
 *      3  lilac-dmc.Berkeley.EDU (128.32.216.1)  39 ms  39 ms  19 ms
 *      4  ccngw-ner-cc.Berkeley.EDU (128.32.136.23)  39 ms  40 ms  39 ms
 *      5  ccn-nerif22.Berkeley.EDU (128.32.168.22)  39 ms  39 ms  39 ms
 *      6  128.32.197.4 (128.32.197.4)  40 ms  59 ms  59 ms
 *      7  131.119.2.5 (131.119.2.5)  59 ms  59 ms  59 ms
 *      8  129.140.70.13 (129.140.70.13)  99 ms  99 ms  80 ms
 *      9  129.140.71.6 (129.140.71.6)  139 ms  239 ms  319 ms
 *     10  129.140.81.7 (129.140.81.7)  220 ms  199 ms  199 ms
 *     11  nic.merit.edu (35.1.1.48)  239 ms  239 ms  239 ms
 *
 * Note that lines 2 & 3 are the same.  This is due to a buggy
 * kernel on the 2nd hop system -- lbl-csam.arpa -- that forwards
 * packets with a zero ttl.
 *
 * A more interesting example is:
 *
 *     [yak 72]% traceroute allspice.lcs.mit.edu.
 *     traceroute to allspice.lcs.mit.edu (18.26.0.115), 30 hops max
 *      1  helios.ee.lbl.gov (128.3.112.1)  0 ms  0 ms  0 ms
 *      2  lilac-dmc.Berkeley.EDU (128.32.216.1)  19 ms  19 ms  19 ms
 *      3  lilac-dmc.Berkeley.EDU (128.32.216.1)  39 ms  19 ms  19 ms
 *      4  ccngw-ner-cc.Berkeley.EDU (128.32.136.23)  19 ms  39 ms  39 ms
 *      5  ccn-nerif22.Berkeley.EDU (128.32.168.22)  20 ms  39 ms  39 ms
 *      6  128.32.197.4 (128.32.197.4)  59 ms  119 ms  39 ms
 *      7  131.119.2.5 (131.119.2.5)  59 ms  59 ms  39 ms
 *      8  129.140.70.13 (129.140.70.13)  80 ms  79 ms  99 ms
 *      9  129.140.71.6 (129.140.71.6)  139 ms  139 ms  159 ms
 *     10  129.140.81.7 (129.140.81.7)  199 ms  180 ms  300 ms
 *     11  129.140.72.17 (129.140.72.17)  300 ms  239 ms  239 ms
 *     12  * * *
 *     13  128.121.54.72 (128.121.54.72)  259 ms  499 ms  279 ms
 *     14  * * *
 *     15  * * *
 *     16  * * *
 *     17  * * *
 *     18  ALLSPICE.LCS.MIT.EDU (18.26.0.115)  339 ms  279 ms  279 ms
 *
 * (I start to see why I'm having so much trouble with mail to
 * MIT.)  Note that the gateways 12, 14, 15, 16 & 17 hops away
 * either don't send ICMP "time exceeded" messages or send them
 * with a ttl too small to reach us.  14 - 17 are running the
 * MIT C Gateway code that doesn't send "time exceeded"s.  God
 * only knows what's going on with 12.
 *
 * The silent gateway 12 in the above may be the result of a bug in
 * the 4.[23]BSD network code (and its derivatives):  4.x (x <= 3)
 * sends an unreachable message using whatever ttl remains in the
 * original datagram.  Since, for gateways, the remaining ttl is
 * zero, the icmp "time exceeded" is guaranteed to not make it back
 * to us.  The behavior of this bug is slightly more interesting
 * when it appears on the destination system:
 *
 *      1  helios.ee.lbl.gov (128.3.112.1)  0 ms  0 ms  0 ms
 *      2  lilac-dmc.Berkeley.EDU (128.32.216.1)  39 ms  19 ms  39 ms
 *      3  lilac-dmc.Berkeley.EDU (128.32.216.1)  19 ms  39 ms  19 ms
 *      4  ccngw-ner-cc.Berkeley.EDU (128.32.136.23)  39 ms  40 ms  19 ms
 *      5  ccn-nerif35.Berkeley.EDU (128.32.168.35)  39 ms  39 ms  39 ms
 *      6  csgw.Berkeley.EDU (128.32.133.254)  39 ms  59 ms  39 ms
 *      7  * * *
 *      8  * * *
 *      9  * * *
 *     10  * * *
 *     11  * * *
 *     12  * * *
 *     13  rip.Berkeley.EDU (128.32.131.22)  59 ms !  39 ms !  39 ms !
 *
 * Notice that there are 12 "gateways" (13 is the final
 * destination) and exactly the last half of them are "missing".
 * What's really happening is that rip (a Sun-3 running Sun OS3.5)
 * is using the ttl from our arriving datagram as the ttl in its
 * icmp reply.  So, the reply will time out on the return path
 * (with no notice sent to anyone since icmp's aren't sent for
 * icmp's) until we probe with a ttl that's at least twice the path
 * length.  I.e., rip is really only 7 hops away.  A reply that
 * returns with a ttl of 1 is a clue this problem exists.
 * Traceroute prints a "!" after the time if the ttl is <= 1.
 * Since vendors ship a lot of obsolete (DEC's Ultrix, Sun 3.x) or
 * non-standard (HPUX) software, expect to see this problem
 * frequently and/or take care picking the target host of your
 * probes.
 *
 * Other possible annotations after the time are !H, !N, !P (got a host,
 * network or protocol unreachable, respectively), !S or !F (source
 * route failed or fragmentation needed -- neither of these should
 * ever occur and the associated gateway is busted if you see one).  If
 * almost all the probes result in some kind of unreachable, traceroute
 * will give up and exit.
 *
 * Notes
 * -----
 * This program must be run by root or be setuid.  (I suggest that
 * you *don't* make it setuid -- casual use could result in a lot
 * of unnecessary traffic on our poor, congested nets.)
 *
 * This program requires a kernel mod that does not appear in any
 * system available from Berkeley:  A raw ip socket using proto
 * IPPROTO_RAW must interpret the data sent as an ip datagram (as
 * opposed to data to be wrapped in a ip datagram).  See the README
 * file that came with the source to this program for a description
 * of the mods I made to /sys/netinet/raw_ip.c.  Your mileage may
 * vary.  But, again, ANY 4.x (x < 4) BSD KERNEL WILL HAVE TO BE
 * MODIFIED TO RUN THIS PROGRAM.
 *
 * The udp port usage may appear bizarre (well, ok, it is bizarre).
 * The problem is that an icmp message only contains 8 bytes of
 * data from the original datagram.  8 bytes is the size of a udp
 * header so, if we want to associate replies with the original
 * datagram, the necessary information must be encoded into the
 * udp header (the ip id could be used but there's no way to
 * interlock with the kernel's assignment of ip id's and, anyway,
 * it would have taken a lot more kernel hacking to allow this
 * code to set the ip id).  So, to allow two or more users to
 * use traceroute simultaneously, we use this task's pid as the
 * source port (the high bit is set to move the port number out
 * of the "likely" range).  To keep track of which probe is being
 * replied to (so times and/or hop counts don't get confused by a
 * reply that was delayed in transit), we increment the destination
 * port number before each probe.
 *
 * Don't use this as a coding example.  I was trying to find a
 * routing problem and this code sort-of popped out after 48 hours
 * without sleep.  I was amazed it ever compiled, much less ran.
 *
 * I stole the idea for this program from Steve Deering.  Since
 * the first release, I've learned that had I attended the right
 * IETF working group meetings, I also could have stolen it from Guy
 * Almes or Matt Mathis.  I don't know (or care) who came up with
 * the idea first.  I envy the originators' perspicacity and I'm
 * glad they didn't keep the idea a secret.
 *
 * Tim Seaver, Ken Adelman and C. Philip Wood provided bug fixes and/or
 * enhancements to the original distribution.
 *
 * I've hacked up a round-trip-route version of this that works by
 * sending a loose-source-routed udp datagram through the destination
 * back to yourself.  Unfortunately, SO many gateways botch source
 * routing, the thing is almost worthless.  Maybe one day...
 *
 *  -- Van Jacobson (van@helios.ee.lbl.gov)
 *     Tue Dec 20 03:50:13 PST 1988
 */

#include <sys/param.h>
#include <sys/time.h>
#include <sys/socket.h>
#include <sys/file.h>
#include <sys/ioctl.h>
#include <net/if.h>

#if __linux__
#include <endian.h>
#endif
#include <netinet/in_systm.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <netinet/ip_icmp.h>
#include <netinet/udp.h>

#include <netinet/ip6.h>
#include <netinet/icmp6.h>
#include <linux/types.h>

#include <arpa/inet.h>

#include <netdb.h>
#include <stdio.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "SNAPSHOT.h"

#ifndef SOL_IPV6
#define SOL_IPV6 IPPROTO_IPV6
#endif

#define	MAXPACKET	65535
#define MAX_HOSTNAMELEN	NI_MAXHOST

#ifndef FD_SET
#define NFDBITS         (8*sizeof(fd_set))
#define FD_SETSIZE      NFDBITS
#define FD_SET(n, p)    ((p)->fds_bits[(n)/NFDBITS] |= (1 << ((n) % NFDBITS)))
#define FD_CLR(n, p)    ((p)->fds_bits[(n)/NFDBITS] &= ~(1 << ((n) % NFDBITS)))
#define FD_ISSET(n, p)  ((p)->fds_bits[(n)/NFDBITS] & (1 << ((n) % NFDBITS)))
#define FD_ZERO(p)      memset((char *)(p), 0, sizeof(*(p)))
#endif

#define Fprintf (void)fprintf
#define Printf (void)printf

u_char	packet[512];		/* last inbound (icmp) packet */

int	wait_for_reply(int, struct sockaddr_in6 *, struct in6_addr *, int);
int	packet_ok(u_char *buf, int cc, struct sockaddr_in6 *from,
		  struct in6_addr *to, int seq, struct timeval *);
void	send_probe(int seq, int ttl);
double	deltaT (struct timeval *, struct timeval *);
void	print(unsigned char *buf, int cc, struct sockaddr_in6 *from);
void	tvsub (struct timeval *, struct timeval *);
void	usage(void);

int icmp_sock;			/* receive (icmp) socket file descriptor */
int sndsock;			/* send (udp) socket file descriptor */
struct timezone tz;		/* leftover */

struct sockaddr_in6 whereto;	/* Who to try to reach */

struct sockaddr_in6 saddr;
struct sockaddr_in6 firsthop;
char *source = NULL;
char *device = NULL;
char *hostname;

int nprobes = 3;
int max_ttl = 30;
pid_t ident;
u_short port = 32768+666;	/* start udp dest port # for probe packets */
int options;			/* socket options */
int verbose;
int waittime = 5;		/* time to wait for response (in seconds) */
int nflag;			/* print addresses numerically */


struct pkt_format
{
	__u32 ident;
	__u32 seq;
	struct timeval tv;
};

char *sendbuff;
int datalen = sizeof(struct pkt_format);



int main(int argc, char *argv[])
{
	char pa[MAX_HOSTNAMELEN];
	extern char *optarg;
	extern int optind;
	struct hostent *hp;
	struct sockaddr_in6 from, *to;
	int ch, i, on, probe, seq, tos, ttl;
	int socket_errno;

	icmp_sock = socket(AF_INET6, SOCK_RAW, IPPROTO_ICMPV6);
	socket_errno = errno;

	if (setuid(getuid())) {
		perror("traceroute6: setuid");
		exit(-1);
	}

	on = 1;
	seq = tos = 0;
	to = (struct sockaddr_in6 *)&whereto;
	while ((ch = getopt(argc, argv, "dm:np:q:rs:t:w:vi:g:V")) != EOF) {
		switch(ch) {
		case 'd':
			options |= SO_DEBUG;
			break;
		case 'm':
			max_ttl = atoi(optarg);
			if (max_ttl <= 1) {
				Fprintf(stderr,
				    "traceroute: max ttl must be >1.\n");
				exit(1);
			}
			break;
		case 'n':
			nflag++;
			break;
		case 'p':
			port = atoi(optarg);
			if (port < 1) {
				Fprintf(stderr,
				    "traceroute: port must be >0.\n");
				exit(1);
			}
			break;
		case 'q':
			nprobes = atoi(optarg);
			if (nprobes < 1) {
				Fprintf(stderr,
				    "traceroute: nprobes must be >0.\n");
				exit(1);
			}
			break;
		case 'r':
			options |= SO_DONTROUTE;
			break;
		case 's':
			/*
			 * set the ip source address of the outbound
			 * probe (e.g., on a multi-homed host).
			 */
			source = optarg;
			break;
		case 'i':
			device = optarg;
			break;
		case 'g':
			Fprintf(stderr, "Sorry, rthdr is not yet supported\n");
			break;
		case 'v':
			verbose++;
			break;
		case 'w':
			waittime = atoi(optarg);
			if (waittime <= 1) {
				Fprintf(stderr,
				    "traceroute: wait must be >1 sec.\n");
				exit(1);
			}
			break;
		case 'V':
			printf("traceroute6 utility, iputils-ss%s\n", SNAPSHOT);
			exit(0);
		default:
			usage();
		}
	}
	argc -= optind;
	argv += optind;

	if (argc < 1)
		usage();

	setlinebuf (stdout);

	(void) memset((char *)&whereto, 0, sizeof(whereto));

	to->sin6_family = AF_INET6;
	to->sin6_port = htons(port);

	if (inet_pton(AF_INET6, *argv, &to->sin6_addr) > 0) {
		hostname = *argv;
	} else {
		hp = gethostbyname2(*argv, AF_INET6);
		if (hp) {
			memmove((caddr_t)&to->sin6_addr, hp->h_addr, sizeof(to->sin6_addr));
			hostname = (char *)hp->h_name;
		} else {
			(void)fprintf(stderr,
			    "traceroute: unknown host %s\n", *argv);
			exit(1);
		}
	}
	firsthop = *to;
	if (*++argv) {
		datalen = atoi(*argv);
		/* Message for rpm maintainers: have _shame_. If you want
		 * to fix something send the patch to me for sanity checking.
		 * "datalen" patch is a shit. */
		if (datalen == 0)
			datalen = sizeof(struct pkt_format);
		else if (datalen < (int)sizeof(struct pkt_format) ||
			 datalen >= MAXPACKET) {
			Fprintf(stderr,
			    "traceroute: packet size must be %d <= s < %d.\n",
				(int)sizeof(struct pkt_format), MAXPACKET);
			exit(1);
		}
	}

	ident = getpid();

	sendbuff = malloc(datalen);
	if (sendbuff == NULL) {
		fprintf(stderr, "malloc failed\n");
		exit(1);
	}

	if (icmp_sock < 0) {
		errno = socket_errno;
		perror("traceroute6: icmp socket");
		exit(1);
	}

#ifdef IPV6_RECVPKTINFO
	setsockopt(icmp_sock, SOL_IPV6, IPV6_RECVPKTINFO, &on, sizeof(on));
	setsockopt(icmp_sock, SOL_IPV6, IPV6_2292PKTINFO, &on, sizeof(on));
#else
	setsockopt(icmp_sock, SOL_IPV6, IPV6_PKTINFO, &on, sizeof(on));
#endif

	if (options & SO_DEBUG)
		setsockopt(icmp_sock, SOL_SOCKET, SO_DEBUG,
			   (char *)&on, sizeof(on));
	if (options & SO_DONTROUTE)
		setsockopt(icmp_sock, SOL_SOCKET, SO_DONTROUTE,
			   (char *)&on, sizeof(on));

#ifdef __linux__
	on = 2;
	if (setsockopt(icmp_sock, SOL_RAW, IPV6_CHECKSUM, &on, sizeof(on)) < 0) {
		/* checksum should be enabled by default and setting this
		 * option might fail anyway.
		 */
		fprintf(stderr, "setsockopt(RAW_CHECKSUM) failed - try to continue.");
	}
#endif

	if ((sndsock = socket(AF_INET6, SOCK_DGRAM, 0)) < 0) {
		perror("traceroute: UDP socket");
		exit(5);
	}
#ifdef SO_SNDBUF
	if (setsockopt(sndsock, SOL_SOCKET, SO_SNDBUF, (char *)&datalen,
		       sizeof(datalen)) < 0) {
		perror("traceroute: SO_SNDBUF");
		exit(6);
	}
#endif /* SO_SNDBUF */

	if (options & SO_DEBUG)
		(void) setsockopt(sndsock, SOL_SOCKET, SO_DEBUG,
				  (char *)&on, sizeof(on));
	if (options & SO_DONTROUTE)
		(void) setsockopt(sndsock, SOL_SOCKET, SO_DONTROUTE,
				  (char *)&on, sizeof(on));

	if (source == NULL) {
		socklen_t alen;
		int probe_fd = socket(AF_INET6, SOCK_DGRAM, 0);

		if (probe_fd < 0) {
			perror("socket");
			exit(1);
		}
		if (device) {
			if (setsockopt(probe_fd, SOL_SOCKET, SO_BINDTODEVICE, device, strlen(device)+1) == -1)
				perror("WARNING: interface is ignored");
		}
		firsthop.sin6_port = htons(1025);
		if (connect(probe_fd, (struct sockaddr*)&firsthop, sizeof(firsthop)) == -1) {
			perror("connect");
			exit(1);
		}
		alen = sizeof(saddr);
		if (getsockname(probe_fd, (struct sockaddr*)&saddr, &alen) == -1) {
			perror("getsockname");
			exit(1);
		}
		saddr.sin6_port = 0;
		close(probe_fd);
	} else {
		(void) memset((char *)&saddr, 0, sizeof(saddr));
		saddr.sin6_family = AF_INET6;
		if (inet_pton(AF_INET6, source, &saddr.sin6_addr) <= 0)
		{
			Printf("traceroute: unknown addr %s\n", source);
			exit(1);
		}
	}

	if (bind(sndsock, (struct sockaddr *)&saddr, sizeof(saddr)) < 0) {
		perror ("traceroute: bind sending socket");
		exit (1);
	}
	if (bind(icmp_sock, (struct sockaddr *)&saddr, sizeof(saddr)) < 0) {
		perror ("traceroute: bind icmp6 socket");
		exit (1);
	}

	Fprintf(stderr, "traceroute to %s (%s)", hostname,
		inet_ntop(AF_INET6, &to->sin6_addr, pa, sizeof(pa)));

	Fprintf(stderr, " from %s",
		inet_ntop(AF_INET6, &saddr.sin6_addr, pa, sizeof(pa)));
	Fprintf(stderr, ", %d hops max, %d byte packets\n", max_ttl, datalen);
	(void) fflush(stderr);

	for (ttl = 1; ttl <= max_ttl; ++ttl) {
		struct in6_addr lastaddr = {{{0,}}};
		int got_there = 0;
		int unreachable = 0;

		Printf("%2d ", ttl);
		for (probe = 0; probe < nprobes; ++probe) {
			int cc, reset_timer;
			struct timeval t1, t2;
			struct timezone tz;
			struct in6_addr to;

			gettimeofday(&t1, &tz);
			send_probe(++seq, ttl);
			reset_timer = 1;

			while ((cc = wait_for_reply(icmp_sock, &from, &to, reset_timer)) != 0) {
				gettimeofday(&t2, &tz);
				if ((i = packet_ok(packet, cc, &from, &to, seq, &t1))) {
					reset_timer = 1;
					if (memcmp(&from.sin6_addr, &lastaddr, sizeof(from.sin6_addr))) {
						print(packet, cc, &from);
						memcpy(&lastaddr,
						       &from.sin6_addr,
						       sizeof(lastaddr));
					}
					Printf("  %g ms", deltaT(&t1, &t2));
					switch(i - 1) {
					case ICMP6_DST_UNREACH_NOPORT:
						++got_there;
						break;

					case ICMP6_DST_UNREACH_NOROUTE:
						++unreachable;
						Printf(" !N");
						break;
					case ICMP6_DST_UNREACH_ADDR:
						++unreachable;
						Printf(" !H");
						break;

					case ICMP6_DST_UNREACH_ADMIN:
						++unreachable;
						Printf(" !S");
						break;
					}
					break;
				} else
					reset_timer = 0;
			}
			if (cc <= 0)
				Printf(" *");
			(void) fflush(stdout);
		}
		putchar('\n');
		if (got_there ||
		    (unreachable > 0 && unreachable >= nprobes-1))
			exit(0);
	}

	return 0;
}

int
wait_for_reply(sock, from, to, reset_timer)
	int sock;
	struct sockaddr_in6 *from;
	struct in6_addr *to;
	int reset_timer;
{
	fd_set fds;
	static struct timeval wait;
	int cc = 0;
	char cbuf[512];

	FD_ZERO(&fds);
	FD_SET(sock, &fds);
	if (reset_timer) {
		/*
		 * traceroute could hang if someone else has a ping
		 * running and our ICMP reply gets dropped but we don't
		 * realize it because we keep waking up to handle those
		 * other ICMP packets that keep coming in.  To fix this,
		 * "reset_timer" will only be true if the last packet that
		 * came in was for us or if this is the first time we're
		 * waiting for a reply since sending out a probe.  Note
		 * that this takes advantage of the select() feature on
		 * Linux where the remaining timeout is written to the
		 * struct timeval area.
		 */
		wait.tv_sec = waittime;
		wait.tv_usec = 0;
	}

	if (select(sock+1, &fds, (fd_set *)0, (fd_set *)0, &wait) > 0) {
		struct iovec iov;
		struct msghdr msg;
		iov.iov_base = packet;
		iov.iov_len = sizeof(packet);
		msg.msg_name = (void *)from;
		msg.msg_namelen = sizeof(*from);
		msg.msg_iov = &iov;
		msg.msg_iovlen = 1;
		msg.msg_flags = 0;
		msg.msg_control = cbuf;
		msg.msg_controllen = sizeof(cbuf);

		cc = recvmsg(icmp_sock, &msg, 0);
		if (cc >= 0) {
			struct cmsghdr *cmsg;
			struct in6_pktinfo *ipi;

			for (cmsg = CMSG_FIRSTHDR(&msg);
			     cmsg;
			     cmsg = CMSG_NXTHDR(&msg, cmsg)) {
				if (cmsg->cmsg_level != SOL_IPV6)
					continue;
				switch (cmsg->cmsg_type) {
				case IPV6_PKTINFO:
#ifdef IPV6_2292PKTINFO
				case IPV6_2292PKTINFO:
#endif
					ipi = (struct in6_pktinfo *)CMSG_DATA(cmsg);
					memcpy(to, ipi, sizeof(*to));
				}
			}
		}
	}

	return(cc);
}


void send_probe(int seq, int ttl)
{
	struct pkt_format *pkt = (struct pkt_format *) sendbuff;
	int i;

	pkt->ident = htonl(ident);
	pkt->seq = htonl(seq);
	gettimeofday(&pkt->tv, &tz);

	i = setsockopt(sndsock, SOL_IPV6, IPV6_UNICAST_HOPS, &ttl, sizeof(ttl));
	if (i < 0)
	{
		perror("setsockopt");
		exit(1);
	}

	do {
		i = sendto(sndsock, sendbuff, datalen, 0,
			   (struct sockaddr *)&whereto, sizeof(whereto));
	} while (i<0 && errno == ECONNREFUSED);

	if (i < 0 || i != datalen)  {
		if (i<0)
			perror("sendto");
		Printf("traceroute: wrote %s %d chars, ret=%d\n", hostname,
			datalen, i);
		(void) fflush(stdout);
	}
}


double deltaT(struct timeval *t1p, struct timeval *t2p)
{
	register double dt;

	dt = (double)(t2p->tv_sec - t1p->tv_sec) * 1000.0 +
	     (double)(t2p->tv_usec - t1p->tv_usec) / 1000.0;
	return (dt);
}


/*
 * Convert an ICMP "type" field to a printable string.
 */
char * pr_type(unsigned char t)
{
	switch(t) {
	/* Unknown */
	case 0:
		return "Error";
	case 1:
		/* ICMP6_DST_UNREACH: */
		return "Destination Unreachable";
	case 2:
		/* ICMP6_PACKET_TOO_BIG: */
		return "Packet Too Big";
	case 3:
		/* ICMP6_TIME_EXCEEDED */
		return "Time Exceeded in Transit";
	case 4:
		/* ICMP6_PARAM_PROB */
		return "Parameter Problem";
	case 128:
		/* ICMP6_ECHO_REQUEST */
		return "Echo Request";
	case 129:
		/* ICMP6_ECHO_REPLY */
		return "Echo Reply";
	case 130:
		/* ICMP6_MEMBERSHIP_QUERY */
		return "Membership Query";
	case 131:
		/* ICMP6_MEMBERSHIP_REPORT */
		return "Membership Report";
	case 132:
		/* ICMP6_MEMBERSHIP_REDUCTION */
		return "Membership Reduction";
	case 133:
		/* ND_ROUTER_SOLICIT */
		return "Router Solicitation";
	case 134:
		/* ND_ROUTER_ADVERT */
		return "Router Advertisement";
	case 135:
		/* ND_NEIGHBOR_SOLICIT */
		return "Neighbor Solicitation";
	case 136:
		/* ND_NEIGHBOR_ADVERT */
		return "Neighbor Advertisement";
	case 137:
		/* ND_REDIRECT */
		return "Redirect";
	}

	return("OUT-OF-RANGE");
}


int packet_ok(u_char *buf, int cc, struct sockaddr_in6 *from,
	      struct in6_addr *to, int seq,
	      struct timeval *tv)
{
	struct icmp6_hdr *icp;
	u_char type, code;

	icp = (struct icmp6_hdr *) buf;

	type = icp->icmp6_type;
	code = icp->icmp6_code;

	if ((type == ICMP6_TIME_EXCEEDED && code == ICMP6_TIME_EXCEED_TRANSIT) ||
	    type == ICMP6_DST_UNREACH)
	{
		struct ip6_hdr *hip;
		struct udphdr *up;
		int nexthdr;

		hip = (struct ip6_hdr *) (icp + 1);
		up = (struct udphdr *)(hip+1);
		nexthdr = hip->ip6_nxt;

		if (nexthdr == 44) {
			nexthdr = *(unsigned char*)up;
			up++;
		}
		if (nexthdr == IPPROTO_UDP)
		{
			struct pkt_format *pkt;

			pkt = (struct pkt_format *) (up + 1);

			if (ntohl(pkt->ident) == ident &&
			    ntohl(pkt->seq) == seq)
			{
				*tv = pkt->tv;
				return (type == ICMP6_TIME_EXCEEDED ? -1 : code+1);
			}
		}

	}

	if (verbose) {
		unsigned char *p;
		char pa1[MAX_HOSTNAMELEN];
		char pa2[MAX_HOSTNAMELEN];
		int i;

		p = (unsigned char *) (icp + 1);

		Printf("\n%d bytes from %s to %s", cc,
		       inet_ntop(AF_INET6, &from->sin6_addr, pa1, sizeof(pa1)),
		       inet_ntop(AF_INET6, to, pa2, sizeof(pa2)));

		Printf(": icmp type %d (%s) code %d\n", type, pr_type(type),
		       icp->icmp6_code);

		cc -= sizeof(struct icmp6_hdr);
		for (i = 0; i < cc ; i++) {
			if (i % 16 == 0)
				Printf("%04x:", i);
			if (i % 4 == 0)
				Printf(" ");
			Printf("%02x", 0xff & (unsigned)p[i]);
			if (i % 16 == 15 && i + 1 < cc)
				Printf("\n");
		}
		Printf("\n");
	}

	return(0);
}


void print(unsigned char *buf, int cc, struct sockaddr_in6 *from)
{
	char pa[MAX_HOSTNAMELEN];

	if (nflag)
		Printf(" %s", inet_ntop(AF_INET6, &from->sin6_addr,
					pa, sizeof(pa)));
	else
	{
		const char *hostname;
		struct hostent *hp;

		hostname = inet_ntop(AF_INET6, &from->sin6_addr, pa, sizeof(pa));

		if ((hp = gethostbyaddr((char *)&from->sin6_addr,
					sizeof(from->sin6_addr), AF_INET6)))
			hostname = hp->h_name;

		Printf(" %s (%s)", hostname, pa);
	}
}


/*
 * Subtract 2 timeval structs:  out = out - in.
 * Out is assumed to be >= in.
 */
void
tvsub(out, in)
	register struct timeval *out, *in;
{
	if ((out->tv_usec -= in->tv_usec) < 0)   {
		out->tv_sec--;
		out->tv_usec += 1000000;
	}
	out->tv_sec -= in->tv_sec;
}

void usage(void)
{
	fprintf(stderr,
"Usage: traceroute6 [-dnrvV] [-m max_ttl] [-p port#] [-q nqueries]\n\t\
[-s src_addr] [-t tos] [-w wait] host [data size]\n");
	exit(1);
}
