/*
 *  Interface MIB architecture support
 *
 * $Id$
 */
#include <net-snmp/net-snmp-config.h>
#include <net-snmp/net-snmp-features.h>
#include <net-snmp/net-snmp-includes.h>
#include "mibII/mibII_common.h"

#include <net-snmp/agent/net-snmp-agent-includes.h>
#include <net-snmp/data_access/ipaddress.h>
#include <net-snmp/data_access/interface.h>

#include "ip-mib/ipAddressTable/ipAddressTable_constants.h"
#include "ip-mib/ipAddressPrefixTable/ipAddressPrefixTable_constants.h"
#include "mibgroup/util_funcs.h"

#include <errno.h>
#include <sys/ioctl.h>

netsnmp_feature_require(prefix_info)
netsnmp_feature_require(find_prefix_info)

netsnmp_feature_child_of(ipaddress_arch_entry_copy, ipaddress_common)

#ifdef NETSNMP_FEATURE_REQUIRE_IPADDRESS_ARCH_ENTRY_COPY
netsnmp_feature_require(ipaddress_ioctl_entry_copy)
#endif /* NETSNMP_FEATURE_REQUIRE_IPADDRESS_ARCH_ENTRY_COPY */

#if defined (NETSNMP_ENABLE_IPV6)
#include <linux/types.h>
#include <asm/types.h>
#if defined(HAVE_LINUX_RTNETLINK_H)
#include <linux/netlink.h>
#include <linux/rtnetlink.h>
#ifdef RTMGRP_IPV6_PREFIX
#define SUPPORT_PREFIX_FLAGS 1
#endif /* RTMGRP_IPV6_PREFIX */
#endif /* HAVE_LINUX_RTNETLINK_H */
#endif

#include "ipaddress_ioctl.h"
#ifdef SUPPORT_PREFIX_FLAGS
extern prefix_cbx *prefix_head_list;
#endif
int _load_v6(netsnmp_container *container, int idx_offset);
#ifdef HAVE_LINUX_RTNETLINK_H
int
netsnmp_access_ipaddress_extra_prefix_info(int index,
                                           u_long *preferedlt,
                                           ulong *validlt,
                                           char *addr);
#endif

/*
 * initialize arch specific storage
 *
 * @retval  0: success
 * @retval <0: error
 */
int
netsnmp_arch_ipaddress_entry_init(netsnmp_ipaddress_entry *entry)
{
    /*
     * init ipv4 stuff
     */
    if (NULL == netsnmp_ioctl_ipaddress_entry_init(entry))
        return -1;

    /*
     * init ipv6 stuff
     *   so far, we can just share the ipv4 stuff, so nothing to do
     */
    
    return 0;
}

/*
 * cleanup arch specific storage
 */
void
netsnmp_arch_ipaddress_entry_cleanup(netsnmp_ipaddress_entry *entry)
{
    /*
     * cleanup ipv4 stuff
     */
    netsnmp_ioctl_ipaddress_entry_cleanup(entry);

    /*
     * cleanup ipv6 stuff
     *   so far, we can just share the ipv4 stuff, so nothing to do
     */
}

#ifndef NETSNMP_FEATURE_REMOVE_IPADDRESS_ARCH_ENTRY_COPY
/*
 * copy arch specific storage
 */
int
netsnmp_arch_ipaddress_entry_copy(netsnmp_ipaddress_entry *lhs,
                                  netsnmp_ipaddress_entry *rhs)
{
    int rc;

    /*
     * copy ipv4 stuff
     */
    rc = netsnmp_ioctl_ipaddress_entry_copy(lhs, rhs);
    if (rc)
        return rc;

    /*
     * copy ipv6 stuff
     *   so far, we can just share the ipv4 stuff, so nothing to do
     */

    return rc;
}
#endif /* NETSNMP_FEATURE_REMOVE_IPADDRESS_ARCH_ENTRY_COPY */

/*
 * create a new entry
 */
int
netsnmp_arch_ipaddress_create(netsnmp_ipaddress_entry *entry)
{
    if (NULL == entry)
        return -1;

    if (4 == entry->ia_address_len) {
        return _netsnmp_ioctl_ipaddress_set_v4(entry);
    } else if (16 == entry->ia_address_len) {
        return _netsnmp_ioctl_ipaddress_set_v6(entry);
    } else {
        DEBUGMSGT(("access:ipaddress:create", "wrong length of IP address\n"));
        return -2;
    }
}

/*
 * create a new entry
 */
int
netsnmp_arch_ipaddress_delete(netsnmp_ipaddress_entry *entry)
{
    if (NULL == entry)
        return -1;

    if (4 == entry->ia_address_len) {
        return _netsnmp_ioctl_ipaddress_delete_v4(entry);
    } else if (16 == entry->ia_address_len) {
        return _netsnmp_ioctl_ipaddress_delete_v6(entry);
    } else {
        DEBUGMSGT(("access:ipaddress:create", "only ipv4 supported\n"));
        return -2;
    }
}

/**
 *
 * @retval  0 no errors
 * @retval !0 errors
 */
int
netsnmp_arch_ipaddress_container_load(netsnmp_container *container,
                                      u_int load_flags)
{
    int rc = 0, idx_offset = 0;

    if (0 == (load_flags & NETSNMP_ACCESS_IPADDRESS_LOAD_IPV6_ONLY)) {
        rc = _netsnmp_ioctl_ipaddress_container_load_v4(container, idx_offset);
        if(rc < 0) {
            u_int flags = NETSNMP_ACCESS_IPADDRESS_FREE_KEEP_CONTAINER;
            netsnmp_access_ipaddress_container_free(container, flags);
        }
    }

#if defined (NETSNMP_ENABLE_IPV6)

    if (0 == (load_flags & NETSNMP_ACCESS_IPADDRESS_LOAD_IPV4_ONLY)) {
        if (rc < 0)
            rc = 0;

        idx_offset = rc;

        /*
         * load ipv6, ignoring errors if file not found
         */
        rc = _load_v6(container, idx_offset);
        if (-2 == rc)
            rc = 0;
        else if(rc < 0) {
            u_int flags = NETSNMP_ACCESS_IPADDRESS_FREE_KEEP_CONTAINER;
            netsnmp_access_ipaddress_container_free(container, flags);
        }
    }
#endif

    /*
     * return no errors (0) if we found any interfaces
     */
    if(rc > 0)
        rc = 0;

    return rc;
}

#if defined (NETSNMP_ENABLE_IPV6)
/**
 */
int
_load_v6(netsnmp_container *container, int idx_offset)
{
#ifndef HAVE_LINUX_RTNETLINK_H
    DEBUGMSGTL(("access:ipaddress:container",
                "cannot get ip address information"
                "as netlink socket is not available\n"));
    return -1;
#else
    FILE           *in;
    char            line[80], addr[40];
    char            if_name[IFNAMSIZ+1];/* +1 for '\0' because of the ugly sscanf below */ 
    u_char          *buf;
    int             if_index, pfx_len, scope, flags, rc = 0;
    size_t          in_len, out_len;
    netsnmp_ipaddress_entry *entry;
    _ioctl_extras           *extras;
    struct address_flag_info addr_info;
    
    netsnmp_assert(NULL != container);

#define PROCFILE "/proc/net/if_inet6"
    if (!(in = fopen(PROCFILE, "r"))) {
        DEBUGMSGTL(("access:ipaddress:container","could not open " PROCFILE "\n"));
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
        rc = sscanf(line, "%39s %08x %08x %04x %02x %" SNMP_MACRO_VAL_TO_STR(IFNAMSIZ) "s\n",
                    addr, &if_index, &pfx_len, &scope, &flags, if_name);
        if( 6 != rc ) {
            snmp_log(LOG_ERR, PROCFILE " data format error (%d!=6), line ==|%s|\n",
                     rc, line);
            continue;
        }
        DEBUGMSGTL(("access:ipaddress:container",
                    "addr %s, index %d, pfx %d, scope %d, flags 0x%X, name %s\n",
                    addr, if_index, pfx_len, scope, flags, if_name));
        /*
         */
        entry = netsnmp_access_ipaddress_entry_create();
        if(NULL == entry) {
            rc = -3;
            break;
        }

        in_len = entry->ia_address_len = sizeof(entry->ia_address);
        netsnmp_assert(16 == in_len);
        out_len = 0;
        entry->flags = flags;
        buf = entry->ia_address;
        if(1 != netsnmp_hex_to_binary(&buf, &in_len,
                                      &out_len, 0, addr, ":")) {
            snmp_log(LOG_ERR,"error parsing '%s', skipping\n",
                     entry->ia_address);
            netsnmp_access_ipaddress_entry_free(entry);
            continue;
        }
        netsnmp_assert(16 == out_len);
        entry->ia_address_len = out_len;

        entry->ns_ia_index = ++idx_offset;

        /*
         * save if name
         */
        extras = netsnmp_ioctl_ipaddress_extras_get(entry);
        memcpy(extras->name, if_name, sizeof(extras->name));
        extras->flags = flags;

        /*
         * yyy-rks: optimization: create a socket outside the loop and use
         * netsnmp_access_interface_ioctl_ifindex_get() here, since
         * netsnmp_access_interface_index_find will open/close a socket
         * every time it is called.
         */
        entry->if_index = netsnmp_access_interface_index_find(if_name);
        memset(&addr_info, 0, sizeof(struct address_flag_info));
        addr_info = netsnmp_access_other_info_get(entry->if_index, AF_INET6);

        /*
          #define IPADDRESSSTATUSTC_PREFERRED  1
          #define IPADDRESSSTATUSTC_DEPRECATED  2
          #define IPADDRESSSTATUSTC_INVALID  3
          #define IPADDRESSSTATUSTC_INACCESSIBLE  4
          #define IPADDRESSSTATUSTC_UNKNOWN  5
          #define IPADDRESSSTATUSTC_TENTATIVE  6
          #define IPADDRESSSTATUSTC_DUPLICATE  7
        */
        if((flags & IFA_F_PERMANENT) || (!flags))
            entry->ia_status = IPADDRESSSTATUSTC_PREFERRED; /* ?? */
#ifdef IFA_F_TEMPORARY
        else if(flags & IFA_F_TEMPORARY)
            entry->ia_status = IPADDRESSSTATUSTC_PREFERRED; /* ?? */
#endif
        else if(flags & IFA_F_DEPRECATED)
            entry->ia_status = IPADDRESSSTATUSTC_DEPRECATED;
        else if(flags & IFA_F_TENTATIVE)
            entry->ia_status = IPADDRESSSTATUSTC_TENTATIVE;
        else {
            entry->ia_status = IPADDRESSSTATUSTC_UNKNOWN;
            DEBUGMSGTL(("access:ipaddress:ipv6",
                        "unknown flags 0x%x\n", flags));
        }

        /*
         * if it's not multi, it must be uni.
         *  (an ipv6 address is never broadcast)
         */
        if(addr_info.anycastflg)
            entry->ia_type = IPADDRESSTYPE_ANYCAST;
        else
            entry->ia_type = IPADDRESSTYPE_UNICAST;


        entry->ia_prefix_len = pfx_len;

        /*
         * can we figure out if an address is from DHCP?
         * use manual until then...
         *
         *#define IPADDRESSORIGINTC_OTHER  1
         *#define IPADDRESSORIGINTC_MANUAL  2
         *#define IPADDRESSORIGINTC_DHCP  4
         *#define IPADDRESSORIGINTC_LINKLAYER  5
         *#define IPADDRESSORIGINTC_RANDOM  6
         *
         * are 'local' address assigned by link layer??
         */
         if (!flags)
             entry->ia_origin = IPADDRESSORIGINTC_LINKLAYER;
#ifdef IFA_F_TEMPORARY
         else if (flags & IFA_F_TEMPORARY)
             entry->ia_origin = IPADDRESSORIGINTC_RANDOM;
#endif
         else if (IN6_IS_ADDR_LINKLOCAL(entry->ia_address))
             entry->ia_origin = IPADDRESSORIGINTC_LINKLAYER;
         else
             entry->ia_origin = IPADDRESSORIGINTC_MANUAL;

         if(entry->ia_origin == IPADDRESSORIGINTC_LINKLAYER)
            entry->ia_storagetype = STORAGETYPE_PERMANENT;

        /* xxx-rks: what can we do with scope? */
#ifdef HAVE_LINUX_RTNETLINK_H
        if(netsnmp_access_ipaddress_extra_prefix_info(entry->if_index, &entry->ia_prefered_lifetime
                                                      ,&entry->ia_valid_lifetime, addr) < 0){
           DEBUGMSGTL(("access:ipaddress:container", "unable to fetch extra prefix info\n"));
        }
#else
        entry->ia_prefered_lifetime = 0;
        entry->ia_valid_lifetime = 0;
#endif
#ifdef SUPPORT_PREFIX_FLAGS
        {
        prefix_cbx      prefix_val;
        memset(&prefix_val, 0, sizeof(prefix_cbx));
        if(net_snmp_find_prefix_info(&prefix_head_list, addr, &prefix_val) < 0) {
           DEBUGMSGTL(("access:ipaddress:container", "unable to find info\n"));
           entry->ia_onlink_flag = 1;  /*Set by default as true*/
           entry->ia_autonomous_flag = 2; /*Set by default as false*/

        } else {
           entry->ia_onlink_flag = prefix_val.ipAddressPrefixOnLinkFlag; 
           entry->ia_autonomous_flag = prefix_val.ipAddressPrefixAutonomousFlag;
        }
        }
#else
        entry->ia_onlink_flag = 1;  /*Set by default as true*/
        entry->ia_autonomous_flag = 2; /*Set by default as false*/
#endif

        /*
         * add entry to container
         */
        if (CONTAINER_INSERT(container, entry) < 0) {
            DEBUGMSGTL(("access:ipaddress:container","error with ipaddress_entry: insert into container failed.\n"));
            netsnmp_access_ipaddress_entry_free(entry);
            continue;
        }
    }

    fclose(in);

    if(rc<0)
        return rc;

    return idx_offset;
}

struct address_flag_info
netsnmp_access_other_info_get(int index, int family)
{
   struct {
           struct nlmsghdr n;
           struct ifaddrmsg r;
           char   buf[1024];
   } req;
   struct address_flag_info addr;
   struct rtattr    *rta;
   int    status;
   char   buf[16384];
   struct nlmsghdr  *nlmp;
   struct ifaddrmsg *rtmp;
   struct rtattr    *rtatp;
   int    rtattrlen;
   int    sd;

   memset(&addr, 0, sizeof(struct address_flag_info));
   sd = socket(PF_NETLINK, SOCK_DGRAM, NETLINK_ROUTE);
   if(sd < 0) {
      snmp_log(LOG_ERR, "could not open netlink socket\n");
      return addr;
   }

   memset(&req, 0, sizeof(req));
   req.n.nlmsg_len = NLMSG_LENGTH(sizeof(struct ifaddrmsg));
   req.n.nlmsg_flags = NLM_F_REQUEST | NLM_F_ROOT;
   req.n.nlmsg_type = RTM_GETADDR;
   req.r.ifa_family = family;
   rta = (struct rtattr *)(((char *)&req) + NLMSG_ALIGN(req.n.nlmsg_len));
   if(family == AF_INET)
      rta->rta_len = RTA_LENGTH(4);
   else
      rta->rta_len = RTA_LENGTH(16);

    status = send(sd, &req, req.n.nlmsg_len, 0);
    if (status < 0) {
        snmp_log(LOG_ERR, "could not send netlink request\n");
        return addr;
    }

    status = recv(sd, buf, sizeof(buf), 0);
    if (status < 0) {
        snmp_log (LOG_ERR, "could not recieve netlink request\n");
        return addr;
    }

    if(status == 0) {
       snmp_log (LOG_ERR, "nothing to read\n");
       return addr;
    }

    for(nlmp = (struct nlmsghdr *)buf; status > sizeof(*nlmp);) {
        int len = nlmp->nlmsg_len;
        int req_len = len - sizeof(*nlmp);

        if (req_len < 0 || len > status) {
            snmp_log (LOG_ERR, "invalid netlink message\n");
            return addr;
        }

        if (!NLMSG_OK(nlmp, status)) {
            snmp_log (LOG_ERR, "invalid NLMSG message\n");
            return addr;
        }
        rtmp = (struct ifaddrmsg *)NLMSG_DATA(nlmp);
        rtatp = (struct rtattr *)IFA_RTA(rtmp);
        rtattrlen = IFA_PAYLOAD(nlmp);
        if(index == rtmp->ifa_index){
           for (; RTA_OK(rtatp, rtattrlen); rtatp = RTA_NEXT(rtatp, rtattrlen)) {
                if(rtatp->rta_type == IFA_BROADCAST){
                   addr.addr = ((struct in_addr *)RTA_DATA(rtatp))->s_addr;
                   addr.bcastflg = 1;
                }
                if(rtatp->rta_type == IFA_ANYCAST){
                   addr.addr = ((struct in_addr *)RTA_DATA(rtatp))->s_addr;
                   addr.anycastflg = 1;
                }
           }
        }
        status -= NLMSG_ALIGN(len);
        nlmp = (struct nlmsghdr*)((char*)nlmp + NLMSG_ALIGN(len));
    }
    close(sd);
    return addr;
#endif
}

#ifdef HAVE_LINUX_RTNETLINK_H
int
netsnmp_access_ipaddress_extra_prefix_info(int index, u_long *preferedlt,
                                           ulong *validlt, char *addr)
{

    struct {
            struct nlmsghdr nlhdr;
            struct ifaddrmsg ifaceinfo;
            char   buf[1024];
    } req;

    struct rtattr        *rta;
    int                  status;
    char                 buf[16384];
    char                 tmpaddr[40];
    struct nlmsghdr      *nlmp;
    struct ifaddrmsg     *rtmp;
    struct rtattr        *rtatp;
    struct ifa_cacheinfo *cache_info;
    struct in6_addr      *in6p;
    int                  rtattrlen;
    int                  sd;
    int                  reqaddr = 0;
    sd = socket(PF_NETLINK, SOCK_DGRAM, NETLINK_ROUTE);
    if(sd < 0) {
       snmp_log(LOG_ERR, "could not open netlink socket\n");
       return -1;
    }
    memset(&req, 0, sizeof(req));
    req.nlhdr.nlmsg_len = NLMSG_LENGTH(sizeof(struct ifaddrmsg));
    req.nlhdr.nlmsg_flags = NLM_F_REQUEST | NLM_F_ROOT;
    req.nlhdr.nlmsg_type = RTM_GETADDR;
    req.ifaceinfo.ifa_family = AF_INET6;
    rta = (struct rtattr *)(((char *)&req) + NLMSG_ALIGN(req.nlhdr.nlmsg_len));
    rta->rta_len = RTA_LENGTH(16); /*For ipv6*/

    status = send (sd, &req, req.nlhdr.nlmsg_len, 0);
    if (status < 0) {
        snmp_log(LOG_ERR, "could not send netlink request\n");
        return -1;
    }
    status = recv (sd, buf, sizeof(buf), 0);
    if (status < 0) {
        snmp_log (LOG_ERR, "could not recieve netlink request\n");
        return -1;
    }
    if (status == 0) {
       snmp_log (LOG_ERR, "nothing to read\n");
       return -1;
    }
    for (nlmp = (struct nlmsghdr *)buf; status > sizeof(*nlmp); ){

        int len = nlmp->nlmsg_len;
        int req_len = len - sizeof(*nlmp);

        if (req_len < 0 || len > status) {
            snmp_log (LOG_ERR, "invalid netlink message\n");
            return -1;
        }

        if (!NLMSG_OK (nlmp, status)) {
            snmp_log (LOG_ERR, "invalid NLMSG message\n");
            return -1;
        }
        rtmp = (struct ifaddrmsg *)NLMSG_DATA(nlmp);
        rtatp = (struct rtattr *)IFA_RTA(rtmp);
        rtattrlen = IFA_PAYLOAD(nlmp);
        if(index == rtmp->ifa_index) {
           for (; RTA_OK(rtatp, rtattrlen); rtatp = RTA_NEXT(rtatp, rtattrlen)) {
                if(rtatp->rta_type == IFA_ADDRESS) {
                   in6p = (struct in6_addr *)RTA_DATA(rtatp);
                   sprintf(tmpaddr, "%04x%04x%04x%04x%04x%04x%04x%04x", NIP6(*in6p));
                   if(!strcmp(tmpaddr ,addr))
                       reqaddr = 1;
                }
                if(rtatp->rta_type == IFA_CACHEINFO) {
                   cache_info = (struct ifa_cacheinfo *)RTA_DATA(rtatp);
                   if(reqaddr) {
                      reqaddr = 0;
                      *validlt = cache_info->ifa_valid;
                      *preferedlt = cache_info->ifa_prefered;
                   }

                }

           }
        }
        status -= NLMSG_ALIGN(len);
        nlmp = (struct nlmsghdr*)((char*)nlmp + NLMSG_ALIGN(len));
    }
    close(sd);
    return 0;
}
#endif
#endif

