#ifndef _SNMPSSHDOMAIN_H
#define _SNMPSSHDOMAIN_H

config_require(IPv4Base)
config_require(SocketBase)

#ifdef NETSNMP_TRANSPORT_SSH_DOMAIN

#include <net-snmp/library/snmp_transport.h>

#if HAVE_NETINET_IN_H
#include <netinet/in.h>
#endif

#ifdef __cplusplus
extern          "C" {
#endif

/*
 * The SNMP over SSH over IPv4 transport domain is identified by
 * transportDomainSshIpv4 as defined in RFC 3419.
 */

#define TRANSPORT_DOMAIN_SSH_IP		1,3,6,1,2,1,100,1,100
NETSNMP_IMPORT const oid netsnmp_snmpSSHDomain[];
enum { netsnmp_snmpSSHDomain_len = 9 };

netsnmp_transport *netsnmp_ssh_transport(struct sockaddr_in *addr, int local);

/*
 * "Constructor" for transport domain object.
 */

void            netsnmp_ssh_ctor(void);

#ifdef __cplusplus
}
#endif
#endif                          /*NETSNMP_TRANSPORT_SSH_DOMAIN */

#endif/*_SNMPSSHDOMAIN_H*/
