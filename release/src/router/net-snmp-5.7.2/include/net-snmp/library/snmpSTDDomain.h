#ifndef _SNMPSTDDOMAIN_H
#define _SNMPSTDDOMAIN_H

#ifdef NETSNMP_TRANSPORT_STD_DOMAIN

#include <net-snmp/library/snmp_transport.h>

#ifdef __cplusplus
extern          "C" {
#endif

/*
 * The SNMP over STD over IPv4 transport domain is identified by
 * transportDomainStdIpv4 as defined in RFC 3419.
 */

#define TRANSPORT_DOMAIN_STD_IP		1,3,6,1,2,1,100,1,101
extern oid netsnmp_snmpSTDDomain[];

    typedef struct netsnmp_std_data_s {
       int outfd;
       int childpid;
       char *prog;
    } netsnmp_std_data;
    
    netsnmp_transport *netsnmp_std_transport(const char *instring,
                                             size_t instring_len,
                                             const char *default_target);

    /*
     * "Constructor" for transport domain object.  
     */

    void            netsnmp_std_ctor(void);

#ifdef __cplusplus
}
#endif
#endif                          /*NETSNMP_TRANSPORT_STD_DOMAIN */

#endif/*_SNMPSTDDOMAIN_H*/
