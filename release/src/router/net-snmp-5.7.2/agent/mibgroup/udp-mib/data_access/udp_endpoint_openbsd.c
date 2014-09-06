/*
 *  udp_endpointTable MIB architecture support for OpenBSD
 */
#include <net-snmp/net-snmp-config.h>
#include <net-snmp/net-snmp-includes.h>

#include <net-snmp/agent/net-snmp-agent-includes.h>
#include <net-snmp/data_access/ipaddress.h>
#include <net-snmp/data_access/udp_endpoint.h>
#include <net-snmp/agent/auto_nlist.h>

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
    /** xxx-rks:9 udp_endpoint delete not implemented */
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

/**
 *
 * @retval  0 no errors
 * @retval !0 errors
 */
static int
_load(netsnmp_container *container, u_int load_flags)
{
    struct inpcbtable table;
    struct inpcb   *head, *next, *prev;
    struct inpcb   inpcb;
    netsnmp_udp_endpoint_entry  *entry;
    int      rc = 0;

    /*
     *  Read in the buffer containing the TCP table data
     */
    if (!auto_nlist(UDB_SYMBOL, (char *) &table, sizeof(table))) {
	DEBUGMSGTL(("udp-mib/udp_endpoint_openbsd", "Failed to read udp_symbol\n"));
	return -1;
    }

    prev = (struct inpcb *)&CIRCLEQ_FIRST(&table.inpt_queue);
    prev = NULL;
    head = next = CIRCLEQ_FIRST(&table.inpt_queue);

    while (next) {
	NETSNMP_KLOOKUP(next, (char *)&inpcb, sizeof(inpcb));
	if (prev && CIRCLEQ_PREV(&inpcb, inp_queue) != prev) {
	    snmp_log(LOG_ERR,"udbtable link error\n");
	    break;
	}
	prev = next;
	next = CIRCLEQ_NEXT(&inpcb, inp_queue);

#if !defined(NETSNMP_ENABLE_IPV6)
        if (inpcb.inp_flags & INP_IPV6)
            goto skip;
#endif
        entry = netsnmp_access_udp_endpoint_entry_create();
        if (NULL == entry) {
            rc = -3;
            break;
        }

        /** oddly enough, these appear to already be in network order */
        entry->loc_port = ntohs(inpcb.inp_lport);
        entry->rmt_port = ntohs(inpcb.inp_fport);
        entry->pid = 0;
        
        /** the addr string may need work */
	if (inpcb.inp_flags & INP_IPV6) {
	    entry->loc_addr_len = entry->rmt_addr_len = 16;
	    memcpy(entry->loc_addr, &inpcb.inp_laddr6, 16);
	    memcpy(entry->rmt_addr, &inpcb.inp_faddr6, 16);
	}
	else {
	    entry->loc_addr_len = entry->rmt_addr_len = 4;
	    memcpy(entry->loc_addr, &inpcb.inp_laddr, 4);
	    memcpy(entry->rmt_addr, &inpcb.inp_faddr, 4);
	}

        /*
         * add entry to container
         */
        entry->index = CONTAINER_SIZE(container) + 1;
        CONTAINER_INSERT(container, entry);
#if !defined(NETSNMP_ENABLE_IPV6)
    skip:
#endif
	if (next == head)
	    break;
    }

    if (rc < 0)
        return rc;

    return 0;
}
