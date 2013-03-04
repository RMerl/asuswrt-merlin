/* Copyright (C) 2000-2002 Joakim Axelsson <gozem@linux.nu>
 *                         Patrick Schaaf <bof@bof.de>
 * Copyright (C) 2003-2008 Jozsef Kadlecsik <kadlec@blackhole.kfki.hu>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

/* Kernel module implementing an IP set type: the single bitmap type */

#include <linux/module.h>
#include <linux/ip.h>
#include <linux/skbuff.h>
#include <linux/errno.h>
#include <asm/uaccess.h>
#include <asm/bitops.h>
#include <linux/spinlock.h>

#include <linux/netfilter_ipv4/ip_set_ipmap.h>

static inline ip_set_ip_t
ip_to_id(const struct ip_set_ipmap *map, ip_set_ip_t ip)
{
	return ((ip & map->netmask) - map->first_ip)/map->hosts;
}

static inline int
ipmap_test(const struct ip_set *set, ip_set_ip_t ip)
{
	const struct ip_set_ipmap *map = set->data;
	
	if (ip < map->first_ip || ip > map->last_ip)
		return -ERANGE;

	DP("set: %s, ip:%u.%u.%u.%u", set->name, HIPQUAD(ip));
	return !!test_bit(ip_to_id(map, ip), map->members);
}

#define KADT_CONDITION

UADT(ipmap, test)
KADT(ipmap, test, ipaddr)

static inline int
ipmap_add(struct ip_set *set, ip_set_ip_t ip)
{
	struct ip_set_ipmap *map = set->data;

	if (ip < map->first_ip || ip > map->last_ip)
		return -ERANGE;

	DP("set: %s, ip:%u.%u.%u.%u", set->name, HIPQUAD(ip));
	if (test_and_set_bit(ip_to_id(map, ip), map->members))
		return -EEXIST;

	return 0;
}

UADT(ipmap, add)
KADT(ipmap, add, ipaddr)

static inline int
ipmap_del(struct ip_set *set, ip_set_ip_t ip)
{
	struct ip_set_ipmap *map = set->data;

	if (ip < map->first_ip || ip > map->last_ip)
		return -ERANGE;

	DP("set: %s, ip:%u.%u.%u.%u", set->name, HIPQUAD(ip));
	if (!test_and_clear_bit(ip_to_id(map, ip), map->members))
		return -EEXIST;
	
	return 0;
}

UADT(ipmap, del)
KADT(ipmap, del, ipaddr)

static inline int
__ipmap_create(const struct ip_set_req_ipmap_create *req,
	       struct ip_set_ipmap *map)
{
	map->netmask = req->netmask;

	if (req->netmask == 0xFFFFFFFF) {
		map->hosts = 1;
		map->sizeid = map->last_ip - map->first_ip + 1;
	} else {
		unsigned int mask_bits, netmask_bits;
		ip_set_ip_t mask;

		map->first_ip &= map->netmask;	/* Should we better bark? */

		mask = range_to_mask(map->first_ip, map->last_ip, &mask_bits);
		netmask_bits = mask_to_bits(map->netmask);

		if ((!mask && (map->first_ip || map->last_ip != 0xFFFFFFFF))
		    || netmask_bits <= mask_bits)
			return -ENOEXEC;

		DP("mask_bits %u, netmask_bits %u",
		   mask_bits, netmask_bits);
		map->hosts = 2 << (32 - netmask_bits - 1);
		map->sizeid = 2 << (netmask_bits - mask_bits - 1);
	}
	if (map->sizeid > MAX_RANGE + 1) {
		ip_set_printk("range too big, %d elements (max %d)",
			       map->sizeid, MAX_RANGE+1);
		return -ENOEXEC;
	}
	DP("hosts %u, sizeid %u", map->hosts, map->sizeid);
	return bitmap_bytes(0, map->sizeid - 1);
}

BITMAP_CREATE(ipmap)
BITMAP_DESTROY(ipmap)
BITMAP_FLUSH(ipmap)

static inline void
__ipmap_list_header(const struct ip_set_ipmap *map,
		    struct ip_set_req_ipmap_create *header)
{
	header->netmask = map->netmask;
}

BITMAP_LIST_HEADER(ipmap)
BITMAP_LIST_MEMBERS_SIZE(ipmap, ip_set_ip_t, map->sizeid,
			 test_bit(i, map->members))

static void
ipmap_list_members(const struct ip_set *set, void *data, char dont_align)
{
	const struct ip_set_ipmap *map = set->data;
	uint32_t i, n = 0;
	ip_set_ip_t *d;
	
	if (dont_align) {
		memcpy(data, map->members, map->size);
		return;
	}
	
	for (i = 0; i < map->sizeid; i++)
		if (test_bit(i, map->members)) {
			d = data + n * IPSET_ALIGN(sizeof(ip_set_ip_t));
			*d = map->first_ip + i * map->hosts;
			n++;
		}
}

IP_SET_TYPE(ipmap, IPSET_TYPE_IP | IPSET_DATA_SINGLE)

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Jozsef Kadlecsik <kadlec@blackhole.kfki.hu>");
MODULE_DESCRIPTION("ipmap type of IP sets");

REGISTER_MODULE(ipmap)
