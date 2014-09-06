#ifndef _SNMPUNIXDOMAIN_H
#define _SNMPUNIXDOMAIN_H

#ifdef NETSNMP_TRANSPORT_UNIX_DOMAIN

#if defined(cygwin) || defined(mingw32) || defined(mingw32msvc)
    config_error(Unix domain protocol support unavailable for this platform)
#endif

#if HAVE_SYS_SOCKET_H
#include <sys/socket.h>
#endif
#if HAVE_SYS_UN_H
#include <sys/un.h>
#endif

#include <net-snmp/library/snmp_transport.h>

config_require(SocketBase)

#ifdef __cplusplus
extern          "C" {
#endif

/*
 * The SNMP over local socket transport domain is identified by
 * transportDomainLocal as defined in RFC 3419.
 */

#define TRANSPORT_DOMAIN_LOCAL	1,3,6,1,2,1,100,1,13
NETSNMP_IMPORT oid netsnmp_UnixDomain[];

netsnmp_transport *netsnmp_unix_transport(struct sockaddr_un *addr,
                                          int local);
void netsnmp_unix_agent_config_tokens_register(void);
void netsnmp_unix_parse_security(const char *token, char *param);
int netsnmp_unix_getSecName(void *opaque, int olength,
                            const char *community,
                            size_t community_len, const char **secName,
                            const char **contextName);


/*
 * "Constructor" for transport domain object.  
 */

void            netsnmp_unix_ctor(void);

/*
 * Support functions
 */
void            netsnmp_unix_create_path_with_mode(int mode);
void            netsnmp_unix_dont_create_path(void);

#ifdef __cplusplus
}
#endif
#else

#define netsnmp_unix_create_path_with_mode(x)
#define netsnmp_unix_dont_create_path()

#endif                          /*NETSNMP_TRANSPORT_UNIX_DOMAIN */

#endif/*_SNMPUNIXDOMAIN_H*/
