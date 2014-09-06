#include <sys/socket.h>
#include <sys/types.h>
#include <sys/ioctl.h>
#include <net/if.h>
#include <linux/sockios.h>
#include <ifaddrs.h>

/* use kernel's ethtool.h  */

#include <linux/types.h>
typedef __u64 u64;
typedef __u32 u32;
typedef __u16 u16;
typedef __u8 u8;
#include <linux/ethtool.h>

/* structure for storing the interface names in the system */

struct ifname {
    struct ifname *ifn_next;
    char name [IF_NAMESIZE];
};

struct ifname *etherstats_interface_name_list_get (struct ifname *, int *);
int etherstats_interface_name_list_free (struct ifname *list_head);
int etherstats_interface_ioctl_ifindex_get (int fd, const char *name);
int _etherStats_ioctl_get(int fd, int which, struct ifreq *ifrq, const char* name);
int interface_ioctl_etherstats_get(etherStatsTable_rowreq_ctx *rowreq_ctx, int fd, const char* name);

/* for maintainability */

#define BROADCOM_RECEIVE_JABBERS                "rx_jabbers"

#define ETHERSTATSJABBERS(x)                    strstr(x, BROADCOM_RECEIVE_JABBERS)

