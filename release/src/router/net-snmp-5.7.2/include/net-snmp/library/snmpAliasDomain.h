#ifndef _SNMPALIASDOMAIN_H
#define _SNMPALIASDOMAIN_H

#ifdef NETSNMP_TRANSPORT_ALIAS_DOMAIN

#ifdef __cplusplus
extern          "C" {
#endif

#include <net-snmp/library/snmp_transport.h>
#include <net-snmp/library/asn1.h>

/*
 * Simple aliases for complex transport strings that can be specified
 * via the snmp.conf file and the 'alias' token.
 */

#define TRANSPORT_DOMAIN_ALIAS_IP		1,3,6,1,2,1,100,1,5
NETSNMP_IMPORT oid netsnmp_snmpALIASDomain[];

/*
 * "Constructor" for transport domain object.  
 */

void            netsnmp_alias_ctor(void);

#ifdef __cplusplus
}
#endif
#endif                          /*NETSNMP_TRANSPORT_ALIAS_DOMAIN */

#endif/*_SNMPALIASDOMAIN_H*/
