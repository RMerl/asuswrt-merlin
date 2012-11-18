/* Copyright (C) 2005-2008 Jozsef Kadlecsik <kadlec@blackhole.kfki.hu>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

/* Kernel module implementing an IP set type: the iptree type */

#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/ip.h>
#include <linux/jiffies.h>
#include <linux/skbuff.h>
#include <linux/slab.h>
#include <linux/delay.h>
#include <linux/errno.h>
#include <asm/uaccess.h>
#include <asm/bitops.h>
#include <linux/spinlock.h>
#include <linux/timer.h>

#include <linux/netfilter_ipv4/ip_set.h>
#include <linux/netfilter_ipv4/ip_set_bitmaps.h>
#include <linux/netfilter_ipv4/ip_set_iptree.h>

static int limit = MAX_RANGE;

/* Garbage collection interval in seconds: */
#define IPTREE_GC_TIME		5*60
/* Sleep so many milliseconds before trying again
 * to delete the gc timer at destroying/flushing a set */
#define IPTREE_DESTROY_SLEEP	100

static __KMEM_CACHE_T__ *branch_cachep;
static __KMEM_CACHE_T__ *leaf_cachep;


#if defined(__LITTLE_ENDIAN)
#define ABCD(a,b,c,d,addrp) do {		\
	a = ((unsigned char *)addrp)[3];	\
	b = ((unsigned char *)addrp)[2];	\
	c = ((unsigned char *)addrp)[1];	\
	d = ((unsigned char *)addrp)[0];	\
} while (0)
#elif defined(__BIG_ENDIAN)
#define ABCD(a,b,c,d,addrp) do {		\
	a = ((unsigned char *)addrp)[0];	\
	b = ((unsigned char *)addrp)[1];	\
	c = ((unsigned char *)addrp)[2];	\
	d = ((unsigned char *)addrp)[3];	\
} while (0)
#else
#error "Please fix asm/byteorder.h"
#endif /* __LITTLE_ENDIAN */

#define TESTIP_WALK(map, elem, branch) do {	\
	if ((map)->tree[elem]) {		\
		branch = (map)->tree[elem];	\
	} else 					\
		return 0;			\
} while (0)

static inline int
iptree_test(struct ip_set *set, ip_set_ip_t ip)
{
	struct ip_set_iptree *map = set->data;
	struct ip_set_iptreeb *btree;
	struct ip_set_iptreec *ctree;
	struct ip_set_iptreed *dtree;
	unsigned char a,b,c,d;

	if (!ip)
		return -ERANGE;
	
	ABCD(a, b, c, d, &ip);
	DP("%u %u %u %u timeout %u", a, b, c, d, map->timeout);
	TESTIP_WALK(map, a, btree);
	TESTIP_WALK(btree, b, ctree);
	TESTIP_WALK(ctree, c, dtree);
	DP("%lu %lu", dtree->expires[d], jiffies);
	return dtree->expires[d]
	       && (!map->timeout
		   || time_after(dtree->expires[d], jiffies));
}

#define KADT_CONDITION

UADT(iptree, test)
KADT(iptree, test, ipaddr)

#define ADDIP_WALK(map, elem, branch, type, cachep) do {	\
	if ((map)->tree[elem]) {				\
		DP("found %u", elem);				\
		branch = (map)->tree[elem];			\
	} else {						\
		branch = (type *)				\
			kmem_cache_alloc(cachep, GFP_ATOMIC);	\
		if (branch == NULL)				\
			return -ENOMEM;				\
		memset(branch, 0, sizeof(*branch));		\
		(map)->tree[elem] = branch;			\
		DP("alloc %u", elem);				\
	}							\
} while (0)	

static inline int
iptree_add(struct ip_set *set, ip_set_ip_t ip, unsigned int timeout)
{
	struct ip_set_iptree *map = set->data;
	struct ip_set_iptreeb *btree;
	struct ip_set_iptreec *ctree;
	struct ip_set_iptreed *dtree;
	unsigned char a,b,c,d;
	int ret = 0;
	
	if (!ip || map->elements >= limit)
		/* We could call the garbage collector
		 * but it's probably overkill */
		return -ERANGE;
	
	ABCD(a, b, c, d, &ip);
	DP("%u %u %u %u timeout %u", a, b, c, d, timeout);
	ADDIP_WALK(map, a, btree, struct ip_set_iptreeb, branch_cachep);
	ADDIP_WALK(btree, b, ctree, struct ip_set_iptreec, branch_cachep);
	ADDIP_WALK(ctree, c, dtree, struct ip_set_iptreed, leaf_cachep);
	if (dtree->expires[d]
	    && (!map->timeout || time_after(dtree->expires[d], jiffies)))
	    	ret = -EEXIST;
	if (map->timeout && timeout == 0)
		timeout = map->timeout;
	dtree->expires[d] = map->timeout ? (timeout * HZ + jiffies) : 1;
	/* Lottery: I won! */
	if (dtree->expires[d] == 0)
		dtree->expires[d] = 1;
	DP("%u %lu", d, dtree->expires[d]);
	if (ret == 0)
		map->elements++;
	return ret;
}

UADT(iptree, add, req->timeout)
KADT(iptree, add, ipaddr, 0)

#define DELIP_WALK(map, elem, branch) do {	\
	if ((map)->tree[elem]) {		\
		branch = (map)->tree[elem];	\
	} else 					\
		return -EEXIST;			\
} while (0)

static inline int
iptree_del(struct ip_set *set, ip_set_ip_t ip)
{
	struct ip_set_iptree *map = set->data;
	struct ip_set_iptreeb *btree;
	struct ip_set_iptreec *ctree;
	struct ip_set_iptreed *dtree;
	unsigned char a,b,c,d;
	
	if (!ip)
		return -ERANGE;
		
	ABCD(a, b, c, d, &ip);
	DELIP_WALK(map, a, btree);
	DELIP_WALK(btree, b, ctree);
	DELIP_WALK(ctree, c, dtree);

	if (dtree->expires[d]) {
		dtree->expires[d] = 0;
		map->elements--;
		return 0;
	}
	return -EEXIST;
}

UADT(iptree, del)
KADT(iptree, del, ipaddr)

#define LOOP_WALK_BEGIN(map, i, branch) \
	for (i = 0; i < 256; i++) {	\
		if (!(map)->tree[i])	\
			continue;	\
		branch = (map)->tree[i]

#define LOOP_WALK_END }

static void
ip_tree_gc(unsigned long ul_set)
{
	struct ip_set *set = (struct ip_set *) ul_set;
	struct ip_set_iptree *map = set->data;
	struct ip_set_iptreeb *btree;
	struct ip_set_iptreec *ctree;
	struct ip_set_iptreed *dtree;
	unsigned int a,b,c,d;
	unsigned char i,j,k;

	i = j = k = 0;
	DP("gc: %s", set->name);
	write_lock_bh(&set->lock);
	LOOP_WALK_BEGIN(map, a, btree);
	LOOP_WALK_BEGIN(btree, b, ctree);
	LOOP_WALK_BEGIN(ctree, c, dtree);
	for (d = 0; d < 256; d++) {
		if (dtree->expires[d]) {
			DP("gc: %u %u %u %u: expires %lu jiffies %lu",
			    a, b, c, d,
			    dtree->expires[d], jiffies);
			if (map->timeout
			    && time_before(dtree->expires[d], jiffies)) {
			    	dtree->expires[d] = 0;
			    	map->elements--;
			} else
				k = 1;
		}
	}
	if (k == 0) {
		DP("gc: %s: leaf %u %u %u empty",
		    set->name, a, b, c);
		kmem_cache_free(leaf_cachep, dtree);
		ctree->tree[c] = NULL;
	} else {
		DP("gc: %s: leaf %u %u %u not empty",
		    set->name, a, b, c);
		j = 1;
		k = 0;
	}
	LOOP_WALK_END;
	if (j == 0) {
		DP("gc: %s: branch %u %u empty",
		    set->name, a, b);
		kmem_cache_free(branch_cachep, ctree);
		btree->tree[b] = NULL;
	} else {
		DP("gc: %s: branch %u %u not empty",
		    set->name, a, b);
		i = 1;
		j = k = 0;
	}
	LOOP_WALK_END;
	if (i == 0) {
		DP("gc: %s: branch %u empty",
		    set->name, a);
		kmem_cache_free(branch_cachep, btree);
		map->tree[a] = NULL;
	} else {
		DP("gc: %s: branch %u not empty",
		    set->name, a);
		i = j = k = 0;
	}
	LOOP_WALK_END;
	write_unlock_bh(&set->lock);
	
	map->gc.expires = jiffies + map->gc_interval * HZ;
	add_timer(&map->gc);
}

static inline void
init_gc_timer(struct ip_set *set)
{
	struct ip_set_iptree *map = set->data;

	/* Even if there is no timeout for the entries,
	 * we still have to call gc because delete
	 * do not clean up empty branches */
	map->gc_interval = IPTREE_GC_TIME;
	init_timer(&map->gc);
	map->gc.data = (unsigned long) set;
	map->gc.function = ip_tree_gc;
	map->gc.expires = jiffies + map->gc_interval * HZ;
	add_timer(&map->gc);
}

static int
iptree_create(struct ip_set *set, const void *data, u_int32_t size)
{
	const struct ip_set_req_iptree_create *req = data;
	struct ip_set_iptree *map;

	if (size != sizeof(struct ip_set_req_iptree_create)) {
		ip_set_printk("data length wrong (want %zu, have %lu)",
			      sizeof(struct ip_set_req_iptree_create),
			      (unsigned long)size);
		return -EINVAL;
	}

	map = kmalloc(sizeof(struct ip_set_iptree), GFP_KERNEL);
	if (!map) {
		DP("out of memory for %zu bytes",
		   sizeof(struct ip_set_iptree));
		return -ENOMEM;
	}
	memset(map, 0, sizeof(*map));
	map->timeout = req->timeout;
	map->elements = 0;
	set->data = map;

	init_gc_timer(set);

	return 0;
}

static inline void
__flush(struct ip_set_iptree *map)
{
	struct ip_set_iptreeb *btree;
	struct ip_set_iptreec *ctree;
	struct ip_set_iptreed *dtree;
	unsigned int a,b,c;

	LOOP_WALK_BEGIN(map, a, btree);
	LOOP_WALK_BEGIN(btree, b, ctree);
	LOOP_WALK_BEGIN(ctree, c, dtree);
	kmem_cache_free(leaf_cachep, dtree);
	LOOP_WALK_END;
	kmem_cache_free(branch_cachep, ctree);
	LOOP_WALK_END;
	kmem_cache_free(branch_cachep, btree);
	LOOP_WALK_END;
	map->elements = 0;
}

static void
iptree_destroy(struct ip_set *set)
{
	struct ip_set_iptree *map = set->data;

	/* gc might be running */
	while (!del_timer(&map->gc))
		msleep(IPTREE_DESTROY_SLEEP);
	__flush(map);
	kfree(map);
	set->data = NULL;
}

static void
iptree_flush(struct ip_set *set)
{
	struct ip_set_iptree *map = set->data;
	unsigned int timeout = map->timeout;
	
	/* gc might be running */
	while (!del_timer(&map->gc))
		msleep(IPTREE_DESTROY_SLEEP);
	__flush(map);
	memset(map, 0, sizeof(*map));
	map->timeout = timeout;

	init_gc_timer(set);
}

static void
iptree_list_header(const struct ip_set *set, void *data)
{
	const struct ip_set_iptree *map = set->data;
	struct ip_set_req_iptree_create *header = data;

	header->timeout = map->timeout;
}

static int
iptree_list_members_size(const struct ip_set *set, char dont_align)
{
	const struct ip_set_iptree *map = set->data;
	struct ip_set_iptreeb *btree;
	struct ip_set_iptreec *ctree;
	struct ip_set_iptreed *dtree;
	unsigned int a,b,c,d;
	unsigned int count = 0;

	LOOP_WALK_BEGIN(map, a, btree);
	LOOP_WALK_BEGIN(btree, b, ctree);
	LOOP_WALK_BEGIN(ctree, c, dtree);
	for (d = 0; d < 256; d++) {
		if (dtree->expires[d]
		    && (!map->timeout || time_after(dtree->expires[d], jiffies)))
		    	count++;
	}
	LOOP_WALK_END;
	LOOP_WALK_END;
	LOOP_WALK_END;

	DP("members %u", count);
	return (count * IPSET_VALIGN(sizeof(struct ip_set_req_iptree), dont_align));
}

static void
iptree_list_members(const struct ip_set *set, void *data, char dont_align)
{
	const struct ip_set_iptree *map = set->data;
	struct ip_set_iptreeb *btree;
	struct ip_set_iptreec *ctree;
	struct ip_set_iptreed *dtree;
	unsigned int a,b,c,d;
	size_t offset = 0, datasize;
	struct ip_set_req_iptree *entry;

	datasize = IPSET_VALIGN(sizeof(struct ip_set_req_iptree), dont_align);
	LOOP_WALK_BEGIN(map, a, btree);
	LOOP_WALK_BEGIN(btree, b, ctree);
	LOOP_WALK_BEGIN(ctree, c, dtree);
	for (d = 0; d < 256; d++) {
		if (dtree->expires[d]
		    && (!map->timeout || time_after(dtree->expires[d], jiffies))) {
		    	entry = data + offset;
		    	entry->ip = ((a << 24) | (b << 16) | (c << 8) | d);
		    	entry->timeout = !map->timeout ? 0
				: (dtree->expires[d] - jiffies)/HZ;
			offset += datasize;
		}
	}
	LOOP_WALK_END;
	LOOP_WALK_END;
	LOOP_WALK_END;
}

IP_SET_TYPE(iptree, IPSET_TYPE_IP | IPSET_DATA_SINGLE)

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Jozsef Kadlecsik <kadlec@blackhole.kfki.hu>");
MODULE_DESCRIPTION("iptree type of IP sets");
module_param(limit, int, 0600);
MODULE_PARM_DESC(limit, "maximal number of elements stored in the sets");

static int __init ip_set_iptree_init(void)
{
	int ret;
	
	branch_cachep = KMEM_CACHE_CREATE("ip_set_iptreeb",
					  sizeof(struct ip_set_iptreeb));
	if (!branch_cachep) {
		printk(KERN_ERR "Unable to create ip_set_iptreeb slab cache\n");
		ret = -ENOMEM;
		goto out;
	}
	leaf_cachep = KMEM_CACHE_CREATE("ip_set_iptreed",
					sizeof(struct ip_set_iptreed));
	if (!leaf_cachep) {
		printk(KERN_ERR "Unable to create ip_set_iptreed slab cache\n");
		ret = -ENOMEM;
		goto free_branch;
	}
	ret = ip_set_register_set_type(&ip_set_iptree);
	if (ret == 0)
		goto out;

	kmem_cache_destroy(leaf_cachep);
    free_branch:	
	kmem_cache_destroy(branch_cachep);
    out:
	return ret;
}

static void __exit ip_set_iptree_fini(void)
{
	/* FIXME: possible race with ip_set_create() */
	ip_set_unregister_set_type(&ip_set_iptree);
	kmem_cache_destroy(leaf_cachep);
	kmem_cache_destroy(branch_cachep);
}

module_init(ip_set_iptree_init);
module_exit(ip_set_iptree_fini);
