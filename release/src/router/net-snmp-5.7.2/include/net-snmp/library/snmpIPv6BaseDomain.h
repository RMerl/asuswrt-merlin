/* IPV6 base transport support functions
 */
#ifndef SNMPIPV6BASE_H
#define SNMPIPV6BASE_H

#if HAVE_NETINET_IN_H
#include <netinet/in.h>
#endif

#include <net-snmp/library/snmp_transport.h>

#ifdef __cplusplus
extern          "C" {
#endif

/*
 * Prototypes
 */

    NETSNMP_IMPORT
    char *netsnmp_ipv6_fmtaddr(const char *prefix, netsnmp_transport *t,
                               void *data, int len);
    NETSNMP_IMPORT
    int netsnmp_sockaddr_in6_2(struct sockaddr_in6 *addr,
                               const char *inpeername,
                               const char *default_target);
    int netsnmp_sockaddr_in6(struct sockaddr_in6 *addr,
                             const char *inpeername, int remote_port);

#ifdef __cplusplus
}
#endif
#endif /* SNMPIPV6BASE_H */

