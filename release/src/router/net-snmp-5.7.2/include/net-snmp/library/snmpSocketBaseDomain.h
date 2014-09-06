#ifndef SNMPSOCKETBASEDOMAIN_H
#define SNMPSOCKETBASEDOMAIN_H

#if HAVE_SYS_SOCKET_H
#include <sys/socket.h>
#endif

#include <net-snmp/library/snmp_transport.h>

#ifdef __cplusplus
extern          "C" {
#endif

/*
 * Prototypes
 */
    int netsnmp_socketbase_close(netsnmp_transport *t);
    int netsnmp_sock_buffer_set(int s, int optname, int local, int size);
    int netsnmp_set_non_blocking_mode(int sock, int non_blocking_mode);

#ifdef __cplusplus
}
#endif

#endif /* SNMPSOCKETBASEDOMAIN_H */
