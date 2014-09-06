#ifndef _SNMPTLSTCPDOMAIN_H
#define _SNMPTLSTCPDOMAIN_H

#if HAVE_NETINET_IN_H
#include <netinet/in.h>
#endif

config_require(IPv4Base)
config_require(SocketBase)
config_require(TLSBase)

#include <net-snmp/library/snmpIPv4BaseDomain.h>

#ifdef __cplusplus
extern          "C" {
#endif

#define TRANSPORT_DOMAIN_TLS_TCP_IP	1,3,6,1,6,1,8
NETSNMP_IMPORT oid netsnmpTLSTCPDomain[7];
NETSNMP_IMPORT size_t netsnmpTLSTCPDomain_len;

netsnmp_transport *netsnmp_tlstcp_transport(const char *addr_string, int local);

/*
 * Register any configuration tokens specific to the agent.  
 */

void            netsnmp_tlstcp_agent_config_tokens_register(void);

/*
 * "Constructor" for transport domain object.  
 */

void            netsnmp_tlstcp_ctor(void);

#ifdef __cplusplus
}
#endif
#endif/*_SNMPTLSTCPDOMAIN_H*/
