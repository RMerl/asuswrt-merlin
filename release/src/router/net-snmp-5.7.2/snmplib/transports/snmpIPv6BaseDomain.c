/* IPV6 base transport support functions
 */

#include <net-snmp/net-snmp-config.h>

#ifdef NETSNMP_ENABLE_IPV6

#include <net-snmp/types.h>
#include <net-snmp/library/snmpIPv6BaseDomain.h>
#include <net-snmp/library/system.h>

#include <stddef.h>
#include <stdio.h>
#include <sys/types.h>
#include <ctype.h>
#if HAVE_STDLIB_H
#include <stdlib.h>
#endif
#if HAVE_STRING_H
#include <string.h>
#else
#include <strings.h>
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
#if HAVE_NET_IF_H
#include <net/if.h>
#endif

#if HAVE_DMALLOC_H
#include <dmalloc.h>
#endif

#include <net-snmp/types.h>
#include <net-snmp/library/snmp_debug.h>
#include <net-snmp/library/default_store.h>
#include <net-snmp/library/snmp_logging.h>

#include "inet_ntop.h"
#include "inet_pton.h"


#if defined(WIN32) && !defined(IF_NAMESIZE)
#define IF_NAMESIZE 12
#endif


#if defined(HAVE_WINSOCK_H) && !defined(mingw32)
static const struct in6_addr in6addr_any = IN6ADDR_ANY_INIT;
#endif


static unsigned
netsnmp_if_nametoindex(const char *ifname)
{
#if defined(WIN32)
    return atoi(ifname);
#elif defined(HAVE_IF_NAMETOINDEX)
    return if_nametoindex(ifname);
#else
    return 0;
#endif
}

static char *
netsnmp_if_indextoname(unsigned ifindex, char *ifname)
{
#if defined(WIN32)
    snprintf(ifname, IF_NAMESIZE, "%u", ifindex);
    return ifname;
#elif defined(HAVE_IF_NAMETOINDEX)
    return if_indextoname(ifindex, ifname);
#else
    return NULL;
#endif
}

char *
netsnmp_ipv6_fmtaddr(const char *prefix, netsnmp_transport *t,
                     void *data, int len)
{
    struct sockaddr_in6 *to = NULL;
    char addr[INET6_ADDRSTRLEN];
    char tmp[INET6_ADDRSTRLEN + 18];

    DEBUGMSGTL(("netsnmp_ipv6", "fmtaddr: t = %p, data = %p, len = %d\n", t,
                data, len));
    if (data != NULL && len == sizeof(struct sockaddr_in6)) {
        to = (struct sockaddr_in6 *) data;
    } else if (t != NULL && t->data != NULL) {
        to = (struct sockaddr_in6 *) t->data;
    }
    if (to == NULL) {
        snprintf(tmp, sizeof(tmp), "%s: unknown", prefix);
    } else {
        char scope_id[IF_NAMESIZE + 1] = "";

#if defined(HAVE_STRUCT_SOCKADDR_IN6_SIN6_SCOPE_ID)
	if (to->sin6_scope_id
            && netsnmp_if_indextoname(to->sin6_scope_id, &scope_id[1])) {
            scope_id[0] = '%';
        }
#endif
        snprintf(tmp, sizeof(tmp), "%s: [%s%s]:%hu", prefix,
                 inet_ntop(AF_INET6, (void *) &(to->sin6_addr), addr,
                           INET6_ADDRSTRLEN), scope_id, ntohs(to->sin6_port));
    }
    tmp[sizeof(tmp)-1] = '\0';
    return strdup(tmp);
}

int
netsnmp_sockaddr_in6_2(struct sockaddr_in6 *addr,
                       const char *inpeername, const char *default_target)
{
    char           *cp = NULL, *peername = NULL;
    char            debug_addr[INET6_ADDRSTRLEN];
#if HAVE_GETADDRINFO
    struct addrinfo *addrs = NULL;
    int             err;
#elif HAVE_GETIPNODEBYNAME
    struct hostent *hp = NULL;
    int             err;
#elif HAVE_GETHOSTBYNAME
    struct hostent *hp = NULL;
#endif
    int             portno;

    if (addr == NULL) {
        return 0;
    }

    DEBUGMSGTL(("netsnmp_sockaddr_in6",
		"addr %p, peername \"%s\", default_target \"%s\"\n",
                addr, inpeername ? inpeername : "[NIL]",
		default_target ? default_target : "[NIL]"));

    memset(addr, 0, sizeof(struct sockaddr_in6));
    addr->sin6_family = AF_INET6;
    addr->sin6_addr = in6addr_any;
    addr->sin6_port = htons((u_short)SNMP_PORT);

    {
      int port = netsnmp_ds_get_int(NETSNMP_DS_LIBRARY_ID,
				    NETSNMP_DS_LIB_DEFAULT_PORT);
      if (port != 0)
        addr->sin6_port = htons((u_short)port);
      else if (default_target != NULL)
	netsnmp_sockaddr_in6_2(addr, default_target, NULL);
    }

    if (inpeername != NULL) {
        /*
         * Duplicate the peername because we might want to mank around with
         * it.  
         */

        peername = strdup(inpeername);
        if (peername == NULL) {
            return 0;
        }

        for (cp = peername; *cp && isdigit((unsigned char) *cp); cp++);
        portno = atoi(peername);
        if (!*cp &&  portno != 0) {
            /*
             * Okay, it looks like JUST a port number.  
             */
            DEBUGMSGTL(("netsnmp_sockaddr_in6", "totally numeric: %d\n",
                        portno));
            addr->sin6_port = htons((u_short)portno);
            goto resolved;
        }

        /*
         * See if it is an IPv6 address covered with square brackets. Also check
         * for an appended :port.  
         */
        if (*peername == '[') {
            cp = strchr(peername, ']');
            if (cp != NULL) {
	      /*
	       * See if it is an IPv6 link-local address with interface
	       * name as <zone_id>, like fe80::1234%eth0.
	       * Please refer to the internet draft, IPv6 Scoped Address Architecture
	       * http://www.ietf.org/internet-drafts/draft-ietf-ipngwg-scoping-arch-04.txt
	       *
	       */
	        char *scope_id;
	        unsigned int if_index = 0;
                *cp = '\0';
		scope_id = strchr(peername + 1, '%');
		if (scope_id != NULL) {
		    *scope_id = '\0';
		    if_index = netsnmp_if_nametoindex(scope_id + 1);
		}
                if (*(cp + 1) == ':') {
                    portno = atoi(cp+2);
                    if (portno != 0 &&
                        inet_pton(AF_INET6, peername + 1,
                                  (void *) &(addr->sin6_addr))) {
                        DEBUGMSGTL(("netsnmp_sockaddr_in6",
                                    "IPv6 address with port suffix :%d\n",
                                    portno));
                        if (portno > 0 && portno < 0xffff) {
                            addr->sin6_port = htons((u_short)portno);
                        } else {
                            DEBUGMSGTL(("netsnmp_sockaddr_in6", "invalid port number: %d", portno));
                            return 0;
                        }

#if defined(HAVE_STRUCT_SOCKADDR_IN6_SIN6_SCOPE_ID)
                        addr->sin6_scope_id = if_index;
#endif
                        goto resolved;
                    }
                } else {
                    if (inet_pton
                        (AF_INET6, peername + 1,
                         (void *) &(addr->sin6_addr))) {
                        DEBUGMSGTL(("netsnmp_sockaddr_in6",
                                    "IPv6 address with square brackets\n"));
                        portno = netsnmp_ds_get_int(NETSNMP_DS_LIBRARY_ID, 
				                    NETSNMP_DS_LIB_DEFAULT_PORT);
                        if (portno <= 0)
                            portno = SNMP_PORT;
                        addr->sin6_port = htons((u_short)portno);
#if defined(HAVE_STRUCT_SOCKADDR_IN6_SIN6_SCOPE_ID)
                        addr->sin6_scope_id = if_index;
#endif
                        goto resolved;
                    }
                }
		if (scope_id != NULL) {
		  *scope_id = '%';
		}
		*cp = ']';
            }
        }

        cp = strrchr(peername, ':');
        if (cp != NULL) {
	    char *scope_id;
	    unsigned int if_index = 0;
	    *cp = '\0';
	    scope_id = strchr(peername + 1, '%');
	    if (scope_id != NULL) {
	        *scope_id = '\0';
	        if_index = netsnmp_if_nametoindex(scope_id + 1);
	    }
            portno = atoi(cp + 1);
            if (portno != 0 &&
                inet_pton(AF_INET6, peername,
                          (void *) &(addr->sin6_addr))) {
                DEBUGMSGTL(("netsnmp_sockaddr_in6",
                            "IPv6 address with port suffix :%d\n",
                            atoi(cp + 1)));
                if (portno > 0 && portno < 0xffff) {
                    addr->sin6_port = htons((u_short)portno);
                } else {
                    DEBUGMSGTL(("netsnmp_sockaddr_in6", "invalid port number: %d", portno));
                    return 0;
                }

#if defined(HAVE_STRUCT_SOCKADDR_IN6_SIN6_SCOPE_ID)
                addr->sin6_scope_id = if_index;
#endif
                goto resolved;
            }
	    if (scope_id != NULL) {
	      *scope_id = '%';
	    }
            *cp = ':';
        }

        /*
         * See if it is JUST an IPv6 address.  
         */
        if (inet_pton(AF_INET6, peername, (void *) &(addr->sin6_addr))) {
            DEBUGMSGTL(("netsnmp_sockaddr_in6", "just IPv6 address\n"));
            goto resolved;
        }

        /*
         * Well, it must be a hostname then, possibly with an appended :port.
         * Sort that out first.  
         */

        cp = strrchr(peername, ':');
        if (cp != NULL) {
            *cp = '\0';
            portno = atoi(cp + 1);
            if (portno != 0) {
                DEBUGMSGTL(("netsnmp_sockaddr_in6",
                            "hostname(?) with port suffix :%d\n",
                            portno));
                if (portno > 0 && portno < 0xffff) {
                    addr->sin6_port = htons((u_short)portno);
                } else {
                    DEBUGMSGTL(("netsnmp_sockaddr_in6", "invalid port number: %d", portno));
                    return 0;
                }

            } else {
                /*
                 * No idea, looks bogus but we might as well pass the full thing to
                 * the name resolver below.  
                 */
                *cp = ':';
                DEBUGMSGTL(("netsnmp_sockaddr_in6",
                            "hostname(?) with embedded ':'?\n"));
            }
            /*
             * Fall through.  
             */
        }

        if (peername[0] == '\0') {
          DEBUGMSGTL(("netsnmp_sockaddr_in6", "empty hostname\n"));
          free(peername);
          return 0;
        }

#if HAVE_GETADDRINFO
        {
            struct addrinfo hint = { 0 };
            hint.ai_flags = 0;
            hint.ai_family = PF_INET6;
            hint.ai_socktype = SOCK_DGRAM;
            hint.ai_protocol = 0;

            err = netsnmp_getaddrinfo(peername, NULL, &hint, &addrs);
        }
        if (err != 0) {
#if HAVE_GAI_STRERROR
            snmp_log(LOG_ERR, "getaddrinfo(\"%s\", NULL, ...): %s\n", peername,
                     gai_strerror(err));
#else
            snmp_log(LOG_ERR, "getaddrinfo(\"%s\", NULL, ...): (error %d)\n",
                     peername, err);
#endif
            free(peername);
            return 0;
        }
        if (addrs != NULL) {
        DEBUGMSGTL(("netsnmp_sockaddr_in6", "hostname (resolved okay)\n"));
        memcpy(&addr->sin6_addr,
               &((struct sockaddr_in6 *) addrs->ai_addr)->sin6_addr,
               sizeof(struct in6_addr));
		freeaddrinfo(addrs);
        }
		else {
        DEBUGMSGTL(("netsnmp_sockaddr_in6", "Failed to resolve IPv6 hostname\n"));
		}
#elif HAVE_GETIPNODEBYNAME
        hp = getipnodebyname(peername, AF_INET6, 0, &err);
        if (hp == NULL) {
            DEBUGMSGTL(("netsnmp_sockaddr_in6",
                        "hostname (couldn't resolve = %d)\n", err));
            free(peername);
            return 0;
        }
        DEBUGMSGTL(("netsnmp_sockaddr_in6", "hostname (resolved okay)\n"));
        memcpy(&(addr->sin6_addr), hp->h_addr, hp->h_length);
#elif HAVE_GETHOSTBYNAME
        hp = netsnmp_gethostbyname(peername);
        if (hp == NULL) {
            DEBUGMSGTL(("netsnmp_sockaddr_in6",
                        "hostname (couldn't resolve)\n"));
            free(peername);
            return 0;
        } else {
            if (hp->h_addrtype != AF_INET6) {
                DEBUGMSGTL(("netsnmp_sockaddr_in6",
                            "hostname (not AF_INET6!)\n"));
                free(peername);
                return 0;
            } else {
                DEBUGMSGTL(("netsnmp_sockaddr_in6",
                            "hostname (resolved okay)\n"));
                memcpy(&(addr->sin6_addr), hp->h_addr, hp->h_length);
            }
        }
#else                           /*HAVE_GETHOSTBYNAME */
        /*
         * There is no name resolving function available.  
         */
        snmp_log(LOG_ERR,
                 "no getaddrinfo()/getipnodebyname()/gethostbyname()\n");
        free(peername);
        return 0;
#endif                          /*HAVE_GETHOSTBYNAME */
    } else {
        DEBUGMSGTL(("netsnmp_sockaddr_in6", "NULL peername"));
        return 0;
    }

  resolved:
    DEBUGMSGTL(("netsnmp_sockaddr_in6", "return { AF_INET6, [%s]:%hu }\n",
                inet_ntop(AF_INET6, &addr->sin6_addr, debug_addr,
                          sizeof(debug_addr)), ntohs(addr->sin6_port)));
    free(peername);
    return 1;
}


int
netsnmp_sockaddr_in6(struct sockaddr_in6 *addr,
                     const char *inpeername, int remote_port)
{
    char buf[sizeof(remote_port) * 3 + 2];
    sprintf(buf, ":%u", remote_port);
    return netsnmp_sockaddr_in6_2(addr, inpeername, remote_port ? buf : NULL);
}

#endif /* NETSNMP_ENABLE_IPV6 */
