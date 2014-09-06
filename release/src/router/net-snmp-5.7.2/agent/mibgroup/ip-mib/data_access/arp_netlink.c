/*
 *  Interface MIB architecture support
 */
#include <net-snmp/net-snmp-config.h>
#include <net-snmp/net-snmp-includes.h>

#include <net-snmp/agent/net-snmp-agent-includes.h>
#include <net-snmp/data_access/arp.h>
#include <net-snmp/data_access/interface.h>

#include <errno.h>
#include <sys/types.h>
#include <linux/types.h>
#include <linux/rtnetlink.h>


static int fillup_entry_info(netsnmp_arp_entry *entry, struct nlmsghdr *h);
static void netsnmp_access_arp_read_netlink(int fd, void *data);

/**
 */
netsnmp_arp_access *
netsnmp_access_arp_create(u_int init_flags,
                          NetsnmpAccessArpUpdate *update_hook,
                          NetsnmpAccessArpGC *gc_hook,
                          int *cache_timeout, int *cache_flags,
                          char *cache_expired)
{
    netsnmp_arp_access *access;

    access = SNMP_MALLOC_TYPEDEF(netsnmp_arp_access);
    if (NULL == access) {
        snmp_log(LOG_ERR,"malloc error in netsnmp_access_arp_create\n");
        return NULL;
    }

    access->arch_magic = NULL;
    access->magic = NULL;
    access->update_hook = update_hook;
    access->gc_hook = gc_hook;
    access->synchronized = 0;

    if (cache_timeout != NULL)
        *cache_timeout = 5;
    if (cache_flags != NULL)
        *cache_flags |= NETSNMP_CACHE_RESET_TIMER_ON_USE | NETSNMP_CACHE_DONT_FREE_BEFORE_LOAD;
    access->cache_expired = cache_expired;

    DEBUGMSGTL(("access:netlink:arp", "create arp cache\n"));

    return access;
}

int netsnmp_access_arp_delete(netsnmp_arp_access *access)
{
    if (NULL == access)
        return 0;

    netsnmp_access_arp_unload(access);
    free(access);

    return 0;
}

int netsnmp_access_arp_load(netsnmp_arp_access *access)
{
    int r, fd = (uintptr_t) access->arch_magic;
    struct {
        struct nlmsghdr n;
        struct ndmsg r;
    } req;

    if (access->synchronized)
        return 0;

    if (fd == 0) {
        struct sockaddr_nl sa;

        fd = socket(PF_NETLINK, SOCK_DGRAM, NETLINK_ROUTE);
        if (fd < 0) {
            snmp_log(LOG_ERR,"netsnmp_access_arp_load: netlink socket create error\n");
            return -1;
        }
        access->arch_magic = (void *)(uintptr_t)fd;

        memset(&sa, 0, sizeof(sa));
        sa.nl_family = AF_NETLINK;
        sa.nl_groups = RTMGRP_NEIGH;
        if (bind(fd, (struct sockaddr*) &sa, sizeof(sa)) < 0) {
            snmp_log(LOG_ERR,"netsnmp_access_arp_load: netlink bind failed\n");
            return -1;
        }

        if (register_readfd(fd, netsnmp_access_arp_read_netlink, access) != 0) {
            snmp_log(LOG_ERR,"netsnmp_access_arp_load: error registering netlink socket\n");
            return -1;
        }
    }

    DEBUGMSGTL(("access:netlink:arp", "synchronizing arp table\n"));

    access->generation++;

    memset(&req, 0, sizeof(req));
    req.n.nlmsg_len = sizeof(req);
    req.n.nlmsg_flags = NLM_F_REQUEST | NLM_F_ROOT;
    req.n.nlmsg_type = RTM_GETNEIGH;
    req.r.ndm_family = AF_UNSPEC;

    r = send(fd, &req, req.n.nlmsg_len, 0);
    if (r < 0) {
        snmp_log(LOG_ERR,"netsnmp_access_arp_refresh: send failed\n");
        return -1;
    }

    while (!access->synchronized)
        netsnmp_access_arp_read_netlink(fd, access);
    access->gc_hook(access);

    return 0;
}

int netsnmp_access_arp_unload(netsnmp_arp_access *access)
{
    int fd;

    DEBUGMSGTL(("access:netlink:arp", "unload arp cache\n"));

    fd = (uintptr_t) access->arch_magic;
    if (fd > 0) {
         unregister_readfd(fd);
         close(fd);
         access->arch_magic = NULL;
	 access->synchronized = 0;
    }
    return 0;
}

static void netsnmp_access_arp_read_netlink(int fd, void *data)
{
    netsnmp_arp_access *access = (netsnmp_arp_access *) data;
    netsnmp_arp_entry *entry;
    char buf[16384];
    struct nlmsghdr *h;
    int r, len;

    do {
        r = recv(fd, buf, sizeof(buf), MSG_DONTWAIT);
        if (r < 0) {
            if (errno == EINTR)
                continue;
            if (errno == EAGAIN)
                return;
            snmp_log(LOG_WARNING, "netlink buffer overrun\n");
            access->synchronized = 0;
            if (access->cache_expired != NULL)
		*access->cache_expired = 1;
            return;
        }
    } while (0);
    len = r;

    for (h = (struct nlmsghdr *) buf; NLMSG_OK(h, len); h = NLMSG_NEXT(h, len)) {
         if (h->nlmsg_type == NLMSG_DONE) {
             access->synchronized = 1;
             continue;
         }

         entry = netsnmp_access_arp_entry_create();
         if (NULL == entry)
             break;

         DEBUGMSGTL(("access:netlink:arp", "arp netlink notification\n"));
    
         entry->generation = access->generation;
         r = fillup_entry_info (entry, h);
         if (r > 0) {
             access->update_hook(access, entry);
         } else {
             if (r < 0) {
                 NETSNMP_LOGONCE((LOG_ERR, "filling entry info failed\n"));
                 DEBUGMSGTL(("access:netlink:arp", "filling entry info failed\n"));
             }
             netsnmp_access_arp_entry_free(entry);
         }
    }
}

static int
fillup_entry_info(netsnmp_arp_entry *entry, struct nlmsghdr *nlmp)
{
    struct ndmsg   *rtmp;
    struct rtattr  *tb[NDA_MAX + 1], *rta;
    int             length;

    rtmp = (struct ndmsg *) NLMSG_DATA(nlmp);
    switch (nlmp->nlmsg_type) {
    case RTM_NEWNEIGH:
        if (rtmp->ndm_state == NUD_FAILED)
            entry->flags = NETSNMP_ACCESS_ARP_ENTRY_FLAG_DELETE;
        else
            entry->flags = 0;
        break;
    case RTM_DELNEIGH:
        entry->flags = NETSNMP_ACCESS_ARP_ENTRY_FLAG_DELETE;
        break;
    case RTM_GETNEIGH:
        return 0;
    default:
        DEBUGMSGTL(("access:netlink:arp",
                    "Wrong Netlink message type %d\n", nlmp->nlmsg_type));
        return -1;
    }

    if (rtmp->ndm_state == NUD_NOARP) {
        /* NUD_NOARP is for broadcast addresses and similar,
         * drop them silently */
        return 0;
    }

    memset(tb, 0, sizeof(struct rtattr *) * (NDA_MAX + 1));
    length = nlmp->nlmsg_len - NLMSG_LENGTH(sizeof(*rtmp));
    rta = ((struct rtattr *) (((char *) (rtmp)) + NLMSG_ALIGN(sizeof(struct ndmsg))));
    while (RTA_OK(rta, length)) {
        if (rta->rta_type <= NDA_MAX)
            tb[rta->rta_type] = rta;
        rta = RTA_NEXT(rta, length);
    }

    /*
     * Fill up the index and addresses
     */
    entry->if_index = rtmp->ndm_ifindex;
    if (tb[NDA_DST]) {
        entry->arp_ipaddress_len = RTA_PAYLOAD(tb[NDA_DST]);
        if (entry->arp_ipaddress_len > sizeof(entry->arp_ipaddress)) {
            snmp_log(LOG_ERR, "netlink ip address length %d is too long\n",
                     entry->arp_ipaddress_len);
            return -1;
        }
        memcpy(entry->arp_ipaddress, RTA_DATA(tb[NDA_DST]),
       entry->arp_ipaddress_len);
    }
    if (tb[NDA_LLADDR]) {
        entry->arp_physaddress_len = RTA_PAYLOAD(tb[NDA_LLADDR]);
        if (entry->arp_physaddress_len > sizeof(entry->arp_physaddress)) {
            snmp_log(LOG_ERR, "netlink hw address length %d is too long\n",
                     entry->arp_physaddress_len);
            return -1;
        }
        memcpy(entry->arp_physaddress, RTA_DATA(tb[NDA_LLADDR]),
               entry->arp_physaddress_len);
    }

    switch (rtmp->ndm_state) {
    case NUD_INCOMPLETE:
        entry->arp_state = INETNETTOMEDIASTATE_INCOMPLETE;
        break;
    case NUD_REACHABLE:
    case NUD_PERMANENT:
        entry->arp_state = INETNETTOMEDIASTATE_REACHABLE;
        break;
    case NUD_STALE:
        entry->arp_state = INETNETTOMEDIASTATE_STALE;
        break;
    case NUD_DELAY:
        entry->arp_state = INETNETTOMEDIASTATE_DELAY;
        break;
    case NUD_PROBE:
        entry->arp_state = INETNETTOMEDIASTATE_PROBE;
        break;
    case NUD_FAILED:
        entry->arp_state = INETNETTOMEDIASTATE_INVALID;
        break;
    case NUD_NONE:
        entry->arp_state = INETNETTOMEDIASTATE_UNKNOWN;
        break;
    default:
        snmp_log(LOG_ERR, "Unrecognized ARP entry state %d", rtmp->ndm_state);
        break;
    }

    switch (rtmp->ndm_state) {
    case NUD_INCOMPLETE:
    case NUD_FAILED:
    case NUD_NONE:
        entry->arp_type = INETNETTOMEDIATYPE_INVALID;
        break;
    case NUD_REACHABLE:
    case NUD_STALE:
    case NUD_DELAY:
    case NUD_PROBE:
        entry->arp_type = INETNETTOMEDIATYPE_DYNAMIC;
        break;
    case NUD_PERMANENT:
        entry->arp_type = INETNETTOMEDIATYPE_STATIC;
        break;
    default:
        entry->arp_type = INETNETTOMEDIATYPE_LOCAL;
        break;
    }

    return 1;
}
