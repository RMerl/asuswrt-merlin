#ifndef SNMPUDPBASEDOMAIN_H
#define SNMPUDPBASEDOMAIN_H

config_require(SocketBase)

#include <net-snmp/library/snmp_transport.h>

#ifdef __cplusplus
extern          "C" {
#endif

/*
 * Definitions
 */
#ifdef  MSG_DONTWAIT
#define NETSNMP_DONTWAIT MSG_DONTWAIT
#else
#define NETSNMP_DONTWAIT 0
#endif

#ifdef  MSG_NOSIGNAL
#define NETSNMP_NOSIGNAL MSG_NOSIGNAL
#else
#define NETSNMP_NOSIGNAL 0
#endif

/*
 * Prototypes
 */
    void _netsnmp_udp_sockopt_set(int fd, int local);
    int netsnmp_udpbase_recv(netsnmp_transport *t, void *buf, int size,
                             void **opaque, int *olength);
    int netsnmp_udpbase_send(netsnmp_transport *t, void *buf, int size,
                             void **opaque, int *olength);

#if defined(linux) && defined(IP_PKTINFO) \
    || defined(IP_RECVDSTADDR) && !defined(_MSC_VER)
    int netsnmp_udpbase_recvfrom(int s, void *buf, int len,
                                 struct sockaddr *from, socklen_t *fromlen,
                                 struct sockaddr *dstip, socklen_t *dstlen,
                                 int *if_index);
    int netsnmp_udpbase_sendto(int fd, struct in_addr *srcip, int if_index,
                               struct sockaddr *remote, void *data, int len);
#endif

#ifdef __cplusplus
}
#endif

#endif /* SNMPUDPBASEDOMAIN_H */
