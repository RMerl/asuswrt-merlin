#ifndef _IP_CONNTRACK_MMS_H
#define _IP_CONNTRACK_MMS_H
/* MMS tracking. */

#ifdef __KERNEL__
#include <linux/netfilter_ipv4/lockhelp.h>

DECLARE_LOCK_EXTERN(ip_mms_lock);

#define MMS_PORT                         1755
#define MMS_SRV_MSG_ID                   196610

#define MMS_SRV_MSG_OFFSET               36
#define MMS_SRV_UNICODE_STRING_OFFSET    60
#define MMS_SRV_CHUNKLENLV_OFFSET        16
#define MMS_SRV_CHUNKLENLM_OFFSET        32
#define MMS_SRV_MESSAGELENGTH_OFFSET     8
#endif

/* This structure is per expected connection */
struct ip_ct_mms_expect {
	u_int32_t len;
	u_int32_t padding;
	u_int16_t port;
};

/* This structure exists only once per master */
struct ip_ct_mms_master {
};

#endif /* _IP_CONNTRACK_MMS_H */
