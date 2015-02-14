/* Copyright (C) 2003-2008 Jozsef Kadlecsik <kadlec@blackhole.kfki.hu>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

/* Kernel module implementing a port set type as a bitmap */

#include <linux/module.h>
#include <linux/ip.h>
#include <linux/tcp.h>
#include <linux/udp.h>
#include <linux/skbuff.h>
#include <linux/errno.h>
#include <asm/uaccess.h>
#include <asm/bitops.h>
#include <linux/spinlock.h>

#include <net/ip.h>

#include <linux/netfilter_ipv4/ip_set_portmap.h>
#include <linux/netfilter_ipv4/ip_set_getport.h>

static inline int
portmap_test(const struct ip_set *set, ip_set_ip_t port)
{
	const struct ip_set_portmap *map = set->data;

	if (port < map->first_ip || port > map->last_ip)
		return -ERANGE;
		
	DP("set: %s, port: %u", set->name, port);
	return !!test_bit(port - map->first_ip, map->members);
}

#define KADT_CONDITION			\
	if (ip == INVALID_PORT)		\
		return 0;	

UADT(portmap, test)
KADT(portmap, test, get_port)

static inline int
portmap_add(struct ip_set *set, ip_set_ip_t port)
{
	struct ip_set_portmap *map = set->data;

	if (port < map->first_ip || port > map->last_ip)
		return -ERANGE;
	if (test_and_set_bit(port - map->first_ip, map->members))
		return -EEXIST;
	
	DP("set: %s, port %u", set->name, port);
	return 0;
}

UADT(portmap, add)
KADT(portmap, add, get_port)

static inline int
portmap_del(struct ip_set *set, ip_set_ip_t port)
{
	struct ip_set_portmap *map = set->data;

	if (port < map->first_ip || port > map->last_ip)
		return -ERANGE;
	if (!test_and_clear_bit(port - map->first_ip, map->members))
		return -EEXIST;
	
	DP("set: %s, port %u", set->name, port);
	return 0;
}

UADT(portmap, del)
KADT(portmap, del, get_port)

static inline int
__portmap_create(const struct ip_set_req_portmap_create *req,
		 struct ip_set_portmap *map)
{
	if (req->to - req->from > MAX_RANGE) {
		ip_set_printk("range too big, %d elements (max %d)",
			      req->to - req->from + 1, MAX_RANGE+1);
		return -ENOEXEC;
	}
	return bitmap_bytes(req->from, req->to);
}

BITMAP_CREATE(portmap)
BITMAP_DESTROY(portmap)
BITMAP_FLUSH(portmap)

static inline void
__portmap_list_header(const struct ip_set_portmap *map,
		      struct ip_set_req_portmap_create *header)
{
}

BITMAP_LIST_HEADER(portmap)
BITMAP_LIST_MEMBERS_SIZE(portmap, ip_set_ip_t, (map->last_ip - map->first_ip + 1),
			 test_bit(i, map->members))

static void
portmap_list_members(const struct ip_set *set, void *data, char dont_align)
{
	const struct ip_set_portmap *map = set->data;
	uint32_t i, n = 0;
	ip_set_ip_t *d;
	
	if (dont_align) {
		memcpy(data, map->members, map->size);
		return;
	}
	
	for (i = 0; i < map->last_ip - map->first_ip + 1; i++)
		if (test_bit(i, map->members)) {
			d = data + n * IPSET_ALIGN(sizeof(ip_set_ip_t));
			*d = map->first_ip + i;
			n++;
		}
}

IP_SET_TYPE(portmap, IPSET_TYPE_PORT | IPSET_DATA_SINGLE)

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Jozsef Kadlecsik <kadlec@blackhole.kfki.hu>");
MODULE_DESCRIPTION("portmap type of IP sets");

REGISTER_MODULE(portmap)
