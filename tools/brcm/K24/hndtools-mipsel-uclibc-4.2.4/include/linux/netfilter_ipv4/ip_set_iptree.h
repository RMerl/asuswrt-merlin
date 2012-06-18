#ifndef __IP_SET_IPTREE_H
#define __IP_SET_IPTREE_H

#include <linux/netfilter_ipv4/ip_set.h>

#define SETTYPE_NAME "iptree"

struct ip_set_iptreed {
	unsigned long expires[256];	   	/* x.x.x.ADDR */
};

struct ip_set_iptreec {
	struct ip_set_iptreed *tree[256];	/* x.x.ADDR.* */
};

struct ip_set_iptreeb {
	struct ip_set_iptreec *tree[256];	/* x.ADDR.*.* */
};

struct ip_set_iptree {
	unsigned int timeout;
	unsigned int gc_interval;
#ifdef __KERNEL__
	uint32_t elements;		/* number of elements */
	struct timer_list gc;
	struct ip_set_iptreeb *tree[256];	/* ADDR.*.*.* */
#endif
};

struct ip_set_req_iptree_create {
	unsigned int timeout;
};

struct ip_set_req_iptree {
	ip_set_ip_t ip;
	unsigned int timeout;
};

#endif	/* __IP_SET_IPTREE_H */
