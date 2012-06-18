#ifndef __IP_SET_IPTREEMAP_H
#define __IP_SET_IPTREEMAP_H

#include <linux/netfilter_ipv4/ip_set.h>

#define SETTYPE_NAME "iptreemap"

#ifdef __KERNEL__
struct ip_set_iptreemap_d {
	unsigned char bitmap[32]; /* x.x.x.y */
};

struct ip_set_iptreemap_c {
	struct ip_set_iptreemap_d *tree[256]; /* x.x.y.x */
};

struct ip_set_iptreemap_b {
	struct ip_set_iptreemap_c *tree[256]; /* x.y.x.x */
	unsigned char dirty[32];
};
#endif

struct ip_set_iptreemap {
	unsigned int gc_interval;
#ifdef __KERNEL__
	struct timer_list gc;
	struct ip_set_iptreemap_b *tree[256]; /* y.x.x.x */
#endif
};

struct ip_set_req_iptreemap_create {
	unsigned int gc_interval;
};

struct ip_set_req_iptreemap {
	ip_set_ip_t ip;
	ip_set_ip_t end;
};

#endif /* __IP_SET_IPTREEMAP_H */
