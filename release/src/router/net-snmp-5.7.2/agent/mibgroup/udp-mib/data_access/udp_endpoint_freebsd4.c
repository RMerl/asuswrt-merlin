/*
 *  UDP MIB architecture support for FreeBSD/DragonFlyBsd
 */
#include <net-snmp/net-snmp-config.h>
#include <net-snmp/net-snmp-includes.h>

#include <net-snmp/agent/net-snmp-agent-includes.h>
#include <net-snmp/data_access/ipaddress.h>
#include <net-snmp/data_access/udp_endpoint.h>

#include "udp-mib/udpEndpointTable/udpEndpointTable_constants.h"
#include "udp-mib/data_access/udp_endpoint_private.h"

#include "mibII/mibII_common.h"

#if HAVE_NETINET_UDP_H
#include <netinet/udp.h>
#endif
#if HAVE_NETINET_UDP_VAR_H
#include <netinet/udp_var.h>
#endif

static int _load(netsnmp_container *container, u_int flags);

/*
 * initialize arch specific storage
 *
 * @retval  0: success
 * @retval <0: error
 */
int
netsnmp_arch_udp_endpoint_entry_init(netsnmp_udp_endpoint_entry *entry)
{
    /*
     * init
     */
    return 0;
}

/*
 * cleanup arch specific storage
 */
void
netsnmp_arch_udp_endpoint_entry_cleanup(netsnmp_udp_endpoint_entry *entry)
{
    /*
     * cleanup
     */
}

/*
 * copy arch specific storage
 */
int
netsnmp_arch_udp_endpoint_entry_copy(netsnmp_udp_endpoint_entry *lhs,
                                  netsnmp_udp_endpoint_entry *rhs)
{
    return 0;
}

/*
 * delete an entry
 */
int
netsnmp_arch_udp_endpoint_entry_delete(netsnmp_udp_endpoint_entry *entry)
{
    if (NULL == entry)
        return -1;
    /** xxx-rks:9 tcpConn delete not implemented */
    return -1;
}


/**
 *
 * @retval  0 no errors
 * @retval !0 errors
 */
int
netsnmp_arch_udp_endpoint_container_load(netsnmp_container *container,
                                    u_int load_flags )
{
    int rc = 0;

    DEBUGMSGTL(("access:udp_endpoint:container",
                "udp_endpoint_container_arch_load (flags %x)\n", load_flags));

    if (NULL == container) {
        snmp_log(LOG_ERR, "no container specified/found for access_udp_endpoint\n");
        return -1;
    }

    rc = _load(container, load_flags);

    return rc;
}

#define NS_ELEM struct xinpcb

/**
 *
 * @retval  0 no errors
 * @retval !0 errors
 */
static int
_load(netsnmp_container *container, u_int load_flags)
{
    size_t   len;
    int      sname[] = { CTL_NET, PF_INET, IPPROTO_UDP, UDPCTL_PCBLIST };
    char     *udpcb_buf = NULL;
#if defined(dragonfly)
    struct xinpcb  *xig = NULL;
#else
    struct xinpgen *xig = NULL;
#endif
    netsnmp_udp_endpoint_entry  *entry;
    int      rc = 0;

    /*
     *  Read in the buffer containing the TCP table data
     */
    len = 0;
    if (sysctl(sname, 4, 0, &len, 0, 0) < 0 ||
       (udpcb_buf = malloc(len)) == NULL)
        return -1;
    if (sysctl(sname, 4, udpcb_buf, &len, 0, 0) < 0) {
        free(udpcb_buf);
        return -1;
    }

    /*
     *  Unpick this into the constituent 'xinpgen' structures, and extract
     *     the 'inpcb' elements into a linked list (built in reverse)
     */
#if defined(dragonfly)
    xig = (struct xinpcb  *) udpcb_buf;
#else
    xig = (struct xinpgen *) udpcb_buf;
    xig = (struct xinpgen *) ((char *) xig + xig->xig_len);
#endif

#if defined(dragonfly)
    while (xig && (xig->xi_len >= sizeof(struct xinpcb)))
#else
    while (xig && (xig->xig_len > sizeof(struct xinpgen)))
#endif
    {
	NS_ELEM pcb = *((NS_ELEM *) xig);
#if defined(dragonfly)
	xig = (struct xinpcb  *) ((char *) xig + xig->xi_len);
#else
	xig = (struct xinpgen *) ((char *) xig + xig->xig_len);
#endif

#if !defined(NETSNMP_ENABLE_IPV6)
        if (pcb.xi_inp.inp_vflag & INP_IPV6)
	    continue;
#endif

        entry = netsnmp_access_udp_endpoint_entry_create();
        if(NULL == entry) {
            rc = -3;
            break;
        }

        /** oddly enough, these appear to already be in network order */
        entry->loc_port = htons(pcb.xi_inp.inp_lport);
        entry->rmt_port = htons(pcb.xi_inp.inp_fport);
        entry->pid = 0;
        
        /** the addr string may need work */
	if (pcb.xi_inp.inp_vflag & INP_IPV6) {
	    entry->loc_addr_len = entry->rmt_addr_len = 16;
	    memcpy(entry->loc_addr, &pcb.xi_inp.in6p_laddr, 16);
	    memcpy(entry->rmt_addr, &pcb.xi_inp.in6p_faddr, 16);
	}
	else {
	    entry->loc_addr_len = entry->rmt_addr_len = 4;
	    memcpy(entry->loc_addr, &pcb.xi_inp.inp_laddr, 4);
	    memcpy(entry->rmt_addr, &pcb.xi_inp.inp_faddr, 4);
	}

        /*
         * add entry to container
         */
	entry->index = CONTAINER_SIZE(container) + 1;
        CONTAINER_INSERT(container, entry);
    }

    if(rc<0)
        return rc;

    return 0;
}
