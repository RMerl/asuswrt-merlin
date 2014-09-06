/*
 *  udpEndpointTable MIB architecture support
 *
 * $Id$
 */
#include <net-snmp/net-snmp-config.h>
#include <net-snmp/net-snmp-features.h>
#include <net-snmp/net-snmp-includes.h>

#include <net-snmp/agent/net-snmp-agent-includes.h>
#include <net-snmp/library/file_utils.h>
#include <net-snmp/library/text_utils.h>

#include <net-snmp/data_access/ipaddress.h>
#include <net-snmp/data_access/udp_endpoint.h>

#include "udp-mib/udpEndpointTable/udpEndpointTable_constants.h"
#include "mibgroup/util_funcs/get_pid_from_inode.h"
#include "udp_endpoint_private.h"

#include <fcntl.h>

netsnmp_feature_require(text_utils)
netsnmp_feature_child_of(udp_endpoint_all, libnetsnmpmibs)
netsnmp_feature_child_of(udp_endpoint_writable, udp_endpoint_all)

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
#ifndef NETSNMP_FEATURE_REMOVE_UDP_ENDPOINT_WRITABLE
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
netsnmp_arch_udp_endpoint_delete(netsnmp_udp_endpoint_entry *entry)
{
    if (NULL == entry)
        return -1;
    /** xxx-rks:9 udp_endpoint delete not implemented */
    return -1;
}
#endif /* NETSNMP_FEATURE_REMOVE_UDP_ENDPOINT_WRITABLE */


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

    /* Setup the pid_from_inode table, and fill it.*/
    netsnmp_get_pid_from_inode_init();

    rc = _load4(container, load_flags);
    if(rc < 0) {
        u_int flags = NETSNMP_ACCESS_UDP_ENDPOINT_FREE_KEEP_CONTAINER;
        netsnmp_access_udp_endpoint_container_free(container, flags);
        return rc;
    }

#if defined (NETSNMP_ENABLE_IPV6)
    rc = _load6(container, load_flags);
    if(rc < 0) {
        u_int flags = NETSNMP_ACCESS_UDP_ENDPOINT_FREE_KEEP_CONTAINER;
        netsnmp_access_udp_endpoint_container_free(container, flags);
        return rc;
    }
#endif

    return 0;
}

/**
 * @internal
 * process token value index line
 */
static int
_process_line_udp_ep(netsnmp_line_info *line_info, void *mem,
                     struct netsnmp_line_process_info_s* lpi)
{
    netsnmp_udp_endpoint_entry *ep = (netsnmp_udp_endpoint_entry *)mem;
    char                 *ptr, *sep;
    u_char               *u_ptr;
    size_t                u_ptr_len, offset, len;
    unsigned long long    inode;
    size_t                count = 0;

    /*
     * skip 'sl'
     */
    ptr = skip_not_white(line_info->start);
    if (NULL == ptr) {
        DEBUGMSGTL(("access:udp_endpoint", "no sl '%s'\n",
                       line_info->start));
        return PMLP_RC_MEMORY_UNUSED;
    }
    ptr = skip_white(ptr);
    if (NULL == ptr) {
        DEBUGMSGTL(("text:util:tvi", "no space after sl '%s'\n",
                    line_info->start));
        return PMLP_RC_MEMORY_UNUSED;
    }

    /*
     * get local address. ignore error on hex conversion, since that
     * function doesn't like the ':' between address and port. check the
     * offset to see if it worked. May need to flip string too.
     */
    u_ptr = ep->loc_addr;
    u_ptr_len = sizeof(ep->loc_addr);
    sep = strchr(ptr, ':');
    if (NULL == sep) {
        DEBUGMSGTL(("text:util:tvi", "no ':' '%s'\n",
                    line_info->start));
        return PMLP_RC_MEMORY_UNUSED;
    }
    len = (sep - ptr);
    if (-1 == netsnmp_addrstr_hton(ptr, len)) {
        DEBUGMSGTL(("text:util:tvi", "bad length %d for loc addr '%s'\n",
                    (int)u_ptr_len, line_info->start));
        return PMLP_RC_MEMORY_UNUSED;
    }
    offset = 0;
    netsnmp_hex_to_binary(&u_ptr, &u_ptr_len, &offset, 0, ptr, NULL);
    if ((4 != offset) && (16 != offset)) {
        DEBUGMSGTL(("text:util:tvi", "bad offset %d for loc addr '%s'\n",
                    (int)offset, line_info->start));
        return PMLP_RC_MEMORY_UNUSED;
    }
    ep->loc_addr_len = offset;
    ptr += (offset * 2);
    ++ptr; /* skip ':' */

    /*
     * get local port
     */
    ep->loc_port = strtol(ptr, &ptr, 16);
    ptr = skip_white(ptr);

    /*
     * get remote address. ignore error on hex conversion, since that
     * function doesn't like the ':' between address and port. check the
     * offset to see if it worked. May need to flip string too.
     */
    u_ptr = ep->rmt_addr;
    u_ptr_len = sizeof(ep->rmt_addr);
    sep = strchr(ptr, ':');
    if (NULL == sep) {
        DEBUGMSGTL(("text:util:tvi", "no ':' '%s'\n",
                    line_info->start));
        return PMLP_RC_MEMORY_UNUSED;
    }
    len = (sep - ptr);
    if (-1 == netsnmp_addrstr_hton(ptr, len)) {
        DEBUGMSGTL(("text:util:tvi", "bad length %d for rmt addr '%s'\n",
                    (int)u_ptr_len, line_info->start));
        return PMLP_RC_MEMORY_UNUSED;
    }
    offset = 0;
    netsnmp_hex_to_binary(&u_ptr, &u_ptr_len, &offset, 0, ptr, NULL);
    if ((4 != offset) && (16 != offset)) {
        DEBUGMSGTL(("text:util:tvi", "bad offset %d for rmt addr '%s'\n",
                    (int)offset, line_info->start));
        return PMLP_RC_MEMORY_UNUSED;
    }
    ep->rmt_addr_len = offset;
    ptr += (offset * 2);
    ++ptr; /* skip ':' */

    /*
     * get remote port
     */
    ep->rmt_port = strtol(ptr, &ptr, 16);
    ptr = skip_white(ptr);

    /*
     * get state too
     */
    ep->state = strtol(ptr, &ptr, 16);

    /*
     * Use inode as instance value.
     */
    while (count != 5) {
	ptr = skip_white(ptr);
	ptr = skip_not_white(ptr);
	count++;
    }
    inode = strtoull(ptr, &ptr, 0);
    ep->instance = (u_int)inode;

    /*
     * get the pid also
     */
    ep->pid = netsnmp_get_pid_from_inode(inode);

    ep->index = (uintptr_t)(lpi->user_context);
    lpi->user_context = (void*)((char*)(lpi->user_context) + 1);

    ep->oid_index.oids = &ep->index;
    ep->oid_index.len = 1;

    return PMLP_RC_MEMORY_USED;
}

/**
 *
 * @retval  0 no errors
 * @retval !0 errors
 */
static int
_load4(netsnmp_container *container, u_int load_flags)
{
    netsnmp_file              *fp;
    netsnmp_line_process_info  lpi;

    if (NULL == container)
        return -1;

    /*
     * allocate file resources
     */
    fp = netsnmp_file_fill(NULL, "/proc/net/udp" , O_RDONLY, 0, 0);
    if (NULL == fp) /** msg already logged */
        return -2;
    
    memset(&lpi, 0x0, sizeof(lpi));
    lpi.mem_size = sizeof(netsnmp_udp_endpoint_entry);
    lpi.process = _process_line_udp_ep;
    lpi.user_context = (void*)0;

    container = netsnmp_file_text_parse(fp, container, PM_USER_FUNCTION,
                                        0, &lpi);
    netsnmp_file_release(fp);
    return (NULL == container);
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
    netsnmp_file              *fp;
    netsnmp_line_process_info  lpi;

    if (NULL == container)
        return -1;

    /*
     * allocate file resources
     */
    fp = netsnmp_file_fill(NULL, "/proc/net/udp6" , O_RDONLY, 0, 0);
    if (NULL == fp) /** msg already logged */
        return -2;
    
    memset(&lpi, 0x0, sizeof(lpi));
    lpi.mem_size = sizeof(netsnmp_udp_endpoint_entry);
    lpi.process = _process_line_udp_ep;
    lpi.user_context = (void*)CONTAINER_SIZE(container);

    container = netsnmp_file_text_parse(fp, container, PM_USER_FUNCTION,
                                        0, &lpi);
    netsnmp_file_release(fp);
    return (NULL == container);
}
#endif /* NETSNMP_ENABLE_IPV6 */
