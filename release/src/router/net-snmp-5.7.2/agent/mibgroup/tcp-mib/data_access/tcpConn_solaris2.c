#include <net-snmp/net-snmp-config.h>
#include <net-snmp/net-snmp-includes.h>

#include <net-snmp/agent/net-snmp-agent-includes.h>
#include <net-snmp/data_access/tcpConn.h>

#include "tcp-mib/tcpConnectionTable/tcpConnectionTable_constants.h"
#include "tcp-mib/data_access/tcpConn_private.h"

#include "kernel_sunos5.h"

static int _load_tcpconn_table_v4(netsnmp_container *, int);
#if defined(NETSNMP_ENABLE_IPV6) && defined(SOLARIS_HAVE_IPV6_MIB_SUPPORT)
static int _load_tcpconn_table_v6(netsnmp_container *, int);
#endif

int 
netsnmp_arch_tcpconn_entry_init(netsnmp_tcpconn_entry *ep)
{
    init_kernel_sunos5();
    return 0;
}

void 
netsnmp_arch_tcpconn_entry_cleanup(netsnmp_tcpconn_entry *ep)
{
    /*
     * Do nothing
     */
}

int 
netsnmp_arch_tcpconn_entry_copy(netsnmp_tcpconn_entry *ep1,
                netsnmp_tcpconn_entry *ep2)
{
    /*
     * Do nothing
     */
    return 0;
}

#ifdef TCPCONN_DELETE_SUPPORTED
int 
netsnmp_arch_tcpconn_entry_delete(netsnmp_tcpconn_entry *ep)
{
    /*
     * Not implemented 
     */
    return (-1);
}
#endif /* TCPCONN_DELETE_SUPPORTED */

int 
netsnmp_arch_tcpconn_container_load(netsnmp_container * container, 
                    u_int load_flag)
{
    int rc;

    if ((rc = _load_tcpconn_table_v4(container, load_flag)) != 0) {
        u_int flag = NETSNMP_ACCESS_TCPCONN_FREE_KEEP_CONTAINER;
        netsnmp_access_tcpconn_container_free(container, flag);
        return (rc);
    }
#if defined(NETSNMP_ENABLE_IPV6) && defined(SOLARIS_HAVE_IPV6_MIB_SUPPORT)
    if ((rc = _load_tcpconn_table_v6(container, load_flag)) != 0) {
        u_int flag = NETSNMP_ACCESS_TCPCONN_FREE_KEEP_CONTAINER;
        netsnmp_access_tcpconn_container_free(container, flag);
        return (rc);
    }
#endif
    return (0);
}

static int 
_load_tcpconn_table_v4(netsnmp_container *container, int flag) 
{
    mib2_tcpConnEntry_t   tc;
    netsnmp_tcpconn_entry *ep;
    req_e                  req = GET_FIRST;

    DEBUGMSGT(("access:tcpconn:container", "load v4\n"));

    while (getMibstat(MIB_TCP_CONN, &tc, sizeof(tc), req, 
                          &Get_everything, 0)==0) {
        req = GET_NEXT;
        if ((flag & NETSNMP_ACCESS_TCPCONN_LOAD_ONLYLISTEN && 
             tc.tcpConnState != MIB2_TCP_listen) ||
            (flag & NETSNMP_ACCESS_TCPCONN_LOAD_NOLISTEN &&
             tc.tcpConnState == MIB2_TCP_listen)) {
            continue;
        }
        ep = netsnmp_access_tcpconn_entry_create();
        if (ep == NULL)
            return (-1);
        DEBUGMSGT(("access:tcpconn:container", "add entry\n"));

        /* 
         * local address/port. 
         */
        ep->loc_addr_len = sizeof(tc.tcpConnLocalAddress);
        if (sizeof(ep->loc_addr) < ep->loc_addr_len) {
            netsnmp_access_tcpconn_entry_free(ep);
            return (-1);
        }
        (void)memcpy(&ep->loc_addr, &tc.tcpConnLocalAddress, ep->loc_addr_len);

        ep->loc_port = tc.tcpConnLocalPort;

        /* 
         * remote address/port. The address length is the same as the
         * local address, so no check needed.
         */
        ep->rmt_addr_len = sizeof(tc.tcpConnRemAddress);
        (void)memcpy(&ep->rmt_addr, &tc.tcpConnRemAddress, ep->rmt_addr_len);

        ep->rmt_port = tc.tcpConnRemPort;
        
        /* state/pid */
        ep->tcpConnState = tc.tcpConnState;
        ep->pid = 0;
        ep->arch_data = NULL;

        /* index */
        ep->arbitrary_index = CONTAINER_SIZE(container) + 1;        
        CONTAINER_INSERT(container, (void *)ep);
    }
    return (0);
}

#if defined(NETSNMP_ENABLE_IPV6) && defined(SOLARIS_HAVE_IPV6_MIB_SUPPORT)
static int 
_load_tcpconn_table_v6(netsnmp_container *container, int flag) 
{
    mib2_tcp6ConnEntry_t  tc6;
    netsnmp_tcpconn_entry *ep;
    req_e                 req = GET_FIRST;

    DEBUGMSGT(("access:tcpconn:container", "load v6\n"));

    while (getMibstat(MIB_TCP6_CONN, &tc6, sizeof(tc6), req, 
                      &Get_everything, 0)==0) {
        req = GET_NEXT;
        if ((flag & NETSNMP_ACCESS_TCPCONN_LOAD_ONLYLISTEN && 
             tc6.tcp6ConnState != MIB2_TCP_listen) ||
            (flag & NETSNMP_ACCESS_TCPCONN_LOAD_NOLISTEN &&
             tc6.tcp6ConnState == MIB2_TCP_listen)) {
            continue;
        }
        ep = netsnmp_access_tcpconn_entry_create();
        if (ep == NULL)
            return (-1);
        DEBUGMSGT(("access:tcpconn:container", "add entry\n"));

        /* 
         * local address/port. 
         */
        ep->loc_addr_len = sizeof(tc6.tcp6ConnLocalAddress);
        if (sizeof(ep->loc_addr) < ep->loc_addr_len) {
            netsnmp_access_tcpconn_entry_free(ep);
            return (-1);
        }
        (void)memcpy(&ep->loc_addr, &tc6.tcp6ConnLocalAddress,
                     ep->loc_addr_len);

        ep->loc_port = tc6.tcp6ConnLocalPort;

        /* remote address/port */
        ep->rmt_addr_len = sizeof(tc6.tcp6ConnRemAddress);
        (void)memcpy(&ep->rmt_addr, &tc6.tcp6ConnRemAddress, ep->rmt_addr_len);

        ep->rmt_port = tc6.tcp6ConnRemPort;
        
        /* state/pid */
        ep->tcpConnState = tc6.tcp6ConnState;
        ep->pid = 0;
        ep->arch_data = NULL;

        /* index */
        ep->arbitrary_index = CONTAINER_SIZE(container) + 1;        
        CONTAINER_INSERT(container, (void *)ep);
    }
    return (0);
}
#endif /*defined(NETSNMP_ENABLE_IPV6)&&defined(SOLARIS_HAVE_IPV6_MIB_SUPPORT)*/
