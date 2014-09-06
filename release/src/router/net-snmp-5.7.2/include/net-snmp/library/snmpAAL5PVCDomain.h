#ifndef _SNMPAAL5PVCDOMAIN_H
#define _SNMPAAL5PVCDOMAIN_H

#ifdef NETSNMP_TRANSPORT_AAL5PVC_DOMAIN

#ifdef __cplusplus
extern          "C" {
#endif

#if HAVE_SYS_SOCKET_H
#include <sys/socket.h>
#endif

#ifndef linux
    config_error(AAL5 PVC support unavailable for this platform -Linux only-);
#endif

#include <atm.h>

#include <net-snmp/library/snmp_transport.h>
#include <net-snmp/library/asn1.h>

extern oid  netsnmp_AAL5PVCDomain[]; /* = { NETSNMP_ENTERPRISE_MIB, 3, 3, 3 }; */

netsnmp_transport *netsnmp_aal5pvc_transport(struct sockaddr_atmpvc *addr,
                                             int local);

/*
 * "Constructor" for transport domain object.  
 */

void            netsnmp_aal5pvc_ctor(void);

#ifdef __cplusplus
}
#endif
#endif                          /*NETSNMP_TRANSPORT_AAL5PVC_DOMAIN */

#endif/*_SNMPAAL5PVCDOMAIN_H*/
