/*	$OpenBSD: inet6.c,v 1.31 2004/11/17 01:47:20 itojun Exp $	*/
/*	BSDI inet.c,v 2.3 1995/10/24 02:19:29 prb Exp	*/
/*
 * Copyright (c) 1983, 1988, 1993
 *	The Regents of the University of California.  All rights reserved.
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

#ifdef  INHERITED_CODE
#ifndef lint
#if 0
static char sccsid[] = "@(#)inet.c	8.4 (Berkeley) 4/20/94";
#else
/*__RCSID("$OpenBSD: inet6.c,v 1.31 2004/11/17 01:47:20 itojun Exp $");*/
/*__RCSID("KAME Id: inet6.c,v 1.10 2000/02/09 10:49:31 itojun Exp");*/
#endif
#endif /* not lint */
#endif

#include <net-snmp/net-snmp-config.h>

#if HAVE_UNISTD_H
#include <unistd.h>
#endif
#if HAVE_WINSOCK_H
#include "winstub.h"
#endif
#if HAVE_SYS_SOCKET_H
#include <sys/socket.h>
#endif
#if HAVE_NETDB_H
#include <netdb.h>
#endif
#if HAVE_SYS_TYPES_H
#include <sys/types.h>
#endif

#if HAVE_NETINET_IN_H
#include <netinet/in.h>
#endif
#if HAVE_ARPA_INET_H
#include <arpa/inet.h>
#endif

#include <net-snmp/net-snmp-includes.h>

#include "main.h"
#include "netstat.h"

struct stat_table {
    unsigned int entry;      /* entry number in table */
    /*
     * format string to printf(description, value) 
     * warning: the %d must be before the %s 
     */
    char            description[80];
};

char	*inet6name(const unsigned char *);
void	inet6print(unsigned char *, int, const char *, int);

/*
 * Print a summary of TCPv6 connections
 * Listening processes are suppressed unless the
 *   -a (all) flag is specified.
 */
const char     *tcp6states[] = {
    "",
    "CLOSED",
    "LISTEN",
    "SYNSENT",
    "SYNRECEIVED",
    "ESTABLISHED",
    "FINWAIT1",
    "FINWAIT2",
    "CLOSEWAIT",
    "LASTACK",
    "CLOSING",
    "TIMEWAIT"
};
#define TCP_NSTATES 11

void
tcp6protopr(const char *name)
{
    netsnmp_variable_list *var, *vp;
    oid    ipv6TcpConnState_oid[] = { 1,3,6,1,2,1,6,16,1,6 };
    size_t ipv6TcpConnState_len   = OID_LENGTH( ipv6TcpConnState_oid );
    int    state, i;
    unsigned char   localAddr[16], remoteAddr[16];
    int    localPort,     remotePort,  ifIndex;
    int    first = 1;

    /*
     * Walking the v6 tcpConnState column will provide all
     *   the necessary information.
     */
    var = NULL;
    snmp_varlist_add_variable( &var, ipv6TcpConnState_oid,
                                     ipv6TcpConnState_len,
                                     ASN_NULL, NULL,  0);
    if (netsnmp_query_walk( var, ss ) != SNMP_ERR_NOERROR)
        return;
    if (var->type == ASN_NULL)    /* No entries */
        return;

    for (vp = var; vp ; vp=vp->next_variable) {
        state = *vp->val.integer;
        if (!aflag && state == MIB_TCPCONNSTATE_LISTEN)
            continue;

        if (first) {
            printf("Active Internet Connections");
            if (aflag)
                printf(" (including servers)");
            putchar('\n');
            printf("%-5.5s %-28.28s %-28.28s %4s %s\n",
                   "Proto", "Local Address", "Remote Address", "I/F", "(state)");
            first = 0;
        }
        
        /* Extract the local/remote information from the index values */
        for (i=0; i<16; i++)
            localAddr[i]  = vp->name[ 10+i ];
        localPort    = vp->name[ 26 ];
        for (i=0; i<16; i++)
            remoteAddr[i] = vp->name[ 27+i ];
        remotePort   = vp->name[ 43 ];
        ifIndex      = vp->name[ 44 ];

        printf("%-5.5s", name);
        inet6print(localAddr,  localPort,  name, 1);
        inet6print(remoteAddr, remotePort, name, 0);
        if ( state < 1 || state > TCP_NSTATES )
            printf(" %4d %d\n", ifIndex, state );
        else
            printf(" %4d %s\n", ifIndex, tcp6states[ state ]);
    }
    snmp_free_varbind( var );
}

/*
 * Print a summary of UDPv6 "connections"
 *    XXX - what about "listening" services ??
 */
void
udp6protopr(const char *name)
{
    netsnmp_variable_list *var, *vp;
    oid    ipv6UdpLocalAddress_oid[] = { 1,3,6,1,2,1,7,6,1,1 };
    size_t ipv6UdpLocalAddress_len   = OID_LENGTH( ipv6UdpLocalAddress_oid );
    int    localPort, ifIndex;

    /*
     * Walking a single column of the udpTable will provide
     *   all the necessary information from the index values.
     */
    var = NULL;
    snmp_varlist_add_variable( &var, ipv6UdpLocalAddress_oid,
                                     ipv6UdpLocalAddress_len,
                                     ASN_NULL, NULL,  0);
    if (netsnmp_query_walk( var, ss ) != SNMP_ERR_NOERROR)
        return;
    if (var->type == ASN_NULL)    /* No entries */
        return;

    printf("Active Internet Connections\n");
    printf("%-5.5s %-28.28s %4s\n", "Proto", "Local Address", "I/F");
    for (vp = var; vp ; vp=vp->next_variable) {
        printf("%-5.5s", name);
        /*
         * Extract the local port from the index values, but take
         *   the IP address from the varbind value, (which is why
         *   we walked udpLocalAddress rather than udpLocalPort)
         */
        localPort = vp->name[ vp->name_length-2 ];
        ifIndex   = vp->name[ vp->name_length-1 ];
        inet6print(vp->val.string, localPort, name, 1);
        printf(" %4d\n", ifIndex );
    }
    snmp_free_varbind( var );
}


	/*********************
	 *
	 *  IPv6 statistics
	 *
	 *********************/

/*
 *  Unlike the equivalent IPv4 statistics display routine,
 *    the IPv6 version must walk the columns of a table
 *    and total the statistics for each column (rather
 *    than simply retrieving individual scalar values)
 */
void
_dump_v6stats( const char *name, oid *oid_buf, size_t buf_len,
               struct stat_table *stable )
{
    netsnmp_variable_list *var, *vp;
    struct stat_table     *sp;
    long   *stats;
    oid stat;
    unsigned int max_stat = 0;
    int    active   = 0;

    var = NULL;
    for (sp=stable; sp->entry; sp++) {
        oid_buf[buf_len-1] = sp->entry;
        if (sp->entry > max_stat)
            max_stat = sp->entry;
        snmp_varlist_add_variable( &var, oid_buf, buf_len,
                                   ASN_NULL, NULL,  0);
    }
    oid_buf[buf_len-1] = stable[0].entry;
    stats = (long *)calloc(max_stat+1, sizeof(long));
    
    /*
     * Walk the specified column(s), and total the individual statistics
     */
    while (1) {
        if (netsnmp_query_getnext( var, ss ) != SNMP_ERR_NOERROR)
            break;
        if ( snmp_oid_compare( oid_buf,   buf_len,
                               var->name, buf_len) != 0 )
            break;    /* End of Table */
            
        for ( vp=var; vp; vp=vp->next_variable ) {
            stat = vp->name[ buf_len-1 ];
            stats[stat] += *vp->val.integer;
        }
        active=1;
    }
    if (!active) {
        free( stats );
        snmp_free_varbind( var );
        return;     /* No statistics to display */
    }

    /*
     * Display the results
     */
    printf("%s:\n", name);
    for (sp=stable; sp->entry; sp++) {
        /*
         * If '-Cs' was specified twice,
         *   then only display non-zero stats.
         */
        if ( stats[sp->entry] > 0 || sflag == 1 ) {
            putchar('\t');
            printf(sp->description, stats[sp->entry],
                             plural(stats[sp->entry]));
            putchar('\n');
        }
    }
    free( stats );
    snmp_free_varbind( var );
}


/*
 * Dump IP6 statistics.
 */
void
ip6_stats(const char *name)
{
    oid               ip6stats_oid[] = { 1, 3, 6, 1, 2, 1, 55, 1, 6, 1, 0 };
    size_t            ip6stats_len   = OID_LENGTH( ip6stats_oid );
    struct stat_table ip6stats_tbl[] = {
        {1, "%d total datagram%s received"},
        {2, "%d datagram%s with header errors"},
        {3, "%d oversized datagram%s"},
        {4, "%d datagram%s with no route"},
        {5, "%d datagram%s with an invalid destination address"},
        {6, "%d datagram%s with unknown protocol"},
        {7, "%d short datagram%s discarded"},
        {8, "%d datagram%s discarded"},
        {9, "%d datagram%s delivered"},
        {10, "%d datagram%s forwarded"},
        {11, "%d output datagram request%s"},
        {12, "%d output datagram%s discarded"},
        {13, "%d datagram%s fragmented"},
        {14, "%d fragmentation failure%s"},
        {15, "%d fragment%s created"},
        {16, "%d fragment%s received"},
        {17, "%d datagram%s reassembled"},
        {18, "%d reassembly failure%s"},
        {19, "%d multicast datagram%s received"},
        {20, "%d multicast datagram%s transmitted"},
        {0, ""}
    };

    _dump_v6stats( name, ip6stats_oid, ip6stats_len, ip6stats_tbl );
}

/*
 * Dump IPv6 per-interface statistics - Omitted
 */


/*
 * Dump ICMP6 statistics.
 */
void
icmp6_stats(const char *name)
{
    oid               icmp6stats_oid[] = { 1, 3, 6, 1, 2, 1, 56, 1, 1, 1, 0 };
    size_t            icmp6stats_len   = OID_LENGTH( icmp6stats_oid );
    struct stat_table icmp6stats_tbl[] = {
        {1, "%d total message%s received"},
        {2, "%d message%s dropped due to errors"},
        {18, "%d ouput message request%s"},
        {19, "%d output message%s discarded"},
        {0, ""}
    };
    struct stat_table icmp6_inhistogram[] = {
        {3, "Destination unreachable: %d"},
        {4, "Admin Prohibit: %d"},
        {5, "Time Exceeded: %d"},
        {6, "Parameter Problem: %d"},
        {7, "Too Big: %d"},
        {8, "Echo Request: %d"},
        {9, "Echo Reply: %d"},
        {10, "Router Solicit: %d"},
        {11, "Router Advert: %d"},
        {12, "Neighbor Solicit: %d"},
        {13, "Neighbor Advert: %d"},
        {14, "Redirect: %d"},
        {15, "Group Member Request: %d"},
        {16, "Group Member Reply:%d"},
        {17, "Group Member Reduce:%d"},
        {0, ""}
    };
    struct stat_table icmp6_outhistogram[] = {
        {20, "Destination unreachable: %d"},
        {21, "Admin Prohibit: %d"},
        {22, "Time Exceeded: %d"},
        {23, "Parameter Problem: %d"},
        {24, "Too Big: %d"},
        {25, "Echo Request: %d"},
        {26, "Echo Reply: %d"},
        {27, "Router Solicit: %d"},
        {28, "Router Advert: %d"},
        {29, "Neighbor Solicit: %d"},
        {30, "Neighbor Advert: %d"},
        {31, "Redirect: %d"},
        {32, "Group Member Request: %d"},
        {33, "Group Member Reply:%d"},
        {34, "Group Member Reduce:%d"},
        {0, ""}
    };

    _dump_v6stats( name, icmp6stats_oid, icmp6stats_len, icmp6stats_tbl );
    _dump_v6stats( "    Input Histogram",
                         icmp6stats_oid, icmp6stats_len, icmp6_inhistogram );
    _dump_v6stats( "    Output Histogram",
                         icmp6stats_oid, icmp6stats_len, icmp6_outhistogram );
}

/*
 * Dump ICMPv6 per-interface statistics - Omitted
 */


/*
 * Ommitted:
 *     Dump PIM statistics
 *     Dump raw ip6 statistics
 */



/*
 * Pretty print an Internet address (net address + port).
 * If the nflag was specified, use numbers instead of names.
 */

void
inet6print(unsigned char *in6, int port, const char *proto, int local)
{

#define GETSERVBYPORT6(port, proto, ret) do { \
	if (strcmp((proto), "tcp6") == 0) \
		(ret) = getservbyport((int)(port), "tcp"); \
	else if (strcmp((proto), "udp6") == 0) \
		(ret) = getservbyport((int)(port), "udp"); \
	else \
		(ret) = getservbyport((int)(port), (proto)); \
	} while (0)

	struct servent *sp = NULL;
	char line[80], *cp;
	unsigned width;
	int len = sizeof line;

	width = Aflag ? 12 : 16;
	if (vflag && width < strlen(inet6name(in6)))
		width = strlen(inet6name(in6));
	snprintf(line, len, "%.*s.", width, inet6name(in6));
	len -= strlen(line);
	if (len <= 0)
		goto bail;

	cp = strchr(line, '\0');
	if (!nflag && port && local)
		GETSERVBYPORT6(port, proto, sp);
	if (sp || port == 0)
		snprintf(cp, len, "%.8s", sp ? sp->s_name : "*");
	else
		snprintf(cp, len, "%d", ntohs((u_short)port));
	width = Aflag ? 18 : 22;
	if (vflag && width < strlen(line))
		width = strlen(line);
bail:
	printf(" %-*.*s", width, width, line);
}

/*
 * Construct an Internet address representation.
 * If the nflag has been supplied, give
 * numeric value, otherwise try for symbolic name.
 */

char *
inet6name(const unsigned char *in6)
{
	char *cp;
	static char line[NI_MAXHOST];
	static char domain[MAXHOSTNAMELEN];
	static int first = 1;
#ifdef NETSNMP_ENABLE_IPV6
	struct hostent *hp;
	char hbuf[NI_MAXHOST];
	const int niflag = NI_NUMERICHOST;
	struct sockaddr_in6 sin6;
	const struct in6_addr *in6p = (const struct in6_addr *)in6;
#endif

	if (first && !nflag) {
		first = 0;
		if (gethostname(domain, sizeof(domain)) == 0 &&
		    (cp = strchr(domain, '.')))
			(void) strlcpy(domain, cp + 1, sizeof domain);
		else
			domain[0] = '\0';
	}
#ifdef NETSNMP_ENABLE_IPV6
	cp = NULL;
	if (!nflag && !IN6_IS_ADDR_UNSPECIFIED(in6p)) {
		hp = netsnmp_gethostbyaddr((const char *)in6p, sizeof(*in6p),
                                           AF_INET6);
		if (hp) {
			if ((cp = strchr(hp->h_name, '.')) &&
			    !strcmp(cp + 1, domain))
				*cp = 0;
			cp = hp->h_name;
		}
	}
	if (IN6_IS_ADDR_UNSPECIFIED(in6p))
		strlcpy(line, "*", sizeof(line));
	else if (cp)
		strlcpy(line, cp, sizeof(line));
	else {
		memset(&sin6, 0, sizeof(sin6));
/*		sin6.sin6_len = sizeof(sin6);   */
		sin6.sin6_family = AF_INET6;
		sin6.sin6_addr = *in6p;
#ifdef __KAME__
		if (IN6_IS_ADDR_LINKLOCAL(in6p) ||
		    IN6_IS_ADDR_MC_LINKLOCAL(in6p)) {
			sin6.sin6_scope_id =
			    ntohs(*(const uint16_t *)&in6p->s6_addr[2]);
			sin6.sin6_addr.s6_addr[2] = 0;
			sin6.sin6_addr.s6_addr[3] = 0;
		}
#endif
		if (getnameinfo((struct sockaddr *)&sin6, sizeof(sin6),
		    hbuf, sizeof(hbuf), NULL, 0, niflag) != 0)
			strlcpy(hbuf, "?", sizeof hbuf);
		strlcpy(line, hbuf, sizeof(line));
	}
#else
	strlcpy(line, "[[XXX - inet6 address]]", sizeof(line));
#endif
	return (line);
}

#ifdef TCP6
/*
 * Dump the contents of a TCP6 PCB - Omitted
 */
#endif
