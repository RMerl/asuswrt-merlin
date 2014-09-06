/*	$OpenBSD: inet.c,v 1.92 2005/02/10 14:25:08 itojun Exp $	*/
/*	$NetBSD: inet.c,v 1.14 1995/10/03 21:42:37 thorpej Exp $	*/

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
static char sccsid[] = "from: @(#)inet.c	8.4 (Berkeley) 4/20/94";
#else
static const char *rcsid = "$OpenBSD: inet.c,v 1.92 2005/02/10 14:25:08 itojun Exp $";
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
    unsigned int  entry;      /* entry number in table */
    /*
     * format string to printf(description, value) 
     * warning: the %d must be before the %s 
     */
    char            description[80];
};

char	*inetname(struct in_addr *);
void	inetprint(struct in_addr *, int, const char *, int);

	/*
	 * Print a summary of connections related to
	 *   an Internet protocol (kread-based) - Omitted
	 */

/*
 * Print a summary of TCP connections
 * Listening processes are suppressed unless the
 *   -a (all) flag is specified.
 */
const char     *tcpstates[] = {
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

static void
tcpprotoprint_line(const char *name, netsnmp_variable_list *vp, int *first)
{
    int    state, width;
    char  *cp;
    union {
        struct in_addr addr;
        char      data[4];
    } tmpAddr;
    oid    localPort, remotePort;
    struct in_addr localAddr, remoteAddr;

	state = *vp->val.integer;
	if (!aflag && state == MIB_TCPCONNSTATE_LISTEN) {
		return;
	}

	if (*first) {
		printf("Active Internet (%s) Connections", name);
		if (aflag)
			printf(" (including servers)");
		putchar('\n');
		width = Aflag ? 18 : 22;
		printf("%-5.5s %*.*s %*.*s %s\n",
			   "Proto", -width, width, "Local Address",
						-width, width, "Remote Address", "(state)");
		*first = 0;
	}
	
	/* Extract the local/remote information from the index values */
	cp = tmpAddr.data;
	cp[0] = vp->name[ 10 ] & 0xff;
	cp[1] = vp->name[ 11 ] & 0xff;
	cp[2] = vp->name[ 12 ] & 0xff;
	cp[3] = vp->name[ 13 ] & 0xff;
	localAddr.s_addr = tmpAddr.addr.s_addr;
	localPort        = ntohs(vp->name[ 14 ]);
	cp = tmpAddr.data;
	cp[0] = vp->name[ 15 ] & 0xff;
	cp[1] = vp->name[ 16 ] & 0xff;
	cp[2] = vp->name[ 17 ] & 0xff;
	cp[3] = vp->name[ 18 ] & 0xff;
	remoteAddr.s_addr = tmpAddr.addr.s_addr;
	remotePort        = ntohs(vp->name[ 19 ]);

	printf("%-5.5s", name);
	inetprint(&localAddr,  localPort,  name, 1);
	inetprint(&remoteAddr, remotePort, name, 0);
	if (state < 1 || state > TCP_NSTATES) {
		printf("%d\n", state );
	} else {
		printf("%s\n", tcpstates[state]);
	}
}

static void
tcpprotopr_get(const char *name, oid *root, size_t root_len)
{
    netsnmp_variable_list *var, *vp;
    int    first = 1;

    /*
     * Walking the tcpConnState column will provide all
     *   the necessary information.
     */
    var = NULL;
    snmp_varlist_add_variable( &var, root, root_len,
                                   ASN_NULL, NULL,  0);
    if (!var)
        return;
    if (netsnmp_query_walk( var, ss ) != SNMP_ERR_NOERROR)
        return;

    for (vp = var; vp ; vp=vp->next_variable) {
        tcpprotoprint_line(name, vp, &first);
    }
    snmp_free_varbind( var );
}

/*
 * Print a summary of UDP "connections"
 *    XXX - what about "listening" services ??
 */
void
udpprotopr(const char *name)
{
    netsnmp_variable_list *var, *vp;
    oid    udpLocalAddress_oid[] = { 1,3,6,1,2,1,7,5,1,1 };
    size_t udpLocalAddress_len   = OID_LENGTH( udpLocalAddress_oid );
    union {
        struct in_addr addr;
        char      data[4];
    } tmpAddr;
    struct in_addr localAddr;
    oid    localPort;
    char  *cp;

    /*
     * Walking a single column of the udpTable will provide
     *   all the necessary information from the index values.
     */
    var = NULL;
    snmp_varlist_add_variable( &var, udpLocalAddress_oid, udpLocalAddress_len,
                                   ASN_NULL, NULL,  0);
    if (!var)
        return;
    if (netsnmp_query_walk( var, ss ) != SNMP_ERR_NOERROR)
        return;

    printf("Active Internet (%s) Connections\n", name);
    printf("%-5.5s %-28.28s\n", "Proto", "Local Address");
    for (vp = var; vp ; vp=vp->next_variable) {
        printf("%-5.5s", name);
        /*
         * Extract the local port from the index values, but take
         *   the IP address from the varbind value, (which is why
         *   we walked udpLocalAddress rather than udpLocalPort)
         */
        cp = tmpAddr.data;
        cp[0] = vp->name[ 10 ] & 0xff;
        cp[1] = vp->name[ 11 ] & 0xff;
        cp[2] = vp->name[ 12 ] & 0xff;
        cp[3] = vp->name[ 13 ] & 0xff;
        localAddr.s_addr = tmpAddr.addr.s_addr;
        localPort        = ntohs( (u_short)(vp->name[ 14 ]));
        inetprint(&localAddr, localPort, name, 1);
        putchar('\n');
    }
    snmp_free_varbind( var );
}

void
tcpprotopr_bulkget(const char *name, oid *root, size_t root_len)
{
    netsnmp_variable_list *vp;
	netsnmp_pdu    *pdu, *response;
    oid             tcpConnState_oid[MAX_OID_LEN];
	size_t          tcpConnState_len;
    int    first = 1;
    int    running = 1;
    int    status;

	/*
     * setup initial object name
     */
	memmove(tcpConnState_oid, root, sizeof(oid) * root_len);
	tcpConnState_len = root_len;

    /*
     * Walking the tcpConnState column will provide all
     *   the necessary information.
     */
	while (running) {
        /*
         * create PDU for GETBULK request and add object name to request
         */
        pdu = snmp_pdu_create(SNMP_MSG_GETBULK);
        pdu->non_repeaters = 0;
        pdu->max_repetitions = max_getbulk;    /* fill the packet */
        snmp_add_null_var(pdu, tcpConnState_oid, tcpConnState_len);

        /*
         * do the request
         */
        status = snmp_synch_response(ss, pdu, &response);
        if (status == STAT_SUCCESS) {
            if (response->errstat == SNMP_ERR_NOERROR) {
				for (vp = response->variables; vp ; vp=vp->next_variable) {
                    if ((vp->name_length < root_len) ||
							(memcmp(root, vp->name, sizeof(oid) * root_len) != 0)) {
                        /*
                         * not part of this subtree
                         */
                        running = 0;
                        continue;
                    }

					tcpprotoprint_line(name, vp, &first);

                    if ((vp->type != SNMP_ENDOFMIBVIEW) &&
                        (vp->type != SNMP_NOSUCHOBJECT) &&
                        (vp->type != SNMP_NOSUCHINSTANCE)) {
                        /*
                         * Check if last variable, and if so, save for next request.
                         */
                        if (vp->next_variable == NULL) {
                            memmove(tcpConnState_oid, vp->name,
                                    vp->name_length * sizeof(oid));
                            tcpConnState_len = vp->name_length;
                        }
                    } else {
                        /*
                         * an exception value, so stop
                         */
                        running = 0;
                    }
				}
            } else {
                /*
                 * error in response, print it
                 */
                running = 0;
            }
        } else if (status == STAT_TIMEOUT) {
            running = 0;
        } else {                /* status == STAT_ERROR */
            running = 0;
        }

        if (response) {
            snmp_free_pdu(response);
		}
	}
}

void
tcpprotopr(const char *name)
{
    oid    tcpConnState_oid[] = { 1,3,6,1,2,1,6,13,1,1 };
    size_t tcpConnState_len   = OID_LENGTH( tcpConnState_oid );
	int    use_getbulk = 1;

#ifndef NETSNMP_DISABLE_SNMPV1
    if (ss->version == SNMP_VERSION_1) {
        use_getbulk = 0;
	}
#endif

	if (use_getbulk) {
		tcpprotopr_bulkget(name, tcpConnState_oid, tcpConnState_len);
	} else {
		tcpprotopr_get(name, tcpConnState_oid, tcpConnState_len);
	}
}


	/*********************
	 *
	 *  Internet-protocol statistics
	 *
	 *********************/

void
_dump_stats( const char *name, oid *oid_buf, size_t buf_len,
             struct stat_table *stable )
{
    netsnmp_variable_list *var, *vp;
    struct stat_table     *sp;
    oid    stat;

    var = NULL;
    for (sp=stable; sp->entry; sp++) {
        oid_buf[buf_len-2] = sp->entry;
        snmp_varlist_add_variable( &var, oid_buf, buf_len,
                                   ASN_NULL, NULL,  0);
    }
 
    if (netsnmp_query_get( var, ss ) != SNMP_ERR_NOERROR) {
        /* Need to fix and re-try SNMPv1 errors */
        snmp_free_var( var );
        return;
    }

    printf("%s:\n", name);
    sp=stable;
    for (vp=var; vp; vp=vp->next_variable, sp++) {
        /*
         * Match the returned results against
         *   the original stats table.
         */
        stat =  vp->name[buf_len-2];
        while (sp->entry < stat) {
            sp++;
            if (sp->entry == 0)
                break;
        }
        if (sp->entry > stat)
            continue;

        /* Skip exceptions or missing values */
        if ( !vp->val.integer )
            continue;
        /*
         * If '-Cs' was specified twice,
         *   then only display non-zero stats.
         */
        if ( *vp->val.integer > 0 || sflag == 1 ) {
            putchar('\t');
            printf(sp->description, *vp->val.integer,
                             plural(*vp->val.integer));
            putchar('\n');
        }
    }
    snmp_free_varbind( var );
}


/*
 * Dump IP statistics.
 */
void
ip_stats(const char *name)
{
    oid               ipstats_oid[] = { 1, 3, 6, 1, 2, 1, 4, 0, 0 };
    size_t            ipstats_len   = OID_LENGTH( ipstats_oid );
    struct stat_table ipstats_tbl[] = {
        {3, "%d total datagram%s received"},
        {4, "%d datagram%s with header errors"},
        {5, "%d datagram%s with an invalid destination address"},
        {6, "%d datagram%s forwarded"},
        {7, "%d datagram%s with unknown protocol"},
        {8, "%d datagram%s discarded"},
        {9, "%d datagram%s delivered"},
        {10, "%d output datagram request%s"},
        {11, "%d output datagram%s discarded"},
        {12, "%d datagram%s with no route"},
        {14, "%d fragment%s received"},
        {15, "%d datagram%s reassembled"},
        {16, "%d reassembly failure%s"},
        {17, "%d datagram%s fragmented"},
        {18, "%d fragmentation failure%s"},
        {19, "%d fragment%s created"},
        {23, "%d route%s discarded"},
        {0, ""}
    };

    _dump_stats( name, ipstats_oid, ipstats_len, ipstats_tbl );
}


/*
 * Dump ICMP statistics.
 */
void
icmp_stats(const char *name)
{
    oid               icmpstats_oid[] = { 1, 3, 6, 1, 2, 1, 5, 0, 0 };
    size_t            icmpstats_len   = OID_LENGTH( icmpstats_oid );
    struct stat_table icmpstats_tbl[] = {
        {1, "%d total message%s received"},
        {2, "%d message%s dropped due to errors"},
        {14, "%d ouput message request%s"},
        {15, "%d output message%s discarded"},
        {0, ""}
    };
    struct stat_table icmp_inhistogram[] = {
        {3, "Destination unreachable: %d"},
        {4, "Time Exceeded: %d"},
        {5, "Parameter Problem: %d"},
        {6, "Source Quench: %d"},
        {7, "Redirect: %d"},
        {8, "Echo Request: %d"},
        {9, "Echo Reply: %d"},
        {10, "Timestamp Request: %d"},
        {11, "Timestamp Reply: %d"},
        {12, "Address Mask Request: %d"},
        {13, "Address Mask Reply: %d"},
        {0, ""}
    };
    struct stat_table icmp_outhistogram[] = {
        {16, "Destination unreachable: %d"},
        {17, "Time Exceeded: %d"},
        {18, "Parameter Problem: %d"},
        {19, "Source Quench: %d"},
        {20, "Redirect: %d"},
        {21, "Echo Request: %d"},
        {22, "Echo Reply: %d"},
        {23, "Timestamp Request: %d"},
        {24, "Timestamp Reply: %d"},
        {25, "Address Mask Request: %d"},
        {26, "Address Mask Reply: %d"},
        {0, ""}
    };

    _dump_stats( name, icmpstats_oid, icmpstats_len, icmpstats_tbl );
    _dump_stats( "    Input Histogram",
                       icmpstats_oid, icmpstats_len, icmp_inhistogram );
    _dump_stats( "    Output Histogram",
                       icmpstats_oid, icmpstats_len, icmp_outhistogram );
}


/*
 * Dump TCP statistics.
 */
void
tcp_stats(const char *name)
{
    oid               tcpstats_oid[] = { 1, 3, 6, 1, 2, 1, 6, 0, 0 };
    size_t            tcpstats_len   = OID_LENGTH( tcpstats_oid );
    struct stat_table tcpstats_tbl[] = {
        {5, "%d active open%s"},
        {6, "%d passive open%s"},
        {7, "%d failed attempt%s"},
        {8, "%d reset%s of established connections"},
        {9, "%d current established connection%s"},
        {10, "%d segment%s received"},
        {11, "%d segment%s sent"},
        {12, "%d segment%s retransmitted"},
        {14, "%d invalid segment%s received"},
        {15, "%d reset%s sent"},
        {0, ""}
    };
    _dump_stats( name, tcpstats_oid, tcpstats_len, tcpstats_tbl );
}


/*
 * Dump UDP statistics.
 */
void
udp_stats(const char *name)
{
    oid               udpstats_oid[] = { 1, 3, 6, 1, 2, 1, 7, 0, 0 };
    size_t            udpstats_len   = OID_LENGTH( udpstats_oid );
    struct stat_table udpstats_tbl[] = {
        {1, "%d total datagram%s received"},
        {2, "%d datagram%s to invalid port"},
        {3, "%d datagram%s dropped due to errors"},
        {4, "%d output datagram request%s"},
        {0, ""}
    };
    _dump_stats( name, udpstats_oid, udpstats_len, udpstats_tbl );
}


/*
 * Omitted:
 *     Dump IGMP     statistics
 *     Dump PIM      statistics
 *     Dump AH       statistics
 *     Dump etherip  statistics
 *     Dump ESP      statistics
 *     Dump IP-in-IP statistics
 *     Dump CARP     statistics
 *     Dump pfsync   statistics
 *     Dump IPCOMP   statistics
 */

	/*
	 * Utility routines
	 */

/*
 * Translation of RPC service names - Omitted
 */

/*
 * Pretty print an Internet address (net address + port).
 * If the nflag was specified, use numbers instead of names.
 */
void
inetprint(struct in_addr *in, int port, const char *proto, int local)
{
	struct servent *sp = NULL;
	char line[80], *cp;
	int width;

	snprintf(line, sizeof line, "%.*s.", (Aflag && !nflag) ? 12 : 16,
	    inetname(in));
	cp = strchr(line, '\0');
	if (!nflag && port)
		sp = getservbyport((int)port, proto);
	if (sp || port == 0)
		snprintf(cp, line + sizeof line - cp, "%.8s",
		    sp ? sp->s_name : "*");
     /*
      * Translation of RPC service names - Omitted
      */
	else
		snprintf(cp, line + sizeof line - cp, "%d", ntohs(port));
	width = Aflag ? 18 : 22;
	printf(" %-*.*s", width, width, line);
}

/*
 * Construct an Internet address representation.
 * If the nflag has been supplied, give
 * numeric value, otherwise try for symbolic name.
 */
char *
inetname(struct in_addr *inp)
{
	char *cp;
	static char line[50];
	struct hostent *hp;
	struct netent *np;
	static char domain[MAXHOSTNAMELEN];
	static int first = 1;
#if defined (WIN32) || defined (cygwin)
        char host_temp[] = "localhost";
#endif

	if (first && !nflag) {
		first = 0;
		if (gethostname(domain, sizeof(domain)) == 0 &&
		    (cp = strchr(domain, '.')))
			(void) strlcpy(domain, cp + 1, sizeof domain);
		else
			domain[0] = '\0';
	}
	cp = NULL;
	if (!nflag && inp->s_addr != INADDR_ANY) {
		int net = inet_netof(*inp);
		int lna = inet_lnaof(*inp);

		if (lna == INADDR_ANY) {
			np = getnetbyaddr(net, AF_INET);
			if (np)
				cp = np->n_name;
		}
		if (cp == NULL) {
			hp = netsnmp_gethostbyaddr((char *)inp, sizeof (*inp),
                                                   AF_INET);
			if (hp) {
				if ((cp = strchr(hp->h_name, '.')) &&
				    !strcmp(cp + 1, domain))
					*cp = '\0';
#if defined (WIN32) || defined (cygwin)
                                        /* Windows insists on returning the computer name for 127.0.0.1
                                         * even if the hosts file lists something else such as 'localhost'.
                                         * If we are trying to look up 127.0.0.1, just return 'localhost'   */
                                        if (!strcmp(inet_ntoa(*inp),"127.0.0.1"))
                                             cp = host_temp;
                                        else
#endif                                                                          
				cp = hp->h_name;
			}
		}
	}
	if (inp->s_addr == INADDR_ANY)
		snprintf(line, sizeof line, "*");
	else if (cp)
		snprintf(line, sizeof line, "%s", cp);
	else {
		inp->s_addr = ntohl(inp->s_addr);
#define C(x)	(unsigned)((x) & 0xff)
		snprintf(line, sizeof line, "%u.%u.%u.%u",
		    C(inp->s_addr >> 24), C(inp->s_addr >> 16),
		    C(inp->s_addr >> 8), C(inp->s_addr));
	}
	return (line);
}
