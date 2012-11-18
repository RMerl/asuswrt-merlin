/* Copyright (C) 2008 Jozsef Kadlecsik <kadlec@blackhole.kfki.hu>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

/* Kernel module implementing an ip+port+ip hash set */

#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/ip.h>
#include <linux/tcp.h>
#include <linux/udp.h>
#include <linux/skbuff.h>
#include <linux/netfilter_ipv4/ip_set_jhash.h>
#include <linux/errno.h>
#include <asm/uaccess.h>
#include <asm/bitops.h>
#include <linux/spinlock.h>
#include <linux/random.h>

#include <net/ip.h>

#include <linux/netfilter_ipv4/ip_set_ipportiphash.h>
#include <linux/netfilter_ipv4/ip_set_getport.h>

static int limit = MAX_RANGE;

#define jhash_ip2(map, i, ipport, ip1)		\
	jhash_2words(ipport, ip1, *(map->initval + i))

static inline __u32
ipportiphash_id(struct ip_set *set,
		ip_set_ip_t ip, ip_set_ip_t port, ip_set_ip_t ip1)
{
	struct ip_set_ipportiphash *map = set->data;
	__u32 id;
	u_int16_t i;
	struct ipportip *elem;

	ip = pack_ip_port(map, ip, port);
	if (!(ip || ip1))
		return UINT_MAX;
	
	for (i = 0; i < map->probes; i++) {
		id = jhash_ip2(map, i, ip, ip1) % map->hashsize;
		DP("hash key: %u", id);
		elem = HARRAY_ELEM(map->members, struct ipportip *, id);
		if (elem->ip == ip && elem->ip1 == ip1)
			return id;
		/* No shortcut - there can be deleted entries. */
	}
	return UINT_MAX;
}

static inline int
ipportiphash_test(struct ip_set *set,
		  ip_set_ip_t ip, ip_set_ip_t port, ip_set_ip_t ip1)
{
	struct ip_set_ipportiphash *map = set->data;
	
	if (ip < map->first_ip || ip > map->last_ip)
		return -ERANGE;

	return (ipportiphash_id(set, ip, port, ip1) != UINT_MAX);
}

#define KADT_CONDITION						\
	ip_set_ip_t port, ip1;					\
								\
	if (flags[2] == 0)					\
		return 0;					\
								\
	port = get_port(skb, ++flags);				\
	ip1 = ipaddr(skb, ++flags);				\
								\
	if (port == INVALID_PORT)				\
		return 0;

UADT(ipportiphash, test, req->port, req->ip1)
KADT(ipportiphash, test, ipaddr, port, ip1)

static inline int
__ipportip_add(struct ip_set_ipportiphash *map,
	       ip_set_ip_t ip, ip_set_ip_t ip1)
{
	__u32 probe;
	u_int16_t i;
	struct ipportip *elem, *slot = NULL;

	for (i = 0; i < map->probes; i++) {
		probe = jhash_ip2(map, i, ip, ip1) % map->hashsize;
		elem = HARRAY_ELEM(map->members, struct ipportip *, probe);
		if (elem->ip == ip && elem->ip1 == ip1)
			return -EEXIST;
		if (!(slot || elem->ip || elem->ip1))
			slot = elem;
		/* There can be deleted entries, must check all slots */
	}
	if (slot) {
		slot->ip = ip;
		slot->ip1 = ip1;
		map->elements++;
		return 0;
	}
	/* Trigger rehashing */
	return -EAGAIN;
}

static inline int
__ipportiphash_add(struct ip_set_ipportiphash *map,
		   struct ipportip *elem)
{
	return __ipportip_add(map, elem->ip, elem->ip1);
}

static inline int
ipportiphash_add(struct ip_set *set,
		 ip_set_ip_t ip, ip_set_ip_t port, ip_set_ip_t ip1)
{
	struct ip_set_ipportiphash *map = set->data;
	
	if (map->elements > limit)
		return -ERANGE;
	if (ip < map->first_ip || ip > map->last_ip)
		return -ERANGE;

	ip = pack_ip_port(map, ip, port);
	if (!(ip || ip1))
		return -ERANGE;
	
	return __ipportip_add(map, ip, ip1);
}

UADT(ipportiphash, add, req->port, req->ip1)
KADT(ipportiphash, add, ipaddr, port, ip1)

static inline void
__ipportiphash_retry(struct ip_set_ipportiphash *tmp,
		     struct ip_set_ipportiphash *map)
{
	tmp->first_ip = map->first_ip;
	tmp->last_ip = map->last_ip;
}

HASH_RETRY2(ipportiphash, struct ipportip)

static inline int
ipportiphash_del(struct ip_set *set,
	       ip_set_ip_t ip, ip_set_ip_t port, ip_set_ip_t ip1)
{
	struct ip_set_ipportiphash *map = set->data;
	ip_set_ip_t id;
	struct ipportip *elem;

	if (ip < map->first_ip || ip > map->last_ip)
		return -ERANGE;

	id = ipportiphash_id(set, ip, port, ip1);

	if (id == UINT_MAX)
		return -EEXIST;
		
	elem = HARRAY_ELEM(map->members, struct ipportip *, id);
	elem->ip = elem->ip1 = 0;
	map->elements--;

	return 0;
}

UADT(ipportiphash, del, req->port, req->ip1)
KADT(ipportiphash, del, ipaddr, port, ip1)

static inline int
__ipportiphash_create(const struct ip_set_req_ipportiphash_create *req,
		      struct ip_set_ipportiphash *map)
{
	if (req->to - req->from > MAX_RANGE) {
		ip_set_printk("range too big, %d elements (max %d)",
			      req->to - req->from + 1, MAX_RANGE+1);
		return -ENOEXEC;
	}
	map->first_ip = req->from;
	map->last_ip = req->to;
	return 0;
}

HASH_CREATE(ipportiphash, struct ipportip)
HASH_DESTROY(ipportiphash)
HASH_FLUSH(ipportiphash, struct ipportip)

static inline void
__ipportiphash_list_header(const struct ip_set_ipportiphash *map,
			   struct ip_set_req_ipportiphash_create *header)
{
	header->from = map->first_ip;
	header->to = map->last_ip;
}

HASH_LIST_HEADER(ipportiphash)
HASH_LIST_MEMBERS_SIZE(ipportiphash, struct ipportip)
HASH_LIST_MEMBERS_MEMCPY(ipportiphash, struct ipportip,
			 (elem->ip || elem->ip1))

IP_SET_RTYPE(ipportiphash, IPSET_TYPE_IP | IPSET_TYPE_PORT
			   | IPSET_TYPE_IP1 | IPSET_DATA_TRIPLE)

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Jozsef Kadlecsik <kadlec@blackhole.kfki.hu>");
MODULE_DESCRIPTION("ipportiphash type of IP sets");
module_param(limit, int, 0600);
MODULE_PARM_DESC(limit, "maximal number of elements stored in the sets");

REGISTER_MODULE(ipportiphash)
