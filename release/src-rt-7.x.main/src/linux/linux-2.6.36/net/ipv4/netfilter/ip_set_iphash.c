/* Copyright (C) 2003-2008 Jozsef Kadlecsik <kadlec@blackhole.kfki.hu>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

/* Kernel module implementing an ip hash set */

#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/ip.h>
#include <linux/skbuff.h>
#include <linux/netfilter_ipv4/ip_set_jhash.h>
#include <linux/errno.h>
#include <asm/uaccess.h>
#include <asm/bitops.h>
#include <linux/spinlock.h>
#include <linux/random.h>

#include <net/ip.h>

#include <linux/netfilter_ipv4/ip_set_iphash.h>

static int limit = MAX_RANGE;

static inline __u32
iphash_id(struct ip_set *set, ip_set_ip_t ip)
{
	struct ip_set_iphash *map = set->data;
	__u32 id;
	u_int16_t i;
	ip_set_ip_t *elem;


	ip &= map->netmask;	
	DP("set: %s, ip:%u.%u.%u.%u", set->name, HIPQUAD(ip));
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
iphash_test(struct ip_set *set, ip_set_ip_t ip)
{
	return (ip && iphash_id(set, ip) != UINT_MAX);
}

#define KADT_CONDITION

UADT(iphash, test)
KADT(iphash, test, ipaddr)

static inline int
__iphash_add(struct ip_set_iphash *map, ip_set_ip_t *ip)
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
iphash_add(struct ip_set *set, ip_set_ip_t ip)
{
	struct ip_set_iphash *map = set->data;
	
	if (!ip || map->elements >= limit)
		return -ERANGE;

	ip &= map->netmask;
	return __iphash_add(map, &ip);
}

UADT(iphash, add)
KADT(iphash, add, ipaddr)

static inline void
__iphash_retry(struct ip_set_iphash *tmp, struct ip_set_iphash *map)
{
	tmp->netmask = map->netmask;
}

HASH_RETRY(iphash, ip_set_ip_t)

static inline int
iphash_del(struct ip_set *set, ip_set_ip_t ip)
{
	struct ip_set_iphash *map = set->data;
	ip_set_ip_t id, *elem;

	if (!ip)
		return -ERANGE;

	id = iphash_id(set, ip);
	if (id == UINT_MAX)
		return -EEXIST;
		
	elem = HARRAY_ELEM(map->members, ip_set_ip_t *, id);
	*elem = 0;
	map->elements--;

	return 0;
}

UADT(iphash, del)
KADT(iphash, del, ipaddr)

static inline int
__iphash_create(const struct ip_set_req_iphash_create *req,
		struct ip_set_iphash *map)
{
	map->netmask = req->netmask;
	
	return 0;
}

HASH_CREATE(iphash, ip_set_ip_t)
HASH_DESTROY(iphash)

HASH_FLUSH(iphash, ip_set_ip_t)

static inline void
__iphash_list_header(const struct ip_set_iphash *map,
		     struct ip_set_req_iphash_create *header)
{    
	header->netmask = map->netmask;
}

HASH_LIST_HEADER(iphash)
HASH_LIST_MEMBERS_SIZE(iphash, ip_set_ip_t)
HASH_LIST_MEMBERS(iphash, ip_set_ip_t)

IP_SET_RTYPE(iphash, IPSET_TYPE_IP | IPSET_DATA_SINGLE)

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Jozsef Kadlecsik <kadlec@blackhole.kfki.hu>");
MODULE_DESCRIPTION("iphash type of IP sets");
module_param(limit, int, 0600);
MODULE_PARM_DESC(limit, "maximal number of elements stored in the sets");

REGISTER_MODULE(iphash)
