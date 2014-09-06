#ifndef SNMPTCPBASEDOMAIN_H
#define SNMPTCPBASEDOMAIN_H

#include <net-snmp/library/snmp_transport.h>

#ifdef __cplusplus
extern          "C" {
#endif

/*
 * Prototypes
 */
    int netsnmp_tcpbase_recv(netsnmp_transport *t, void *buf, int size,
                             void **opaque, int *olength);
    int netsnmp_tcpbase_send(netsnmp_transport *t, void *buf, int size,
                             void **opaque, int *olength);
        
#ifdef __cplusplus
}
#endif

#endif /* SNMPTCPBASEDOMAIN_H */
