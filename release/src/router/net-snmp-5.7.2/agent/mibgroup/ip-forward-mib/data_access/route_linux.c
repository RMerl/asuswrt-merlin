/*
 *  Interface MIB architecture support
 *
 * $Id$
 */
#include <net-snmp/net-snmp-config.h>
#include <net-snmp/net-snmp-includes.h>
#include "mibII/mibII_common.h"

#include <net-snmp/agent/net-snmp-agent-includes.h>
#include <net-snmp/data_access/interface.h>
#include <net-snmp/data_access/route.h>
#include <net-snmp/data_access/ipaddress.h>

#include "ip-forward-mib/data_access/route_ioctl.h"
#include "ip-forward-mib/inetCidrRouteTable/inetCidrRouteTable_constants.h"
#include "if-mib/data_access/interface_ioctl.h"

static int
_type_from_flags(unsigned int flags)
{
    /*
     *  RTF_GATEWAY RTF_UP RTF_DYNAMIC RTF_CACHE
     *  RTF_MODIFIED RTF_EXPIRES RTF_NONEXTHOP
     *  RTF_DYNAMIC RTF_LOCAL RTF_PREFIX_RT
     *
     * xxx: can we distinguish between reject & blackhole?
     */
    if (flags & RTF_UP) {
        if (flags & RTF_GATEWAY)
            return INETCIDRROUTETYPE_REMOTE;
        else /*if (flags & RTF_LOCAL) */
            return INETCIDRROUTETYPE_LOCAL;
    } else 
        return 0; /* route not up */

}
static int
_load_ipv4(netsnmp_container* container, u_long *index )
{
    FILE           *in;
    char            line[256];
    netsnmp_route_entry *entry = NULL;
    char            name[16];
    int             fd;

    DEBUGMSGTL(("access:route:container",
                "route_container_arch_load ipv4\n"));

    netsnmp_assert(NULL != container);

    /*
     * fetch routes from the proc file-system:
     */
    if (!(in = fopen("/proc/net/route", "r"))) {
        NETSNMP_LOGONCE((LOG_ERR, "cannot open /proc/net/route\n"));
        return -2;
    }

    /*
     * create socket for ioctls (see NOTE[1], below)
     */
    fd = socket(AF_INET, SOCK_DGRAM, 0);
    if(fd < 0) {
        snmp_log(LOG_ERR, "could not create socket\n");
        fclose(in);
        return -2;
    }

    fgets(line, sizeof(line), in); /* skip header */

    while (fgets(line, sizeof(line), in)) {
        char            rtent_name[32];
        int             refcnt, rc;
        uint32_t        dest, nexthop, mask;
        unsigned        flags, use;

        entry = netsnmp_access_route_entry_create();

        /*
         * as with 1.99.14:
         *    Iface Dest     GW       Flags RefCnt Use Met Mask     MTU  Win IRTT
         * BE eth0  00000000 C0A80101 0003  0      0   0   FFFFFFFF 1500 0   0 
         * LE eth0  00000000 0101A8C0 0003  0      0   0   00FFFFFF    0 0   0  
         */
        rc = sscanf(line, "%s %x %x %x %d %u %d %x %*d %*d %*d\n",
                    rtent_name, &dest, &nexthop,
                    /*
                     * XXX: fix type of the args 
                     */
                    &flags, &refcnt, &use, &entry->rt_metric1,
                    &mask);
        DEBUGMSGTL(("9:access:route:container", "line |%s|\n", line));
        if (8 != rc) {
            snmp_log(LOG_ERR,
                     "/proc/net/route data format error (%d!=8), line ==|%s|",
                     rc, line);
            
            netsnmp_access_route_entry_free(entry);        
            continue;
        }

        /*
         * temporary null terminated name
         */
        strlcpy(name, rtent_name, sizeof(name));

        /*
         * don't bother to try and get the ifindex for routes with
         * no interface name.
         * NOTE[1]: normally we'd use netsnmp_access_interface_index_find,
         * but since that will open/close a socket, and we might
         * have a lot of routes, call the ioctl routine directly.
         */
        if ('*' != name[0])
            entry->if_index =
                netsnmp_access_interface_ioctl_ifindex_get(fd,name);

        /*
         * arbitrary index
         */
        entry->ns_rt_index = ++(*index);

#ifdef USING_IP_FORWARD_MIB_IPCIDRROUTETABLE_IPCIDRROUTETABLE_MODULE
        memcpy(&entry->rt_mask, &mask, 4);
        /** entry->rt_tos = XXX; */
        /** rt info ?? */
#endif
        /*
         * copy dest & next hop
         */
        entry->rt_dest_type = INETADDRESSTYPE_IPV4;
        entry->rt_dest_len = 4;
        memcpy(entry->rt_dest, &dest, 4);

        entry->rt_nexthop_type = INETADDRESSTYPE_IPV4;
        entry->rt_nexthop_len = 4;
        memcpy(entry->rt_nexthop, &nexthop, 4);

        /*
         * count bits in mask
         */
        mask = htonl(mask);
        entry->rt_pfx_len = netsnmp_ipaddress_ipv4_prefix_len(mask);

#ifdef USING_IP_FORWARD_MIB_INETCIDRROUTETABLE_INETCIDRROUTETABLE_MODULE
        /*
    inetCidrRoutePolicy OBJECT-TYPE 
        SYNTAX     OBJECT IDENTIFIER 
        MAX-ACCESS not-accessible 
        STATUS     current 
        DESCRIPTION 
               "This object is an opaque object without any defined 
                semantics.  Its purpose is to serve as an additional 
                index which may delineate between multiple entries to 
                the same destination.  The value { 0 0 } shall be used 
                as the default value for this object."
        */
        /*
         * on linux, default routes all look alike, and would have the same
         * indexed based on dest and next hop. So we use the if index
         * as the policy, to distinguise between them. Hopefully this is
         * unique.
         * xxx-rks: It should really only be for the duplicate case, but that
         *     would be more complicated than I want to get into now. Fix later.
         */
        if (0 == nexthop) {
            entry->rt_policy = calloc(3, sizeof(oid));
            entry->rt_policy[2] = entry->if_index;
            entry->rt_policy_len = sizeof(oid)*3;
        }
#endif

        /*
         * get protocol and type from flags
         */
        entry->rt_type = _type_from_flags(flags);
        
        entry->rt_proto = (flags & RTF_DYNAMIC)
            ? IANAIPROUTEPROTOCOL_ICMP : IANAIPROUTEPROTOCOL_LOCAL;

        /*
         * insert into container
         */
        if (CONTAINER_INSERT(container, entry) < 0)
        {
            DEBUGMSGTL(("access:route:container", "error with route_entry: insert into container failed.\n"));
            netsnmp_access_route_entry_free(entry);
            continue;
        }
    }

    fclose(in);
    close(fd);
    return 0;
}

#ifdef NETSNMP_ENABLE_IPV6
static int
_load_ipv6(netsnmp_container* container, u_long *index )
{
    FILE           *in;
    char            line[256];
    netsnmp_route_entry *entry = NULL;

    DEBUGMSGTL(("access:route:container",
                "route_container_arch_load ipv6\n"));

    netsnmp_assert(NULL != container);

    /*
     * fetch routes from the proc file-system:
     */
    if (!(in = fopen("/proc/net/ipv6_route", "r"))) {
        DEBUGMSGTL(("9:access:route:container", "cannot open /proc/net/ipv6_route\n"));
        return -2;
    }
    
    fgets(line,sizeof(line),in); /* skip header */
    while (fgets(line, sizeof(line), in)) {
        char            c_name[IFNAMSIZ+1];
        char            c_dest[33], c_src[33], c_next[33];
        int             rc;
        unsigned int    dest_pfx, flags;
        size_t          buf_len, buf_offset;
        u_char          *temp_uchar_ptr;

        entry = netsnmp_access_route_entry_create();

        /*
         * based on /usr/src/linux/net/ipv6/route.c, kernel 2.6.7:
         *
         * [        Dest addr /         plen ]
         * fe80000000000000025056fffec00008 80 \
         *
         * [ (?subtree) : src addr/plen : 0/0]
         * 00000000000000000000000000000000 00 \
         *
         * [        next hop              ][ metric ][ref ctn][ use   ]
         * 00000000000000000000000000000000 00000000 00000000 00000000 \
         *
         * [ flags ][dev name]
         * 80200001       lo
         */
        rc = sscanf(line, "%32s %2x %32s %*x %32s %x %*x %*x %x %"
                    SNMP_MACRO_VAL_TO_STR(IFNAMSIZ) "s\n",
                    c_dest, &dest_pfx, c_src, /*src_pfx,*/ c_next,
                    &entry->rt_metric1, /** ref,*/ /* use, */ &flags, c_name);
        DEBUGMSGTL(("9:access:route:container", "line |%s|\n", line));
        if (7 != rc) {
            snmp_log(LOG_ERR,
                     "/proc/net/ipv6_route data format error (%d!=8), "
                     "line ==|%s|", rc, line);
            continue;
        }

        /*
         * temporary null terminated name
         */
        c_name[ sizeof(c_name)-1 ] = 0;
        entry->if_index = se_find_value_in_slist("interfaces", c_name);
        if(SE_DNE == entry->if_index) {
            snmp_log(LOG_ERR,"unknown interface in /proc/net/ipv6_route "
                     "('%s')\n", c_name);
            netsnmp_access_route_entry_free(entry);
            continue;
        }
        /*
         * arbitrary index
         */
        entry->ns_rt_index = ++(*index);

#ifdef USING_IP_FORWARD_MIB_IPCIDRROUTETABLE_IPCIDRROUTETABLE_MODULE
        /** entry->rt_mask = mask; */ /* IPv4 only */
        /** entry->rt_tos = XXX; */
        /** rt info ?? */
#endif
        /*
         * convert hex addresses to binary
         */
        entry->rt_dest_type = INETADDRESSTYPE_IPV6;
        entry->rt_dest_len = 16;
        buf_len = sizeof(entry->rt_dest);
        buf_offset = 0;
        temp_uchar_ptr = entry->rt_dest;
        netsnmp_hex_to_binary(&temp_uchar_ptr, &buf_len, &buf_offset, 0,
                              c_dest, NULL);

        entry->rt_nexthop_type = INETADDRESSTYPE_IPV6;
        entry->rt_nexthop_len = 16;
        buf_len = sizeof(entry->rt_nexthop);
        buf_offset = 0;
        temp_uchar_ptr = entry->rt_nexthop;
        netsnmp_hex_to_binary(&temp_uchar_ptr, &buf_len, &buf_offset, 0,
                              c_next, NULL);

        entry->rt_pfx_len = dest_pfx;

#ifdef USING_IP_FORWARD_MIB_INETCIDRROUTETABLE_INETCIDRROUTETABLE_MODULE
        /*
    inetCidrRoutePolicy OBJECT-TYPE 
        SYNTAX     OBJECT IDENTIFIER 
        MAX-ACCESS not-accessible 
        STATUS     current 
        DESCRIPTION 
               "This object is an opaque object without any defined 
                semantics.  Its purpose is to serve as an additional 
                index which may delineate between multiple entries to 
                the same destination.  The value { 0 0 } shall be used 
                as the default value for this object."
        */
        /*
         * on linux, default routes all look alike, and would have the same
         * indexed based on dest and next hop. So we use our arbitrary index
         * as the policy, to distinguish between them.
         */
        entry->rt_policy = calloc(3, sizeof(oid));
        entry->rt_policy[2] = entry->ns_rt_index;
        entry->rt_policy_len = sizeof(oid)*3;
#endif

        /*
         * get protocol and type from flags
         */
        entry->rt_type = _type_from_flags(flags);
        
        entry->rt_proto = (flags & RTF_DYNAMIC)
            ? IANAIPROUTEPROTOCOL_ICMP : IANAIPROUTEPROTOCOL_LOCAL;

        /*
         * insert into container
         */
        CONTAINER_INSERT(container, entry);
    }

    fclose(in);
    return 0;
}
#endif

/** arch specific load
 * @internal
 *
 * @retval  0 success
 * @retval -1 no container specified
 * @retval -2 could not open data file
 */
int
netsnmp_access_route_container_arch_load(netsnmp_container* container,
                                         u_int load_flags)
{
    u_long          count = 0;
    int             rc;

    DEBUGMSGTL(("access:route:container",
                "route_container_arch_load (flags %x)\n", load_flags));

    if (NULL == container) {
        snmp_log(LOG_ERR, "no container specified/found for access_route\n");
        return -1;
    }

    rc = _load_ipv4(container, &count);
    
#ifdef NETSNMP_ENABLE_IPV6
    if((0 != rc) || (load_flags & NETSNMP_ACCESS_ROUTE_LOAD_IPV4_ONLY))
        return rc;

    /*
     * load ipv6. ipv6 module might not be loaded,
     * so ignore -2 err (file not found)
     */
    rc = _load_ipv6(container, &count);
    if (-2 == rc)
        rc = 0;
#endif

    return rc;
}

/*
 * create a new entry
 */
int
netsnmp_arch_route_create(netsnmp_route_entry *entry)
{
    if (NULL == entry)
        return -1;

    if (4 != entry->rt_dest_len) {
        DEBUGMSGT(("access:route:create", "only ipv4 supported\n"));
        return -2;
    }

    return _netsnmp_ioctl_route_set_v4(entry);
}

/*
 * create a new entry
 */
int
netsnmp_arch_route_delete(netsnmp_route_entry *entry)
{
    if (NULL == entry)
        return -1;

    if (4 != entry->rt_dest_len) {
        DEBUGMSGT(("access:route:create", "only ipv4 supported\n"));
        return -2;
    }

    return _netsnmp_ioctl_route_delete_v4(entry);
}


