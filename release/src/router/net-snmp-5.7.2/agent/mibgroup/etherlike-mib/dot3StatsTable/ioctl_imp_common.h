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

struct ifname *dot3stats_interface_name_list_get (struct ifname *, int *);
int dot3stats_interface_name_list_free (struct ifname *list_head);
int dot3stats_interface_ioctl_ifindex_get (int fd, const char *name);
int _dot3Stats_ioctl_get(int fd, int which, struct ifreq *ifrq, const char* name);
int interface_ioctl_dot3stats_get(dot3StatsTable_rowreq_ctx *rowreq_ctx, int fd, const char* name);
int interface_ioctl_dot3stats_duplex_get(dot3StatsTable_rowreq_ctx *rowreq_ctx, int fd, const char* name);


/* for maintainability */

#define INTEL_RECEIVE_ALIGN_ERRORS                      "rx_align_errors"
#define BROADCOM_RECEIVE_ALIGN_ERRORS                   INTEL_RECEIVE_ALIGN_ERRORS

#define INTEL_TRANSMIT_MULTIPLE_COLLISIONS              "tx_multi_coll_ok"
#define BROADCOM_TRANSMIT_MULTIPLE_COLLISIONS_BNX2      "tx_multi_collisions"
#define BROADCOM_TRANSMIT_MULTIPLE_COLLISIONS_TG3       "tx_mult_collisions"

#define INTEL_TRANSMIT_LATE_COLLISIONS                  "tx_abort_late_coll"
#define BROADCOM_TRANSMIT_LATE_COLLISIONS               "tx_late_collisions"

#define INTEL_TRANSMIT_SINGLE_COLLISIONS                "tx_single_coll_ok"
#define BROADCOM_TRANSMIT_SINGLE_COLLISIONS             "tx_single_collisions"

#define BROADCOM_TRANSMIT_EXCESS_COLLISIONS_BNX2        "tx_excess_collisions"
#define BROADCOM_TRANSMIT_EXCESS_COLLISIONS_TG3         "tx_excessive_collisions"


#define DOT3STATSALIGNMENTERRORS(x)            strstr(x, INTEL_RECEIVE_ALIGN_ERRORS)

#define DOT3STATSMULTIPLECOLLISIONFRAMES(x)    (strstr(x, INTEL_TRANSMIT_MULTIPLE_COLLISIONS)) || \
                                               (strstr(x, BROADCOM_TRANSMIT_MULTIPLE_COLLISIONS_BNX2)) || \
                                               (strstr(x, BROADCOM_TRANSMIT_MULTIPLE_COLLISIONS_TG3))

#define DOT3STATSLATECOLLISIONS(x)             (strstr(x, INTEL_TRANSMIT_LATE_COLLISIONS)) || \
                                               (strstr(x, BROADCOM_TRANSMIT_LATE_COLLISIONS))

#define DOT3STATSSINGLECOLLISIONFRAMES(x)      (strstr(x, INTEL_TRANSMIT_SINGLE_COLLISIONS)) || \
                                               (strstr(x, BROADCOM_TRANSMIT_SINGLE_COLLISIONS))

#define DOT3STATSEXCESSIVECOLLISIONS(x)        (strstr(x, BROADCOM_TRANSMIT_EXCESS_COLLISIONS_BNX2)) || \
                                               (strstr(x, BROADCOM_TRANSMIT_EXCESS_COLLISIONS_TG3))

