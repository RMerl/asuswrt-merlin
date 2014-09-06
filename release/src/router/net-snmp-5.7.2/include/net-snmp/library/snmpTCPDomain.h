#ifndef _SNMPTCPDOMAIN_H
#define _SNMPTCPDOMAIN_H

#if HAVE_NETINET_IN_H
#include <netinet/in.h>
#endif

config_require(IPv4Base)
config_require(SocketBase)
config_require(TCPBase)

#ifdef NETSNMP_TRANSPORT_TCP_DOMAIN

#include <net-snmp/library/snmpIPv4BaseDomain.h>

#ifdef __cplusplus
extern          "C" {
#endif

/*
 * The SNMP over TCP over IPv4 transport domain is identified by
 * transportDomainTcpIpv4 as defined in RFC 3419.
 */

#define TRANSPORT_DOMAIN_TCP_IP		1,3,6,1,2,1,100,1,5
NETSNMP_IMPORT oid netsnmp_snmpTCPDomain[];

netsnmp_transport *netsnmp_tcp_transport(struct sockaddr_in *addr, int local);

/*
 * "Constructor" for transport domain object.  
 */

void            netsnmp_tcp_ctor(void);

#ifdef __cplusplus
}
#endif
#endif                          /*NETSNMP_TRANSPORT_TCP_DOMAIN */

#endif/*_SNMPTCPDOMAIN_H*/
