/* IPV4 base transport support functions
 */

#include <net-snmp/net-snmp-config.h>

#include <net-snmp/types.h>
#include <net-snmp/library/snmpIPv4BaseDomain.h>

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

#include <net-snmp/types.h>
#include <net-snmp/library/snmp_debug.h>
#include <net-snmp/library/default_store.h>
#include <net-snmp/library/system.h>


#ifndef INADDR_NONE
#define INADDR_NONE     -1
#endif

int
netsnmp_sockaddr_in(struct sockaddr_in *addr,
                    const char *inpeername, int remote_port)
{
    char buf[sizeof(int) * 3 + 2];
    sprintf(buf, ":%u", remote_port);
    return netsnmp_sockaddr_in2(addr, inpeername, remote_port ? buf : NULL);
}

int
netsnmp_sockaddr_in2(struct sockaddr_in *addr,
                     const char *inpeername, const char *default_target)
{
    int ret;

    if (addr == NULL) {
        return 0;
    }

    DEBUGMSGTL(("netsnmp_sockaddr_in",
                "addr %p, inpeername \"%s\", default_target \"%s\"\n",
                addr, inpeername ? inpeername : "[NIL]",
                default_target ? default_target : "[NIL]"));

    memset(addr, 0, sizeof(struct sockaddr_in));
    addr->sin_addr.s_addr = htonl(INADDR_ANY);
    addr->sin_family = AF_INET;
    addr->sin_port = htons((u_short)SNMP_PORT);

    {
	int port = netsnmp_ds_get_int(NETSNMP_DS_LIBRARY_ID,
				      NETSNMP_DS_LIB_DEFAULT_PORT);

	if (port != 0) {
	    addr->sin_port = htons((u_short)port);
	} else if (default_target != NULL)
	    netsnmp_sockaddr_in2(addr, default_target, NULL);
    }

    if (inpeername != NULL && *inpeername != '\0') {
	const char     *host, *port;
	char           *peername = NULL;
        char           *cp;
        /*
         * Duplicate the peername because we might want to mank around with
         * it.
         */

        peername = strdup(inpeername);
        if (peername == NULL) {
            return 0;
        }

        /*
         * Try and extract an appended port number.
         */
        cp = strchr(peername, ':');
        if (cp != NULL) {
            *cp = '\0';
            port = cp + 1;
            host = peername;
        } else {
            host = NULL;
            port = peername;
        }

        /*
         * Try to convert the user port specifier
         */
        if (port && *port == '\0')
            port = NULL;

        if (port != NULL) {
            long int l;
            char* ep;

            DEBUGMSGTL(("netsnmp_sockaddr_in", "check user service %s\n",
                        port));

            l = strtol(port, &ep, 10);
            if (ep != port && *ep == '\0' && 0 <= l && l <= 0x0ffff)
                addr->sin_port = htons((u_short)l);
            else {
                if (host == NULL) {
                    DEBUGMSGTL(("netsnmp_sockaddr_in",
                                "servname not numeric, "
				"check if it really is a destination)\n"));
                    host = port;
                    port = NULL;
                } else {
                    DEBUGMSGTL(("netsnmp_sockaddr_in",
                                "servname not numeric\n"));
                    free(peername);
                    return 0;
                }
            }
        }

        /*
         * Try to convert the user host specifier
         */
        if (host && *host == '\0')
            host = NULL;

        if (host != NULL) {
            DEBUGMSGTL(("netsnmp_sockaddr_in",
                        "check destination %s\n", host));


            if (strcmp(peername, "255.255.255.255") == 0 ) {
                /*
                 * The explicit broadcast address hack
                 */
                DEBUGMSGTL(("netsnmp_sockaddr_in", "Explicit UDP broadcast\n"));
                addr->sin_addr.s_addr = INADDR_NONE;
            } else {
                ret =
                    netsnmp_gethostbyname_v4(peername, &addr->sin_addr.s_addr);
                if (ret < 0) {
                    DEBUGMSGTL(("netsnmp_sockaddr_in",
                                "couldn't resolve hostname\n"));
                    free(peername);
                    return 0;
                }
                DEBUGMSGTL(("netsnmp_sockaddr_in",
                            "hostname (resolved okay)\n"));
            }
        }
	free(peername);
    }

    /*
     * Finished
     */

    DEBUGMSGTL(("netsnmp_sockaddr_in", "return { AF_INET, %s:%hu }\n",
                inet_ntoa(addr->sin_addr), ntohs(addr->sin_port)));
    return 1;
}

char *
netsnmp_ipv4_fmtaddr(const char *prefix, netsnmp_transport *t,
                     void *data, int len)
{
    netsnmp_indexed_addr_pair *addr_pair = NULL;
    struct hostent *host;
    char tmp[64];

    if (data != NULL && len == sizeof(netsnmp_indexed_addr_pair)) {
	addr_pair = (netsnmp_indexed_addr_pair *) data;
    } else if (t != NULL && t->data != NULL) {
	addr_pair = (netsnmp_indexed_addr_pair *) t->data;
    }

    if (addr_pair == NULL) {
        snprintf(tmp, sizeof(tmp), "%s: unknown", prefix);
    } else {
        struct sockaddr_in *to = NULL;
        to = (struct sockaddr_in *) &(addr_pair->remote_addr);
        if (to == NULL) {
            snprintf(tmp, sizeof(tmp), "%s: unknown->[%s]:%hu", prefix,
                     inet_ntoa(addr_pair->local_addr.sin.sin_addr),
                     ntohs(addr_pair->local_addr.sin.sin_port));
        } else if ( t && t->flags & NETSNMP_TRANSPORT_FLAG_HOSTNAME ) {
            /* XXX: hmm...  why isn't this prefixed */
            /* assuming intentional */
            host = netsnmp_gethostbyaddr((char *)&to->sin_addr, 4, AF_INET);
            return (host ? strdup(host->h_name) : NULL); 
        } else {
            snprintf(tmp, sizeof(tmp), "%s: [%s]:%hu->", prefix,
                     inet_ntoa(to->sin_addr), ntohs(to->sin_port));
            snprintf(tmp + strlen(tmp), sizeof(tmp)-strlen(tmp), "[%s]:%hu",
                     inet_ntoa(addr_pair->local_addr.sin.sin_addr),
                     ntohs(addr_pair->local_addr.sin.sin_port));
        }
    }
    tmp[sizeof(tmp)-1] = '\0';
    return strdup(tmp);
}

