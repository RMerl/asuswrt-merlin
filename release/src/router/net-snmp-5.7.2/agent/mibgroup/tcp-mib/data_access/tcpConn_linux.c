/*
 *  tcpConnTable MIB architecture support
 *
 * $Id$
 */
#include <net-snmp/net-snmp-config.h>
#include <net-snmp/net-snmp-includes.h>

#include <net-snmp/agent/net-snmp-agent-includes.h>
#include <net-snmp/data_access/tcpConn.h>

#include "tcp-mib/tcpConnectionTable/tcpConnectionTable_constants.h"
#include "tcp-mib/data_access/tcpConn_private.h"
#include "mibgroup/util_funcs/get_pid_from_inode.h"
static int
linux_states[12] = { 1, 5, 3, 4, 6, 7, 11, 1, 8, 9, 2, 10 };

static int _load4(netsnmp_container *container, u_int flags);
#if defined (NETSNMP_ENABLE_IPV6)
static int _load6(netsnmp_container *container, u_int flags);
#endif

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

#ifdef TCPCONN_DELETE_SUPPORTED
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
#endif /* TCPCONN_DELETE_SUPPORTED */

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

    /* Setup the pid_from_inode table, and fill it.*/
    netsnmp_get_pid_from_inode_init();

    if (NULL == container) {
        snmp_log(LOG_ERR, "no container specified/found for access_tcpconn\n");
        return -1;
    }

    rc = _load4(container, load_flags);

#if defined (NETSNMP_ENABLE_IPV6)
    if((0 != rc) || (load_flags & NETSNMP_ACCESS_TCPCONN_LOAD_IPV4_ONLY))
        return rc;

    /*
     * load ipv6. ipv6 module might not be loaded,
     * so ignore -2 err (file not found)
     */
    rc = _load6(container, load_flags);
    if (-2 == rc)
        rc = 0;
#endif

    return rc;
}

/**
 *
 * @retval  0 no errors
 * @retval !0 errors
 */
static int
_load4(netsnmp_container *container, u_int load_flags)
{
    int             rc = 0;
    FILE           *in;
    char            line[160];
    
    netsnmp_assert(NULL != container);

#define PROCFILE "/proc/net/tcp"
    if (!(in = fopen(PROCFILE, "r"))) {
        snmp_log(LOG_ERR,"could not open " PROCFILE "\n");
        return -2;
    }
    
    fgets(line, sizeof(line), in); /* skip header */

    /*
     *   sl  local_address rem_address   st tx_queue rx_queue tr tm->when retrnsmt   uid  timeout inode
     *   0: 00000000:8000 00000000:0000 0A 00000000:00000000 00:00000000 00000000    29        0 1028 1 df7b1b80 300 0 0 2 -1
     */
    while (fgets(line, sizeof(line), in)) {
        netsnmp_tcpconn_entry *entry;
        unsigned int    state, local_port, remote_port, tmp_state;
        unsigned long long inode;
        size_t          buf_len, offset;
        char            local_addr[10], remote_addr[10];
        u_char         *tmp_ptr;

        if (6 != (rc = sscanf(line, "%*d: %8[0-9A-Z]:%x %8[0-9A-Z]:%x %x %*x:%*x %*x:%*x %*x %*x %*x %llu",
                              local_addr, &local_port,
                              remote_addr, &remote_port, &tmp_state, &inode))) {
            DEBUGMSGT(("access:tcpconn:container",
                       "error parsing line (%d != 6)\n", rc));
            DEBUGMSGT(("access:tcpconn:container"," line '%s'\n", line));
            continue;
        }
        DEBUGMSGT(("verbose:access:tcpconn:container"," line '%s'\n", line));

        /*
         * check if we care about listen state
         */
        state = (tmp_state & 0xf) < 12 ? linux_states[tmp_state & 0xf] : 2;
        if (load_flags) {
            if (TCPCONNECTIONSTATE_LISTEN == state) {
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

        /*
         */
        entry = netsnmp_access_tcpconn_entry_create();
        if(NULL == entry) {
            rc = -3;
            break;
        }

        /** oddly enough, these appear to already be in network order */
        entry->loc_port = (unsigned short) local_port;
        entry->rmt_port = (unsigned short) remote_port;
        entry->tcpConnState = state;
        entry->pid = netsnmp_get_pid_from_inode(inode);

        /** the addr string may need work */
        buf_len = strlen(local_addr);
        if ((8 != buf_len) ||
            (-1 == netsnmp_addrstr_hton(local_addr, 8))) {
            DEBUGMSGT(("verbose:access:tcpconn:container",
                       " error processing local address\n"));
            netsnmp_access_tcpconn_entry_free(entry);
            continue;
        }
        offset = 0;
        tmp_ptr = entry->loc_addr;
        rc = netsnmp_hex_to_binary(&tmp_ptr, &buf_len,
                                   &offset, 0, local_addr, NULL);
        entry->loc_addr_len = offset;
        if ( 4 != entry->loc_addr_len ) {
            DEBUGMSGT(("access:tcpconn:container",
                       "error parsing local addr (%d != 4)\n",
                       entry->loc_addr_len));
            DEBUGMSGT(("access:tcpconn:container"," line '%s'\n", line));
            netsnmp_access_tcpconn_entry_free(entry);
            continue;
        }

        /** the addr string may need work */
        buf_len = strlen((char*)remote_addr);
        if ((8 != buf_len) ||
            (-1 == netsnmp_addrstr_hton(remote_addr, 8))) {
            DEBUGMSGT(("verbose:access:tcpconn:container",
                       " error processing remote address\n"));
            netsnmp_access_tcpconn_entry_free(entry);
            continue;
        }
        offset = 0;
        tmp_ptr = entry->rmt_addr;
        rc = netsnmp_hex_to_binary(&tmp_ptr, &buf_len,
                                   &offset, 0, remote_addr, NULL);
        entry->rmt_addr_len = offset;
        if ( 4 != entry->rmt_addr_len ) {
            DEBUGMSGT(("access:tcpconn:container",
                       "error parsing remote addr (%d != 4)\n",
                       entry->rmt_addr_len));
            DEBUGMSGT(("access:tcpconn:container"," line '%s'\n", line));
            netsnmp_access_tcpconn_entry_free(entry);
            continue;
        }

        /*
         * add entry to container
         */
        entry->arbitrary_index = CONTAINER_SIZE(container) + 1;
        CONTAINER_INSERT(container, entry);
    }

    fclose(in);

    if(rc<0)
        return rc;

    return 0;
}

#if defined (NETSNMP_ENABLE_IPV6)
/**
 *
 * @retval  0 no errors
 * @retval !0 errors
 */
static int
_load6(netsnmp_container *container, u_int load_flags)
{
    int             rc = 0;
    FILE           *in;
    char            line[180];

    netsnmp_assert(NULL != container);

#undef PROCFILE
#define PROCFILE "/proc/net/tcp6"
    if (!(in = fopen(PROCFILE, "r"))) {
        DEBUGMSGTL(("access:tcpconn:container","could not open " PROCFILE "\n"));
        return -2;
    }

    fgets(line, sizeof(line), in); /* skip header */

    /*
     * Note: PPC (big endian)
     *
     *   sl  local_address rem_address   st tx_queue rx_queue tr tm->when retrnsmt   uid  timeout inode
     *  0: 00000000000000000000000000000001:1466 00000000000000000000000000000000:0000 0A 00000000:00000000 00:00000000 00000000   500        0 326699 1 efb81580 3000 0 0 2 -1
     */
    while (fgets(line, sizeof(line), in)) {
        netsnmp_tcpconn_entry *entry;
        int             state, local_port, remote_port, tmp_state;
        unsigned long long  inode;
        size_t          buf_len, offset;
        char            local_addr[48], remote_addr[48];
        u_char         *tmp_ptr;

        if (6 != (rc = sscanf(line, "%*d: %47[0-9A-Z]:%x %47[0-9A-Z]:%x %x %*x:%*x %*x:%*x %*x %*x %*x %llu",
                              local_addr, &local_port,
                              remote_addr, &remote_port, &tmp_state, &inode))) {
            DEBUGMSGT(("access:tcpconn:container",
                       "error parsing line (%d != 6)\n", rc));
            DEBUGMSGT(("access:tcpconn:container"," line '%s'\n", line));
            continue;
        }
        DEBUGMSGT(("verbose:access:tcpconn:container"," line '%s'\n", line));

        /*
         * check if we care about listen state
         */
        state = (tmp_state & 0xf) < 12 ? linux_states[tmp_state & 0xf] : 2;
        if (load_flags) {
            if (TCPCONNECTIONSTATE_LISTEN == state) {
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

        /*
         */
        entry = netsnmp_access_tcpconn_entry_create();
        if(NULL == entry) {
            rc = -3;
            break;
        }

        /** oddly enough, these appear to already be in network order */
        entry->loc_port = (unsigned short) local_port;
        entry->rmt_port = (unsigned short) remote_port;
        entry->tcpConnState = state;
        entry->pid = netsnmp_get_pid_from_inode(inode);

        /** the addr string may need work */
        buf_len = strlen(local_addr);
        if ((32 != buf_len) ||
            (-1 == netsnmp_addrstr_hton(local_addr, 32))) {
            DEBUGMSGT(("verbose:access:tcpconn:container",
                       " error processing local address\n"));
            netsnmp_access_tcpconn_entry_free(entry);
            continue;
        }
        offset = 0;
        tmp_ptr = entry->loc_addr;
        rc = netsnmp_hex_to_binary(&tmp_ptr, &buf_len,
                                   &offset, 0, local_addr, NULL);
        entry->loc_addr_len = offset;
        if (( 16 != entry->loc_addr_len ) && ( 20 != entry->loc_addr_len )) {
            DEBUGMSGT(("access:tcpconn:container",
                       "error parsing local addr (%d != 16|20)\n",
                       entry->loc_addr_len));
            DEBUGMSGT(("access:tcpconn:container"," line '%s'\n", line));
            netsnmp_access_tcpconn_entry_free(entry);
            continue;
        }

        buf_len = strlen((char*)remote_addr);
        if ((32 != buf_len) ||
            (-1 == netsnmp_addrstr_hton(remote_addr, 32))) {
            DEBUGMSGT(("verbose:access:tcpconn:container",
                       " error processing remote address\n"));
            netsnmp_access_tcpconn_entry_free(entry);
            continue;
        }
        offset = 0;
        tmp_ptr = entry->rmt_addr;
        rc = netsnmp_hex_to_binary(&tmp_ptr, &buf_len,
                                   &offset, 0, remote_addr, NULL);
        entry->rmt_addr_len = offset;
        if (( 16 != entry->rmt_addr_len ) && ( 20 != entry->rmt_addr_len )) {
            DEBUGMSGT(("access:tcpconn:container",
                       "error parsing remote addr (%d != 16|20)\n",
                       entry->rmt_addr_len));
            DEBUGMSGT(("access:tcpconn:container"," line '%s'\n", line));
            netsnmp_access_tcpconn_entry_free(entry);
            continue;
        }


        /*
         * add entry to container
         */
        entry->arbitrary_index = CONTAINER_SIZE(container) + 1;
        CONTAINER_INSERT(container, entry);
    }

    fclose(in);

    if(rc<0)
        return rc;

    return 0;
}
#endif /* NETSNMP_ENABLE_IPV6 */
