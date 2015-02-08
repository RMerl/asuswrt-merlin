/* Copyright (C) 2003-2008 Jozsef Kadlecsik <kadlec@blackhole.kfki.hu>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

/* Kernel module implementing a cidr nethash set */

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

#include <linux/netfilter_ipv4/ip_set_nethash.h>

static int limit = MAX_RANGE;

static inline __u32
nethash_id_cidr(const struct ip_set_nethash *map,
		ip_set_ip_t ip,
		uint8_t cidr)
{
	__u32 id;
	u_int16_t i;
	ip_set_ip_t *elem;

	ip = pack_ip_cidr(ip, cidr);
	if (!ip)
		return MAX_RANGE;
	
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

static inline __u32
nethash_id(struct ip_set *set, ip_set_ip_t ip)
{
	const struct ip_set_nethash *map = set->data;
	__u32 id = UINT_MAX;
	int i;

	for (i = 0; i < 30 && map->cidr[i]; i++) {
		id = nethash_id_cidr(map, ip, map->cidr[i]);
		if (id != UINT_MAX)
			break;
	}
	return id;
}

static inline int
nethash_test_cidr(struct ip_set *set, ip_set_ip_t ip, uint8_t cidr)
{
	const struct ip_set_nethash *map = set->data;

	return (nethash_id_cidr(map, ip, cidr) != UINT_MAX);
}

static inline int
nethash_test(struct ip_set *set, ip_set_ip_t ip)
{
	return (nethash_id(set, ip) != UINT_MAX);
}

static int
nethash_utest(struct ip_set *set, const void *data, u_int32_t size)
{
	const struct ip_set_req_nethash *req = data;

	if (req->cidr <= 0 || req->cidr > 32)
		return -EINVAL;
	return (req->cidr == 32 ? nethash_test(set, req->ip)
		: nethash_test_cidr(set, req->ip, req->cidr));
}

#define KADT_CONDITION

KADT(nethash, test, ipaddr)

static inline int
__nethash_add(struct ip_set_nethash *map, ip_set_ip_t *ip)
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
nethash_add(struct ip_set *set, ip_set_ip_t ip, uint8_t cidr)
{
	struct ip_set_nethash *map = set->data;
	int ret;
	
	if (map->elements >= limit || map->nets[cidr-1] == UINT16_MAX)
		return -ERANGE;	
	if (cidr <= 0 || cidr >= 32)
		return -EINVAL;

	ip = pack_ip_cidr(ip, cidr);
	if (!ip)
		return -ERANGE;
	
	ret = __nethash_add(map, &ip);
	if (ret == 0) {
		if (!map->nets[cidr-1]++)
			add_cidr_size(map->cidr, cidr);
	}
	
	return ret;
}

#undef KADT_CONDITION
#define KADT_CONDITION							\
	struct ip_set_nethash *map = set->data;				\
	uint8_t cidr = map->cidr[0] ? map->cidr[0] : 31;

UADT(nethash, add, req->cidr)
KADT(nethash, add, ipaddr, cidr)

static inline void
__nethash_retry(struct ip_set_nethash *tmp, struct ip_set_nethash *map)
{
	memcpy(tmp->cidr, map->cidr, sizeof(tmp->cidr));
	memcpy(tmp->nets, map->nets, sizeof(tmp->nets));
}

HASH_RETRY(nethash, ip_set_ip_t)

static inline int
nethash_del(struct ip_set *set, ip_set_ip_t ip, uint8_t cidr)
{
	struct ip_set_nethash *map = set->data;
	ip_set_ip_t id, *elem;

	if (cidr <= 0 || cidr >= 32)
		return -EINVAL;	
	
	id = nethash_id_cidr(map, ip, cidr);
	if (id == UINT_MAX)
		return -EEXIST;
		
	elem = HARRAY_ELEM(map->members, ip_set_ip_t *, id);
	*elem = 0;
	map->elements--;
	if (!map->nets[cidr-1]--)
		del_cidr_size(map->cidr, cidr);
	return 0;
}

UADT(nethash, del, req->cidr)
KADT(nethash, del, ipaddr, cidr)

static inline int
__nethash_create(const struct ip_set_req_nethash_create *req,
		 struct ip_set_nethash *map)
{
	memset(map->cidr, 0, sizeof(map->cidr));
	memset(map->nets, 0, sizeof(map->nets));
	
	return 0;
}

HASH_CREATE(nethash, ip_set_ip_t)
HASH_DESTROY(nethash)

HASH_FLUSH_CIDR(nethash, ip_set_ip_t)

static inline void
__nethash_list_header(const struct ip_set_nethash *map,
		      struct ip_set_req_nethash_create *header)
{    
}

HASH_LIST_HEADER(nethash)
HASH_LIST_MEMBERS_SIZE(nethash, ip_set_ip_t)
HASH_LIST_MEMBERS(nethash, ip_set_ip_t)

IP_SET_RTYPE(nethash, IPSET_TYPE_IP | IPSET_DATA_SINGLE)

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Jozsef Kadlecsik <kadlec@blackhole.kfki.hu>");
MODULE_DESCRIPTION("nethash type of IP sets");
module_param(limit, int, 0600);
MODULE_PARM_DESC(limit, "maximal number of elements stored in the sets");

REGISTER_MODULE(nethash)
