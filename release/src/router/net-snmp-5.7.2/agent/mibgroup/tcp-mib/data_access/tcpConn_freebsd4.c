/*
 *  tcpConnTable MIB architecture support for FreeBSD/DragonFlyBSD
 */
#include <net-snmp/net-snmp-config.h>
#include <net-snmp/net-snmp-includes.h>

#include <net-snmp/agent/net-snmp-agent-includes.h>
#include <net-snmp/data_access/tcpConn.h>

#include "tcp-mib/tcpConnectionTable/tcpConnectionTable_constants.h"
#include "tcp-mib/data_access/tcpConn_private.h"

#include "mibII/mibII_common.h"

#if HAVE_NETINET_TCP_H
#include <netinet/tcp.h>
#endif
#if HAVE_NETINET_TCP_TIMER_H
#include <netinet/tcp_timer.h>
#endif
#if HAVE_NETINET_TCPIP_H
#include <netinet/tcpip.h>
#endif
#if HAVE_NETINET_TCP_VAR_H
#include <netinet/tcp_var.h>
#endif

static int _load(netsnmp_container *container, u_int flags);

/*
 * initialize arch specific storage
 *
 * @retval  0: success
 * @retval <0: error
 */
int
netsnmp_arch_tcpconn_entry_init(netsnmp_tcpconn_entry *entry)
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
netsnmp_arch_tcpconn_entry_cleanup(netsnmp_tcpconn_entry *entry)
{
    /*
     * cleanup
     */
}

/*
 * copy arch specific storage
 */
int
netsnmp_arch_tcpconn_entry_copy(netsnmp_tcpconn_entry *lhs,
                                  netsnmp_tcpconn_entry *rhs)
{
    return 0;
}

/*
 * delete an entry
 */
int
netsnmp_arch_tcpconn_entry_delete(netsnmp_tcpconn_entry *entry)
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
netsnmp_arch_tcpconn_container_load(netsnmp_container *container,
                                    u_int load_flags )
{
    int rc = 0;

    DEBUGMSGTL(("access:tcpconn:container",
                "tcpconn_container_arch_load (flags %x)\n", load_flags));

    if (NULL == container) {
        snmp_log(LOG_ERR, "no container specified/found for access_tcpconn\n");
        return -1;
    }

    rc = _load(container, load_flags);

    return rc;
}

#if defined(freebsd4) || defined(darwin)
    #define NS_ELEM struct xtcpcb
#else
    #define NS_ELEM struct xinpcb
#endif

/**
 *
 * @retval  0 no errors
 * @retval !0 errors
 */
static int
_load(netsnmp_container *container, u_int load_flags)
{
    size_t   len;
    int      sname[] = { CTL_NET, PF_INET, IPPROTO_TCP, TCPCTL_PCBLIST };
    char     *tcpcb_buf = NULL;
#if defined(dragonfly)
    struct xinpcb  *xig = NULL;
    int      StateMap[] = { 1, 1, 2, 3, 4, 5, 8, 6, 10, 9, 7, 11 };
#else
    struct xinpgen *xig = NULL;
    int      StateMap[] = { 1, 2, 3, 4, 5, 8, 6, 10, 9, 7, 11 };
#endif
    netsnmp_tcpconn_entry  *entry;
    int      state;
    int      rc = 0;

    /*
     *  Read in the buffer containing the TCP table data
     */
    len = 0;
    if (sysctl(sname, 4, 0, &len, 0, 0) < 0 ||
       (tcpcb_buf = malloc(len)) == NULL)
        return -1;
    if (sysctl(sname, 4, tcpcb_buf, &len, 0, 0) < 0) {
        free(tcpcb_buf);
        return -1;
    }

    /*
     *  Unpick this into the constituent 'xinpgen' structures, and extract
     *     the 'inpcb' elements into a linked list (built in reverse)
     */
#if defined(dragonfly)
    xig = (struct xinpcb  *) tcpcb_buf;
#else
    xig = (struct xinpgen *) tcpcb_buf;
    xig = (struct xinpgen *) ((char *) xig + xig->xig_len);
#endif

#if defined(dragonfly)
    while (xig && (xig->xi_len > sizeof(struct xinpcb)))
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
	state = StateMap[pcb.xt_tp.t_state];

	if (load_flags) {
	    if (state == TCPCONNECTIONSTATE_LISTEN) {
		if (load_flags & NETSNMP_ACCESS_TCPCONN_LOAD_NOLISTEN) {
		    DEBUGMSGT(("verbose:access:tcpconn:container",
			       " skipping listen\n"));
		    continue;
		}
	    }
	    else if (load_flags & NETSNMP_ACCESS_TCPCONN_LOAD_ONLYLISTEN) {
		DEBUGMSGT(("verbose:access:tcpconn:container",
			    " skipping non-listen\n"));
		continue;
	    }
	}

#if !defined(NETSNMP_ENABLE_IPV6)
	if (pcb.xt_inp.inp_vflag & INP_IPV6)
	    continue;
#endif

        entry = netsnmp_access_tcpconn_entry_create();
        if(NULL == entry) {
            rc = -3;
            break;
        }

        /** oddly enough, these appear to already be in network order */
        entry->loc_port = htons(pcb.xt_inp.inp_lport);
        entry->rmt_port = htons(pcb.xt_inp.inp_fport);
        entry->tcpConnState = state;
        entry->pid = 0;
        
        /** the addr string may need work */
	if (pcb.xt_inp.inp_vflag & INP_IPV6) {
	    entry->loc_addr_len = entry->rmt_addr_len = 16;
	    memcpy(entry->loc_addr, &pcb.xt_inp.in6p_laddr, 16);
	    memcpy(entry->rmt_addr, &pcb.xt_inp.in6p_faddr, 16);
	}
	else {
	    entry->loc_addr_len = entry->rmt_addr_len = 4;
	    memcpy(entry->loc_addr, &pcb.xt_inp.inp_laddr, 4);
	    memcpy(entry->rmt_addr, &pcb.xt_inp.inp_faddr, 4);
	}

        /*
         * add entry to container
         */
        entry->arbitrary_index = CONTAINER_SIZE(container) + 1;
        CONTAINER_INSERT(container, entry);
    }

    if(rc<0)
        return rc;

    return 0;
}
