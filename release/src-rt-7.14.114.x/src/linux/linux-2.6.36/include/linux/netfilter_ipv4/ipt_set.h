#ifndef _IPT_SET_H
#define _IPT_SET_H

#include <linux/netfilter_ipv4/ip_set.h>

struct ipt_set_info {
	ip_set_id_t index;
	u_int32_t flags[IP_SET_MAX_BINDINGS + 1];
};

/* match info */
struct ipt_set_info_match {
	struct ipt_set_info match_set;
};

struct ipt_set_info_target {
	struct ipt_set_info add_set;
	struct ipt_set_info del_set;
};

#endif /*_IPT_SET_H*/
