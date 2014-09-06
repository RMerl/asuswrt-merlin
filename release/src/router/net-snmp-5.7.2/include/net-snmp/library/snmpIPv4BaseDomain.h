/* IPV4 base transport support functions
 */
#ifndef SNMPIPV4BASE_H
#define SNMPIPV4BASE_H

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

    char *netsnmp_ipv4_fmtaddr(const char *prefix, netsnmp_transport *t,
                               void *data, int len);

/*
 * Convert a "traditional" peername into a sockaddr_in structure which is
 * written to *addr_  Returns 1 if the conversion was successful, or 0 if it
 * failed
 */

    int netsnmp_sockaddr_in(struct sockaddr_in *addr, const char *peername,
                            int remote_port);
    int netsnmp_sockaddr_in2(struct sockaddr_in *addr, const char *inpeername,
                             const char *default_target);

#ifdef __cplusplus
}
#endif
#endif /* SNMPIPV4BASE_H */
