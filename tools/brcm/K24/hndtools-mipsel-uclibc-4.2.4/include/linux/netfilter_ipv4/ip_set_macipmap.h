#ifndef __IP_SET_MACIPMAP_H
#define __IP_SET_MACIPMAP_H

#include <linux/netfilter_ipv4/ip_set.h>
#include <linux/netfilter_ipv4/ip_set_bitmaps.h>

#define SETTYPE_NAME "macipmap"

/* general flags */
#define IPSET_MACIP_MATCHUNSET	1

/* per ip flags */
#define IPSET_MACIP_ISSET	1

struct ip_set_macipmap {
	void *members;			/* the macipmap proper */
	ip_set_ip_t first_ip;		/* host byte order, included in range */
	ip_set_ip_t last_ip;		/* host byte order, included in range */
	u_int32_t flags;
	u_int32_t size;			/* size of the ipmap proper */
};

struct ip_set_req_macipmap_create {
	ip_set_ip_t from;
	ip_set_ip_t to;
	u_int32_t flags;
};

struct ip_set_req_macipmap {
	ip_set_ip_t ip;
	unsigned char ethernet[ETH_ALEN];
};

struct ip_set_macip {
	unsigned short match;
	unsigned char ethernet[ETH_ALEN];
};

#endif	/* __IP_SET_MACIPMAP_H */
