/* Copyright (C) 2008 Jozsef Kadlecsik <kadlec@blackhole.kfki.hu>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

/* Kernel module implementing an ip+port+net hash set */

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

#include <linux/netfilter_ipv4/ip_set_ipportnethash.h>
#include <linux/netfilter_ipv4/ip_set_getport.h>

static int limit = MAX_RANGE;

#define jhash_ip2(map, i, ipport, ip1)		\
	jhash_2words(ipport, ip1, *(map->initval + i))

static inline __u32
ipportnethash_id_cidr(struct ip_set *set,
		      ip_set_ip_t ip, ip_set_ip_t port,
		      ip_set_ip_t ip1, uint8_t cidr)
{
	struct ip_set_ipportnethash *map = set->data;
	__u32 id;
	u_int16_t i;
	struct ipportip *elem;

	ip = pack_ip_port(map, ip, port);
	ip1 = pack_ip_cidr(ip1, cidr);
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

static inline __u32
ipportnethash_id(struct ip_set *set,
		 ip_set_ip_t ip, ip_set_ip_t port, ip_set_ip_t ip1)
{
	struct ip_set_ipportnethash *map = set->data;
	__u32 id = UINT_MAX;
	int i;

	for (i = 0; i < 30 && map->cidr[i]; i++) {
		id = ipportnethash_id_cidr(set, ip, port, ip1, map->cidr[i]);
		if (id != UINT_MAX)
			break;
	}
	return id;
}

static inline int
ipportnethash_test_cidr(struct ip_set *set,
			ip_set_ip_t ip, ip_set_ip_t port,
			ip_set_ip_t ip1, uint8_t cidr)
{
	struct ip_set_ipportnethash *map = set->data;
	
	if (ip < map->first_ip || ip > map->last_ip)
		return -ERANGE;

	return (ipportnethash_id_cidr(set, ip, port, ip1, cidr) != UINT_MAX);
}

static inline int
ipportnethash_test(struct ip_set *set,
		  ip_set_ip_t ip, ip_set_ip_t port, ip_set_ip_t ip1)
{
	struct ip_set_ipportnethash *map = set->data;
	
	if (ip < map->first_ip || ip > map->last_ip)
		return -ERANGE;

	return (ipportnethash_id(set, ip, port, ip1) != UINT_MAX);
}

static int
ipportnethash_utest(struct ip_set *set, const void *data, u_int32_t size)
{
	const struct ip_set_req_ipportnethash *req = data;

	if (req->cidr <= 0 || req->cidr > 32)
		return -EINVAL;
	return (req->cidr == 32 
		? ipportnethash_test(set, req->ip, req->port, req->ip1)
		: ipportnethash_test_cidr(set, req->ip, req->port,
					  req->ip1, req->cidr));
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

KADT(ipportnethash, test, ipaddr, port, ip1)

static inline int
__ipportnet_add(struct ip_set_ipportnethash *map,
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
__ipportnethash_add(struct ip_set_ipportnethash *map,
		    struct ipportip *elem)
{
	return __ipportnet_add(map, elem->ip, elem->ip1);
}

static inline int
ipportnethash_add(struct ip_set *set,
		 ip_set_ip_t ip, ip_set_ip_t port,
		 ip_set_ip_t ip1, uint8_t cidr)
{
	struct ip_set_ipportnethash *map = set->data;
	struct ipportip;
	int ret;
	
	if (map->elements > limit)
		return -ERANGE;
	if (ip < map->first_ip || ip > map->last_ip)
		return -ERANGE;
	if (cidr <= 0 || cidr >= 32)
		return -EINVAL;
	if (map->nets[cidr-1] == UINT16_MAX)
		return -ERANGE;

	ip = pack_ip_port(map, ip, port);
	ip1 = pack_ip_cidr(ip1, cidr);
	if (!(ip || ip1))
		return -ERANGE;
	
	ret =__ipportnet_add(map, ip, ip1);
	if (ret == 0) {
		if (!map->nets[cidr-1]++)
			add_cidr_size(map->cidr, cidr);
	}
	return ret;
}

#undef KADT_CONDITION
#define KADT_CONDITION							\
	struct ip_set_ipportnethash *map = set->data;			\
	uint8_t cidr = map->cidr[0] ? map->cidr[0] : 31;		\
	ip_set_ip_t port, ip1;						\
									\
	if (flags[2] == 0)						\
		return 0;						\
									\
	port = get_port(skb, flags++);					\
	ip1 = ipaddr(skb, flags++);					\
									\
	if (port == INVALID_PORT)					\
		return 0;

UADT(ipportnethash, add, req->port, req->ip1, req->cidr)
KADT(ipportnethash, add, ipaddr, port, ip1, cidr)

static inline void
__ipportnethash_retry(struct ip_set_ipportnethash *tmp,
		      struct ip_set_ipportnethash *map)
{
	tmp->first_ip = map->first_ip;
	tmp->last_ip = map->last_ip;
	memcpy(tmp->cidr, map->cidr, sizeof(tmp->cidr));
	memcpy(tmp->nets, map->nets, sizeof(tmp->nets));
}

HASH_RETRY2(ipportnethash, struct ipportip)

static inline int
ipportnethash_del(struct ip_set *set,
	          ip_set_ip_t ip, ip_set_ip_t port,
	          ip_set_ip_t ip1, uint8_t cidr)
{
	struct ip_set_ipportnethash *map = set->data;
	ip_set_ip_t id;
	struct ipportip *elem;

	if (ip < map->first_ip || ip > map->last_ip)
		return -ERANGE;
	if (!ip)
		return -ERANGE;
	if (cidr <= 0 || cidr >= 32)
		return -EINVAL;	

	id = ipportnethash_id_cidr(set, ip, port, ip1, cidr);

	if (id == UINT_MAX)
		return -EEXIST;
		
	elem = HARRAY_ELEM(map->members, struct ipportip *, id);
	elem->ip = elem->ip1 = 0;
	map->elements--;
	if (!map->nets[cidr-1]--)
		del_cidr_size(map->cidr, cidr);

	return 0;
}

UADT(ipportnethash, del, req->port, req->ip1, req->cidr)
KADT(ipportnethash, del, ipaddr, port, ip1, cidr)

static inline int
__ipportnethash_create(const struct ip_set_req_ipportnethash_create *req,
		       struct ip_set_ipportnethash *map)
{
	if (req->to - req->from > MAX_RANGE) {
		ip_set_printk("range too big, %d elements (max %d)",
			      req->to - req->from + 1, MAX_RANGE+1);
		return -ENOEXEC;
	}
	map->first_ip = req->from;
	map->last_ip = req->to;
	memset(map->cidr, 0, sizeof(map->cidr));
	memset(map->nets, 0, sizeof(map->nets));
	return 0;
}

HASH_CREATE(ipportnethash, struct ipportip)
HASH_DESTROY(ipportnethash)
HASH_FLUSH_CIDR(ipportnethash, struct ipportip);

static inline void
__ipportnethash_list_header(const struct ip_set_ipportnethash *map,
			    struct ip_set_req_ipportnethash_create *header)
{
	header->from = map->first_ip;
	header->to = map->last_ip;
}

HASH_LIST_HEADER(ipportnethash)

HASH_LIST_MEMBERS_SIZE(ipportnethash, struct ipportip)
HASH_LIST_MEMBERS_MEMCPY(ipportnethash, struct ipportip,
			 (elem->ip || elem->ip1))

IP_SET_RTYPE(ipportnethash, IPSET_TYPE_IP | IPSET_TYPE_PORT
			    | IPSET_TYPE_IP1 | IPSET_DATA_TRIPLE)

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Jozsef Kadlecsik <kadlec@blackhole.kfki.hu>");
MODULE_DESCRIPTION("ipportnethash type of IP sets");
module_param(limit, int, 0600);
MODULE_PARM_DESC(limit, "maximal number of elements stored in the sets");

REGISTER_MODULE(ipportnethash)
