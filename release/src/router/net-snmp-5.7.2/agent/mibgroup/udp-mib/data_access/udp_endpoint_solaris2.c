#include <net-snmp/net-snmp-config.h>
#include <net-snmp/net-snmp-features.h>
#include <net-snmp/net-snmp-includes.h>

#include <net-snmp/agent/net-snmp-agent-includes.h>
#include <net-snmp/data_access/ipaddress.h>
#include <net-snmp/data_access/udp_endpoint.h>

#include "udp-mib/udpEndpointTable/udpEndpointTable_constants.h"
#include "udp-mib/data_access/udp_endpoint_private.h"

#include "kernel_sunos5.h"

netsnmp_feature_require(netsnmp_access_udp_endpoint_entry_create)
netsnmp_feature_child_of(udp_endpoint_all, libnetsnmpmibs)
netsnmp_feature_child_of(udp_endpoint_writable, udp_endpoint_all)

static int _load_udp_endpoint_table_v4(netsnmp_container *, int);
#if defined(NETSNMP_ENABLE_IPV6) && defined(SOLARIS_HAVE_IPV6_MIB_SUPPORT)
static int _load_udp_endpoint_table_v6(netsnmp_container *, int);
#endif

#ifndef NETSNMP_FEATURE_REMOVE_UDP_ENDPOINT_WRITABLE
int 
netsnmp_arch_udp_endpoint_entry_init(netsnmp_udp_endpoint_entry *ep)
{
    init_kernel_sunos5();
    return 0;
}

void 
netsnmp_arch_udp_endpoint_entry_cleanup(netsnmp_udp_endpoint_entry *ep)
{
    /*
     * Do nothing
     */
}

int 
netsnmp_arch_udp_endpoint_entry_copy(netsnmp_udp_endpoint_entry *ep1,
                netsnmp_udp_endpoint_entry *ep2)
{
    /*
     * Do nothing
     */
    return 0;
}

int 
netsnmp_arch_udp_endpoint_delete(netsnmp_udp_endpoint_entry *ep)
{
    /*
     * Not implemented 
     */
    return (-1);
}
#endif /* NETSNMP_FEATURE_REMOVE_UDP_ENDPOINT_WRITABLE */

int 
netsnmp_arch_udp_endpoint_container_load(netsnmp_container * container, 
                    u_int load_flag)
{
    int rc;

    if ((rc = _load_udp_endpoint_table_v4(container, load_flag)) != 0) {
        u_int flag = NETSNMP_ACCESS_UDP_ENDPOINT_FREE_KEEP_CONTAINER;
        netsnmp_access_udp_endpoint_container_free(container, flag);
        return (rc);
    }
#if defined(NETSNMP_ENABLE_IPV6) && defined(SOLARIS_HAVE_IPV6_MIB_SUPPORT)
    if ((rc = _load_udp_endpoint_table_v6(container, load_flag)) != 0) {
        u_int flag = NETSNMP_ACCESS_UDP_ENDPOINT_FREE_KEEP_CONTAINER;
        netsnmp_access_udp_endpoint_container_free(container, flag);
        return (rc);
    }
#endif
    return (0);
}

static int 
_load_udp_endpoint_table_v4(netsnmp_container *container, int flag) 
{
    netsnmp_udp_endpoint_entry *ep;
    mib2_udpEntry_t ue;
    req_e           req = GET_FIRST;

    DEBUGMSGT(("access:udp_endpoint:container", "load v4\n"));

    while (getMibstat(MIB_UDP_LISTEN, &ue, sizeof(ue), req, 
                          &Get_everything, 0)==0) {
        req = GET_NEXT;
        ep = netsnmp_access_udp_endpoint_entry_create();
        if (ep == NULL)
            return (-1);
        DEBUGMSGT(("access:udp_endpoint:container", "add entry\n"));

        /* 
         * local address/port. 
         */
        ep->loc_addr_len = sizeof(ue.udpLocalAddress);
        if (sizeof(ep->loc_addr) < ep->loc_addr_len) {
            netsnmp_access_udp_endpoint_entry_free(ep);
            return (-1);
        }
        memcpy(&ep->loc_addr, &ue.udpLocalAddress, ep->loc_addr_len);

        ep->loc_port = ue.udpLocalPort;

        /* 
         * remote address/port. The address length is the same as the
         * local address, so no check needed. If the remote address is
         * unspecfied, then the type should be set to "unknown" (per RFC 4113).
         */
        if (ue.udpEntryInfo.ue_RemoteAddress == INADDR_ANY) {
            ep->rmt_addr_len = 0;
        } else { 
            ep->rmt_addr_len = sizeof(ue.udpEntryInfo.ue_RemoteAddress);
            memcpy(&ep->rmt_addr, &ue.udpEntryInfo.ue_RemoteAddress, 
                   ep->rmt_addr_len);
        }

        ep->rmt_port = ue.udpEntryInfo.ue_RemotePort;

        /* 
         * instance - if there is support for RFC 4293, then we also have
         * support for RFC 4113.
         */
#ifdef SOLARIS_HAVE_RFC4293_SUPPORT 
        ep->instance = ue.udpInstance; 
#else
        ep->instance = 0;
#endif
        
        /* state */
        ep->state = 0; 

        /* index */
        ep->index = CONTAINER_SIZE(container) + 1;        
        ep->oid_index.oids = &ep->index;
        ep->oid_index.len = 1; 

        CONTAINER_INSERT(container, (void *)ep);
    }
    return (0);
}

#if defined(NETSNMP_ENABLE_IPV6) && defined(SOLARIS_HAVE_IPV6_MIB_SUPPORT)
static int 
_load_udp_endpoint_table_v6(netsnmp_container *container, int flag) 
{
    netsnmp_udp_endpoint_entry *ep;
    mib2_udp6Entry_t ue6;
    req_e            req = GET_FIRST;

    DEBUGMSGT(("access:udp_endpoint:container", "load v6\n"));

    while (getMibstat(MIB_UDP6_ENDPOINT, &ue6, sizeof(ue6), req, 
                      &Get_everything, 0)==0) {
        req = GET_NEXT;
        ep = netsnmp_access_udp_endpoint_entry_create();
        if (ep == NULL)
            return (-1);
        DEBUGMSGT(("access:udp_endpoint:container", "add entry\n"));

        /* 
         * local address/port. 
         */
        ep->loc_addr_len = sizeof(ue6.udp6LocalAddress);
        if (sizeof(ep->loc_addr) < ep->loc_addr_len) {
            netsnmp_access_udp_endpoint_entry_free(ep);
            return (-1);
        }
        (void)memcpy(&ep->loc_addr, &ue6.udp6LocalAddress, ep->loc_addr_len);

        ep->loc_port = ue6.udp6LocalPort;

        /* remote address/port */
        if (IN6_IS_ADDR_UNSPECIFIED(&ue6.udp6EntryInfo.ue_RemoteAddress)) {
            ep->rmt_addr_len = 0;
        } else {
            ep->rmt_addr_len = sizeof(ue6.udp6EntryInfo.ue_RemoteAddress);
            (void)memcpy(&ep->rmt_addr, &ue6.udp6EntryInfo.ue_RemoteAddress,
                         ep->rmt_addr_len);
        }
        ep->rmt_port = ue6.udp6EntryInfo.ue_RemotePort;

        /* instance */
#ifdef SOLARIS_HAVE_RFC4293_SUPPORT  
        ep->instance = ue6.udp6Instance; 
#else
        ep->instance = 0;
#endif
        /* state */
        ep->state = 0; 

        /* index */
        ep->index = CONTAINER_SIZE(container) + 1;        
        ep->oid_index.oids = &ep->index;
        ep->oid_index.len = 1;

        CONTAINER_INSERT(container, (void *)ep);
    }
    return (0);
}
#endif /* defined(NETSNMP_ENABLE_IPV6)&&defined(SOLARIS_HAVE_IPV6_MIB_SUPPORT)*/
