#ifndef __IP_SET_IPHASH_H
#define __IP_SET_IPHASH_H

#include <linux/netfilter_ipv4/ip_set.h>
#include <linux/netfilter_ipv4/ip_set_hashes.h>

#define SETTYPE_NAME "iphash"

struct ip_set_iphash {
	ip_set_ip_t *members;		/* the iphash proper */
	uint32_t elements;		/* number of elements */
	uint32_t hashsize;		/* hash size */
	uint16_t probes;		/* max number of probes  */
	uint16_t resize;		/* resize factor in percent */
	ip_set_ip_t netmask;		/* netmask */
	initval_t initval[0];		/* initvals for jhash_1word */
};

struct ip_set_req_iphash_create {
	uint32_t hashsize;
	uint16_t probes;
	uint16_t resize;
	ip_set_ip_t netmask;
};

struct ip_set_req_iphash {
	ip_set_ip_t ip;
};

#endif	/* __IP_SET_IPHASH_H */
