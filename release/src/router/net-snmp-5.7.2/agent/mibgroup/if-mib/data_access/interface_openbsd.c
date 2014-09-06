/*
 *  Interface MIB architecture support
 *
 * $Id$
 */
#include <net-snmp/net-snmp-config.h>
#include <net-snmp/net-snmp-features.h>
#include <net-snmp/net-snmp-includes.h>
#include "mibII/mibII_common.h"
#include "if-mib/ifTable/ifTable_constants.h"

netsnmp_feature_child_of(interface_arch_set_admin_status, interface_all)

#include <net-snmp/agent/net-snmp-agent-includes.h>

#if HAVE_SYS_IOCTL_H
#include <sys/ioctl.h>
#else
#error "linux should have sys/ioctl header"
#endif

#include <net-snmp/data_access/interface.h>
#include <net-snmp/data_access/ipaddress.h>
#include "if-mib/data_access/interface.h"

#include <sys/types.h>
#include <sys/time.h>
#include <net/if.h>
#include <net/if_dl.h>
#include <net/if_types.h>
#include <net/if_media.h>

#define starttime (*(const struct timeval*)netsnmp_get_agent_starttime())

int
netsnmp_openbsd_interface_get_if_speed(char *name, u_int *speed, u_int *speed_high);

void
netsnmp_arch_interface_init(void)
{
    /*
     * nothing to do
     */
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
    return if_nametoindex(name);
}

/* sa_len roundup macro. */
#define ROUNDUP(a) \
  ((a) > 0 ? (1 + (((a) - 1) | (sizeof(long) - 1))) : sizeof(long))

/*
 *
 * @retval  0 success
 * @retval -1 no container specified
 * @retval -2 could not get interface info
 * @retval -3 could not create entry (probably malloc)
 */
int
netsnmp_arch_interface_container_load(netsnmp_container* container,
                                      u_int load_flags)
{
    netsnmp_interface_entry *entry = NULL;
    u_char *if_list = NULL, *cp;
    size_t if_list_size = 0;
    struct if_msghdr *ifp;
    int sysctl_oid[] = { CTL_NET, PF_ROUTE, 0, 0, NET_RT_IFLIST, 0 };
    struct ifa_msghdr *ifa;
    struct sockaddr *a;
    struct sockaddr_dl *adl;
    int amask;
    char *if_name;
    int flags;

    DEBUGMSGTL(("access:interface:container:arch",
                "load (flags %p)\n", load_flags));

    if (NULL == container) {
        snmp_log(LOG_ERR, "no container specified/found for interface\n");
        return -1;
    }

    if (sysctl(sysctl_oid, sizeof(sysctl_oid) / sizeof(int), 0, &if_list_size, 0, 0) == -1) {
        snmp_log(LOG_ERR, "could not get interface info (size)\n");
        return -2;
    }

    if_list = (u_char*)malloc(if_list_size);
    if (if_list == NULL) {
        snmp_log(LOG_ERR, "could not allocate memory for interface info (%u bytes)\n", if_list_size);
        return -3;
    } else {
        DEBUGMSGTL(("access:interface:container:arch",
                    "allocated %u bytes for if_list\n", if_list_size));
    }

    if (sysctl(sysctl_oid, sizeof(sysctl_oid) / sizeof(int), if_list, &if_list_size, 0, 0) == -1) {
        snmp_log(LOG_ERR, "could not get interface info\n");
        free(if_list);
        return -2;
    }

    /* 1st pass: create interface entries */
    for (cp = if_list; cp < if_list + if_list_size; cp += ifp->ifm_msglen) {

        ifp = (struct if_msghdr *) cp;
        if_name = NULL;
        flags = 0;
        adl = NULL;

        if (ifp->ifm_type != RTM_IFINFO)
            continue;

        if (ifp->ifm_addrs & RTA_IFP) {
            a = (struct sockaddr *) (ifp + 1);
            /* if_msghdr is followed by one or more sockaddrs, of which we need only RTA_IFP */
            /* most of the time RTA_IFP is the first address we get, hence the shortcut */
            if ((ifp->ifm_addrs & (~RTA_IFP - 1)) != 0) {
                /* skip all addresses up to RTA_IFP. */
                for (amask = (RTA_IFP >> 1); amask != 0; amask >>= 1) {
                    if (ifp->ifm_addrs & amask) { a = (struct sockaddr *) ( ((char *) a) + ROUNDUP(a->sa_len) ); }
                }
            }
            adl = (struct sockaddr_dl *) a;
            if_name = (char *) adl->sdl_data;
            if_name[adl->sdl_nlen] = '\0';
        }
        if (!(ifp->ifm_addrs & RTA_IFP) || if_name == NULL) {
            snmp_log(LOG_ERR, "ifm_index %u: no interface name in message, skipping\n", ifp->ifm_index);
            continue;
        }

        entry = netsnmp_access_interface_entry_create(if_name, ifp->ifm_index);
        if(NULL == entry) {
            netsnmp_access_interface_container_free(container,
                                                    NETSNMP_ACCESS_INTERFACE_FREE_NOFLAGS);
            free(if_list);
            return -3;
        }

        /* get physical address */
        if (adl != NULL && adl->sdl_alen > 0) {
            entry->paddr_len = adl->sdl_alen;
            entry->paddr = (char*)malloc(entry->paddr_len);
            memcpy(entry->paddr, adl->sdl_data + adl->sdl_nlen, adl->sdl_alen);
            DEBUGMSGTL(("access:interface:container:arch",
                        "%s: paddr_len=%d, entry->paddr=%x:%x:%x:%x:%x:%x\n",
                        if_name, entry->paddr_len,
                        entry->paddr[0], entry->paddr[1], entry->paddr[2],
                        entry->paddr[3], entry->paddr[4], entry->paddr[5]));
        } else {
            entry->paddr = (char*)malloc(6);
            entry->paddr_len = 6;
            memset(entry->paddr, 0, 6);
        }

        entry->mtu = ifp->ifm_data.ifi_mtu;
        entry->type = ifp->ifm_data.ifi_type;


        entry->ns_flags |= NETSNMP_INTERFACE_FLAGS_HAS_IF_FLAGS;
        entry->os_flags = ifp->ifm_flags;
                
        if (ifp->ifm_flags & IFF_UP) {
            entry->admin_status = IFADMINSTATUS_UP;
#if defined( LINK_STATE_UP ) && defined( LINK_STATE_DOWN )
            if (ifp->ifm_data.ifi_link_state == LINK_STATE_UP) {
                entry->oper_status = IFOPERSTATUS_UP;
            } else if (ifp->ifm_data.ifi_link_state == LINK_STATE_DOWN) {
                entry->oper_status = IFOPERSTATUS_DOWN;
            } else {
                entry->oper_status = IFOPERSTATUS_UNKNOWN;
            }
#else
            entry->oper_status = IFOPERSTATUS_UP;
#endif
        } else {
            entry->admin_status = IFADMINSTATUS_DOWN;
            /* IF-MIB specifically says that ifOperStatus should be down in this case */ 
            entry->oper_status = IFOPERSTATUS_DOWN;
        }

        entry->ns_flags |= NETSNMP_INTERFACE_FLAGS_HAS_V4_REASMMAX;
        entry->reasm_max = IP_MAXPACKET;

        /* get counters */
        entry->stats.ibytes.low = ifp->ifm_data.ifi_ibytes; entry->stats.ibytes.high = 0;
        entry->stats.iucast.low = ifp->ifm_data.ifi_ipackets; entry->stats.iucast.high = 0;
        entry->stats.imcast.low = ifp->ifm_data.ifi_imcasts; entry->stats.imcast.high = 0;
        entry->stats.ierrors = ifp->ifm_data.ifi_ierrors;
        entry->stats.idiscards = ifp->ifm_data.ifi_iqdrops;
        entry->stats.iunknown_protos = ifp->ifm_data.ifi_noproto;

        entry->stats.obytes.low = ifp->ifm_data.ifi_obytes; entry->stats.obytes.high = 0;
        entry->stats.oucast.low = ifp->ifm_data.ifi_opackets; entry->stats.oucast.high = 0;
        entry->stats.omcast.low = ifp->ifm_data.ifi_omcasts; entry->stats.omcast.high = 0;
        entry->stats.oerrors = ifp->ifm_data.ifi_oerrors;
        entry->ns_flags |=  NETSNMP_INTERFACE_FLAGS_HAS_BYTES |
                            NETSNMP_INTERFACE_FLAGS_HAS_DROPS |
                            NETSNMP_INTERFACE_FLAGS_HAS_MCAST_PKTS;

        if (timercmp(&ifp->ifm_data.ifi_lastchange, &starttime, >)) {
            entry->lastchange = (ifp->ifm_data.ifi_lastchange.tv_sec - starttime.tv_sec) * 100;
            entry->ns_flags |= NETSNMP_INTERFACE_FLAGS_HAS_LASTCHANGE;
        } else {
            entry->lastchange = 0;
        }

        if (ifp->ifm_flags & IFF_PROMISC) entry->promiscuous = 1;

        /* try to guess the speed from media type */
        netsnmp_openbsd_interface_get_if_speed(entry->name, &entry->speed, &entry->speed_high);
        if (entry->speed_high != 0) {
            entry->ns_flags |= NETSNMP_INTERFACE_FLAGS_HAS_HIGH_SPEED;
        } else {
            /* or resort to ifi_baudrate */
            entry->speed = ifp->ifm_data.ifi_baudrate;
        }
        
        netsnmp_access_interface_entry_overrides(entry);

        CONTAINER_INSERT(container, entry);
        DEBUGMSGTL(("access:interface:container:arch",
                    "created entry %u for %s\n", entry->index, entry->name));
    } /* for (each interface entry) */

    /* pass 2: walk addresses */
    for (cp = if_list; cp < if_list + if_list_size; cp += ifa->ifam_msglen) {

        ifa = (struct ifa_msghdr *) cp;

        if (ifa->ifam_type != RTM_NEWADDR)
            continue;

        DEBUGMSGTL(("access:interface:container:arch",
                    "received 0x%x in RTM_NEWADDR for ifindex %u\n",
                    ifa->ifam_addrs, ifa->ifam_index));

        entry = netsnmp_access_interface_entry_get_by_index(container, ifa->ifam_index);
        if (entry == NULL) {
            snmp_log(LOG_ERR, "address for a nonexistent interface? index=%d", ifa->ifam_index);
            continue;
        }

        /* walk the list of addresses received.
           we do not use actual addresses, the sole purpose of this is to set flags */
        a = (struct sockaddr *) (ifa + 1);
        for (amask = ifa->ifam_addrs; amask != 0; amask >>= 1) {
            if ((amask & 1) != 0) {
                DEBUGMSGTL(("access:interface:container:arch",
                            "%s: a=%p, sa_len=%d, sa_family=0x%x\n",
                            entry->name, a, a->sa_len, a->sa_family));

                if (a->sa_family == AF_INET)
                    entry->ns_flags |= NETSNMP_INTERFACE_FLAGS_HAS_IPV4;
                else if (a->sa_family == AF_INET6)
                    entry->ns_flags |= NETSNMP_INTERFACE_FLAGS_HAS_IPV6;

                a = (struct sockaddr *) ( ((char *) a) + ROUNDUP(a->sa_len) );
            }
        }
        DEBUGMSGTL(("access:interface:container:arch",
                    "%s: flags=0x%x\n", entry->name, entry->ns_flags));
    }

    if (if_list != NULL)
        free(if_list);

    return 0;
}

/*
 * subroutine to translate known media typed to speed.
 * see /usr/include/net/if_media.h for definitions
 */

void
_openbsd_interface_ifmedia_to_speed(int media, u_int *speed, u_int *speed_high)
{
    *speed = 0; *speed_high = 0;

    switch (IFM_TYPE(media)) {
        case IFM_ETHER:
            switch (IFM_SUBTYPE(media)) {
                case IFM_10_T:
                case IFM_10_2:
                case IFM_10_5:
                case IFM_10_STP:
                case IFM_10_FL:
                    *speed = 10000000; *speed_high = 10; break;
                case IFM_100_TX:
                case IFM_100_FX:
                case IFM_100_T4:
                case IFM_100_VG:
                case IFM_100_T2:
                    *speed = 100000000; *speed_high = 100; break;
                case IFM_1000_LX:
		case IFM_1000_SX:
                case IFM_1000_CX:
#ifdef IFM_1000_T
                case IFM_1000_T:
#endif
                    *speed = 1000000000; *speed_high = 1000; break;
#ifdef IFM_10GBASE_SR
                case IFM_10GBASE_SR:
                case IFM_10GBASE_LR:
                    *speed = (u_int) -1; /* 4294967295; */ *speed_high = 10000; break;
#endif
            }
            break;
    }
    return;
}

/*
 * @retval  0 speed could not be determined (error, unknown media)
 * @retval >0 speed, equal to *speed.
 *            sets *speed=2^31 and returns *speed_high=speed/10^6 as required by ifSpeed/ifHighSpeed.
 */

int
netsnmp_openbsd_interface_get_if_speed(char *name, u_int *speed, u_int *speed_high)
{
    int s;
    struct ifmediareq ifmr;
    int *media_list, i;
    u_int t_speed, t_speed_high; 
    u_int m_speed, m_speed_high;

    *speed = 0; *speed_high = 0;

    if ((s = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
        return 0;
    }

    memset(&ifmr, 0, sizeof(ifmr));
    strlcpy(ifmr.ifm_name, name, sizeof(ifmr.ifm_name));

    if (ioctl(s, SIOCGIFMEDIA, (caddr_t)&ifmr) < 0 || ifmr.ifm_count == 0) {
        close(s);
        return 0;
    }

    /*
     * try to get speed from current media.
     * if unsuccessful (e.g., interface is down), get a list of capabilities,
     * try each and return maximum speed the interface is capable of.
     */

    _openbsd_interface_ifmedia_to_speed(ifmr.ifm_current, speed, speed_high);

    if (*speed == 0 &&
        (media_list = (int *) malloc(ifmr.ifm_count * sizeof(int))) != NULL ) {

        ifmr.ifm_ulist = media_list;

        if (ioctl(s, SIOCGIFMEDIA, (caddr_t)&ifmr) == 0) {
            m_speed = 0; m_speed_high = 0;
            for (i = 0; i < ifmr.ifm_count; i++) {

                _openbsd_interface_ifmedia_to_speed(media_list[i], &t_speed, &t_speed_high);

                if (t_speed_high > m_speed_high ||
                    (t_speed_high == m_speed_high && t_speed > t_speed)) {
                    m_speed_high = t_speed_high; m_speed = t_speed;
                }
            }
            *speed = m_speed; *speed_high = m_speed_high;
        }
        free(media_list);
    }

    close(s);

    DEBUGMSGTL(("access:interface:container:arch",
                "%s: speed: %u, speed_high: %u\n",
                name, *speed, *speed_high));

    return *speed;
}

#ifndef NETSNMP_FEATURE_REMOVE_INTERFACE_ARCH_SET_ADMIN_STATUS
int
netsnmp_arch_set_admin_status(netsnmp_interface_entry * entry,
                              int ifAdminStatus_val)
{
    DEBUGMSGTL(("access:interface:arch", "set_admin_status\n"));

    /* TODO: implement this call */

    /* not implemented */
    snmp_log(LOG_ERR, "netsnmp_arch_set_admin_status is not (yet) implemented for FreeBSD.\n");

    return -4;
}
#endif /* NETSNMP_FEATURE_REMOVE_INTERFACE_ARCH_SET_ADMIN_STATUS */

