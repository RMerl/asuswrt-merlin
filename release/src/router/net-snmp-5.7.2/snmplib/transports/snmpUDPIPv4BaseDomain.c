/* IPV4 base transport support functions
 */

#include <net-snmp/net-snmp-config.h>

#include <net-snmp/types.h>
#include <net-snmp/library/snmpUDPIPv4BaseDomain.h>

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
#include <errno.h>

#include <net-snmp/types.h>
#include <net-snmp/library/snmp_debug.h>
#include <net-snmp/library/tools.h>
#include <net-snmp/library/snmp_assert.h>
#include <net-snmp/library/default_store.h>

#include <net-snmp/library/snmpSocketBaseDomain.h>

#if (defined(linux) && defined(IP_PKTINFO)) \
    || defined(IP_RECVDSTADDR) && HAVE_STRUCT_MSGHDR_MSG_CONTROL \
                               && HAVE_STRUCT_MSGHDR_MSG_FLAGS

int netsnmp_udpipv4_recvfrom(int s, void *buf, int len, struct sockaddr *from,
                             socklen_t *fromlen, struct sockaddr *dstip,
                             socklen_t *dstlen, int *if_index)
{
    return netsnmp_udpbase_recvfrom(s, buf, len, from, fromlen, dstip, dstlen,
                                    if_index);
}

int netsnmp_udpipv4_sendto(int fd, struct in_addr *srcip, int if_index,
                           struct sockaddr *remote, void *data, int len)
{
    return netsnmp_udpbase_sendto(fd, srcip, if_index, remote, data, len);
}
#endif /* (linux && IP_PKTINFO) || IP_RECVDSTADDR */

netsnmp_transport *
netsnmp_udpipv4base_transport(struct sockaddr_in *addr, int local)
{
    netsnmp_transport *t = NULL;
    int             rc = 0, rc2;
    char           *client_socket = NULL;
    netsnmp_indexed_addr_pair addr_pair;
    socklen_t       local_addr_len;

#ifdef NETSNMP_NO_LISTEN_SUPPORT
    if (local)
        return NULL;
#endif /* NETSNMP_NO_LISTEN_SUPPORT */

    if (addr == NULL || addr->sin_family != AF_INET) {
        return NULL;
    }

    memset(&addr_pair, 0, sizeof(netsnmp_indexed_addr_pair));
    memcpy(&(addr_pair.remote_addr), addr, sizeof(struct sockaddr_in));

    t = SNMP_MALLOC_TYPEDEF(netsnmp_transport);
    netsnmp_assert_or_return(t != NULL, NULL);

    DEBUGIF("netsnmp_udpbase") {
        char *str = netsnmp_udp_fmtaddr(NULL, (void *)&addr_pair,
                                        sizeof(netsnmp_indexed_addr_pair));
        DEBUGMSGTL(("netsnmp_udpbase", "open %s %s\n",
                    local ? "local" : "remote", str));
        free(str);
    }

    t->sock = socket(PF_INET, SOCK_DGRAM, 0);
    DEBUGMSGTL(("UDPBase", "openned socket %d as local=%d\n", t->sock, local)); 
    if (t->sock < 0) {
        netsnmp_transport_free(t);
        return NULL;
    }

    _netsnmp_udp_sockopt_set(t->sock, local);

    if (local) {
#ifndef NETSNMP_NO_LISTEN_SUPPORT
        /*
         * This session is inteneded as a server, so we must bind on to the
         * given IP address, which may include an interface address, or could
         * be INADDR_ANY, but certainly includes a port number.
         */

        t->local = (u_char *) malloc(6);
        if (t->local == NULL) {
            netsnmp_transport_free(t);
            return NULL;
        }
        memcpy(t->local, (u_char *) & (addr->sin_addr.s_addr), 4);
        t->local[4] = (htons(addr->sin_port) & 0xff00) >> 8;
        t->local[5] = (htons(addr->sin_port) & 0x00ff) >> 0;
        t->local_length = 6;

#if defined(linux) && defined(IP_PKTINFO)
        { 
            int sockopt = 1;
            if (setsockopt(t->sock, SOL_IP, IP_PKTINFO, &sockopt, sizeof sockopt) == -1) {
                DEBUGMSGTL(("netsnmp_udpbase", "couldn't set IP_PKTINFO: %s\n",
                    strerror(errno)));
                netsnmp_transport_free(t);
                return NULL;
            }
            DEBUGMSGTL(("netsnmp_udpbase", "set IP_PKTINFO\n"));
        }
#elif defined(IP_RECVDSTADDR)
        {
            int sockopt = 1;
            if (setsockopt(t->sock, IPPROTO_IP, IP_RECVDSTADDR, &sockopt, sizeof sockopt) == -1) {
                DEBUGMSGTL(("netsnmp_udp", "couldn't set IP_RECVDSTADDR: %s\n",
                            strerror(errno)));
                netsnmp_transport_free(t);
                return NULL;
            }
            DEBUGMSGTL(("netsnmp_udp", "set IP_RECVDSTADDR\n"));
        }
#endif
        rc = bind(t->sock, (struct sockaddr *) addr,
                  sizeof(struct sockaddr));
        if (rc != 0) {
            netsnmp_socketbase_close(t);
            netsnmp_transport_free(t);
            return NULL;
        }
        t->data = NULL;
        t->data_length = 0;
#else /* NETSNMP_NO_LISTEN_SUPPORT */
        return NULL;
#endif /* NETSNMP_NO_LISTEN_SUPPORT */
    } else {
        /*
         * This is a client session.  If we've been given a
         * client address to send from, then bind to that.
         * Otherwise the send will use "something sensible".
         */
        client_socket = netsnmp_ds_get_string(NETSNMP_DS_LIBRARY_ID,
                                              NETSNMP_DS_LIB_CLIENT_ADDR);
        if (client_socket) {
            struct sockaddr_in client_addr;
            netsnmp_sockaddr_in2(&client_addr, client_socket, NULL);
            client_addr.sin_port = 0;
            DEBUGMSGTL(("netsnmp_udpbase", "binding socket: %d\n", t->sock));
            rc = bind(t->sock, (struct sockaddr *)&client_addr,
                  sizeof(struct sockaddr));
            if ( rc != 0 ) {
                DEBUGMSGTL(("netsnmp_udpbase", "failed to bind for clientaddr: %d %s\n",
                            errno, strerror(errno)));
                netsnmp_socketbase_close(t);
                netsnmp_transport_free(t);
                return NULL;
            }
            local_addr_len = sizeof(addr_pair.local_addr);
            rc2 = getsockname(t->sock, (struct sockaddr*)&addr_pair.local_addr,
                              &local_addr_len);
            netsnmp_assert(rc2 == 0);
        }

        DEBUGIF("netsnmp_udpbase") {
            char *str = netsnmp_udp_fmtaddr(NULL, (void *)&addr_pair,
                                            sizeof(netsnmp_indexed_addr_pair));
            DEBUGMSGTL(("netsnmp_udpbase", "client open %s\n", str));
            free(str);
        }

        /*
         * Save the (remote) address in the
         * transport-specific data pointer for later use by netsnmp_udp_send.
         */

        t->data = malloc(sizeof(netsnmp_indexed_addr_pair));
        t->remote = (u_char *)malloc(6);
        if (t->data == NULL || t->remote == NULL) {
            netsnmp_transport_free(t);
            return NULL;
        }
        memcpy(t->remote, (u_char *) & (addr->sin_addr.s_addr), 4);
        t->remote[4] = (htons(addr->sin_port) & 0xff00) >> 8;
        t->remote[5] = (htons(addr->sin_port) & 0x00ff) >> 0;
        t->remote_length = 6;
        memcpy(t->data, &addr_pair, sizeof(netsnmp_indexed_addr_pair));
        t->data_length = sizeof(netsnmp_indexed_addr_pair);
    }

    return t;
}
