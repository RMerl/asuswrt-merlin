/*
 * standard Net-SNMP includes 
 */

#include <net-snmp/net-snmp-config.h>
#include <net-snmp/net-snmp-includes.h>
#include <net-snmp/agent/net-snmp-agent-includes.h>

/*
 * include our parent header 
 */
#include "etherlike-mib/dot3StatsTable/dot3StatsTable.h"
#include "etherlike-mib/dot3StatsTable/dot3StatsTable_data_access.h"
#include "etherlike-mib/dot3StatsTable/ioctl_imp_common.h"

/*
 * @retval  0 success
 * @retval -1 getifaddrs failed 
 * @retval -2 memory allocation failed
 */

struct ifname *
dot3stats_interface_name_list_get (struct ifname *list_head, int *retval)
{
    struct ifaddrs *addrs = NULL, *p = NULL;
    struct ifname *nameptr1=NULL, *nameptr2 = NULL;

    DEBUGMSGTL(("access:dot3StatsTable:interface_name_list_get",
                "called\n"));

    if ((getifaddrs(&addrs)) < 0) {
        DEBUGMSGTL(("access:dot3StatsTable:interface_name_list_get",
                    "getifaddrs failed\n"));
        snmp_log (LOG_ERR, "access:dot3StatsTable,interface_name_list_get, getifaddrs failed\n");
        *retval = -1;
        return NULL;
    }

    for (p = addrs; p; p = p->ifa_next) {

        if (!list_head) {
            if ( (list_head = (struct ifname *) malloc (sizeof(struct ifname))) < 0) {
                DEBUGMSGTL(("access:dot3StatsTable:interface_name_list_get",
                            "memory allocation failed\n"));
                snmp_log (LOG_ERR, "access:dot3StatsTable,interface_name_list_get, memory allocation failed\n");
                freeifaddrs(addrs);
                *retval = -2;
                return NULL;
            }
            memset(list_head, 0, sizeof (struct ifname));
            strlcpy(list_head->name, p->ifa_name, IF_NAMESIZE);
            continue;
        }

         for (nameptr1 = list_head; nameptr1; nameptr2 = nameptr1, nameptr1 = nameptr1->ifn_next)
            if (!strncmp(p->ifa_name, nameptr1->name, IF_NAMESIZE))
                break;

        if (nameptr1)
            continue;

        if ( (nameptr2->ifn_next = (struct ifname *) malloc (sizeof(struct ifname))) < 0) {
            DEBUGMSGTL(("access:dot3StatsTable:interface_name_list_get",
                        "memory allocation failed\n"));
            snmp_log (LOG_ERR, "access:dot3StatsTable,interface_name_list_get, memory allocation failed\n");
            dot3stats_interface_name_list_free (list_head);
            freeifaddrs(addrs);
            *retval = -2;
            return NULL;
        }
        nameptr2 = nameptr2->ifn_next;
        memset(nameptr2, 0, sizeof (struct ifname));
        strlcpy(nameptr2->name, p->ifa_name, IF_NAMESIZE);
        continue;

    }

    freeifaddrs(addrs);
    return list_head;
}

/*
 * @retval 0 success
 * @retval -1 invalid pointer
 */

int
dot3stats_interface_name_list_free (struct ifname *list_head)
{
    struct ifname *nameptr1 = NULL, *nameptr2 = NULL;

    DEBUGMSGTL(("access:dot3StatsTable:interface_name_list_free",
                "called\n"));

    if (!list_head) {
        snmp_log (LOG_ERR, "access:dot3StatsTable:interface_name_list_free: invalid pointer list_head");
        DEBUGMSGTL(("access:dot3StatsTable:interface_name_list_free",
                    "invalid pointer list_head\n"));
        return -1;
    }

    for (nameptr1 = list_head; nameptr1; nameptr1 = nameptr2) {
            nameptr2 = nameptr1->ifn_next;
            free (nameptr1);
    }

    return 0;
}

/*
 * @retval  0 : not found
 * @retval !0 : ifIndex
 */

int 
dot3stats_interface_ioctl_ifindex_get (int fd, const char *name) {
#ifndef SIOCGIFINDEX
    return 0;
#else
    struct ifreq    ifrq;
    int rc = 0;

    DEBUGMSGTL(("access:dot3StatsTable:interface_ioctl_ifindex_get", "called\n"));
                 
    rc = _dot3Stats_ioctl_get(fd, SIOCGIFINDEX, &ifrq, name);
    if (rc < 0) {
        DEBUGMSGTL(("access:dot3StatsTable:interface_ioctl_ifindex_get",
                    "error on interface '%s'\n", name));
        snmp_log (LOG_ERR, "access:dot3StatsTable:interface_ioctl_ifindex_get, error on interface '%s'\n", name);
        return 0;

    }

    return ifrq.ifr_ifindex;
#endif /* SIOCGIFINDEX */
}

# if HAVE_LINUX_RTNETLINK_H /* { NETLINK */
/*
 * The following code is based upon code I got from Stephen Hemminger
 */
#include <linux/rtnetlink.h>
#include <errno.h>

struct rtnl_handle {
    int                     fd;
    struct sockaddr_nl      local;
    struct sockaddr_nl      peer;
    __u32                   seq;
    __u32                   dump;
};

struct ifstat_ent {
    struct ifstat_ent *next;
    const char *name;
    int ifindex;
    struct rtnl_link_stats stats;
};

typedef int (*rtnl_filter_t)(const struct sockaddr_nl *, struct nlmsghdr *n, void *);

struct rtnl_dump_filter_arg
{
    rtnl_filter_t filter;
    void *arg1;
    rtnl_filter_t junk;
    void *arg2;
};

static struct ifstat_ent *kern_db;
static const int rcvbuf_size = 1024 * 1024;

static int
rtnl_open_byproto(struct rtnl_handle *rth, unsigned subscriptions, int protocol)
{
    socklen_t addr_len;
    int sndbuf = 32768;

    memset(rth, 0, sizeof(*rth));

    rth->fd = socket(AF_NETLINK, SOCK_RAW, protocol);
    if (rth->fd < 0) {
        snmp_log(LOG_ERR, "Cannot open netlink socket");
        return -1;
    }

    if (setsockopt(rth->fd,SOL_SOCKET,SO_SNDBUF,&sndbuf,sizeof(sndbuf)) < 0) {
        snmp_log(LOG_ERR, "SO_SNDBUF");
        return -1;
    }

    if (setsockopt(rth->fd,SOL_SOCKET,SO_RCVBUF,&rcvbuf_size,sizeof(rcvbuf_size)) < 0) {
        snmp_log(LOG_ERR, "SO_RCVBUF");
        return -1;
    }

    memset(&rth->local, 0, sizeof(rth->local));
    rth->local.nl_family = AF_NETLINK;
    rth->local.nl_groups = subscriptions;

    if (bind(rth->fd, (struct sockaddr*)&rth->local, sizeof(rth->local)) < 0) {
        snmp_log(LOG_ERR, "Cannot bind netlink socket");
        return -1;
    }
    addr_len = sizeof(rth->local);
    if (getsockname(rth->fd, (struct sockaddr*)&rth->local, &addr_len) < 0) {
        snmp_log(LOG_ERR, "Cannot getsockname");
        return -1;
    }
    if (addr_len != sizeof(rth->local)) {
        snmp_log(LOG_ERR, "Wrong address length %d\n", addr_len);
        return -1;
    }
    if (rth->local.nl_family != AF_NETLINK) {
        snmp_log(LOG_ERR, "Wrong address family %d\n", rth->local.nl_family);
        return -1;
    }
    rth->seq = time(NULL);
    return 0;
}

static int
rtnl_open(struct rtnl_handle *rth, unsigned subscriptions)
{
    return rtnl_open_byproto(rth, subscriptions, NETLINK_ROUTE);
}

static void
rtnl_close(struct rtnl_handle *rth)
{
    if (rth->fd != -1)
        close(rth->fd);
    rth->fd = -1;

    return;
}

static int
rtnl_wilddump_request(struct rtnl_handle *rth, int family, int type)
{
    struct {
        struct nlmsghdr nlh;
        struct rtgenmsg g;
    } req;

    memset(&req, 0, sizeof(req));
    req.nlh.nlmsg_len = sizeof(req);
    req.nlh.nlmsg_type = type;
    req.nlh.nlmsg_flags = NLM_F_ROOT|NLM_F_MATCH|NLM_F_REQUEST;
    req.nlh.nlmsg_pid = 0;
    req.nlh.nlmsg_seq = rth->dump = ++rth->seq;
    req.g.rtgen_family = family;

    return send(rth->fd, (void*)&req, sizeof(req), 0);
}

static int
parse_rtattr(struct rtattr *tb[], int max, struct rtattr *rta, int len)
{
    memset(tb, 0, sizeof(struct rtattr *) * (max + 1));
    while (RTA_OK(rta, len))
    {
        if (rta->rta_type <= max)
            tb[rta->rta_type] = rta;
        rta = RTA_NEXT(rta,len);
    }

    if (len)
        snmp_log(LOG_ERR, "parse_rtattr: !!!Deficit %d, rta_len=%d\n", len, rta->rta_len);

    return 0;
}

static int
get_nlmsg(const struct sockaddr_nl *who, struct nlmsghdr *m, void *arg)
{
    struct ifinfomsg *ifi = NLMSG_DATA(m);
    struct rtattr * tb[IFLA_MAX+1];
    int len = m->nlmsg_len;
    struct ifstat_ent *n;

    if (m->nlmsg_type != RTM_NEWLINK)
        return 0;

    len -= NLMSG_LENGTH(sizeof(*ifi));
    if (len < 0)
        return -1;

    parse_rtattr(tb, IFLA_MAX, IFLA_RTA(ifi), len);
    if (tb[IFLA_IFNAME] == NULL || tb[IFLA_STATS] == NULL)
        return 0;

    n = malloc(sizeof(*n));
    memset(n, 0, sizeof(*n));

    n->ifindex = ifi->ifi_index;
    n->name = strdup(RTA_DATA(tb[IFLA_IFNAME]));
    memcpy(&n->stats, RTA_DATA(tb[IFLA_STATS]), sizeof(n->stats));
    n->next = kern_db;
    kern_db = n;
    return 0;
}

static int
rtnl_dump_filter_l(struct rtnl_handle *rth,
                   const struct rtnl_dump_filter_arg *arg)
{
    struct sockaddr_nl nladdr;
    struct iovec iov;
    struct msghdr msg = {
        .msg_name = &nladdr,
        .msg_namelen = sizeof(nladdr),
        .msg_iov = &iov,
        .msg_iovlen = 1,
    };
    char buf[16384];

    iov.iov_base = buf;
    while (1) {
        int status;
        const struct rtnl_dump_filter_arg *a;

        iov.iov_len = sizeof(buf);
        status = recvmsg(rth->fd, &msg, 0);

        if (status < 0) {
            if (errno == EINTR || errno == EAGAIN)
                continue;
            fprintf(stderr, "netlink receive error %s (%d)\n",
                            strerror(errno), errno);
            return -1;
        }

        if (status == 0) {
            fprintf(stderr, "EOF on netlink\n");
            return -1;
        }

        for (a = arg; a->filter; a++) {
            struct nlmsghdr *h = (struct nlmsghdr*)buf;

            while (NLMSG_OK(h, status)) {
                int err;

                if (nladdr.nl_pid != 0 ||
                     h->nlmsg_pid != rth->local.nl_pid ||
                     h->nlmsg_seq != rth->dump) {
                    if (a->junk) {
                        err = a->junk(&nladdr, h, a->arg2);
                        if (err < 0)
                            return err;
                    }
                    goto skip_it;
                }

                if (h->nlmsg_type == NLMSG_DONE)
                    return 0;
                if (h->nlmsg_type == NLMSG_ERROR) {
                    struct nlmsgerr *err = (struct nlmsgerr*)NLMSG_DATA(h);
                    if (h->nlmsg_len < NLMSG_LENGTH(sizeof(struct nlmsgerr))) {
                        fprintf(stderr, "ERROR truncated\n");
                    } else {
                        errno = -err->error;
                        perror("RTNETLINK answers");
                    }
                    return -1;
                }
                err = a->filter(&nladdr, h, a->arg1);
                if (err < 0)
                    return err;

skip_it:
                h = NLMSG_NEXT(h, status);
            }
        } while (0);
        if (msg.msg_flags & MSG_TRUNC) {
            fprintf(stderr, "Message truncated\n");
            continue;
        }
        if (status) {
            fprintf(stderr, "!!!Remnant of size %d\n", status);
            exit(1);
        }
    }
}

static int
rtnl_dump_filter(struct rtnl_handle *rth,
                 rtnl_filter_t filter,
                 void *arg1,
                 rtnl_filter_t junk,
                 void *arg2)
{
    const struct rtnl_dump_filter_arg a[2] = {
        { .filter = filter, .arg1 = arg1, .junk = junk, .arg2 = arg2 },
        { .filter = NULL,   .arg1 = NULL, .junk = NULL, .arg2 = NULL }
    };

    return rtnl_dump_filter_l(rth, a);
}

int
_dot3Stats_netlink_get_errorcntrs(dot3StatsTable_rowreq_ctx *rowreq_ctx, const char *name)
{
    struct rtnl_handle rth;
    struct ifstat_ent *ke;
    int done;

    if (rtnl_open(&rth, 0) < 0)
    {
        snmp_log(LOG_ERR, "_dot3Stats_netlink_get_errorcntrs: rtnl_open() failed\n");
        return 1;
    }

    if (rtnl_wilddump_request(&rth, AF_INET, RTM_GETLINK) < 0)
    {
        snmp_log(LOG_ERR, "_dot3Stats_netlink_get_errorcntrs: Cannot send dump request");
        rtnl_close(&rth);
        return 1;
    }

    if (rtnl_dump_filter(&rth, get_nlmsg, NULL, NULL, NULL) < 0)
    {
        snmp_log(LOG_ERR, "_dot3Stats_netlink_get_errorcntrs: Dump terminated\n");
        rtnl_close(&rth);
        return 1;
    }

    rtnl_close(&rth);

    /*
     * Now scan kern_db for this if's data
     * While doing so, we'll throw away the kern db.
     */
    done = 0;

    while ((ke = kern_db) != NULL)
    {
        if (strcmp(ke->name, name) == 0)
        {
            dot3StatsTable_data *data = &rowreq_ctx->data;

            snmp_log(LOG_ERR, "IFLA_STATS for %s\n", name);

            data->dot3StatsFCSErrors = ke->stats.rx_crc_errors;
            rowreq_ctx->column_exists_flags |= COLUMN_DOT3STATSFCSERRORS_FLAG;

            data->dot3StatsDeferredTransmissions = ke->stats.tx_dropped;
            rowreq_ctx->column_exists_flags |= COLUMN_DOT3STATSDEFERREDTRANSMISSIONS_FLAG;

            data->dot3StatsInternalMacTransmitErrors = ke->stats.tx_fifo_errors;
            rowreq_ctx->column_exists_flags |= COLUMN_DOT3STATSINTERNALMACTRANSMITERRORS_FLAG;

            data->dot3StatsCarrierSenseErrors = ke->stats.tx_carrier_errors;
            rowreq_ctx->column_exists_flags |= COLUMN_DOT3STATSCARRIERSENSEERRORS_FLAG;

            data->dot3StatsFrameTooLongs = ke->stats.rx_frame_errors;
            rowreq_ctx->column_exists_flags |= COLUMN_DOT3STATSFRAMETOOLONGS_FLAG;

            data->dot3StatsInternalMacReceiveErrors = ke->stats.rx_fifo_errors;
            rowreq_ctx->column_exists_flags |= COLUMN_DOT3STATSINTERNALMACRECEIVEERRORS_FLAG;

            done = 1;
        }
        kern_db = ke->next;
        free(ke);
    }

    return !done;
}
# else /* }{ */
int
_dot3Stats_netlink_get_errorcntrs(dot3StatsTable_rowreq_ctx *rowreq_ctx, const char *name)
{
    return 1;
}
# endif /* } */


/*
 * NAME: getulongfromsysclassnetstatistics
 * PURPOSE: To get a single statistics value from /sys/class/net/<ifname>/statistics/<ctrname>
 * ARGUMENTS: ifname: interface name
 *      ctrname: counter name
 *      valuep: where to store value
 * RETURNS: 0 if value not available
 *      non-0 if value available
 */
static int
getulongfromsysclassnetstatistics(const char *ifname, const char *ctrname, u_long *valuep)
{
    char path[256];
    FILE *fp;
    int rv;

    if (ifname == NULL || ctrname == NULL || valuep == NULL)
        return 0;

    snprintf(path, sizeof(path), "/sys/class/net/%s/statistics/%s", ifname, ctrname);
    fp = fopen(path, "rt");
    if (fp == NULL)
        return 0;

    rv = 1;
    if (fscanf(fp, "%lu", valuep) != 1)
        rv = 0;

    fclose(fp);

    return rv;
}

/*
 * NAME: interface_dot3stats_get_errorcounters
 * PURPOSE: To get ethernet error statistics
 * ARGUMENTS: rowreq_ctx: where to store the value(s)
 *      name: interface name
 * RETURNS: nothing. fields not set if data not available
 */
void
interface_dot3stats_get_errorcounters (dot3StatsTable_rowreq_ctx *rowreq_ctx, const char *name)
{
    u_long value;
    dot3StatsTable_data *data = &rowreq_ctx->data;
    FILE *dev;
    const char NETDEV_FILE[] = "/proc/net/dev";

    if (_dot3Stats_netlink_get_errorcntrs(rowreq_ctx, name) == 0)
    {
        snmp_log(LOG_NOTICE, "interface_dot3stats_get_errorcounters: got data from IFLA_STATS\n");
        return;
    }

    if ((dev = fopen(NETDEV_FILE, "r")) != NULL)
    {
        char line[256], *lp, *next;
        size_t namelen = strlen(name);
        unsigned int value;
        unsigned int column;

        while (fgets(line, sizeof(line), dev) != NULL)
        {
            /*    br0:68395635 1038214    0    0    0     0          0    939411 25626606   90708    0    0    0     0       0          0 */
            lp = line;
            while (*lp == ' ' || *lp == '\t')
                lp++;
            if (strncmp(lp, name, namelen) != 0 || lp[namelen] != ':')
                continue;
            lp += namelen + 1;         /* Skip name and colon */

            column = 1;
            while (1)
            {
                value = strtoul(lp, &next, 10);
                if (next == lp)
                    break;             /* no more data */
                switch (column)
                {
                case 3: 
                    data->dot3StatsFCSErrors = value;
                    rowreq_ctx->column_exists_flags |= COLUMN_DOT3STATSFCSERRORS_FLAG;
                    break;
                case 12:
                    data->dot3StatsDeferredTransmissions = value;
                    rowreq_ctx->column_exists_flags |= COLUMN_DOT3STATSDEFERREDTRANSMISSIONS_FLAG;
                    break;
                case 13:
                    data->dot3StatsInternalMacTransmitErrors = value;
                    rowreq_ctx->column_exists_flags |= COLUMN_DOT3STATSINTERNALMACTRANSMITERRORS_FLAG;
                    break;
                case 15:
                    data->dot3StatsCarrierSenseErrors = value;
                    rowreq_ctx->column_exists_flags |= COLUMN_DOT3STATSCARRIERSENSEERRORS_FLAG;
                    break;
                case 6:
                    data->dot3StatsFrameTooLongs = value;
                    rowreq_ctx->column_exists_flags |= COLUMN_DOT3STATSFRAMETOOLONGS_FLAG;
                    break;
                case 5:
                    data->dot3StatsInternalMacReceiveErrors = value;
                    rowreq_ctx->column_exists_flags |= COLUMN_DOT3STATSINTERNALMACRECEIVEERRORS_FLAG;
                    break;
                case 14:
                    data->dot3StatsSingleCollisionFrames = value;
                    rowreq_ctx->column_exists_flags |= COLUMN_DOT3STATSSINGLECOLLISIONFRAMES_FLAG;
                    break;
                }
                column++;
                lp = next;
            }
            break;
        }

        fclose(dev);
    }

    if (!(rowreq_ctx->column_exists_flags & COLUMN_DOT3STATSFCSERRORS_FLAG) &&
          getulongfromsysclassnetstatistics(name, "rx_errors", &value)) {
        data->dot3StatsFCSErrors = value;
        rowreq_ctx->column_exists_flags |= COLUMN_DOT3STATSFCSERRORS_FLAG;
    }
    if (!(rowreq_ctx->column_exists_flags & COLUMN_DOT3STATSDEFERREDTRANSMISSIONS_FLAG) &&
          getulongfromsysclassnetstatistics(name, "tx_dropped", &value)) {
        data->dot3StatsDeferredTransmissions = value;
        rowreq_ctx->column_exists_flags |= COLUMN_DOT3STATSDEFERREDTRANSMISSIONS_FLAG;
    }
    if (!(rowreq_ctx->column_exists_flags & COLUMN_DOT3STATSINTERNALMACTRANSMITERRORS_FLAG) &&
          getulongfromsysclassnetstatistics(name, "tx_fifo_errors", &value)) {
        data->dot3StatsInternalMacTransmitErrors = value;
        rowreq_ctx->column_exists_flags |= COLUMN_DOT3STATSINTERNALMACTRANSMITERRORS_FLAG;
    }
    if (!(rowreq_ctx->column_exists_flags & COLUMN_DOT3STATSCARRIERSENSEERRORS_FLAG) &&
          getulongfromsysclassnetstatistics(name, "tx_carrier_errors", &value)) {
        data->dot3StatsCarrierSenseErrors = value;
        rowreq_ctx->column_exists_flags |= COLUMN_DOT3STATSCARRIERSENSEERRORS_FLAG;
    }
    if (!(rowreq_ctx->column_exists_flags & COLUMN_DOT3STATSFRAMETOOLONGS_FLAG) &&
          getulongfromsysclassnetstatistics(name, "rx_frame_errors", &value)) {
        data->dot3StatsFrameTooLongs = value;
        rowreq_ctx->column_exists_flags |= COLUMN_DOT3STATSFRAMETOOLONGS_FLAG;
    }
    if (!(rowreq_ctx->column_exists_flags & COLUMN_DOT3STATSINTERNALMACRECEIVEERRORS_FLAG) &&
          getulongfromsysclassnetstatistics(name, "rx_fifo_errors", &value)) {
        data->dot3StatsInternalMacReceiveErrors = value;
        rowreq_ctx->column_exists_flags |= COLUMN_DOT3STATSINTERNALMACRECEIVEERRORS_FLAG;
    }

    return;
}

/*
 * @retval  0 success
 * @retval -1 cannot get ETHTOOL_DRVINFO failed 
 * @retval -2 nstats zero - no statistcs available
 * @retval -3 memory allocation for holding the statistics failed
 * @retval -4 cannot get ETHTOOL_GSTRINGS information
 * @retval -5 cannot get ETHTOOL_GSTATS information
 * @retval -6 function not supported if HAVE_LINUX_ETHTOOL_H not defined
 */


int 
interface_ioctl_dot3stats_get (dot3StatsTable_rowreq_ctx *rowreq_ctx, int fd, const char *name) {

#ifdef HAVE_LINUX_ETHTOOL_H
    dot3StatsTable_data *data = &rowreq_ctx->data;
    struct ethtool_drvinfo driver_info;
    struct ethtool_gstrings *eth_strings;
    struct ethtool_stats *eth_stats;
    struct ifreq ifr; 
    unsigned int nstats, size_str, i;
    int err;

    DEBUGMSGTL(("access:dot3StatsTable:interface_ioctl_dot3Stats_get",
                "called\n"));

    memset(&ifr, 0, sizeof(ifr));
    strlcpy(ifr.ifr_name, name, sizeof(ifr.ifr_name));

    memset(&driver_info, 0, sizeof (driver_info));
    driver_info.cmd = ETHTOOL_GDRVINFO;
    ifr.ifr_data = (char *)&driver_info;

    err = _dot3Stats_ioctl_get(fd, SIOCETHTOOL, &ifr, name);
    if (err < 0) {
        DEBUGMSGTL(("access:dot3StatsTable:interface_ioctl_dot3Stats_get",
                    "ETHTOOL_GETDRVINFO failed for interface |%s| \n", name));
        return -1;
    }

    nstats = driver_info.n_stats;
    if (nstats < 1) {
        DEBUGMSGTL(("access:dot3StatsTable:interface_ioctl_dot3Stats_get",
                    "no stats available for interface |%s| \n", name));
        return -2;
    }

    size_str = nstats * ETH_GSTRING_LEN;

    eth_strings = malloc(size_str + sizeof (struct ethtool_gstrings));
    if (!eth_strings) {
        DEBUGMSGTL(("access:dot3StatsTable:interface_ioctl_dot3Stats_get",
                    "no memory available\n"));
        snmp_log (LOG_ERR, "access:dot3StatsTable,interface_ioctl_dot3Stats_get, no memory available\n");

        return -3;
    }
    memset (eth_strings, 0, (size_str + sizeof (struct ethtool_gstrings)));

    eth_stats = malloc (size_str + sizeof (struct ethtool_stats));
    if (!eth_stats) {
        free (eth_strings);
        DEBUGMSGTL(("access:dot3StatsTable:interface_ioctl_dot3Stats_get",
                    "no memory available\n"));
        snmp_log (LOG_ERR, "access:dot3StatsTable,interface_ioctl_dot3Stats_get, no memory available\n");

        return -3;
    }
     memset (eth_stats, 0, (size_str + sizeof (struct ethtool_stats)));

    eth_strings->cmd = ETHTOOL_GSTRINGS;
    eth_strings->string_set = ETH_SS_STATS;
    eth_strings->len = nstats;
    ifr.ifr_data = (char *) eth_strings;
    err = _dot3Stats_ioctl_get(fd, SIOCETHTOOL, &ifr, name);
    if (err < 0) {
        DEBUGMSGTL(("access:dot3StatsTable:interface_ioctl_dot3Stats_get",
                    "cannot get stats strings information for interface |%s| \n", name));
        snmp_log (LOG_ERR, "access:dot3StatsTable,interface_ioctl_dot3Stats_get, cannot get stats strings information for interface |%s| \n", name);

        free(eth_strings);
        free(eth_stats);
        return -4;
    }

    eth_stats->cmd = ETHTOOL_GSTATS;
    eth_stats->n_stats = nstats;
    ifr.ifr_data = (char *) eth_stats;
    err = _dot3Stats_ioctl_get(fd, SIOCETHTOOL, &ifr, name);
    if (err < 0) {
        DEBUGMSGTL(("access:dot3StatsTable:interface_ioctl_dot3Stats_get",
                    "cannot get stats strings information for interface |%s| \n", name));
        snmp_log (LOG_ERR, "access:dot3StatsTable,interface_ioctl_dot3Stats_get, cannot get stats information for interface |%s| \n", name);

        free(eth_strings);
        free(eth_stats);
        return -5;
    }

    for (i = 0; i < nstats; i++) {
        char s[ETH_GSTRING_LEN];

        strlcpy(s, (const char *) &eth_strings->data[i * ETH_GSTRING_LEN],
                sizeof(s));
    
        if (DOT3STATSALIGNMENTERRORS(s)) {
            data->dot3StatsAlignmentErrors = (u_long)eth_stats->data[i];
            rowreq_ctx->column_exists_flags |= COLUMN_DOT3STATSALIGNMENTERRORS_FLAG;
        }

        if (DOT3STATSMULTIPLECOLLISIONFRAMES(s)) {
            data->dot3StatsMultipleCollisionFrames = (u_long)eth_stats->data[i];
            rowreq_ctx->column_exists_flags |= COLUMN_DOT3STATSMULTIPLECOLLISIONFRAMES_FLAG;
        }
            
        if (DOT3STATSLATECOLLISIONS(s)) {
            data->dot3StatsLateCollisions = (u_long)eth_stats->data[i];
            rowreq_ctx->column_exists_flags |= COLUMN_DOT3STATSLATECOLLISIONS_FLAG;
        }

        if (DOT3STATSSINGLECOLLISIONFRAMES(s)) {
            data->dot3StatsSingleCollisionFrames = (u_long)eth_stats->data[i];
            rowreq_ctx->column_exists_flags |= COLUMN_DOT3STATSSINGLECOLLISIONFRAMES_FLAG;
        }

        if (DOT3STATSEXCESSIVECOLLISIONS(s)) {
            data->dot3StatsExcessiveCollisions = (u_long)eth_stats->data[i];
            rowreq_ctx->column_exists_flags |= COLUMN_DOT3STATSEXCESSIVECOLLISIONS_FLAG;
        }
    }

    free(eth_strings);
    free(eth_stats);

    return 0;
#else
    return -6;
#endif
}


/*
 * @retval  0 success
 * @retval -1 ETHTOOL_GSET failed
 * @retval -2 function not supported if HAVE_LINUX_ETHTOOL_H not defined
 */

int
interface_ioctl_dot3stats_duplex_get(dot3StatsTable_rowreq_ctx *rowreq_ctx, int fd, const char* name) {

#ifdef HAVE_LINUX_ETHTOOL_H
    dot3StatsTable_data *data = &rowreq_ctx->data;
    struct ethtool_cmd edata;
    struct ifreq ifr;
    int err;

    DEBUGMSGTL(("access:dot3StatsTable:interface_ioctl_dot3Stats_duplex_get",
                "called\n"));

    memset(&edata, 0, sizeof (edata));
    memset(&ifr, 0, sizeof (ifr));
    edata.cmd = ETHTOOL_GSET;
    ifr.ifr_data = (char *)&edata;

    err = _dot3Stats_ioctl_get (fd, SIOCETHTOOL, &ifr, name);
    if (err < 0) {
        DEBUGMSGTL(("access:dot3StatsTable:interface_ioctl_dot3Stats_duplex_get",
                    "ETHTOOL_GSET failed\n"));

        return -1;
    }
    
    if (err == 0) {
        rowreq_ctx->column_exists_flags |= COLUMN_DOT3STATSDUPLEXSTATUS_FLAG;
        switch (edata.duplex) {
        case DUPLEX_HALF:
            data->dot3StatsDuplexStatus = (u_long) DOT3STATSDUPLEXSTATUS_HALFDUPLEX;
            break;
        case DUPLEX_FULL:
            data->dot3StatsDuplexStatus = (u_long) DOT3STATSDUPLEXSTATUS_FULLDUPLEX;
            break;
        default:
            data->dot3StatsDuplexStatus = (u_long) DOT3STATSDUPLEXSTATUS_UNKNOWN;
            break;
        };
    }

    DEBUGMSGTL(("access:dot3StatsTable:interface_ioctl_dot3Stats_duplex_get",
                "ETHTOOL_GSET processed\n"));
    return err;
#else
    return -2;
#endif
}


/*
 * NAME: interface_sysclassnet_dot3stats_get
 * PURPOSE: To get ethernet statistics from /sys/class/net/...
 * ARGUMENTS: rowreq_ctx: where to store the value(s)
 *	name: interface name
 * RETURNS: nothing. fields not set if data not available
 */
void
interface_sysclassnet_dot3stats_get (dot3StatsTable_rowreq_ctx *rowreq_ctx, const char *name)
{
    u_long value;
    dot3StatsTable_data *data = &rowreq_ctx->data;

    if (getulongfromsysclassnetstatistics(name, "rx_errors", &value)) {
	data->dot3StatsFCSErrors = value;
	rowreq_ctx->column_exists_flags |= COLUMN_DOT3STATSFCSERRORS_FLAG;
    }
    if (getulongfromsysclassnetstatistics(name, "tx_dropped", &value)) {
	data->dot3StatsDeferredTransmissions = value;
	rowreq_ctx->column_exists_flags |= COLUMN_DOT3STATSDEFERREDTRANSMISSIONS_FLAG;
    }
    if (getulongfromsysclassnetstatistics(name, "tx_fifo_errors", &value)) {
	data->dot3StatsInternalMacTransmitErrors = value;
	rowreq_ctx->column_exists_flags |= COLUMN_DOT3STATSINTERNALMACTRANSMITERRORS_FLAG;
    }
    if (getulongfromsysclassnetstatistics(name, "tx_carrier_errors", &value)) {
	data->dot3StatsCarrierSenseErrors = value;
	rowreq_ctx->column_exists_flags |= COLUMN_DOT3STATSCARRIERSENSEERRORS_FLAG;
    }
    if (getulongfromsysclassnetstatistics(name, "rx_frame_errors", &value)) {
	data->dot3StatsFrameTooLongs = value;
	rowreq_ctx->column_exists_flags |= COLUMN_DOT3STATSFRAMETOOLONGS_FLAG;
    }
    if (getulongfromsysclassnetstatistics(name, "rx_fifo_errors", &value)) {
	data->dot3StatsInternalMacReceiveErrors = value;
	rowreq_ctx->column_exists_flags |= COLUMN_DOT3STATSINTERNALMACRECEIVEERRORS_FLAG;
    }

    return;
}



/* ioctl wrapper
 *
 * @param      fd : socket fd to use w/ioctl, or -1 to open/close one
 * @param  which
 * @param ifrq
 * param ifentry : ifentry to update
 * @param name
 *
 * @retval  0 : success
 * @retval -1 : invalid parameters
 * @retval -2 : couldn't create socket
 * @retval -3 : ioctl call failed
 */
int
_dot3Stats_ioctl_get(int fd, int which, struct ifreq *ifrq, const char* name)
{
    int ourfd = -1, rc = 0;

    DEBUGMSGTL(("access:dot3StatsTable:ioctl", "_dot3Stats_ioctl_get\n"));

    /*
     * sanity checks
     */
    if(NULL == name) {
        DEBUGMSGTL(("access:dot3StatsTable:ioctl",
                    "_dot3Stats_ioctl_get invalid ifname '%s'\n", name));
        snmp_log (LOG_ERR, "access:dot3StatsTable:ioctl, _dot3Stats_ioctl_get error on interface '%s'\n", name);
        return -1;
    }

    /*
     * create socket for ioctls
     */
    if(fd < 0) {
        fd = ourfd = socket(AF_INET, SOCK_DGRAM, 0);
        if(ourfd < 0) {
            DEBUGMSGTL(("access:dot3StatsTable:ioctl",
                        "dot3Stats_ioctl_get couldn't create a socket\n"));
            snmp_log (LOG_ERR, "access:dot3StatsTable:ioctl, _dot3Stats_ioctl_get error on interface '%s'\n", name);

            return -2;
        }
    }

    strlcpy(ifrq->ifr_name, name, sizeof(ifrq->ifr_name));
    rc = ioctl(fd, which, ifrq);
    if (rc < 0) {
        DEBUGMSGTL(("access:dot3StatsTable:ioctl",
                    "dot3Stats_ioctl_get ioctl %d returned %d\n", which, rc));
        rc = -3;
    }

    if(ourfd >= 0)
        close(ourfd);

    return rc;
}


