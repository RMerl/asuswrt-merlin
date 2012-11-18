/* Copyright (C) 2008 Jozsef Kadlecsik <kadlec@blackhole.kfki.hu>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

/* Kernel module implementing an IP set type: the setlist type */

#include <linux/module.h>
#include <linux/ip.h>
#include <linux/skbuff.h>
#include <linux/errno.h>

#include <linux/netfilter_ipv4/ip_set.h>
#include <linux/netfilter_ipv4/ip_set_bitmaps.h>
#include <linux/netfilter_ipv4/ip_set_setlist.h>

/*
 * before ==> index, ref
 * after  ==> ref, index
 */

static inline int
next_index_eq(const struct ip_set_setlist *map, int i, ip_set_id_t index)
{
	return i < map->size && map->index[i] == index;
}

static int
setlist_utest(struct ip_set *set, const void *data, u_int32_t size)
{
	const struct ip_set_setlist *map = set->data;
	const struct ip_set_req_setlist *req = data;
	ip_set_id_t index, ref = IP_SET_INVALID_ID;
	int i, res = 0;
	struct ip_set *s;
	
	if (req->before && req->ref[0] == '\0')
		return 0;

	index = __ip_set_get_byname(req->name, &s);
	if (index == IP_SET_INVALID_ID)
		return 0;
	if (req->ref[0] != '\0') {
		ref = __ip_set_get_byname(req->ref, &s);
		if (ref == IP_SET_INVALID_ID)
			goto finish;
	}
	for (i = 0; i < map->size
		    && map->index[i] != IP_SET_INVALID_ID; i++) {
		if (req->before && map->index[i] == index) {
		    	res = next_index_eq(map, i + 1, ref);
		    	break;
		} else if (!req->before) {
			if ((ref == IP_SET_INVALID_ID
			     && map->index[i] == index)
			    || (map->index[i] == ref
			        && next_index_eq(map, i + 1, index))) {
			        res = 1;
			        break;
			}
		}
	}
	if (ref != IP_SET_INVALID_ID)
		__ip_set_put_byindex(ref);
finish:
	__ip_set_put_byindex(index);
	return res;
}

static int
setlist_ktest(struct ip_set *set,
	      const struct sk_buff *skb,
	      const u_int32_t *flags)
{
	struct ip_set_setlist *map = set->data;
	int i, res = 0;
	
	for (i = 0; i < map->size
		    && map->index[i] != IP_SET_INVALID_ID
		    && res == 0; i++)
		res = ip_set_testip_kernel(map->index[i], skb, flags);
	return res;
}

static inline int
insert_setlist(struct ip_set_setlist *map, int i, ip_set_id_t index)
{
	ip_set_id_t tmp;
	int j;

	DP("i: %u, last %u\n", i, map->index[map->size - 1]);	
	if (i >= map->size || map->index[map->size - 1] != IP_SET_INVALID_ID)
		return -ERANGE;
	
	for (j = i; j < map->size
		    && index != IP_SET_INVALID_ID; j++) {
		tmp = map->index[j];
		map->index[j] = index;
		index = tmp;
	}
	return 0;
}

static int
setlist_uadd(struct ip_set *set, const void *data, u_int32_t size)
{
	struct ip_set_setlist *map = set->data;
	const struct ip_set_req_setlist *req = data;
	ip_set_id_t index, ref = IP_SET_INVALID_ID;
	int i, res = -ERANGE;
	struct ip_set *s;
	
	if (req->before && req->ref[0] == '\0')
		return -EINVAL;

	index = __ip_set_get_byname(req->name, &s);
	if (index == IP_SET_INVALID_ID)
		return -EEXIST;
	/* "Loop detection" */
	if (strcmp(s->type->typename, "setlist") == 0)
		goto finish;

	if (req->ref[0] != '\0') {
		ref = __ip_set_get_byname(req->ref, &s);
		if (ref == IP_SET_INVALID_ID) {
			res = -EEXIST;
			goto finish;
		}
	}
	for (i = 0; i < map->size; i++) {
		if (map->index[i] != ref)
			continue;
		if (req->before) 
			res = insert_setlist(map, i, index);
		else
			res = insert_setlist(map,
				ref == IP_SET_INVALID_ID ? i : i + 1,
				index);
		break;
	}
	if (ref != IP_SET_INVALID_ID)
		__ip_set_put_byindex(ref);
	/* In case of success, we keep the reference to the set */
finish:
	if (res != 0)
		__ip_set_put_byindex(index);
	return res;
}

static int
setlist_kadd(struct ip_set *set,
	     const struct sk_buff *skb,
	     const u_int32_t *flags)
{
	struct ip_set_setlist *map = set->data;
	int i, res = -EINVAL;
	
	for (i = 0; i < map->size
		    && map->index[i] != IP_SET_INVALID_ID
		    && res != 0; i++)
		res = ip_set_addip_kernel(map->index[i], skb, flags);
	return res;
}

static inline int
unshift_setlist(struct ip_set_setlist *map, int i)
{
	int j;
	
	for (j = i; j < map->size - 1; j++)
		map->index[j] = map->index[j+1];
	map->index[map->size-1] = IP_SET_INVALID_ID;
	return 0;
}

static int
setlist_udel(struct ip_set *set, const void *data, u_int32_t size)
{
	struct ip_set_setlist *map = set->data;
	const struct ip_set_req_setlist *req = data;
	ip_set_id_t index, ref = IP_SET_INVALID_ID;
	int i, res = -EEXIST;
	struct ip_set *s;
	
	if (req->before && req->ref[0] == '\0')
		return -EINVAL;

	index = __ip_set_get_byname(req->name, &s);
	if (index == IP_SET_INVALID_ID)
		return -EEXIST;
	if (req->ref[0] != '\0') {
		ref = __ip_set_get_byname(req->ref, &s);
		if (ref == IP_SET_INVALID_ID)
			goto finish;
	}
	for (i = 0; i < map->size
	            && map->index[i] != IP_SET_INVALID_ID; i++) {
		if (req->before) {
			if (map->index[i] == index
			    && next_index_eq(map, i + 1, ref)) {
				res = unshift_setlist(map, i);
				break;
			}
		} else if (ref == IP_SET_INVALID_ID) {
			if (map->index[i] == index) {
				res = unshift_setlist(map, i);
				break;
			}
		} else if (map->index[i] == ref
			   && next_index_eq(map, i + 1, index)) {
			res = unshift_setlist(map, i + 1);
			break;
		}
	}
	if (ref != IP_SET_INVALID_ID)
		__ip_set_put_byindex(ref);
finish:
	__ip_set_put_byindex(index);
	/* In case of success, release the reference to the set */
	if (res == 0)
		__ip_set_put_byindex(index);
	return res;
}

static int
setlist_kdel(struct ip_set *set,
	     const struct sk_buff *skb,
	     const u_int32_t *flags)
{
	struct ip_set_setlist *map = set->data;
	int i, res = -EINVAL;
	
	for (i = 0; i < map->size
		    && map->index[i] != IP_SET_INVALID_ID
		    && res != 0; i++)
		res = ip_set_delip_kernel(map->index[i], skb, flags);
	return res;
}

static int
setlist_create(struct ip_set *set, const void *data, u_int32_t size)
{
	struct ip_set_setlist *map;
	const struct ip_set_req_setlist_create *req = data;
	int i;
	
	map = kmalloc(sizeof(struct ip_set_setlist) +
		      req->size * sizeof(ip_set_id_t), GFP_KERNEL);
	if (!map)
		return -ENOMEM;
	map->size = req->size;
	for (i = 0; i < map->size; i++)
		map->index[i] = IP_SET_INVALID_ID;
	
	set->data = map;
	return 0;
}                        

static void
setlist_destroy(struct ip_set *set)
{
	struct ip_set_setlist *map = set->data;
	int i;
	
	for (i = 0; i < map->size
		    && map->index[i] != IP_SET_INVALID_ID; i++)
		__ip_set_put_byindex(map->index[i]);

	kfree(map);
	set->data = NULL;
}

static void
setlist_flush(struct ip_set *set)
{
	struct ip_set_setlist *map = set->data;
	int i;
	
	for (i = 0; i < map->size
		    && map->index[i] != IP_SET_INVALID_ID; i++) {
		__ip_set_put_byindex(map->index[i]);
		map->index[i] = IP_SET_INVALID_ID;
	}
}

static void
setlist_list_header(const struct ip_set *set, void *data)
{
	const struct ip_set_setlist *map = set->data;
	struct ip_set_req_setlist_create *header = data;
	
	header->size = map->size;
}

static int
setlist_list_members_size(const struct ip_set *set, char dont_align)
{
	const struct ip_set_setlist *map = set->data;
	
	return map->size * IPSET_VALIGN(sizeof(ip_set_id_t), dont_align);
}

static void
setlist_list_members(const struct ip_set *set, void *data, char dont_align)
{
	struct ip_set_setlist *map = set->data;
	ip_set_id_t *d;
	int i;
	
	for (i = 0; i < map->size; i++) {
		d = data + i * IPSET_VALIGN(sizeof(ip_set_id_t), dont_align);
		*d = ip_set_id(map->index[i]);
	}
}

IP_SET_TYPE(setlist, IPSET_TYPE_SETNAME | IPSET_DATA_SINGLE)

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Jozsef Kadlecsik <kadlec@blackhole.kfki.hu>");
MODULE_DESCRIPTION("setlist type of IP sets");

REGISTER_MODULE(setlist)
