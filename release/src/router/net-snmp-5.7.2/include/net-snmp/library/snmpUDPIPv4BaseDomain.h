/* UDPIPV4 base transport support functions
 */
#ifndef SNMPUDPIPV4BASE_H
#define SNMPUDPIPV4BASE_H

#if HAVE_SYS_SOCKET_H
#include <sys/socket.h>
#endif
#if HAVE_NETINET_IN_H
#include <netinet/in.h>
#endif

config_require(UDPBase)
config_require(IPv4Base)

#include <net-snmp/library/snmpIPv4BaseDomain.h>
#include <net-snmp/library/snmpUDPBaseDomain.h>

#ifdef __cplusplus
extern          "C" {
#endif

/*
 * Prototypes
 */

    netsnmp_transport *netsnmp_udpipv4base_transport(struct sockaddr_in *addr,
                                                     int local);

#if defined(linux) && defined(IP_PKTINFO) \
    || defined(IP_RECVDSTADDR) && !defined(_MSC_VER)
    int netsnmp_udpipv4_recvfrom(int s, void *buf, int len,
                                 struct sockaddr *from, socklen_t *fromlen,
                                 struct sockaddr *dstip, socklen_t *dstlen,
                                 int *if_index);
    int netsnmp_udpipv4_sendto(int fd, struct in_addr *srcip, int if_index,
                               struct sockaddr *remote, void *data, int len);
#endif


#ifdef __cplusplus
}
#endif
#endif /* SNMPUDPIPV4BASE_H */
