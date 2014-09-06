/*	$OpenBSD: netstat.h,v 1.31 2005/02/10 14:25:08 itojun Exp $	*/
/*	$NetBSD: netstat.h,v 1.6 1996/05/07 02:55:05 thorpej Exp $	*/

/*
 * Copyright (c) 1992, 1993
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
 *
 *	from: @(#)netstat.h	8.2 (Berkeley) 1/4/94
 */

#ifdef HAVE_SYS_CDEFS_H
#include <sys/cdefs.h>
#endif

/* What is the max length of a pointer printed with %p (including 0x)? */
#define PLEN	(LONG_BIT / 4 + 2)

extern int	Aflag;		/* show addresses of protocol control block */
extern int	aflag;		/* show all sockets (including servers) */
extern int	bflag;		/* show bytes instead of packets */
extern int	dflag;		/* show i/f dropped packets */
extern int	gflag;		/* show group (multicast) routing or stats */
extern int	iflag;		/* show interfaces */
extern int	lflag;		/* show routing table with use and ref */
extern int	mflag;		/* show memory stats */
extern int	nflag;		/* show addresses numerically */
extern int	oflag;		/* Open/Net-BSD style octet output */
extern int	pflag;		/* show given protocol */
extern int	qflag;		/* only display non-zero values for output */
extern int	rflag;		/* show routing tables (or routing stats) */
extern int	Sflag;		/* show source address in routing table */
extern int	sflag;		/* show protocol statistics */
extern int	tflag;		/* show i/f watchdog timers */
extern int	vflag;		/* be verbose */

extern int	interval;	/* repeat interval for i/f stats */

extern char	*intrface;	/* desired i/f for stats, or NULL for all i/fs */

extern int	af;		/* address family */
extern int  max_getbulk;  /* specifies the max-repeaters value to use with GETBULK requests */

extern	char *__progname; /* program name, from crt0.o */

const char	*plural(int);

void	tcpprotopr(const char *);
void	udpprotopr(const char *);
void	tcp_stats( const char *);
void	udp_stats( const char *);
void	ip_stats(  const char *);
void	icmp_stats(const char *);

void	tcp6protopr(const char *);
void	udp6protopr(const char *);
void	ip6_stats(  const char *);
void	icmp6_stats(const char *);

void	pr_rthdr(int);
void	pr_encaphdr(void);
void	pr_family(int);
void	rt_stats(void);

char	*routename(in_addr_t);
char	*netname(in_addr_t, in_addr_t);
char	*ns_print(struct sockaddr *);
void	routepr(void);

void	intpr(int);

