#ifndef __IP_SET_IPPORTIPHASH_H
#define __IP_SET_IPPORTIPHASH_H

#include <linux/netfilter_ipv4/ip_set.h>
#include <linux/netfilter_ipv4/ip_set_hashes.h>

#define SETTYPE_NAME			"ipportiphash"

struct ipportip {
	ip_set_ip_t ip;
	ip_set_ip_t ip1;
};

struct ip_set_ipportiphash {
	struct ipportip *members;	/* the ipportip proper */
	uint32_t elements;		/* number of elements */
	uint32_t hashsize;		/* hash size */
	uint16_t probes;		/* max number of probes  */
	uint16_t resize;		/* resize factor in percent */
	ip_set_ip_t first_ip;		/* host byte order, included in range */
	ip_set_ip_t last_ip;		/* host byte order, included in range */
	initval_t initval[0];		/* initvals for jhash_1word */
};

struct ip_set_req_ipportiphash_create {
	uint32_t hashsize;
	uint16_t probes;
	uint16_t resize;
	ip_set_ip_t from;
	ip_set_ip_t to;
};

struct ip_set_req_ipportiphash {
	ip_set_ip_t ip;
	ip_set_ip_t port;
	ip_set_ip_t ip1;
};

#endif	/* __IP_SET_IPPORTIPHASH_H */
