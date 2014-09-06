/*
 *  Interface MIB architecture support
 *
 * $Id: ipv6scopezone_linux.c 14170 2007-04-29 02:22:12Z varun_c $
 */
#include <net-snmp/net-snmp-config.h>
#include <net-snmp/net-snmp-includes.h>

#include <net-snmp/agent/net-snmp-agent-includes.h>
#include <net-snmp/data_access/scopezone.h>

#if defined (NETSNMP_ENABLE_IPV6)
static int _scopezone_v6(netsnmp_container* container, int idx_offset);
#endif

/*
 *
 * @retval  0 success
 * @retval -1 no container specified
 * @retval -2 could not open file
 * @retval -3 could not create entry (probably malloc)
 * @retval -4 file format error
 */
int
netsnmp_access_scopezone_container_arch_load(netsnmp_container* container,
                                             u_int load_flags)
{
    int rc1 = 0;
#if defined (NETSNMP_ENABLE_IPV6)
    int idx_offset = 0;

    if (NULL == container) {
        snmp_log(LOG_ERR, "no container specified/found for access_scopezone_\n");
        return -1;
    }

    rc1 = _scopezone_v6(container, idx_offset);
#endif
    if(rc1 > 0)
        rc1 = 0;
    return rc1;
}

#if defined (NETSNMP_ENABLE_IPV6)

/* scope identifiers, from kernel - include/net/ipv6.h */
#define IPV6_ADDR_LOOPBACK      0x0010U
#define IPV6_ADDR_LINKLOCAL     0x0020U
#define IPV6_ADDR_SITELOCAL     0x0040U

static int
_scopezone_v6(netsnmp_container* container, int idx_offset)
{

    /*
     * On Linux, we support only link-local scope zones.
     * Each interface, which has link-local address, gets unique scope
     * zone index.
     */
    FILE           *in;
    char            line[80], addr[40];
    int             if_index, pfx_len, scope, flags, rc = 0;
    int             last_if_index = -1;
    netsnmp_v6scopezone_entry *entry;
    
    netsnmp_assert(NULL != container);

#define PROCFILE "/proc/net/if_inet6"
    if (!(in = fopen(PROCFILE, "r"))) {
        DEBUGMSGTL(("access:scopezone:container","could not open " PROCFILE "\n"));
        return -2;
    }
    
    /*
     * address index prefix_len scope status if_name
     */
    while (fgets(line, sizeof(line), in)) {
        /*
         * fe800000000000000200e8fffe5b5c93 05 40 20 80 eth0
         *             A                    D  P  S  F  I
         * A: address
         * D: device number
         * P: prefix len
         * S: scope (see include/net/ipv6.h, net/ipv6/addrconf.c)
         * F: flags (see include/linux/rtnetlink.h, net/ipv6/addrconf.c)
         * I: interface
         */
        rc = sscanf(line, "%39s %04x %02x %02x %02x\n",
                    addr, &if_index, &pfx_len, &scope, &flags);
        if( 5 != rc ) {
            snmp_log(LOG_ERR, PROCFILE " data format error (%d!=5), line ==|%s|\n",
                     rc, line);
            continue;
        }
        DEBUGMSGTL(("access:scopezone:container",
                    "addr %s, index %d, pfx %d, scope %d, flags 0x%X\n",
                    addr, if_index, pfx_len, scope, flags));

        if (! (scope & IPV6_ADDR_LINKLOCAL)) {
            DEBUGMSGTL(("access:scopezone:container", 
                        "The address is not link-local, skipping\n"));
            continue;
        }
        /* 
         * Check that the interface was not inserted before, just in case
         * one interface has two or more link-local addresses.
         */
        if (last_if_index == if_index) {
            DEBUGMSGTL(("access:scopezone:container", 
                        "The interface was already inserted, skipping\n"));
            continue;
        }

        last_if_index = if_index; 
        entry = netsnmp_access_scopezone_entry_create();
        if(NULL == entry) {
            rc = -3;
            break;
        }
        entry->ns_scopezone_index = ++idx_offset;
        entry->index = if_index;
        entry->scopezone_linklocal = if_index;
 
        CONTAINER_INSERT(container, entry);
    }
    fclose(in);
    if(rc<0)
        return rc;

    return idx_offset;
}
#endif 
