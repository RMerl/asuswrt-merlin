/* Portions of this file are subject to the following copyright(s).  See
 * the Net-SNMP's COPYING file for more details and other copyrights
 * that may apply:
 */
/*
 * Portions of this file are copyrighted by:
 * Copyright Copyright 2003 Sun Microsystems, Inc. All rights reserved.
 * Use is subject to license terms specified in the COPYING file
 * distributed with the Net-SNMP package.
 */

#include <net-snmp/net-snmp-config.h>

#include <net-snmp/types.h>
#include <net-snmp/library/snmpUDPDomain.h>
#include <net-snmp/library/snmpUDPIPv4BaseDomain.h>

#include <stddef.h>
#include <stdio.h>
#include <sys/types.h>
#include <ctype.h>
#include <errno.h>

#if HAVE_STRING_H
#include <string.h>
#else
#include <strings.h>
#endif
#if HAVE_STDLIB_H
#include <stdlib.h>
#endif
#if HAVE_UNISTD_H
#include <unistd.h>
#endif
#if HAVE_SYS_SOCKET_H
#include <sys/socket.h>
#endif
#if HAVE_NETINET_IN_H
#include <netinet/in.h>
#endif
#if HAVE_ARPA_INET_H
#include <arpa/inet.h>
#endif
#if HAVE_NETDB_H
#include <netdb.h>
#endif
#if HAVE_SYS_UIO_H
#include <sys/uio.h>
#endif

#if HAVE_DMALLOC_H
#include <dmalloc.h>
#endif

#include <net-snmp/types.h>
#include <net-snmp/output_api.h>
#include <net-snmp/config_api.h>

#include <net-snmp/library/snmp_transport.h>
#include <net-snmp/library/snmpSocketBaseDomain.h>
#include <net-snmp/library/system.h>
#include <net-snmp/library/tools.h>

#include "inet_ntop.h"
#include "inet_pton.h"

#ifndef INADDR_NONE
#define INADDR_NONE	-1
#endif

#ifndef INET_ADDRSTRLEN
#define INET_ADDRSTRLEN 16
#endif

static netsnmp_tdomain udpDomain;

/*
 * needs to be in sync with the definitions in snmplib/snmpTCPDomain.c 
 * and perl/agent/agent.xs 
 */
typedef netsnmp_indexed_addr_pair netsnmp_udp_addr_pair;

/*
 * not static, since snmpUDPIPv6Domain needs it, but not public, either.
 * (ie don't put it in a public header.)
 */
void _netsnmp_udp_sockopt_set(int fd, int server);
int
netsnmp_sockaddr_in2(struct sockaddr_in *addr,
                     const char *inpeername, const char *default_target);

/*
 * Return a string representing the address in data, or else the "far end"
 * address if data is NULL.  
 */

char *
netsnmp_udp_fmtaddr(netsnmp_transport *t, void *data, int len)
{
    return netsnmp_ipv4_fmtaddr("UDP", t, data, len);
}



#if (defined(linux) && defined(IP_PKTINFO)) \
    || defined(IP_RECVDSTADDR) && HAVE_STRUCT_MSGHDR_MSG_CONTROL \
                               && HAVE_STRUCT_MSGHDR_MSG_FLAGS

int netsnmp_udp_recvfrom(int s, void *buf, int len, struct sockaddr *from, socklen_t *fromlen, struct sockaddr *dstip, socklen_t *dstlen, int *if_index)
{
    /** udpiv4 just calls udpbase. should we skip directly to there? */
    return netsnmp_udpipv4_recvfrom(s, buf, len, from, fromlen, dstip, dstlen,
                                    if_index);
}

int netsnmp_udp_sendto(int fd, struct in_addr *srcip, int if_index, struct sockaddr *remote,
                void *data, int len)
{
    /** udpiv4 just calls udpbase. should we skip directly to there? */
    return netsnmp_udpipv4_sendto(fd, srcip, if_index, remote, data, len);
}
#endif /* (linux && IP_PKTINFO) || IP_RECVDSTADDR */

/*
 * Open a UDP-based transport for SNMP.  Local is TRUE if addr is the local
 * address to bind to (i.e. this is a server-type session); otherwise addr is 
 * the remote address to send things to.  
 */

netsnmp_transport *
netsnmp_udp_transport(struct sockaddr_in *addr, int local)
{
    netsnmp_transport *t = NULL;

    t = netsnmp_udpipv4base_transport(addr, local);
    if (NULL == t) {
        return NULL;
    }

    /*
     * Set Domain
     */

    t->domain = netsnmpUDPDomain;
    t->domain_length = netsnmpUDPDomain_len;

    /*
     * 16-bit length field, 8 byte UDP header, 20 byte IPv4 header  
     */

    t->msgMaxSize = 0xffff - 8 - 20;
    t->f_recv     = netsnmp_udpbase_recv;
    t->f_send     = netsnmp_udpbase_send;
    t->f_close    = netsnmp_socketbase_close;
    t->f_accept   = NULL;
    t->f_fmtaddr  = netsnmp_udp_fmtaddr;

    return t;
}

#if !defined(NETSNMP_DISABLE_SNMPV1) || !defined(NETSNMP_DISABLE_SNMPV2C)
/*
 * The following functions provide the "com2sec" configuration token
 * functionality for compatibility.
 */

#define EXAMPLE_NETWORK		"NETWORK"
#define EXAMPLE_COMMUNITY	"COMMUNITY"

typedef struct com2SecEntry_s {
    const char *secName;
    const char *contextName;
    struct com2SecEntry_s *next;
    in_addr_t   network;
    in_addr_t   mask;
    const char  community[1];
} com2SecEntry;

static com2SecEntry   *com2SecList = NULL, *com2SecListLast = NULL;

void
netsnmp_udp_parse_security(const char *token, char *param)
{
    char            secName[VACMSTRINGLEN + 1];
    size_t          secNameLen;
    char            contextName[VACMSTRINGLEN + 1];
    size_t          contextNameLen;
    char            community[COMMUNITY_MAX_LEN + 1];
    size_t          communityLen;
    char            source[270]; /* dns-name(253)+/(1)+mask(15)+\0(1) */
    struct in_addr  network, mask;

    /*
     * Get security, source address/netmask and community strings.
     */

    param = copy_nword( param, secName, sizeof(secName));
    if (strcmp(secName, "-Cn") == 0) {
        if (!param) {
            config_perror("missing CONTEXT_NAME parameter");
            return;
        }
        param = copy_nword( param, contextName, sizeof(contextName));
        contextNameLen = strlen(contextName) + 1;
        if (contextNameLen > VACMSTRINGLEN) {
            config_perror("context name too long");
            return;
        }
        if (!param) {
            config_perror("missing NAME parameter");
            return;
        }
        param = copy_nword( param, secName, sizeof(secName));
    } else {
        contextNameLen = 0;
    }

    secNameLen = strlen(secName) + 1;
    if (secNameLen == 1) {
        config_perror("empty NAME parameter");
        return;
    } else if (secNameLen > VACMSTRINGLEN) {
        config_perror("security name too long");
        return;
    }

    if (!param) {
        config_perror("missing SOURCE parameter");
        return;
    }
    param = copy_nword( param, source, sizeof(source));
    if (source[0] == '\0') {
        config_perror("empty SOURCE parameter");
        return;
    }
    if (strncmp(source, EXAMPLE_NETWORK, strlen(EXAMPLE_NETWORK)) == 0) {
        config_perror("example config NETWORK not properly configured");
        return;
    }

    if (!param) {
        config_perror("missing COMMUNITY parameter");
        return;
    }
    param = copy_nword( param, community, sizeof(community));
    if (community[0] == '\0') {
        config_perror("empty COMMUNITY parameter");
        return;
    }
    communityLen = strlen(community) + 1;
    if (communityLen >= COMMUNITY_MAX_LEN) {
        config_perror("community name too long");
        return;
    }
    if (communityLen == sizeof(EXAMPLE_COMMUNITY) &&
        memcmp(community, EXAMPLE_COMMUNITY, sizeof(EXAMPLE_COMMUNITY)) == 0) {
        config_perror("example config COMMUNITY not properly configured");
        return;
    }

    /* Deal with the "default" case first. */
    if (strcmp(source, "default") == 0) {
        network.s_addr = 0;
        mask.s_addr = 0;
    } else {
        /* Split the source/netmask parts */
        char *strmask = strchr(source, '/');
        if (strmask != NULL)
            /* Mask given. */
            *strmask++ = '\0';

        /* Try interpreting as a dotted quad. */
        if (inet_pton(AF_INET, source, &network) == 0) {
            /* Nope, wasn't a dotted quad.  Must be a hostname. */
            int ret = netsnmp_gethostbyname_v4(source, &network.s_addr);
            if (ret < 0) {
                config_perror("cannot resolve source hostname");
                return;
            }
        }

        /* Now work out the mask. */
        if (strmask == NULL || *strmask == '\0') {
            /* No mask was given. Assume /32 */
            mask.s_addr = (in_addr_t)(~0UL);
        } else {
            /* Try to interpret mask as a "number of 1 bits". */
            char* cp;
            long maskLen = strtol(strmask, &cp, 10);
            if (*cp == '\0') {
                if (0 < maskLen && maskLen <= 32)
                    mask.s_addr = htonl((in_addr_t)(~0UL << (32 - maskLen)));
                else if (maskLen == 0)
                    mask.s_addr = 0;
                else {
                    config_perror("bad mask length");
                    return;
                }
            }
            /* Try to interpret mask as a dotted quad. */
            else if (inet_pton(AF_INET, strmask, &mask) == 0) {
                config_perror("bad mask");
                return;
            }

            /* Check that the network and mask are consistent. */
            if (network.s_addr & ~mask.s_addr) {
                config_perror("source/mask mismatch");
                return;
            }
        }
    }

    {
        void* v = malloc(offsetof(com2SecEntry, community) + communityLen +
                         secNameLen + contextNameLen);

        com2SecEntry* e = (com2SecEntry*)v;
        char* last = ((char*)v) + offsetof(com2SecEntry, community);

        if (v == NULL) {
            config_perror("memory error");
            return;
        }

        /*
         * Everything is okay.  Copy the parameters to the structure allocated
         * above and add it to END of the list.
         */

        {
          char buf1[INET_ADDRSTRLEN];
          char buf2[INET_ADDRSTRLEN];
          DEBUGMSGTL(("netsnmp_udp_parse_security",
                      "<\"%s\", %s/%s> => \"%s\"\n", community,
                      inet_ntop(AF_INET, &network, buf1, sizeof(buf1)),
                      inet_ntop(AF_INET, &mask, buf2, sizeof(buf2)),
                      secName));
        }

        memcpy(last, community, communityLen);
        last += communityLen;
        memcpy(last, secName, secNameLen);
        e->secName = last;
        last += secNameLen;
        if (contextNameLen) {
            memcpy(last, contextName, contextNameLen);
            e->contextName = last;
        } else
            e->contextName = last - 1;
        e->network = network.s_addr;
        e->mask = mask.s_addr;
        e->next = NULL;

        if (com2SecListLast != NULL) {
            com2SecListLast->next = e;
            com2SecListLast = e;
        } else {
            com2SecListLast = com2SecList = e;
        }
    }
}

void
netsnmp_udp_com2SecList_free(void)
{
    com2SecEntry   *e = com2SecList;
    while (e != NULL) {
        com2SecEntry   *tmp = e;
        e = e->next;
        free(tmp);
    }
    com2SecList = com2SecListLast = NULL;
}
#endif /* support for community based SNMP */

void
netsnmp_udp_agent_config_tokens_register(void)
{
#if !defined(NETSNMP_DISABLE_SNMPV1) || !defined(NETSNMP_DISABLE_SNMPV2C)
    register_app_config_handler("com2sec", netsnmp_udp_parse_security,
                                netsnmp_udp_com2SecList_free,
                                "[-Cn CONTEXT] secName IPv4-network-address[/netmask] community");
#endif /* support for community based SNMP */
}



/*
 * Return 0 if there are no com2sec entries, or return 1 if there ARE com2sec
 * entries.  On return, if a com2sec entry matched the passed parameters,
 * then *secName points at the appropriate security name, or is NULL if the
 * parameters did not match any com2sec entry.
 */

#if !defined(NETSNMP_DISABLE_SNMPV1) || !defined(NETSNMP_DISABLE_SNMPV2C)
int
netsnmp_udp_getSecName(void *opaque, int olength,
                       const char *community,
                       size_t community_len, const char **secName,
                       const char **contextName)
{
    const com2SecEntry *c;
    netsnmp_udp_addr_pair *addr_pair = (netsnmp_udp_addr_pair *) opaque;
    struct sockaddr_in *from = (struct sockaddr_in *) &(addr_pair->remote_addr);
    char           *ztcommunity = NULL;

    if (secName != NULL) {
        *secName = NULL;  /* Haven't found anything yet */
    }

    /*
     * Special case if there are NO entries (as opposed to no MATCHING
     * entries).
     */

    if (com2SecList == NULL) {
        DEBUGMSGTL(("netsnmp_udp_getSecName", "no com2sec entries\n"));
        return 0;
    }

    /*
     * If there is no IPv4 source address, then there can be no valid security
     * name.
     */

   DEBUGMSGTL(("netsnmp_udp_getSecName", "opaque = %p (len = %d), sizeof = %d, family = %d (%d)\n",
   opaque, olength, (int)sizeof(netsnmp_udp_addr_pair), from->sin_family, AF_INET));
    if (opaque == NULL || olength != sizeof(netsnmp_udp_addr_pair) ||
        from->sin_family != AF_INET) {
        DEBUGMSGTL(("netsnmp_udp_getSecName",
		    "no IPv4 source address in PDU?\n"));
        return 1;
    }

    DEBUGIF("netsnmp_udp_getSecName") {
	ztcommunity = (char *)malloc(community_len + 1);
	if (ztcommunity != NULL) {
	    memcpy(ztcommunity, community, community_len);
	    ztcommunity[community_len] = '\0';
	}

	DEBUGMSGTL(("netsnmp_udp_getSecName", "resolve <\"%s\", 0x%08lx>\n",
		    ztcommunity ? ztcommunity : "<malloc error>",
		    (unsigned long)(from->sin_addr.s_addr)));
    }

    for (c = com2SecList; c != NULL; c = c->next) {
        {
            char buf1[INET_ADDRSTRLEN];
            char buf2[INET_ADDRSTRLEN];
            DEBUGMSGTL(("netsnmp_udp_getSecName","compare <\"%s\", %s/%s>",
                        c->community,
                        inet_ntop(AF_INET, &c->network, buf1, sizeof(buf1)),
                        inet_ntop(AF_INET, &c->mask, buf2, sizeof(buf2))));
        }
        if ((community_len == strlen(c->community)) &&
	    (memcmp(community, c->community, community_len) == 0) &&
            ((from->sin_addr.s_addr & c->mask) == c->network)) {
            DEBUGMSG(("netsnmp_udp_getSecName", "... SUCCESS\n"));
            if (secName != NULL) {
                *secName = c->secName;
                *contextName = c->contextName;
            }
            break;
        }
        DEBUGMSG(("netsnmp_udp_getSecName", "... nope\n"));
    }
    if (ztcommunity != NULL) {
        free(ztcommunity);
    }
    return 1;
}
#endif /* support for community based SNMP */


netsnmp_transport *
netsnmp_udp_create_tstring(const char *str, int local,
			   const char *default_target)
{
    struct sockaddr_in addr;

    if (netsnmp_sockaddr_in2(&addr, str, default_target)) {
        return netsnmp_udp_transport(&addr, local);
    } else {
        return NULL;
    }
}


netsnmp_transport *
netsnmp_udp_create_ostring(const u_char * o, size_t o_len, int local)
{
    struct sockaddr_in addr;

    if (o_len == 6) {
        unsigned short porttmp = (o[4] << 8) + o[5];
        addr.sin_family = AF_INET;
        memcpy((u_char *) & (addr.sin_addr.s_addr), o, 4);
        addr.sin_port = htons(porttmp);
        return netsnmp_udp_transport(&addr, local);
    }
    return NULL;
}


void
netsnmp_udp_ctor(void)
{
    udpDomain.name = netsnmpUDPDomain;
    udpDomain.name_length = netsnmpUDPDomain_len;
    udpDomain.prefix = (const char**)calloc(2, sizeof(char *));
    udpDomain.prefix[0] = "udp";

    udpDomain.f_create_from_tstring     = NULL;
    udpDomain.f_create_from_tstring_new = netsnmp_udp_create_tstring;
    udpDomain.f_create_from_ostring     = netsnmp_udp_create_ostring;

    netsnmp_tdomain_register(&udpDomain);
}
