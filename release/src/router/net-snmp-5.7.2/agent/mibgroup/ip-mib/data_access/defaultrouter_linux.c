/*
 *  Interface MIB architecture support
 *
 * $Id:$
 */
#include <net-snmp/net-snmp-config.h>
#include <net-snmp/net-snmp-includes.h>

#include <net-snmp/agent/net-snmp-agent-includes.h>
#include <net-snmp/data_access/defaultrouter.h>

#include "ip-mib/ipDefaultRouterTable/ipDefaultRouterTable.h"

#include <asm/types.h>
#ifdef HAVE_LINUX_RTNETLINK_H
#include <linux/netlink.h>
#include <linux/rtnetlink.h>
#endif
#include <sys/socket.h>
#include <arpa/inet.h>
#include <errno.h>
#include <unistd.h>

#define RCVBUF_SIZE 32768
#define SNDBUF_SIZE 512

#ifdef NETSNMP_ENABLE_IPV6
#define DR_ADDRSTRLEN  INET6_ADDRSTRLEN
#else
#define DR_ADDRSTRLEN  INET_ADDRSTRLEN
#endif

/**---------------------------------------------------------------------*/
/*
 * local static prototypes
 */
static int _load(netsnmp_container *container);


/*
 * initialize arch specific storage
 *
 * @retval  0: success
 * @retval <0: error
 */
int
netsnmp_arch_defaultrouter_entry_init(netsnmp_defaultrouter_entry *entry)
{
    /*
     * init
     */
    return 0;
}

/**
 *
 * @retval  0 no errors
 * @retval !0 errors
 */
int
netsnmp_arch_defaultrouter_container_load(netsnmp_container *container,
                                          u_int load_flags)
{
    int rc = 0;

    DEBUGMSGTL(("access:defaultrouter:entry:arch", "load (linux)\n"));

    rc = _load(container);
    if (rc < 0) {
        u_int flags = NETSNMP_ACCESS_DEFAULTROUTER_FREE_KEEP_CONTAINER;
        netsnmp_access_defaultrouter_container_free(container, flags);
    }

    return rc;
}

/**
 *
 * @retval  0 no errors
 * @retval !0 errors
 */
static int
_load(netsnmp_container *container)
{
#ifndef HAVE_LINUX_RTNETLINK_H
    DEBUGMSGTL(("access:defaultrouter",
                "cannot get default router information"
                "as netlink socket is not available\n"));
    return -1;
#else
    int rc = 0;
    int idx_offset = 0;
    netsnmp_defaultrouter_entry *entry;
    int nlsk;
    struct sockaddr_nl addr;
    int rcvbuf_size = RCVBUF_SIZE;
    unsigned char rcvbufmem[RCVBUF_SIZE + sizeof(intmax_t)];
    unsigned char sndbufmem[SNDBUF_SIZE + sizeof(intmax_t)];
    /*
     * Buffers must be memory aligned.
     * Message structure internal alignment is maintained by the netlink API.
     */
    unsigned char *rcvbuf = rcvbufmem +
          sizeof(intmax_t) - (((intptr_t)rcvbufmem) % sizeof(intmax_t));
    unsigned char *sndbuf = sndbufmem +
          sizeof(intmax_t) - (((intptr_t)sndbufmem) % sizeof(intmax_t));
    struct nlmsghdr *hdr;
    struct rtmsg *rthdr;
    int count;
    int end_of_message = 0;
    long hz = sysconf(_SC_CLK_TCK);

    netsnmp_assert(NULL != container);

    /*
     * Open a netlink socket
     */
    nlsk = socket(AF_NETLINK, SOCK_DGRAM, NETLINK_ROUTE);
    if (nlsk < 0) {
        snmp_log(LOG_ERR, "Could not open netlink socket : %s\n",
                 strerror(errno));
        return -1;
    }

    if (setsockopt(nlsk, SOL_SOCKET, SO_RCVBUF,
                   &rcvbuf_size, sizeof(rcvbuf_size)) < 0) {
        snmp_log(LOG_ERR, "Could not open netlink socket : %s\n",
                 strerror(errno));
        close(nlsk);
        return -1;
    }
    
    memset(&addr, '\0', sizeof(struct sockaddr_nl));
    addr.nl_family = AF_NETLINK;

    memset(sndbuf, '\0', SNDBUF_SIZE);
    hdr = (struct nlmsghdr *)sndbuf;
    hdr->nlmsg_type = RTM_GETROUTE;
    hdr->nlmsg_pid = getpid();
    hdr->nlmsg_seq = 0;
    hdr->nlmsg_flags = NLM_F_ROOT | NLM_F_MATCH | NLM_F_REQUEST;
    hdr->nlmsg_len = NLMSG_LENGTH(sizeof(struct rtmsg));
    rthdr = (struct rtmsg *)NLMSG_DATA(hdr);
    rthdr->rtm_table = RT_TABLE_MAIN;

    /*
     * Send a request to the kernel to dump the routing table to us
     */
    count = sendto(nlsk, sndbuf, hdr->nlmsg_len, 0,
                   (struct sockaddr *)&addr, sizeof(struct sockaddr_nl));
    if (count < 0) {
        snmp_log(LOG_ERR, "unable to send netlink message to kernel : %s\n",
                 strerror(errno));
        close(nlsk);
        return -2;
    }

    /*
     * Now listen for response
     */
    do {
        struct nlmsghdr *nlmhp;
        struct rtmsg *rtmp;
        struct rtattr *rtap;
        struct rta_cacheinfo *rtci;
        socklen_t sock_len;
        int rtcount;

        memset(rcvbuf, '\0', RCVBUF_SIZE);
        sock_len = sizeof(struct sockaddr_nl);

        /*
         * Get the message
         */
        count = recvfrom(nlsk, rcvbuf, RCVBUF_SIZE, 0,
                         (struct sockaddr *)&addr, &sock_len);
        if (count < 0) {
            snmp_log(LOG_ERR, "unable to receive netlink messages: %s\n",
                     strerror(errno));
            rc = -1;
            break;
        }

        /*
         * Walk all of the returned messages
         */
        nlmhp = (struct nlmsghdr *)rcvbuf;
        while (NLMSG_OK(nlmhp, count)) {
            u_char addresstype;
            char   address[NETSNMP_ACCESS_DEFAULTROUTER_BUF_SIZE + 1];
            size_t address_len =  0;
            int    if_index    = -1;
            u_long lifetime    =  0;
            int    preference  = -3;

            /*
             * Make sure the message is ok
             */
            if (nlmhp->nlmsg_type == NLMSG_ERROR) {
                snmp_log(LOG_ERR, "kernel produced nlmsg err\n");
                rc = -1;
                break;
            }

            /*
             * End of message, we're done
             */
            if (nlmhp->nlmsg_type & NLMSG_DONE) {
                end_of_message = 1;
                break;
            }

            /*
             * Get the pointer to the rtmsg struct
             */
            rtmp = NLMSG_DATA(nlmhp);

            /*
             * zero length destination is a default route
             */
            if (rtmp->rtm_dst_len != 0)
                goto next_nlmsghdr;

            /*
             * Start scanning the attributes for needed info
             */
            if (rtmp->rtm_family == AF_INET) {
                addresstype = INETADDRESSTYPE_IPV4;
                lifetime = IPDEFAULTROUTERLIFETIME_MAX;     /* infinity */
            }
#ifdef NETSNMP_ENABLE_IPV6
            else if (rtmp->rtm_family == AF_INET6) {
                addresstype = INETADDRESSTYPE_IPV6;
                /* router lifetime for IPv6 is retrieved by RTA_CACHEINFO */
                lifetime = 0;
            }
#endif
            else
                goto next_nlmsghdr; /* skip, we don't care about this route */

            preference = 0;     /* preference is medium(0) for now */

            rtap = RTM_RTA(rtmp);
            rtcount = RTM_PAYLOAD(nlmhp);
            while (RTA_OK(rtap, rtcount)) {
                switch (rtap->rta_type) {
                    case RTA_OIF:
                        if_index = *(int *)(RTA_DATA(rtap));
                        break;

                    case RTA_GATEWAY:
                        address_len = RTA_PAYLOAD(rtap);
                        memset(address, '\0', sizeof(address));
                        memcpy(address, RTA_DATA(rtap), address_len);
                        break;

#ifdef NETSNMP_ENABLE_IPV6
                    case RTA_CACHEINFO:
                        rtci = RTA_DATA(rtap);
                        if ((rtmp->rtm_flags & RTM_F_CLONED) ||
                            (rtci && rtci->rta_expires)) {
                            lifetime = rtci->rta_expires / hz;
                        }
                        break;
#endif

                    default:
                        break;
                }   /* switch */

                rtap = RTA_NEXT(rtap, rtcount);
            } /* while RTA_OK(rtap) */

            if (address_len != 0 && if_index != -1 &&
                lifetime != 0 && preference != -3 ) {
                DEBUGIF("access:defaultrouter") {
                    char addr_str[DR_ADDRSTRLEN];
                    memset(addr_str, '\0', DR_ADDRSTRLEN);

                    if (rtmp->rtm_family == AF_INET)
                        inet_ntop(AF_INET, address, addr_str, DR_ADDRSTRLEN);
#ifdef NETSNMP_ENABLE_IPV6
                    else
                        inet_ntop(AF_INET6, address, addr_str, DR_ADDRSTRLEN);
#endif
                    DEBUGMSGTL(("access:defaultrouter",
                                "found default route: %s if_index %d "
                                "lifetime %lu preference %d\n",
                                addr_str, if_index, lifetime, preference));
                }

                entry = netsnmp_access_defaultrouter_entry_create();
                if (NULL == entry) {
                    rc = -3;
                    break;
                }

                entry->ns_dr_index    = ++idx_offset;
                entry->dr_addresstype = addresstype;
                entry->dr_address_len = address_len;
                memcpy(entry->dr_address, address,
                       NETSNMP_ACCESS_DEFAULTROUTER_BUF_SIZE);
                entry->dr_if_index    = if_index;
                entry->dr_lifetime    = lifetime;
                entry->dr_preference  = preference;

                if (CONTAINER_INSERT(container, entry) < 0)
                {
                    DEBUGMSGTL(("access:arp:container",
                                "error with defaultrouter_entry: "
                                "insert into container failed.\n"));
                    netsnmp_access_defaultrouter_entry_free(entry);
                }
            }

next_nlmsghdr:
            nlmhp = NLMSG_NEXT(nlmhp, count);
        } /* while NLMSG_OK(nlmhp) */

        if (rc < 0)
            break;

    } while (!end_of_message);

    close(nlsk);
    return rc;
#endif  /* HAVE_LINUX_RTNETLINK_H */
}
