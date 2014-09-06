/*
 *  IP-MIB architecture support
 *
 */
#include <net-snmp/net-snmp-config.h>
#include <net-snmp/net-snmp-features.h>
#include <net-snmp/net-snmp-includes.h>

#include <net-snmp/agent/net-snmp-agent-includes.h>
#include <net-snmp/data_access/ipaddress.h>
#include <net-snmp/data_access/interface.h>

#include "ip-mib/ipAddressTable/ipAddressTable_constants.h"

#include "kernel_sunos5.h"
#include "mibII/mibII_common.h"

netsnmp_feature_child_of(ipaddress_arch_entry_copy, ipaddress_common)

static int _load_v4(netsnmp_container *container, int idx_offset);
#if defined( NETSNMP_ENABLE_IPV6 )
static int _load_v6(netsnmp_container *container, int idx_offset);
#endif

/*
 * initialize arch specific storage
 *
 * @retval  0: success
 * @retval <0: error
 */
int
netsnmp_arch_ipaddress_entry_init(netsnmp_ipaddress_entry *entry)
{
    init_kernel_sunos5();
    return 0;
}

/*
 * cleanup arch specific storage
 */
void
netsnmp_arch_ipaddress_entry_cleanup(netsnmp_ipaddress_entry *entry)
{
    /*
     * Nothing to do.
     */
}

#ifndef NETSNMP_FEATURE_REMOVE_IPADDRESS_ARCH_ENTRY_COPY
/*
 * copy arch specific storage
 */
int
netsnmp_arch_ipaddress_entry_copy(netsnmp_ipaddress_entry *lhs,
                                  netsnmp_ipaddress_entry *rhs)
{
    /*
     * Nothing to do. 
     */
    return 0;
}
#endif /* NETSNMP_FEATURE_REMOVE_IPADDRESS_ARCH_ENTRY_COPY */

/*
 * create a new entry
 */
int
netsnmp_arch_ipaddress_create(netsnmp_ipaddress_entry *entry)
{
    if (NULL == entry)
        return -1;

    DEBUGMSGT(("access:ipaddress:create", "not applicable\n"));
        return 0;
}

/*
 * delete an entry
 */
int
netsnmp_arch_ipaddress_delete(netsnmp_ipaddress_entry *entry)
{
    if (NULL == entry)
        return -1;

    DEBUGMSGT(("access:ipaddress:create", "not applicable\n"));
    return 0;
}

/**
 *
 * @retval  0 no errors
 * @retval !0 errors
 */
int
netsnmp_arch_ipaddress_container_load(netsnmp_container *container,
                                      u_int load_flags)
{
    int rc = 0, idx_offset = 0;

    if (!(load_flags & NETSNMP_ACCESS_IPADDRESS_LOAD_IPV6_ONLY)) {
        rc = _load_v4(container, idx_offset);
        if(rc < 0) {
            u_int flags = NETSNMP_ACCESS_IPADDRESS_FREE_KEEP_CONTAINER;
            netsnmp_access_ipaddress_container_free(container, flags);
        }
    }

#if defined( NETSNMP_ENABLE_IPV6 )

    if (!(load_flags & NETSNMP_ACCESS_IPADDRESS_LOAD_IPV4_ONLY)) {
        if (rc < 0)
            rc = 0;

        idx_offset = rc;

        rc = _load_v6(container, idx_offset);
        if(rc < 0) {
            u_int flags = NETSNMP_ACCESS_IPADDRESS_FREE_KEEP_CONTAINER;
            netsnmp_access_ipaddress_container_free(container, flags);
        }
    }
#endif

    /*
     * return no errors (0) if we found any interfaces
     */
    if(rc > 0)
        rc = 0;
    return rc;
}

/*
 * @retval >=idx_offset ok
 * @retval -1 memory allocation error
 * @retval -2 interface lookup error
 * @retval -3 container error
 */
static int
_load_v4(netsnmp_container *container, int idx_offset)
{
    mib2_ipAddrEntry_t      ipae;
    netsnmp_ipaddress_entry *entry;
    req_e                   req = GET_FIRST;
    int                     rc = 0;

    DEBUGMSGTL(("access:ipaddress:container", "loading v4\n"));
    while ((rc = getMibstat(MIB_IP_ADDR, &ipae, sizeof(ipae), req,
                            &Get_everything, NULL)) == 0) {
        req = GET_NEXT;
        entry = netsnmp_access_ipaddress_entry_create();
        if (entry == NULL)
            return (-1);    
        if (ipae.ipAdEntAddr == INADDR_ANY)
            continue;

        ipae.ipAdEntIfIndex.o_bytes[ipae.ipAdEntIfIndex.o_length] = '\0';
        DEBUGMSGTL(("access:ipaddress:container", "found if %s\n",
                    ipae.ipAdEntIfIndex.o_bytes));
        /* Obtain interface index */
        entry->if_index = 
            netsnmp_access_interface_index_find(ipae.ipAdEntIfIndex.o_bytes);
        if (entry->if_index == 0) {
            DEBUGMSGTL(("access:ipaddress:container", "cannot find if %s\n",
                        ipae.ipAdEntIfIndex.o_bytes));
            netsnmp_access_ipaddress_entry_free(entry);
            return (-2);    
        }

        if (strchr((const char *)&ipae.ipAdEntIfIndex.o_bytes, ':') != 0)
            entry->flags |= NETSNMP_ACCESS_IPADDRESS_ISALIAS;

        /* Get the address */
        entry->ia_address_len = sizeof(ipae.ipAdEntAddr);
        netsnmp_assert(entry->ia_address_len == 4 &&
            entry->ia_address_len <= sizeof(entry->ia_address));
        memcpy(&entry->ia_address, &ipae.ipAdEntAddr, entry->ia_address_len);

        /* prefix */
        entry->ia_prefix_len = ipae.ipAdEntInfo.ae_subnet_len;

        /* set the Origin */
        if (ipae.ipAdEntInfo.ae_flags & IFF_DHCPRUNNING)
            entry->ia_origin = IPADDRESSORIGINTC_DHCP;
        else
            entry->ia_origin = IPADDRESSORIGINTC_MANUAL;

        /* set ipv4 constants */
        entry->ia_type = IPADDRESSTYPE_UNICAST;
        entry->ia_status = IPADDRESSSTATUSTC_PREFERRED;

        entry->ns_ia_index = ++idx_offset;

        DEBUGMSGTL(("access:ipaddress:container", "insert if %" NETSNMP_PRIo "u, addrlen %d\n", 
                    entry->if_index, entry->ia_address_len));

        if (CONTAINER_INSERT(container, entry) < 0) {
            DEBUGMSGTL(("access:ipaddress:container", "unable to insert %s\n", 
                        ipae.ipAdEntIfIndex.o_bytes));
            netsnmp_access_ipaddress_entry_free(entry);
            return (-3);
        }
    }
    return (idx_offset);
}

/*
 * @retval >=idx_offset ok
 * @retval -1 memory allocation error
 * @retval -2 interface lookup error
 * @retval -3 container error
 */
#if defined( NETSNMP_ENABLE_IPV6 )
static int
_load_v6(netsnmp_container *container, int idx_offset)
{
    mib2_ipv6AddrEntry_t    ip6ae;
    netsnmp_ipaddress_entry *entry;
    req_e                   req = GET_FIRST;
    int                     rc = 0;

    DEBUGMSGTL(("access:ipaddress:container", "loading v6... cache %d\n",
                MIB_IP6_ADDR));
    while ((rc = getMibstat(MIB_IP6_ADDR, &ip6ae, sizeof(ip6ae), req,
                            &Get_everything, NULL)) == 0) {
        req = GET_NEXT;
        entry = netsnmp_access_ipaddress_entry_create();
        if (entry == NULL)
            return (-1);    
        if (memcmp((const void *)&ip6ae.ipv6AddrAddress,
                   (const void *)&in6addr_any,
                   sizeof (ip6ae.ipv6AddrAddress)) == 0)
            continue;

        ip6ae.ipv6AddrIfIndex.o_bytes[ip6ae.ipv6AddrIfIndex.o_length] = '\0';
        DEBUGMSGTL(("access:ipaddress:container", "found if %s\n",
                    ip6ae.ipv6AddrIfIndex.o_bytes));

        /* Obtain interface index */
        entry->if_index = 
            netsnmp_access_interface_index_find(
            ip6ae.ipv6AddrIfIndex.o_bytes);
        if (entry->if_index == 0) {
            DEBUGMSGTL(("access:ipaddress:container", "cannot find if %s\n", 
                        ip6ae.ipv6AddrIfIndex.o_bytes));
            netsnmp_access_ipaddress_entry_free(entry);
            return (-2);    
        }

        /* Get the address */
        entry->ia_address_len = sizeof(ip6ae.ipv6AddrAddress);
        netsnmp_assert(entry->ia_address_len == 16 &&
                       entry->ia_address_len <= sizeof(entry->ia_address));
        memcpy(&entry->ia_address, &ip6ae.ipv6AddrAddress, 
               entry->ia_address_len);
               
        /* prefix */
        entry->ia_prefix_len = ip6ae.ipv6AddrPfxLength;

        /* type is anycast? (mib2.h: 1 = yes, 2 = no) */
        entry->ia_type = (ip6ae.ipv6AddrAnycastFlag == 1) ? 
            IPADDRESSTYPE_ANYCAST : IPADDRESSTYPE_UNICAST;

        /* origin (mib2.h: 1 = stateless, 2 = stateful, 3 = unknown) */
        DEBUGMSGTL(("access:ipaddress:container", "origin %d\n", 
                        ip6ae.ipv6AddrType));
        if (ip6ae.ipv6AddrType == 1)
            entry->ia_origin = IPADDRESSORIGINTC_LINKLAYER;
        else if (ip6ae.ipv6AddrInfo.ae_flags & IFF_DHCPRUNNING)
            entry->ia_origin = IPADDRESSORIGINTC_DHCP;
        else
            entry->ia_origin = IPADDRESSORIGINTC_MANUAL;
        
        /* status */
        entry->ia_status = ip6ae.ipv6AddrStatus;

        entry->ns_ia_index = ++idx_offset;
        
        DEBUGMSGTL(("access:ipaddress:container", "insert if %" NETSNMP_PRIo "u, addrlen %d\n", 
                    entry->if_index, entry->ia_address_len));

        if (CONTAINER_INSERT(container, entry) < 0) {
            DEBUGMSGTL(("access:ipaddress:container", "unable to insert %s\n", 
                        ip6ae.ipv6AddrIfIndex.o_bytes));
            netsnmp_access_ipaddress_entry_free(entry);
            return (-3);
        }
    }    
    return (idx_offset);
}
#endif /* defined( NETSNMP_ENABLE_IPV6 ) */
