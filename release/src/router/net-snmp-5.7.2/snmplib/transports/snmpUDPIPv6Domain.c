#include <net-snmp/net-snmp-config.h>

#include <net-snmp/library/snmpUDPIPv6Domain.h>
#include <net-snmp/library/system.h>

#include <net-snmp/types.h>

#ifdef NETSNMP_TRANSPORT_UDPIPV6_DOMAIN

#include <stdio.h>
#include <sys/types.h>
#include <ctype.h>
#include <errno.h>

#if HAVE_STRING_H
#include <string.h>
#else
#include <strings.h>
#endif
#include <stddef.h>
#if HAVE_STDLIB_H
#include <stdlib.h>
#endif
#if HAVE_UNISTD_H
#include <unistd.h>
#endif
#if HAVE_SYS_SOCKET_H
#include <sys/socket.h>
#endif

#if defined(HAVE_WINSOCK_H) && !defined(mingw32)
static const struct in6_addr in6addr_any = IN6ADDR_ANY_INIT;
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
#if HAVE_NET_IF_H
#include <net/if.h>
#endif

#if HAVE_DMALLOC_H
#include <dmalloc.h>
#endif

#if HAVE_STRUCT_SOCKADDR_STORAGE_SS_FAMILY
#define SS_FAMILY ss_family
#elif HAVE_STRUCT_SOCKADDR_STORAGE___SS_FAMILY
#define SS_FAMILY __ss_family
#endif

#if defined(darwin)
#include <stdint.h> /* for uint8_t */
#endif

#include <net-snmp/types.h>
#include <net-snmp/output_api.h>
#include <net-snmp/config_api.h>

#include <net-snmp/library/snmp_transport.h>
#include <net-snmp/library/snmpSocketBaseDomain.h>
#include <net-snmp/library/tools.h>

#include "inet_ntop.h"
#include "inet_pton.h"

oid netsnmp_UDPIPv6Domain[] = { TRANSPORT_DOMAIN_UDP_IPV6 };
static netsnmp_tdomain udp6Domain;

/*
 * from snmpUDPDomain. not static, but not public, either.
 * (ie don't put it in a public header.)
 */
extern void _netsnmp_udp_sockopt_set(int fd, int server);

/*
 * Return a string representing the address in data, or else the "far end"
 * address if data is NULL.  
 */

static char *
netsnmp_udp6_fmtaddr(netsnmp_transport *t, void *data, int len)
{
    return netsnmp_ipv6_fmtaddr("UDP/IPv6", t, data, len);
}



/*
 * You can write something into opaque that will subsequently get passed back 
 * to your send function if you like.  For instance, you might want to
 * remember where a PDU came from, so that you can send a reply there...  
 */

static int
netsnmp_udp6_recv(netsnmp_transport *t, void *buf, int size,
		  void **opaque, int *olength)
{
    int             rc = -1;
    socklen_t       fromlen = sizeof(struct sockaddr_in6);
    struct sockaddr *from;

    if (t != NULL && t->sock >= 0) {
        from = (struct sockaddr *) malloc(sizeof(struct sockaddr_in6));
        if (from == NULL) {
            *opaque = NULL;
            *olength = 0;
            return -1;
        } else {
            memset(from, 0, fromlen);
        }

	while (rc < 0) {
	  rc = recvfrom(t->sock, buf, size, 0, from, &fromlen);
	  if (rc < 0 && errno != EINTR) {
	    break;
	  }
	}

        if (rc >= 0) {
            DEBUGIF("netsnmp_udp6") {
                char *str = netsnmp_udp6_fmtaddr(NULL, from, fromlen);
                DEBUGMSGTL(("netsnmp_udp6",
                            "recvfrom fd %d got %d bytes (from %s)\n", t->sock,
                            rc, str));
                free(str);
            }
        } else {
            DEBUGMSGTL(("netsnmp_udp6", "recvfrom fd %d err %d (\"%s\")\n",
			t->sock, errno, strerror(errno)));
        }
        *opaque = (void *) from;
        *olength = sizeof(struct sockaddr_in6);
    }
    return rc;
}



static int
netsnmp_udp6_send(netsnmp_transport *t, void *buf, int size,
		  void **opaque, int *olength)
{
    int rc = -1;
    struct sockaddr *to = NULL;

    if (opaque != NULL && *opaque != NULL &&
        *olength == sizeof(struct sockaddr_in6)) {
        to = (struct sockaddr *) (*opaque);
    } else if (t != NULL && t->data != NULL &&
               ((t->data_length == sizeof(struct sockaddr_in6)) ||
                (t->data_length == sizeof(netsnmp_indexed_addr_pair)))) {
        to = (struct sockaddr *) (t->data);
    }

    if (to != NULL && t != NULL && t->sock >= 0) {
        DEBUGIF("netsnmp_udp6") {
            char *str = netsnmp_udp6_fmtaddr(NULL, (void *)to,
                                             sizeof(struct sockaddr_in6));
            DEBUGMSGTL(("netsnmp_udp6",
                        "send %d bytes from %p to %s on fd %d\n",
                        size, buf, str, t->sock));
            free(str);
        }
	while (rc < 0) {
	    rc = sendto(t->sock, buf, size, 0, to,sizeof(struct sockaddr_in6));
	    if (rc < 0 && errno != EINTR) {
		break;
	    }
	}
    }
    return rc;
}


/*
 * Open a UDP/IPv6-based transport for SNMP.  Local is TRUE if addr is the
 * local address to bind to (i.e. this is a server-type session); otherwise
 * addr is the remote address to send things to.  
 */

netsnmp_transport *
netsnmp_udp6_transport(struct sockaddr_in6 *addr, int local)
{
    netsnmp_transport *t = NULL;
    int             rc = 0;

#ifdef NETSNMP_NO_LISTEN_SUPPORT
    if (local)
        return NULL;
#endif /* NETSNMP_NO_LISTEN_SUPPORT */

    if (addr == NULL || addr->sin6_family != AF_INET6) {
        return NULL;
    }

    t = SNMP_MALLOC_TYPEDEF(netsnmp_transport);
    if (t == NULL) {
        return NULL;
    }

    DEBUGIF("netsnmp_udp6") {
        char *str = netsnmp_udp6_fmtaddr(NULL, (void *) addr,
                                         sizeof(struct sockaddr_in6));
        DEBUGMSGTL(("netsnmp_udp6", "open %s %s\n", local ? "local" : "remote",
                    str));
        free(str);
    }

    t->domain = netsnmp_UDPIPv6Domain;
    t->domain_length =
        sizeof(netsnmp_UDPIPv6Domain) / sizeof(netsnmp_UDPIPv6Domain[0]);

    t->sock = socket(PF_INET6, SOCK_DGRAM, 0);
    if (t->sock < 0) {
        netsnmp_transport_free(t);
        return NULL;
    }

    _netsnmp_udp_sockopt_set(t->sock, local);

    if (local) {
#ifndef NETSNMP_NO_LISTEN_SUPPORT
        /*
         * This session is intended as a server, so we must bind on to the
         * given IP address, which may include an interface address, or could
         * be INADDR_ANY, but certainly includes a port number.
         */

#ifdef IPV6_V6ONLY
        /* Try to restrict PF_INET6 socket to IPv6 communications only. */
        {
	  int one=1;
	  if (setsockopt(t->sock, IPPROTO_IPV6, IPV6_V6ONLY, (char *)&one, sizeof(one)) != 0) {
	    DEBUGMSGTL(("netsnmp_udp6", "couldn't set IPV6_V6ONLY to %d bytes: %s\n", one, strerror(errno)));
	  } 
	}
#endif

        rc = bind(t->sock, (struct sockaddr *) addr,
		  sizeof(struct sockaddr_in6));
        if (rc != 0) {
            netsnmp_socketbase_close(t);
            netsnmp_transport_free(t);
            return NULL;
        }
        t->local = (unsigned char*)malloc(18);
        if (t->local == NULL) {
            netsnmp_socketbase_close(t);
            netsnmp_transport_free(t);
            return NULL;
        }
        memcpy(t->local, addr->sin6_addr.s6_addr, 16);
        t->local[16] = (addr->sin6_port & 0xff00) >> 8;
        t->local[17] = (addr->sin6_port & 0x00ff) >> 0;
        t->local_length = 18;
        t->data = NULL;
        t->data_length = 0;
#else /* NETSNMP_NO_LISTEN_SUPPORT */
        return NULL;
#endif /* NETSNMP_NO_LISTEN_SUPPORT */
    } else {
        /*
         * This is a client session.  Save the address in the
         * transport-specific data pointer for later use by netsnmp_udp6_send.
         */

        t->data = malloc(sizeof(netsnmp_indexed_addr_pair));
        if (t->data == NULL) {
            netsnmp_socketbase_close(t);
            netsnmp_transport_free(t);
            return NULL;
        }
        memcpy(t->data, addr, sizeof(struct sockaddr_in6));
        t->data_length = sizeof(netsnmp_indexed_addr_pair);
        t->remote = (unsigned char*)malloc(18);
        if (t->remote == NULL) {
            netsnmp_socketbase_close(t);
            netsnmp_transport_free(t);
            return NULL;
        }
        memcpy(t->remote, addr->sin6_addr.s6_addr, 16);
        t->remote[16] = (addr->sin6_port & 0xff00) >> 8;
        t->remote[17] = (addr->sin6_port & 0x00ff) >> 0;
        t->remote_length = 18;
    }

    /*
     * 16-bit length field, 8 byte UDP header, 40 byte IPv6 header.  
     */

    t->msgMaxSize = 0xffff - 8 - 40;
    t->f_recv     = netsnmp_udp6_recv;
    t->f_send     = netsnmp_udp6_send;
    t->f_close    = netsnmp_socketbase_close;
    t->f_accept   = NULL;
    t->f_fmtaddr  = netsnmp_udp6_fmtaddr;

    return t;
}


#if !defined(NETSNMP_DISABLE_SNMPV1) || !defined(NETSNMP_DISABLE_SNMPV2C)
/*
 * The following functions provide the "com2sec6" configuration token
 * functionality for compatibility.
 */

#define EXAMPLE_NETWORK       "NETWORK"
#define EXAMPLE_COMMUNITY     "COMMUNITY"

typedef struct com2Sec6Entry_s {
    const char     *secName;
    const char     *contextName;
    struct com2Sec6Entry_s *next;
    struct in6_addr network;
    struct in6_addr mask;
    const char      community[1];
} com2Sec6Entry;

static com2Sec6Entry  *com2Sec6List = NULL, *com2Sec6ListLast = NULL;


NETSNMP_STATIC_INLINE int
create_com2Sec6Entry(const struct addrinfo* const run,
                     const struct in6_addr* const mask,
                     const char* const secName,
                     const size_t secNameLen,
                     const char* const contextName,
                     const size_t contextNameLen,
                     const char* const community,
                     const size_t communityLen,
                     com2Sec6Entry** const begin,
                     com2Sec6Entry** const end)
{
    const struct sockaddr_in6 * const run_addr =
        (const struct sockaddr_in6*)run->ai_addr;
    int i;

    /* Check that the network and mask are consistent. */
    for (i = 0; i < 16; ++i) {
        if (run_addr->sin6_addr.s6_addr[i] & ~mask->s6_addr[i]) {
            config_perror("source/mask mismatch");
            return 1;
        }
    }

    {
        char buf1[INET6_ADDRSTRLEN];
        char buf2[INET6_ADDRSTRLEN];
        DEBUGMSGTL(("netsnmp_udp6_parse_security",
                    "<\"%s\", %s/%s> => \"%s\"\n",
                    community,
                    inet_ntop(AF_INET6, &run_addr->sin6_addr,
                              buf1, sizeof(buf1)),
                    inet_ntop(AF_INET6, mask, buf2, sizeof(buf2)),
                    secName));
    }

    {
        /* Allocate all the needed chunks */
        void * const v =
            malloc(offsetof(com2Sec6Entry, community) + communityLen +
                   secNameLen + contextNameLen);

        com2Sec6Entry* const e = (com2Sec6Entry*)v;
        char *last = ((char*)v) + offsetof(com2Sec6Entry, community);

        if (v == NULL) {
            config_perror("memory error");
            return 1;
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

        memcpy(&e->network, &run_addr->sin6_addr, sizeof(struct in6_addr));
        memcpy(&e->mask, mask, sizeof(struct in6_addr));

        e->next = NULL;
        if (*end != NULL) {
            (*end)->next = e;
            *end = e;
        } else {
            *end = *begin = e;
        }
    }
    return 0;
}

void
netsnmp_udp6_parse_security(const char *token, char *param)
{
    char            secName[VACMSTRINGLEN + 1];
    size_t          secNameLen;
    char            contextName[VACMSTRINGLEN + 1];
    size_t          contextNameLen;
    char            community[COMMUNITY_MAX_LEN + 1];
    size_t          communityLen;
    char            source[300]; /* dns-name(253)+/(1)+mask(45)+\0(1) */
    struct in6_addr mask;

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

    /* Possible mask cases
     * "default" <=> 0::0/0
     * <hostname>[/] <=> <hostname>/128
     * <hostname>/number <=> <hostname>/number
     * <hostname>/<mask> <=> <hostname>/<mask>
     */
    {
        /* Deal with the "default" case first. */
        const int isdefault = strcmp(source, "default") == 0;

        if (isdefault) {
            memset(mask.s6_addr, '\0', sizeof(mask.s6_addr));
        } else {
            /* Split the source/netmask parts */
            char *strmask = strchr(source, '/');
            if (strmask != NULL)
                /* Mask given. */
                *strmask++ = '\0';

            /* Try to interpret the mask */
            if (strmask == NULL || *strmask == '\0') {
                /* No mask was given. Assume /128 */
                memset(mask.s6_addr, '\xff', sizeof(mask.s6_addr));
            } else {
                /* Try to interpret mask as a "number of 1 bits". */
                char* cp;
                long masklength = strtol(strmask, &cp, 10);
                if (*cp == '\0') {
                    if (0 <= masklength && masklength <= 128) {
                        const int j = masklength / 8;
                        const int jj = masklength % 8;
                        memset(mask.s6_addr, '\xff', j);
                        if (j < 16) {
                            mask.s6_addr[j] = ((uint8_t)~0U << (8 - jj));
                            memset(mask.s6_addr + j + 1, '\0', 15 - j);
                        }
                    } else {
                        config_perror("bad mask length");
                        return;
                    }
                }
                /* Try to interpret mask numerically. */
                else if (inet_pton(AF_INET6, strmask, &mask) != 1) {
                    config_perror("bad mask");
                    return;
                }
            }
        }

        {
            struct sockaddr_in6 pton_addr;
            struct addrinfo hints, *res = NULL;
            memset(&hints, '\0', sizeof(hints));

            /* First check if default, otherwise try to parse as a numeric
             * address, if that also fails try to lookup the address */
            if (isdefault) {
                memset(&pton_addr.sin6_addr.s6_addr, '\0',
                       sizeof(struct in6_addr));
            } else if (inet_pton(AF_INET6, source, &pton_addr.sin6_addr) != 1) {
                /* Nope, wasn't a numeric address. Must be a hostname. */
#if HAVE_GETADDRINFO
                int             gai_error;

                hints.ai_family = AF_INET6;
                hints.ai_socktype = SOCK_DGRAM;
                gai_error = netsnmp_getaddrinfo(source, NULL, &hints, &res);
                if (gai_error != 0) {
                    config_perror(gai_strerror(gai_error));
                    return;
                }
#else
                config_perror("getaddrinfo() not available");
                return;
#endif
            }
            if (res == NULL) {
                hints.ai_addrlen = sizeof(pton_addr);
                hints.ai_addr = (struct sockaddr*)&pton_addr;
                hints.ai_next = NULL;
                res = &hints;
            }

            {
                struct addrinfo *run;
                int    failed = 0;
                com2Sec6Entry *begin = NULL, *end = NULL;

                for (run = res; run && !failed; run = run->ai_next)
                    failed =
                        create_com2Sec6Entry(run, &mask,
                                             secName, secNameLen,
                                             contextName, contextNameLen,
                                             community, communityLen,
                                             &begin, &end);

                if (failed) {
                    /* Free eventually allocated chunks */
                    while (begin) {
                        end = begin;
                        begin = begin->next;
                        free(end);
                    }
                } else if (com2Sec6ListLast != NULL) {
                    com2Sec6ListLast->next = begin;
                    com2Sec6ListLast = end;
                } else {
                    com2Sec6List = begin;
                    com2Sec6ListLast = end;
                }
            }
#if HAVE_GETADDRINFO
            if (res != &hints)
                freeaddrinfo(res);
#endif
        }
    }
}

void
netsnmp_udp6_com2Sec6List_free(void)
{
    com2Sec6Entry  *e = com2Sec6List;
    while (e != NULL) {
        com2Sec6Entry  *tmp = e;
        e = e->next;
        free(tmp);
    }
    com2Sec6List = com2Sec6ListLast = NULL;
}

#endif /* support for community based SNMP */

void
netsnmp_udp6_agent_config_tokens_register(void)
{
#if !defined(NETSNMP_DISABLE_SNMPV1) || !defined(NETSNMP_DISABLE_SNMPV2C)
    register_app_config_handler("com2sec6", netsnmp_udp6_parse_security,
                                netsnmp_udp6_com2Sec6List_free,
                                "[-Cn CONTEXT] secName IPv6-network-address[/netmask] community");
#endif /* support for community based SNMP */
}



#if !defined(NETSNMP_DISABLE_SNMPV1) || !defined(NETSNMP_DISABLE_SNMPV2C)

/*
 * Return 0 if there are no com2sec entries, or return 1 if there ARE com2sec
 * entries.  On return, if a com2sec entry matched the passed parameters,
 * then *secName points at the appropriate security name, or is NULL if the
 * parameters did not match any com2sec entry.
 */

int
netsnmp_udp6_getSecName(void *opaque, int olength,
                        const char *community,
                        int community_len,
                        const char **secName, const char **contextName)
{
    const com2Sec6Entry *c;
    struct sockaddr_in6 *from = (struct sockaddr_in6 *) opaque;
    char           *ztcommunity = NULL;
    char            str6[INET6_ADDRSTRLEN];

    if (secName != NULL) {
        *secName = NULL;  /* Haven't found anything yet */
    }

    /*
     * Special case if there are NO entries (as opposed to no MATCHING
     * entries).
     */

    if (com2Sec6List == NULL) {
        DEBUGMSGTL(("netsnmp_udp6_getSecName", "no com2sec entries\n"));
        return 0;
    }

    /*
     * If there is no IPv6 source address, then there can be no valid security
     * name.
     */

    if (opaque == NULL || olength != sizeof(struct sockaddr_in6)
        || from->sin6_family != PF_INET6) {
        DEBUGMSGTL(("netsnmp_udp6_getSecName",
                    "no IPv6 source address in PDU?\n"));
        return 1;
    }

    ztcommunity = (char *) malloc(community_len + 1);
    if (ztcommunity != NULL) {
        memcpy(ztcommunity, community, community_len);
        ztcommunity[community_len] = '\0';
    }

    inet_ntop(AF_INET6, &from->sin6_addr, str6, sizeof(str6));
    DEBUGMSGTL(("netsnmp_udp6_getSecName", "resolve <\"%s\", %s>\n",
                ztcommunity ? ztcommunity : "<malloc error>", str6));

    for (c = com2Sec6List; c != NULL; c = c->next) {
        {
            char buf1[INET6_ADDRSTRLEN];
            char buf2[INET6_ADDRSTRLEN];
            DEBUGMSGTL(("netsnmp_udp6_getSecName",
                        "compare <\"%s\", %s/%s>", c->community,
                        inet_ntop(AF_INET6, &c->network, buf1, sizeof(buf1)),
                        inet_ntop(AF_INET6, &c->mask, buf2, sizeof(buf2))));
        }
        if ((community_len == (int)strlen(c->community)) &&
            (memcmp(community, c->community, community_len) == 0)) {
            int i, ok = 1;
            for (i = 0; ok && i < 16; ++i)
                if ((from->sin6_addr.s6_addr[i] & c->mask.s6_addr[i]) !=
                    c->network.s6_addr[i])
                    ok = 0;
            if (ok) {
                DEBUGMSG(("netsnmp_udp6_getSecName", "... SUCCESS\n"));
                if (secName != NULL) {
                    *secName = c->secName;
                    *contextName = c->contextName;
                }
            }
        }
    }
    DEBUGMSG(("netsnmp_udp6_getSecName", "... nope\n"));

    if (ztcommunity != NULL) {
        free(ztcommunity);
    }
    return 1;
}
#endif /* support for community based SNMP */

netsnmp_transport *
netsnmp_udp6_create_tstring(const char *str, int local,
			    const char *default_target)
{
    struct sockaddr_in6 addr;

    if (netsnmp_sockaddr_in6_2(&addr, str, default_target)) {
        return netsnmp_udp6_transport(&addr, local);
    } else {
        return NULL;
    }
}


/*
 * See:
 * 
 * http://www.ietf.org/internet-drafts/draft-ietf-ops-taddress-mib-01.txt
 * 
 * (or newer equivalent) for details of the TC which we are using for
 * the mapping here.  
 */

netsnmp_transport *
netsnmp_udp6_create_ostring(const u_char * o, size_t o_len, int local)
{
    struct sockaddr_in6 addr;

    if (o_len == 18) {
        memset((u_char *) & addr, 0, sizeof(struct sockaddr_in6));
        addr.sin6_family = AF_INET6;
        memcpy((u_char *) & (addr.sin6_addr.s6_addr), o, 16);
        addr.sin6_port = (o[16] << 8) + o[17];
        return netsnmp_udp6_transport(&addr, local);
    }
    return NULL;
}


void
netsnmp_udpipv6_ctor(void)
{
    udp6Domain.name = netsnmp_UDPIPv6Domain;
    udp6Domain.name_length = sizeof(netsnmp_UDPIPv6Domain) / sizeof(oid);
    udp6Domain.f_create_from_tstring     = NULL;
    udp6Domain.f_create_from_tstring_new = netsnmp_udp6_create_tstring;
    udp6Domain.f_create_from_ostring     = netsnmp_udp6_create_ostring;
    udp6Domain.prefix = (const char**)calloc(5, sizeof(char *));
    udp6Domain.prefix[0] = "udp6";
    udp6Domain.prefix[1] = "ipv6";
    udp6Domain.prefix[2] = "udpv6";
    udp6Domain.prefix[3] = "udpipv6";

    netsnmp_tdomain_register(&udp6Domain);
}

#else

#ifdef NETSNMP_DLL
/* need this hook for win32 MSVC++ DLL build */
void
netsnmp_udp6_agent_config_tokens_register(void)
{ }
#endif

#endif /* NETSNMP_TRANSPORT_UDPIPV6_DOMAIN */

