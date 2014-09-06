/*
 * standard Net-SNMP includes 
 */
#include <net-snmp/net-snmp-config.h>
#include <net-snmp/net-snmp-includes.h>
#include <net-snmp/agent/net-snmp-agent-includes.h>

/*
 * include our parent header 
 */
#include "rmon-mib/etherStatsTable/etherStatsTable.h"
#include "rmon-mib/etherStatsTable/etherStatsTable_data_access.h"
#include "rmon-mib/etherStatsTable/ioctl_imp_common.h"

/*
 * @retval  0 success
 * @retval -1 getifaddrs failed 
 * @retval -2 memory allocation failed
 */

struct ifname *
etherstats_interface_name_list_get (struct ifname *list_head, int *retval)
{
    struct ifaddrs *addrs = NULL, *p = NULL;
    struct ifname *nameptr1=NULL, *nameptr2 = NULL;

    DEBUGMSGTL(("access:etherStatsTable:interface_name_list_get",
                "called\n"));

    if ((getifaddrs(&addrs)) < 0) {
        DEBUGMSGTL(("access:etherStatsTable:interface_name_list_get",
                    "getifaddrs failed\n"));
        snmp_log (LOG_ERR, "access:etherStatsTable,interface_name_list_get, getifaddrs failed\n");
        *retval = -1;
        return NULL;
    }

    for (p = addrs; p; p = p->ifa_next) {

        if (!list_head) {
            if ( (list_head = (struct ifname *) malloc (sizeof(struct ifname))) < 0) {
                DEBUGMSGTL(("access:etherStatsTable:interface_name_list_get",
                            "memory allocation failed\n"));
                snmp_log (LOG_ERR, "access:etherStatsTable,interface_name_list_get, memory allocation failed\n");
                freeifaddrs(addrs);
                *retval = -2;
                return NULL;
            }
            memset(list_head, 0, sizeof(struct ifname));
            strlcpy(list_head->name, p->ifa_name, IF_NAMESIZE);
            continue;
        }
         for (nameptr1 = list_head; nameptr1; nameptr2 = nameptr1, nameptr1 = nameptr1->ifn_next)
            if (!strncmp(p->ifa_name, nameptr1->name, IF_NAMESIZE))
                break;

        if (nameptr1)
            continue;

        if ( (nameptr2->ifn_next = (struct ifname *) malloc (sizeof(struct ifname))) < 0) {
            DEBUGMSGTL(("access:etherStatsTable:interface_name_list_get",
                        "memory allocation failed\n"));
            snmp_log (LOG_ERR, "access:etherStatsTable,interface_name_list_get, memory allocation failed\n");
            etherstats_interface_name_list_free (list_head);
            freeifaddrs(addrs);
            *retval = -2;
            return NULL;
        }
        nameptr2 = nameptr2->ifn_next;
        memset(nameptr2, 0, sizeof(struct ifname));
        strlcpy(nameptr2->name, p->ifa_name, IF_NAMESIZE);
        continue;
    }

    freeifaddrs(addrs);
    *retval = 0;
    return list_head;
}


/*
 * @retval 0 success
 * @retval -1 invalid pointer
 */

int
etherstats_interface_name_list_free (struct ifname *list_head)
{
    struct ifname *nameptr1 = NULL, *nameptr2 = NULL;

    DEBUGMSGTL(("access:etherStatsTable:interface_name_list_free",
                "called\n"));

    if (!list_head) {
        snmp_log (LOG_ERR, "access:etherStatsTable:interface_name_list_free: invalid pointer list_head");
        DEBUGMSGTL(("access:etherStatsTable:interface_name_list_free",
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
etherstats_interface_ioctl_ifindex_get (int fd, const char *name) {
#ifndef SIOCGIFINDEX
    return 0;
#else
    struct ifreq    ifrq;
    int rc = 0;

    DEBUGMSGTL(("access:etherStatsTable:ioctl", "ifindex_get\n"));

    rc = _etherStats_ioctl_get(fd, SIOCGIFINDEX, &ifrq, name);
    if (rc < 0) {
        DEBUGMSGTL(("access:etherStats:ioctl",
                    "ifindex_get error on inerface '%s'\n", name));
        snmp_log (LOG_ERR, "access:etherStatsTable:ioctl, ifindex_get error on inerface '%s'\n", name);
        return 0;

    }

    return ifrq.ifr_ifindex;
#endif /* SIOCGIFINDEX */
}

/*
 * @retval  0 success
 * @retval -1 cannot get ETHTOOL_DRVINFO failed 
 * @retval -2 n_stats zero - no statistcs available
 * @retval -3 memory allocation for holding the statistics failed
 * @retval -4 cannot get ETHTOOL_GSTRINGS information
 * @retval -5 cannot get ETHTOOL_GSTATS information
 * @retval -6 function not supported if HAVE_LINUX_ETHTOOL_H not defined
 */

int
interface_ioctl_etherstats_get (etherStatsTable_rowreq_ctx *rowreq_ctx , int fd, const char *name) {

#ifdef HAVE_LINUX_ETHTOOL_H

    etherStatsTable_data *data = &rowreq_ctx->data;
    struct ethtool_drvinfo driver_info;
    struct ethtool_gstrings *eth_strings;
    struct ethtool_stats *eth_stats;
    struct ifreq ifr;
    unsigned int nstats, size_str, i;
    int err;

    DEBUGMSGTL(("access:etherStatsTable:interface_ioctl_etherstats_get",
                "called\n"));

    memset(&ifr, 0, sizeof(ifr));
    strlcpy(ifr.ifr_name, name, sizeof(ifr.ifr_name));

    memset(&driver_info, 0, sizeof(driver_info));
    driver_info.cmd = ETHTOOL_GDRVINFO;
    ifr.ifr_data = (char *)&driver_info;

    err = _etherStats_ioctl_get(fd, SIOCETHTOOL, &ifr, name);
    if (err < 0) {
        DEBUGMSGTL(("access:etherStatsTable:interface_ioctl_etherstats_get",
                    "ETHTOOL_GETDRVINFO failed on interface |%s| \n", name));
        return -1;
    }

    nstats = driver_info.n_stats;
    if (nstats < 1) {
        DEBUGMSGTL(("access:etherStatsTable:interface_ioctl_etherstats_get",
                    "no stats available for interface |%s| \n", name));
        return -2;
    }

    size_str = nstats * ETH_GSTRING_LEN;

    eth_strings = malloc(size_str + sizeof (struct ethtool_gstrings));
    if (!eth_strings) {
        DEBUGMSGTL(("access:etherStatsTable:interface_ioctl_etherstats_get",
                    "no memory available\n"));
        snmp_log (LOG_ERR, "access:etherStatsTable,interface_ioctl_etherstats_get, no memory available\n");

        return -3;
    }
    memset (eth_strings, 0, (size_str + sizeof (struct ethtool_gstrings)));

    eth_stats = malloc (size_str + sizeof (struct ethtool_stats));
    if (!eth_stats) {
        free (eth_strings);
        DEBUGMSGTL(("access:etherStatsTable:interface_ioctl_etherstats_get",
                    "no memory available\n"));
        snmp_log (LOG_ERR, "access:etherStatsTable,interface_ioctl_etherstats_get, no memory available\n");

        return -3;
    }
    memset (eth_stats, 0, (size_str + sizeof (struct ethtool_stats)));

    eth_strings->cmd = ETHTOOL_GSTRINGS;
    eth_strings->string_set = ETH_SS_STATS;
    eth_strings->len = nstats;
    ifr.ifr_data = (char *) eth_strings;

    err = _etherStats_ioctl_get(fd, SIOCETHTOOL, &ifr, name);
    if (err < 0) {
        DEBUGMSGTL(("access:etherStatsTable:interface_ioctl_etherstats_get",
                    "cannot get stats strings information for interface |%s| \n", name));
        snmp_log (LOG_ERR, "access:etherStatsTable,interface_ioctl_etherstats_get, cannot get stats strings information for interface |%s| \n", name);

        free(eth_strings);
        free(eth_stats);
        return -4;
    }

    eth_stats->cmd = ETHTOOL_GSTATS;
    eth_stats->n_stats = nstats;
    ifr.ifr_data = (char *) eth_stats;
    err = _etherStats_ioctl_get(fd, SIOCETHTOOL, &ifr, name);
    if (err < 0) {
        DEBUGMSGTL(("access:etherStatsTable:interface_ioctl_etherstats_get",
                    "cannot get stats strings information for interface |%s| \n", name));
        snmp_log (LOG_ERR, "access:etherStatsTable,interface_ioctl_etherstats_get, cannot get stats information for interface |%s| \n", name);

        free(eth_strings);
        free(eth_stats);
        return -5;
    }

    for (i = 0; i < nstats; i++) {
        char s[ETH_GSTRING_LEN];

        strlcpy(s, (const char *) &eth_strings->data[i * ETH_GSTRING_LEN],
                sizeof(s));
        
        if (ETHERSTATSJABBERS(s)) {
            data->etherStatsJabbers = (u_long)eth_stats->data[i];
            rowreq_ctx->column_exists_flags |= COLUMN_ETHERSTATSJABBERS_FLAG;
        }
    }
    free(eth_strings);
    free(eth_stats);

    return 0;
#else
    return -6;
#endif

}


/* ioctl wrapper
 *
 * @param fd : socket fd to use w/ioctl, or -1 to open/close one
 * @param which
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
_etherStats_ioctl_get(int fd, int which, struct ifreq *ifrq, const char* name)
{
    int ourfd = -1, rc = 0;

    DEBUGMSGTL(("access:etherStatsTable:ioctl", "_etherStats_ioctl_get\n"));
    /*
     * sanity checks
     */
    if(NULL == name) {
        DEBUGMSGTL(("access:etherStatsTable:ioctl",
                    "_etherStats_ioctl_get invalid ifname '%s'\n", name));
        snmp_log (LOG_ERR, "access:etherStatsTable:ioctl, _etherStats_ioctl_get error on inerface '%s'\n", name);
        return -1;
    }

    /*
     * create socket for ioctls
     */
    if(fd < 0) {
        fd = ourfd = socket(AF_INET, SOCK_DGRAM, 0);
        if(ourfd < 0) {
            DEBUGMSGTL(("access:etherStatsTable:ioctl",
                        "_etherStats_ioctl_get couldn't create a socket\n"));
            snmp_log (LOG_ERR, "access:etherStatsTable:ioctl, _etherStats_ioctl_get error on inerface '%s'\n", name);

            return -2;
        }
    }

    strlcpy(ifrq->ifr_name, name, sizeof(ifrq->ifr_name));
    rc = ioctl(fd, which, ifrq);
    if (rc < 0) {
        DEBUGMSGTL(("access:etherStatsTable:ioctl",
                    "_etherStats_ioctl_get ioctl %d returned %d\n", which, rc));
        rc = -3;
    }

    if(ourfd >= 0)
        close(ourfd);

    return rc;
}

