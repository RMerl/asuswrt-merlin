#ifndef __IP_SET_SETLIST_H
#define __IP_SET_SETLIST_H

#include <linux/netfilter_ipv4/ip_set.h>

#define SETTYPE_NAME			"setlist"

#define IP_SET_SETLIST_ADD_AFTER	0
#define IP_SET_SETLIST_ADD_BEFORE	1

struct ip_set_setlist {
	uint8_t size;
	ip_set_id_t index[0];
};

struct ip_set_req_setlist_create {
	uint8_t size;
};

struct ip_set_req_setlist {
	char name[IP_SET_MAXNAMELEN];
	char ref[IP_SET_MAXNAMELEN];
	uint8_t before;
};

#endif /* __IP_SET_SETLIST_H */
