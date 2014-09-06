/*
 *  tcpConnTable MIB architecture support for NetBSD
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

static int _load(netsnmp_container *container, u_int flags, int var);

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

    rc = _load(container, load_flags, 4);
#if defined(NETSNMP_ENABLE_IPV6)
    rc = _load(container, load_flags, 6);
#endif

    return rc;
}


/**
 *
 * @retval  0 no errors
 * @retval !0 errors
 */
static int
_load(netsnmp_container *container, u_int load_flags, int ver)
{
    const char *mibname;
    int      mib[8];
    size_t   mib_len;
    struct kinfo_pcb *pcblist;
    size_t   pcb_len;
    int      StateMap[] = { 1, 2, 3, 4, 5, 8, 6, 10, 9, 7, 11 };
    netsnmp_tcpconn_entry  *entry;
    int      state;
    int      i, rc = 0;

    /*
     *  Read in the buffer containing the TCP table data
     */
    switch (ver) {
    case 4:
    	mibname = "net.inet.tcp.pcblist";
    	break;
    case 6:
    	mibname = "net.inet6.tcp6.pcblist";
	break;
    default:
    	snmp_log(LOG_ERR, "tcp-mib:data_access:_load: bad version %d\n", ver);
	return -1;
    }

    if (sysctlnametomib(mibname, mib, &mib_len) == -1) {
    	snmp_log(LOG_ERR, "tcp-mib:data_access:_load: cant resolve mib %s\n", mibname);
	return -1;
    }

    if (sysctl(mib, sizeof(mib) / sizeof(*mib), NULL, &pcb_len, NULL, 0) == -1) {
    	snmp_log(LOG_ERR, "tcp-mib:data_access:_load: cant size mib %s\n", mibname);
	return -1;
    }

    if ((pcblist = malloc(pcb_len)) == NULL) {
    	snmp_log(LOG_ERR, "tcp-mib:data_access:_load: cant allocate mib %s\n", mibname);
	return -1;
    }
    memset(pcblist, 0, pcb_len);

    mib[6] = sizeof(*pcblist);
    mib[7] = pcb_len / sizeof(*pcblist);

    if (sysctl(mib, sizeof(mib) / sizeof(*mib),
		    pcblist, &pcb_len, NULL, 0) == -1) {
    	snmp_log(LOG_ERR, "tcp-mib:data_access:_load: cant size mib %s\n", mibname);
	return -1;
    }

    /*
     *  Unpick this into the constituent structures, and extract
     *     the 'inpcb' elements into a linked list (built in reverse)
     */
    for (i = 0; i < pcb_len / sizeof(*pcblist); i++) {
	struct kinfo_pcb *pcb = pcblist+i;
	state = StateMap[pcb->ki_tstate];

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

        entry = netsnmp_access_tcpconn_entry_create();
        if(NULL == entry) {
            rc = -3;
            break;
        }

        entry->tcpConnState = state;
        entry->pid = 0;
        
	if (ver == 6) {
	    struct sockaddr_in6 src, dst;
	    memcpy(&src, &pcb->ki_s, sizeof(src));
	    memcpy(&dst, &pcb->ki_d, sizeof(dst));
	    entry->loc_addr_len = entry->rmt_addr_len = 16;
	    memcpy(entry->loc_addr, &src.sin6_addr, 16);
	    memcpy(entry->rmt_addr, &dst.sin6_addr, 16);
	    entry->loc_port = ntohs(src.sin6_port);
	    entry->rmt_port = ntohs(dst.sin6_port);
	}
	else {
	    struct sockaddr_in src, dst;
	    memcpy(&src, &pcb->ki_s, sizeof(src));
	    memcpy(&dst, &pcb->ki_d, sizeof(dst));
	    entry->loc_addr_len = entry->rmt_addr_len = 4;
	    memcpy(entry->loc_addr, &src.sin_addr, 4);
	    memcpy(entry->rmt_addr, &dst.sin_addr, 4);
	    entry->loc_port = ntohs(src.sin_port);
	    entry->rmt_port = ntohs(dst.sin_port);
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
