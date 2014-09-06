/*
 *  Interface MIB architecture support for Solaris
 */
#include <net-snmp/net-snmp-config.h>
#include <net-snmp/net-snmp-features.h>
#include <net-snmp/net-snmp-includes.h>
#include "if-mib/ifTable/ifTable_constants.h"
#include "kernel_sunos5.h"
#include "mibII/mibII_common.h"

#include <net-snmp/agent/net-snmp-agent-includes.h>

#include <net-snmp/data_access/interface.h>
#include "if-mib/data_access/interface.h"
#include <sys/ioctl.h>
#include <sys/sockio.h>
#include <strings.h>
#include <string.h>

netsnmp_feature_child_of(interface_arch_set_admin_status, interface_all)

static int _set_ip_flags_v4(netsnmp_interface_entry *, mib2_ifEntry_t *);
static int _match_ifname_v4addr(void *ifname, void *ipaddr);
static int _get_v4addr(mib2_ifEntry_t *ife, mib2_ipAddrEntry_t *e);

static int _set_ip_flags_v6(netsnmp_interface_entry *, mib2_ifEntry_t *);
#ifdef SOLARIS_HAVE_IPV6_MIB_SUPPORT
static int _get_v6addr(mib2_ifEntry_t *ife, mib2_ipv6AddrEntry_t *ipv6e);
static int _match_ifname_v6addr(void *ifname, void *ipaddr);
#endif

void
netsnmp_arch_interface_init(void)
{
    init_kernel_sunos5();
}

/*
 * find the ifIndex for an interface name
 *
 * @retval 0 : no index found
 * @retval >0: ifIndex for interface
 */
oid
netsnmp_arch_interface_index_find(const char *name)
{
#if defined(HAVE_IF_NAMETOINDEX)
    return if_nametoindex(name);
#else /* use GIFINDEX */
    return solaris2_if_nametoindex(name, strlen(name));
#endif /* defined(HAVE_IF_NAMETOINDEX) */
}

/*
 * @retval  0 success
 * @retval -1 no container specified
 * @retval -2 could not create entry (probably malloc)
 */
int
netsnmp_arch_interface_container_load(netsnmp_container* container,
                                      u_int l_flags)
{
    netsnmp_interface_entry *entry = NULL;
    mib2_ifEntry_t          ife; 
    int                     rc;
    req_e                   req = GET_FIRST;
    int                     error = 0;

    DEBUGMSGTL(("access:interface:container:arch", "load (flags %u)\n",
                l_flags));

    if (container == NULL) {
        snmp_log(LOG_ERR, 
                 "no container specified/found for interface\n");
        return -1;
    }

    while ((rc = getMibstat(MIB_INTERFACES, &ife, sizeof(ife), req,
            &Get_everything, NULL)) == 0) {
        
        req = GET_NEXT;
	
        DEBUGMSGTL(("access:interface:container:arch", 
                    "processing '%s'\n", ife.ifDescr.o_bytes));
        entry = 
            netsnmp_access_interface_entry_create(ife.ifDescr.o_bytes, 
                                                  ife.ifIndex);
        if (entry == NULL) { 
            error = 1;
            break;
        }
        entry->ns_flags = 0;

        if (l_flags & NETSNMP_ACCESS_INTERFACE_LOAD_IP4_ONLY &&
            _set_ip_flags_v4(entry, &ife) == 0) {
            netsnmp_access_interface_entry_free(entry);
            continue;
        } else if (l_flags & NETSNMP_ACCESS_INTERFACE_LOAD_IP6_ONLY &&
                   _set_ip_flags_v6(entry, &ife) == 0) {
            netsnmp_access_interface_entry_free(entry);
            continue;
        } else { 
            (void) _set_ip_flags_v4(entry, &ife);
            (void) _set_ip_flags_v6(entry, &ife);
        }

        /*
         * collect the information needed by IF-MIB
         */
        entry->paddr = (char*)malloc(ife.ifPhysAddress.o_length);
        if (entry->paddr == NULL) {
            netsnmp_access_interface_entry_free(entry);
            error = 1;
            break;
        }
        entry->paddr_len = ife.ifPhysAddress.o_length;
        (void)memcpy(entry->paddr, ife.ifPhysAddress.o_bytes, 
                     ife.ifPhysAddress.o_length);

        entry->type = ife.ifType;
        entry->mtu = ife.ifMtu;
        entry->speed = ife.ifSpeed;
        entry->speed_high = entry->speed / 1000000;
        entry->ns_flags |= NETSNMP_INTERFACE_FLAGS_HAS_HIGH_SPEED;
        entry->oper_status = ife.ifOperStatus;
        entry->admin_status = ife.ifAdminStatus;

        if (ife.flags & IFF_PROMISC)
            entry->promiscuous = 1;

        entry->ns_flags |= NETSNMP_INTERFACE_FLAGS_ACTIVE;

        /*
         * Interface Stats.
         */
        if (! (l_flags & NETSNMP_ACCESS_INTERFACE_LOAD_NO_STATS)) {
            entry->ns_flags |=  
                NETSNMP_INTERFACE_FLAGS_HAS_BYTES |
                NETSNMP_INTERFACE_FLAGS_HAS_DROPS |
                NETSNMP_INTERFACE_FLAGS_HAS_MCAST_PKTS;
            if (ife.ifHCInOctets > 0 || ife.ifHCOutOctets > 0) {
                /*
                 * We make the assumption that if we have 
                 * a 64-bit Octet counter, then the other
                 * counters are 64-bit as well.
                 */
                DEBUGMSGTL(("access:interface:container:arch",
                            "interface '%s' have 64-bit stat counters\n",
                            entry->name));
                entry->ns_flags |=  
                    NETSNMP_INTERFACE_FLAGS_HAS_HIGH_BYTES |
                    NETSNMP_INTERFACE_FLAGS_HAS_HIGH_PACKETS;
                /* in stats */
                entry->stats.ibytes.low = ife.ifHCInOctets & 0xffffffff;
                entry->stats.ibytes.high = ife.ifHCInOctets >> 32;
                entry->stats.iucast.low = ife.ifHCInUcastPkts & 0xffffffff;
                entry->stats.iucast.high = ife.ifHCInUcastPkts >> 32;
                entry->stats.imcast.low = ife.ifHCInMulticastPkts & 0xffffffff; 
                entry->stats.imcast.high = ife.ifHCInMulticastPkts >> 32; 
                entry->stats.ibcast.low = ife.ifHCInBroadcastPkts & 0xffffffff;
                entry->stats.ibcast.high = ife.ifHCInBroadcastPkts >> 32;
                /* out stats */
                entry->stats.obytes.low = ife.ifHCOutOctets & 0xffffffff;
                entry->stats.obytes.high = ife.ifHCOutOctets >> 32;
                entry->stats.oucast.low = ife.ifHCOutUcastPkts & 0xffffffff;
                entry->stats.oucast.high = ife.ifHCOutUcastPkts >> 32;
                entry->stats.omcast.low = ife.ifHCOutMulticastPkts & 0xffffffff;
                entry->stats.omcast.high = ife.ifHCOutMulticastPkts >> 32;
                entry->stats.obcast.low = ife.ifHCOutBroadcastPkts & 0xffffffff;
                entry->stats.obcast.high = ife.ifHCOutBroadcastPkts >> 32;
            } else {
                DEBUGMSGTL(("access:interface:container:arch",
                            "interface '%s' have 32-bit stat counters\n",
                            entry->name));
                /* in stats */
                entry->stats.ibytes.low = ife.ifInOctets;
                entry->stats.iucast.low = ife.ifInUcastPkts;
                entry->stats.imcast.low = ife.ifHCInMulticastPkts & 0xffffffff; 
                entry->stats.ibcast.low = ife.ifHCInBroadcastPkts & 0xffffffff;
                /* out stats */
                entry->stats.obytes.low = ife.ifOutOctets;
                entry->stats.oucast.low = ife.ifOutUcastPkts;
                entry->stats.omcast.low = ife.ifHCOutMulticastPkts & 0xffffffff;
                entry->stats.obcast.low = ife.ifHCOutBroadcastPkts & 0xffffffff;
            }
            /* in stats */    
            entry->stats.ierrors = ife.ifInErrors;
            entry->stats.idiscards = ife.ifInDiscards;
            entry->stats.iunknown_protos = ife.ifInUnknownProtos;
            entry->stats.inucast = ife.ifInNUcastPkts;
            /* out stats */
            entry->stats.oerrors = ife.ifOutErrors;
            entry->stats.odiscards = ife.ifOutDiscards;
            entry->stats.onucast = ife.ifOutNUcastPkts;
            entry->stats.oqlen = ife.ifOutQLen;

            /* other stats */
            entry->stats.collisions = ife.ifCollisions;
        }

        netsnmp_access_interface_entry_overrides(entry);

        /*
         * add to container
         */
        CONTAINER_INSERT(container, entry);
    } 
    DEBUGMSGTL(("access:interface:container:arch", "rc = %d\n", rc)); 

    if (error) {
        DEBUGMSGTL(("access:interface:container:arch", 
                    "error %d, free container\n", error));
        netsnmp_access_interface_container_free(container,
            NETSNMP_ACCESS_INTERFACE_FREE_NOFLAGS);
        return -2;
    }

    return 0;
}
/**
 * @internal
 */
static int 
_set_ip_flags_v4(netsnmp_interface_entry *entry, mib2_ifEntry_t *ife)
{
    mib2_ipAddrEntry_t ipv4e; 

    if (_get_v4addr(ife, &ipv4e) > 0) {
        entry->reasm_max_v4 = ipv4e.ipAdEntReasmMaxSize;
        entry->ns_flags |= 
            NETSNMP_INTERFACE_FLAGS_HAS_IPV4 |
            NETSNMP_INTERFACE_FLAGS_HAS_V4_REASMMAX;
#if defined( SOLARIS_HAVE_RFC4293_SUPPORT )
        entry->retransmit_v4 = ipv4e.ipAdEntRetransmitTime;
        entry->ns_flags |= 
            NETSNMP_INTERFACE_FLAGS_HAS_V4_RETRANSMIT;
#endif
        return (1);
    }
    return (0);
}

/**
 * @internal
 */
static int 
_set_ip_flags_v6(netsnmp_interface_entry *entry, mib2_ifEntry_t *ife)
{
#ifdef SOLARIS_HAVE_IPV6_MIB_SUPPORT
    mib2_ipv6AddrEntry_t ipv6e;

    if (_get_v6addr(ife, &ipv6e) > 0) {
        entry->ns_flags |= 
            NETSNMP_INTERFACE_FLAGS_HAS_IPV6;
#if defined( SOLARIS_HAVE_RFC4293_SUPPORT )
        if (ipv6e.ipv6AddrIdentifierLen <= sizeof(entry->v6_if_id)) {
            entry->v6_if_id_len = ipv6e.ipv6AddrIdentifierLen;
            (void)memcpy(&entry->v6_if_id, &ipv6e.ipv6AddrIdentifier, 
                         entry->v6_if_id_len);
            entry->ns_flags |= 
                NETSNMP_INTERFACE_FLAGS_HAS_V6_IFID;
        }
        entry->reasm_max_v6 = ipv6e.ipv6AddrReasmMaxSize;
        entry->retransmit_v6 = ipv6e.ipv6AddrRetransmitTime;
        entry->reachable_time = ipv6e.ipv6AddrReachableTime;
        entry->ns_flags |= 
            NETSNMP_INTERFACE_FLAGS_HAS_V6_REASMMAX |
            NETSNMP_INTERFACE_FLAGS_HAS_V6_RETRANSMIT |
            NETSNMP_INTERFACE_FLAGS_HAS_V6_REACHABLE;

        /* XXX forwarding info missing */
#else
        /* XXX Don't have this info, 1500 is the minimum */
        entry->reasm_max_v6 = 1500; 
        entry->ns_flags |= 
            NETSNMP_INTERFACE_FLAGS_HAS_V6_REASMMAX; /* ??? */
#endif /* SOLARIS_HAVE_RFC4293_SUPPORT */
        return (1);
    }
#endif /* SOLARIS_HAVE_IPV6_MIB_SUPPORT */
    return (0);
}

/**
 * @internal
 */
static int 
_match_ifname_v4addr(void *ifname, void *ipaddr)
{
    DeviceName *devname = &((mib2_ipAddrEntry_t *)ipaddr)->ipAdEntIfIndex;

    return (strncmp((char *)ifname, devname->o_bytes, devname->o_length)); 
        
}

/**
 * @internal
 *
 * Search for address entry that belongs to the IF entry.
 * Returns 1 if an address was found, in which case the entry
 * will be stored in ipv4e. If not entry was found, 0 is returned. 
 * 
 */
static int
_get_v4addr(mib2_ifEntry_t *ife, mib2_ipAddrEntry_t *ipv4e)
{
    int    rc;

    if ((rc = getMibstat(MIB_IP_ADDR, ipv4e, sizeof(*ipv4e), GET_EXACT, 
        &_match_ifname_v4addr, &ife->ifDescr.o_bytes)) == 0)
        return (1);
    memset(ipv4e, '\0', sizeof(*ipv4e));
    return (0);
}

#ifdef SOLARIS_HAVE_IPV6_MIB_SUPPORT
/**
 * @internal
 */
static int 
_match_ifname_v6addr(void *ifname, void *ipaddr)
{
    DeviceName *devname = &((mib2_ipv6AddrEntry_t*)ipaddr)->ipv6AddrIfIndex;

    return (strncmp((char *)ifname, devname->o_bytes, devname->o_length)); 
        
}

/**
 * @internal
 *
 * Search for address entry that belongs to the IF entry.
 * Returns 1 if an address was found, in which case the entry
 * will be stored in ipv4e. If not entry was found, 0 is returned. 
 * 
 */
static int
_get_v6addr(mib2_ifEntry_t *ife, mib2_ipv6AddrEntry_t *ipv6e)
{
    int    rc;

    if ((rc = getMibstat(MIB_IP6_ADDR, ipv6e, sizeof(*ipv6e), GET_EXACT,
        &_match_ifname_v6addr, &ife->ifDescr.o_bytes)) == 0) {
        return (1);
    } 
    memset(ipv6e, '\0', sizeof(*ipv6e));
    return (0);
}
#endif /* SOLARIS_HAVE_IPV6_MIB_SUPPORT */

#ifndef NETSNMP_FEATURE_REMOVE_INTERFACE_ARCH_SET_ADMIN_STATUS
int
netsnmp_arch_set_admin_status(netsnmp_interface_entry * entry,
                              int ifAdminStatus_val)
{
    DEBUGMSGTL(("access:interface:arch", "set_admin_status\n"));

    /* 
     * XXX Not supported yet 
     */
    return (-1);
}
#endif /* NETSNMP_FEATURE_REMOVE_INTERFACE_ARCH_SET_ADMIN_STATUS */
