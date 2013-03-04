/* Copyright (C) 2000-2002 Joakim Axelsson <gozem@linux.nu>
 *                         Patrick Schaaf <bof@bof.de>
 *                         Martin Josefsson <gandalf@wlug.westbo.se>
 * Copyright (C) 2003-2008 Jozsef Kadlecsik <kadlec@blackhole.kfki.hu>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

/* Kernel module implementing an IP set type: the macipmap type */

#include <linux/module.h>
#include <linux/ip.h>
#include <linux/skbuff.h>
#include <linux/errno.h>
#include <asm/uaccess.h>
#include <asm/bitops.h>
#include <linux/spinlock.h>
#include <linux/if_ether.h>

#include <linux/netfilter_ipv4/ip_set_macipmap.h>

static int
macipmap_utest(struct ip_set *set, const void *data, u_int32_t size)
{
	const struct ip_set_macipmap *map = set->data;
	const struct ip_set_macip *table = map->members;	
	const struct ip_set_req_macipmap *req = data;

	if (req->ip < map->first_ip || req->ip > map->last_ip)
		return -ERANGE;

	DP("set: %s, ip:%u.%u.%u.%u", set->name, HIPQUAD(req->ip));		
	if (table[req->ip - map->first_ip].match) {
		return (memcmp(req->ethernet,
			       &table[req->ip - map->first_ip].ethernet,
			       ETH_ALEN) == 0);
	} else {
		return (map->flags & IPSET_MACIP_MATCHUNSET ? 1 : 0);
	}
}

static int
macipmap_ktest(struct ip_set *set,
	       const struct sk_buff *skb,
	       const u_int32_t *flags)
{
	const struct ip_set_macipmap *map = set->data;
	const struct ip_set_macip *table = map->members;
	ip_set_ip_t ip;
	
	ip = ipaddr(skb, flags);

	if (ip < map->first_ip || ip > map->last_ip)
		return 0;

	DP("set: %s, ip:%u.%u.%u.%u", set->name, HIPQUAD(ip));
	if (table[ip - map->first_ip].match) {
		/* Is mac pointer valid?
		 * If so, compare... */
		return (skb_mac_header(skb) >= skb->head
			&& (skb_mac_header(skb) + ETH_HLEN) <= skb->data
			&& (memcmp(eth_hdr(skb)->h_source,
				   &table[ip - map->first_ip].ethernet,
				   ETH_ALEN) == 0));
	} else {
		return (map->flags & IPSET_MACIP_MATCHUNSET ? 1 : 0);
	}
}

/* returns 0 on success */
static inline int
macipmap_add(struct ip_set *set,
	     ip_set_ip_t ip, const unsigned char *ethernet)
{
	struct ip_set_macipmap *map = set->data;
	struct ip_set_macip *table = map->members;

	if (ip < map->first_ip || ip > map->last_ip)
		return -ERANGE;
	if (table[ip - map->first_ip].match)
		return -EEXIST;

	DP("set: %s, ip: %u.%u.%u.%u", set->name, HIPQUAD(ip));
	memcpy(&table[ip - map->first_ip].ethernet, ethernet, ETH_ALEN);
	table[ip - map->first_ip].match = IPSET_MACIP_ISSET;
	return 0;
}

#define KADT_CONDITION						\
	if (!(skb_mac_header(skb) >= skb->head			\
	      && (skb_mac_header(skb) + ETH_HLEN) <= skb->data))\
		return -EINVAL;

UADT(macipmap, add, req->ethernet)
KADT(macipmap, add, ipaddr, eth_hdr(skb)->h_source)

static inline int
macipmap_del(struct ip_set *set, ip_set_ip_t ip)
{
	struct ip_set_macipmap *map = set->data;
	struct ip_set_macip *table = map->members;

	if (ip < map->first_ip || ip > map->last_ip)
		return -ERANGE;
	if (!table[ip - map->first_ip].match)
		return -EEXIST;

	table[ip - map->first_ip].match = 0;
	DP("set: %s, ip: %u.%u.%u.%u", set->name, HIPQUAD(ip));
	return 0;
}

#undef KADT_CONDITION
#define KADT_CONDITION

UADT(macipmap, del)
KADT(macipmap, del, ipaddr)

static inline int
__macipmap_create(const struct ip_set_req_macipmap_create *req,
		  struct ip_set_macipmap *map)
{
	if (req->to - req->from > MAX_RANGE) {
		ip_set_printk("range too big, %d elements (max %d)",
			      req->to - req->from + 1, MAX_RANGE+1);
		return -ENOEXEC;
	}
	map->flags = req->flags;
	return (req->to - req->from + 1) * sizeof(struct ip_set_macip);
}

BITMAP_CREATE(macipmap)
BITMAP_DESTROY(macipmap)
BITMAP_FLUSH(macipmap)

static inline void
__macipmap_list_header(const struct ip_set_macipmap *map,
		       struct ip_set_req_macipmap_create *header)
{
	header->flags = map->flags;
}

BITMAP_LIST_HEADER(macipmap)
BITMAP_LIST_MEMBERS_SIZE(macipmap, struct ip_set_req_macipmap,
			 (map->last_ip - map->first_ip + 1),
			 ((const struct ip_set_macip *)map->members)[i].match)


static void
macipmap_list_members(const struct ip_set *set, void *data, char dont_align)
{
	const struct ip_set_macipmap *map = set->data;
	const struct ip_set_macip *table = map->members;
	uint32_t i, n = 0;
	struct ip_set_req_macipmap *d;
	
	if (dont_align) {
		memcpy(data, map->members, map->size);
		return;
	}
	
	for (i = 0; i < map->last_ip - map->first_ip + 1; i++)
		if (table[i].match) {
			d = data + n * IPSET_ALIGN(sizeof(struct ip_set_req_macipmap));
			d->ip = map->first_ip + i;
			memcpy(d->ethernet, &table[i].ethernet, ETH_ALEN);
			n++;
		}
}

IP_SET_TYPE(macipmap, IPSET_TYPE_IP | IPSET_DATA_SINGLE)

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Jozsef Kadlecsik <kadlec@blackhole.kfki.hu>");
MODULE_DESCRIPTION("macipmap type of IP sets");

REGISTER_MODULE(macipmap)
