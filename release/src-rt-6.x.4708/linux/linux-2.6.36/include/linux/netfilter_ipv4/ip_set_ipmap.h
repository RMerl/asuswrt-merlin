#ifndef __IP_SET_IPMAP_H
#define __IP_SET_IPMAP_H

#include <linux/netfilter_ipv4/ip_set.h>
#include <linux/netfilter_ipv4/ip_set_bitmaps.h>

#define SETTYPE_NAME		"ipmap"

struct ip_set_ipmap {
	void *members;			/* the ipmap proper */
	ip_set_ip_t first_ip;		/* host byte order, included in range */
	ip_set_ip_t last_ip;		/* host byte order, included in range */
	ip_set_ip_t netmask;		/* subnet netmask */
	ip_set_ip_t sizeid;		/* size of set in IPs */
	ip_set_ip_t hosts;		/* number of hosts in a subnet */
	u_int32_t size;			/* size of the ipmap proper */
};

struct ip_set_req_ipmap_create {
	ip_set_ip_t from;
	ip_set_ip_t to;
	ip_set_ip_t netmask;
};

struct ip_set_req_ipmap {
	ip_set_ip_t ip;
};

static inline unsigned int
mask_to_bits(ip_set_ip_t mask)
{
	unsigned int bits = 32;
	ip_set_ip_t maskaddr;
	
	if (mask == 0xFFFFFFFF)
		return bits;
	
	maskaddr = 0xFFFFFFFE;
	while (--bits > 0 && maskaddr != mask)
		maskaddr <<= 1;
	
	return bits;
}

static inline ip_set_ip_t
range_to_mask(ip_set_ip_t from, ip_set_ip_t to, unsigned int *bits)
{
	ip_set_ip_t mask = 0xFFFFFFFE;
	
	*bits = 32;
	while (--(*bits) > 0 && mask && (to & mask) != from)
		mask <<= 1;
		
	return mask;
}
	
#endif /* __IP_SET_IPMAP_H */
