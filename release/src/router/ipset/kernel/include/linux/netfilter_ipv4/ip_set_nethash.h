#ifndef __IP_SET_NETHASH_H
#define __IP_SET_NETHASH_H

#include <linux/netfilter_ipv4/ip_set.h>
#include <linux/netfilter_ipv4/ip_set_hashes.h>

#define SETTYPE_NAME		"nethash"

struct ip_set_nethash {
	ip_set_ip_t *members;		/* the nethash proper */
	uint32_t elements;		/* number of elements */
	uint32_t hashsize;		/* hash size */
	uint16_t probes;		/* max number of probes  */
	uint16_t resize;		/* resize factor in percent */
	uint8_t cidr[30];		/* CIDR sizes */
	uint16_t nets[30];		/* nr of nets by CIDR sizes */
	initval_t initval[0];		/* initvals for jhash_1word */
};

struct ip_set_req_nethash_create {
	uint32_t hashsize;
	uint16_t probes;
	uint16_t resize;
};

struct ip_set_req_nethash {
	ip_set_ip_t ip;
	uint8_t cidr;
};

#endif	/* __IP_SET_NETHASH_H */
