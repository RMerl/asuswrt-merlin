#ifndef __IP_SET_PORTMAP_H
#define __IP_SET_PORTMAP_H

#include <linux/netfilter_ipv4/ip_set.h>
#include <linux/netfilter_ipv4/ip_set_bitmaps.h>

#define SETTYPE_NAME		"portmap"

struct ip_set_portmap {
	void *members;			/* the portmap proper */
	ip_set_ip_t first_ip;		/* host byte order, included in range */
	ip_set_ip_t last_ip;		/* host byte order, included in range */
	u_int32_t size;			/* size of the ipmap proper */
};

struct ip_set_req_portmap_create {
	ip_set_ip_t from;
	ip_set_ip_t to;
};

struct ip_set_req_portmap {
	ip_set_ip_t ip;
};

#endif /* __IP_SET_PORTMAP_H */
