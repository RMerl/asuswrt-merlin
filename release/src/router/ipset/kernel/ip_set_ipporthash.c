/* Copyright (C) 2003-2008 Jozsef Kadlecsik <kadlec@blackhole.kfki.hu>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

/* Kernel module implementing an ip+port hash set */

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

#include <linux/netfilter_ipv4/ip_set_ipporthash.h>
#include <linux/netfilter_ipv4/ip_set_getport.h>

static int limit = MAX_RANGE;

static inline __u32
ipporthash_id(struct ip_set *set, ip_set_ip_t ip, ip_set_ip_t port)
{
	struct ip_set_ipporthash *map = set->data;
	__u32 id;
	u_int16_t i;
	ip_set_ip_t *elem;

	ip = pack_ip_port(map, ip, port);
		
	if (!ip)
		return UINT_MAX;
	
	for (i = 0; i < map->probes; i++) {
		id = jhash_ip(map, i, ip) % map->hashsize;
		DP("hash key: %u", id);
		elem = HARRAY_ELEM(map->members, ip_set_ip_t *, id);
		if (*elem == ip)
			return id;
		/* No shortcut - there can be deleted entries. */
	}
	return UINT_MAX;
}

static inline int
ipporthash_test(struct ip_set *set, ip_set_ip_t ip, ip_set_ip_t port)
{
	struct ip_set_ipporthash *map = set->data;
	
	if (ip < map->first_ip || ip > map->last_ip)
		return -ERANGE;

	return (ipporthash_id(set, ip, port) != UINT_MAX);
}

#define KADT_CONDITION						\
	ip_set_ip_t port;					\
								\
	if (flags[1] == 0)					\
		return 0;					\
								\
	port = get_port(skb, ++flags);				\
								\
	if (port == INVALID_PORT)				\
		return 0;

UADT(ipporthash, test, req->port)
KADT(ipporthash, test, ipaddr, port)

static inline int
__ipporthash_add(struct ip_set_ipporthash *map, ip_set_ip_t *ip)
{
	__u32 probe;
	u_int16_t i;
	ip_set_ip_t *elem, *slot = NULL;

	for (i = 0; i < map->probes; i++) {
		probe = jhash_ip(map, i, *ip) % map->hashsize;
		elem = HARRAY_ELEM(map->members, ip_set_ip_t *, probe);
		if (*elem == *ip)
			return -EEXIST;
		if (!(slot || *elem))
			slot = elem;
		/* There can be deleted entries, must check all slots */
	}
	if (slot) {
		*slot = *ip;
		map->elements++;
		return 0;
	}
	/* Trigger rehashing */
	return -EAGAIN;
}

static inline int
ipporthash_add(struct ip_set *set, ip_set_ip_t ip, ip_set_ip_t port)
{
	struct ip_set_ipporthash *map = set->data;
	if (map->elements > limit)
		return -ERANGE;
	if (ip < map->first_ip || ip > map->last_ip)
		return -ERANGE;

	ip = pack_ip_port(map, ip, port);

	if (!ip)
		return -ERANGE;
	
	return __ipporthash_add(map, &ip);
}

UADT(ipporthash, add, req->port)
KADT(ipporthash, add, ipaddr, port)

static inline void
__ipporthash_retry(struct ip_set_ipporthash *tmp,
		   struct ip_set_ipporthash *map)
{
	tmp->first_ip = map->first_ip;
	tmp->last_ip = map->last_ip;
}

HASH_RETRY(ipporthash, ip_set_ip_t)

static inline int
ipporthash_del(struct ip_set *set, ip_set_ip_t ip, ip_set_ip_t port)
{
	struct ip_set_ipporthash *map = set->data;
	ip_set_ip_t id;
	ip_set_ip_t *elem;

	if (ip < map->first_ip || ip > map->last_ip)
		return -ERANGE;

	id = ipporthash_id(set, ip, port);

	if (id == UINT_MAX)
		return -EEXIST;
		
	elem = HARRAY_ELEM(map->members, ip_set_ip_t *, id);
	*elem = 0;
	map->elements--;

	return 0;
}

UADT(ipporthash, del, req->port)
KADT(ipporthash, del, ipaddr, port)

static inline int
__ipporthash_create(const struct ip_set_req_ipporthash_create *req,
		    struct ip_set_ipporthash *map)
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

HASH_CREATE(ipporthash, ip_set_ip_t)
HASH_DESTROY(ipporthash)
HASH_FLUSH(ipporthash, ip_set_ip_t)

static inline void
__ipporthash_list_header(const struct ip_set_ipporthash *map,
			 struct ip_set_req_ipporthash_create *header)
{
	header->from = map->first_ip;
	header->to = map->last_ip;
}

HASH_LIST_HEADER(ipporthash)
HASH_LIST_MEMBERS_SIZE(ipporthash, ip_set_ip_t)
HASH_LIST_MEMBERS(ipporthash, ip_set_ip_t)

IP_SET_RTYPE(ipporthash, IPSET_TYPE_IP | IPSET_TYPE_PORT | IPSET_DATA_DOUBLE)

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Jozsef Kadlecsik <kadlec@blackhole.kfki.hu>");
MODULE_DESCRIPTION("ipporthash type of IP sets");
module_param(limit, int, 0600);
MODULE_PARM_DESC(limit, "maximal number of elements stored in the sets");

REGISTER_MODULE(ipporthash)
