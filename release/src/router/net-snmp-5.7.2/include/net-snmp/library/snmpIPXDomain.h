#ifndef _SNMPIPXDOMAIN_H
#define _SNMPIPXDOMAIN_H

#ifdef __cplusplus
extern          "C" {
#endif

#include <net-snmp/library/snmp_transport.h>
#include <net-snmp/library/asn1.h>
#if HAVE_NETIPX_IPX_H
#include <netipx/ipx.h>
#endif

#ifndef linux
    config_error(IPX support unavailable for this platform -Linux only-);
#endif

netsnmp_transport *netsnmp_ipx_transport(struct sockaddr_ipx *addr, int local);

/*
 * Convert an textually formatted IPX address into a sockaddr_ipx
 * structure which is written to *addr.  Returns 1 if the conversion
 * was successful, or 0 if it failed.  
 */

int             netsnmp_sockaddr_ipx(struct sockaddr_ipx *addr,
                                     const char *peername);

/*
 * "Constructor" for transport domain object.  
 */

void            netsnmp_ipx_ctor(void);

#ifdef __cplusplus
}
#endif
#endif/*_SNMPIPXDOMAIN_H*/
