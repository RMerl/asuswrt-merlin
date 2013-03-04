/* Copyright (C) 2007 Sven Wegener <sven.wegener@stealer.net>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as published by
 * the Free Software Foundation.
 */

/* This modules implements the iptreemap ipset type. It uses bitmaps to
 * represent every single IPv4 address as a bit. The bitmaps are managed in a
 * tree structure, where the first three octets of an address are used as an
 * index to find the bitmap and the last octet is used as the bit number.
 */

#include <linux/kernel.h>
#include <linux/module.h>
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
#include <linux/netfilter_ipv4/ip_set_iptreemap.h>

#define IPTREEMAP_DEFAULT_GC_TIME (5 * 60)
#define IPTREEMAP_DESTROY_SLEEP (100)

static __KMEM_CACHE_T__ *cachep_b;
static __KMEM_CACHE_T__ *cachep_c;
static __KMEM_CACHE_T__ *cachep_d;

static struct ip_set_iptreemap_d *fullbitmap_d;
static struct ip_set_iptreemap_c *fullbitmap_c;
static struct ip_set_iptreemap_b *fullbitmap_b;

#if defined(__LITTLE_ENDIAN)
#define ABCD(a, b, c, d, addr) \
	do { \
		a = ((unsigned char *)addr)[3]; \
		b = ((unsigned char *)addr)[2]; \
		c = ((unsigned char *)addr)[1]; \
		d = ((unsigned char *)addr)[0]; \
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

#define TESTIP_WALK(map, elem, branch, full) \
	do { \
		branch = (map)->tree[elem]; \
		if (!branch) \
			return 0; \
		else if (branch == full) \
			return 1; \
	} while (0)

#define ADDIP_WALK(map, elem, branch, type, cachep, full) \
	do { \
		branch = (map)->tree[elem]; \
		if (!branch) { \
			branch = (type *) kmem_cache_alloc(cachep, GFP_ATOMIC); \
			if (!branch) \
				return -ENOMEM; \
			memset(branch, 0, sizeof(*branch)); \
			(map)->tree[elem] = branch; \
		} else if (branch == full) { \
			return -EEXIST; \
		} \
	} while (0)

#define ADDIP_RANGE_LOOP(map, a, a1, a2, hint, branch, full, cachep, free) \
	for (a = a1; a <= a2; a++) { \
		branch = (map)->tree[a]; \
		if (branch != full) { \
			if ((a > a1 && a < a2) || (hint)) { \
				if (branch) \
					free(branch); \
				(map)->tree[a] = full; \
				continue; \
			} else if (!branch) { \
				branch = kmem_cache_alloc(cachep, GFP_ATOMIC); \
				if (!branch) \
					return -ENOMEM; \
				memset(branch, 0, sizeof(*branch)); \
				(map)->tree[a] = branch; \
			}

#define ADDIP_RANGE_LOOP_END() \
		} \
	}

#define DELIP_WALK(map, elem, branch, cachep, full) \
	do { \
		branch = (map)->tree[elem]; \
		if (!branch) { \
			return -EEXIST; \
		} else if (branch == full) { \
			branch = kmem_cache_alloc(cachep, GFP_ATOMIC); \
			if (!branch) \
				return -ENOMEM; \
			memcpy(branch, full, sizeof(*full)); \
			(map)->tree[elem] = branch; \
		} \
	} while (0)

#define DELIP_RANGE_LOOP(map, a, a1, a2, hint, branch, full, cachep, free) \
	for (a = a1; a <= a2; a++) { \
		branch = (map)->tree[a]; \
		if (branch) { \
			if ((a > a1 && a < a2) || (hint)) { \
				if (branch != full) \
					free(branch); \
				(map)->tree[a] = NULL; \
				continue; \
			} else if (branch == full) { \
				branch = kmem_cache_alloc(cachep, GFP_ATOMIC); \
				if (!branch) \
					return -ENOMEM; \
				memcpy(branch, full, sizeof(*branch)); \
				(map)->tree[a] = branch; \
			}

#define DELIP_RANGE_LOOP_END() \
		} \
	}

#define LOOP_WALK_BEGIN(map, i, branch) \
	for (i = 0; i < 256; i++) { \
		branch = (map)->tree[i]; \
		if (likely(!branch)) \
			continue;

#define LOOP_WALK_END() \
	}

#define LOOP_WALK_BEGIN_GC(map, i, branch, full, cachep, count) \
	count = -256; \
	for (i = 0; i < 256; i++) { \
		branch = (map)->tree[i]; \
		if (likely(!branch)) \
			continue; \
		count++; \
		if (branch == full) { \
			count++; \
			continue; \
		}

#define LOOP_WALK_END_GC(map, i, branch, full, cachep, count) \
		if (-256 == count) { \
			kmem_cache_free(cachep, branch); \
			(map)->tree[i] = NULL; \
		} else if (256 == count) { \
			kmem_cache_free(cachep, branch); \
			(map)->tree[i] = full; \
		} \
	}

#define LOOP_WALK_BEGIN_COUNT(map, i, branch, inrange, count) \
	for (i = 0; i < 256; i++) { \
		if (!(map)->tree[i]) { \
			if (inrange) { \
				count++; \
				inrange = 0; \
			} \
			continue; \
		} \
		branch = (map)->tree[i];

#define LOOP_WALK_END_COUNT() \
	}

#define GETVALUE1(a, a1, b1, r) \
	(a == a1 ? b1 : r)

#define GETVALUE2(a, b, a1, b1, c1, r) \
	(a == a1 && b == b1 ? c1 : r)

#define GETVALUE3(a, b, c, a1, b1, c1, d1, r) \
	(a == a1 && b == b1 && c == c1 ? d1 : r)

#define CHECK1(a, a1, a2, b1, b2, c1, c2, d1, d2) \
	( \
		GETVALUE1(a, a1, b1, 0) == 0 \
		&& GETVALUE1(a, a2, b2, 255) == 255 \
		&& c1 == 0 \
		&& c2 == 255 \
		&& d1 == 0 \
		&& d2 == 255 \
	)

#define CHECK2(a, b, a1, a2, b1, b2, c1, c2, d1, d2) \
	( \
		GETVALUE2(a, b, a1, b1, c1, 0) == 0 \
		&& GETVALUE2(a, b, a2, b2, c2, 255) == 255 \
		&& d1 == 0 \
		&& d2 == 255 \
	)

#define CHECK3(a, b, c, a1, a2, b1, b2, c1, c2, d1, d2) \
	( \
		GETVALUE3(a, b, c, a1, b1, c1, d1, 0) == 0 \
		&& GETVALUE3(a, b, c, a2, b2, c2, d2, 255) == 255 \
	)


static inline void
free_d(struct ip_set_iptreemap_d *map)
{
	kmem_cache_free(cachep_d, map);
}

static inline void
free_c(struct ip_set_iptreemap_c *map)
{
	struct ip_set_iptreemap_d *dtree;
	unsigned int i;

	LOOP_WALK_BEGIN(map, i, dtree) {
		if (dtree != fullbitmap_d)
			free_d(dtree);
	} LOOP_WALK_END();

	kmem_cache_free(cachep_c, map);
}

static inline void
free_b(struct ip_set_iptreemap_b *map)
{
	struct ip_set_iptreemap_c *ctree;
	unsigned int i;

	LOOP_WALK_BEGIN(map, i, ctree) {
		if (ctree != fullbitmap_c)
			free_c(ctree);
	} LOOP_WALK_END();

	kmem_cache_free(cachep_b, map);
}

static inline int
iptreemap_test(struct ip_set *set, ip_set_ip_t ip)
{
	struct ip_set_iptreemap *map = set->data;
	struct ip_set_iptreemap_b *btree;
	struct ip_set_iptreemap_c *ctree;
	struct ip_set_iptreemap_d *dtree;
	unsigned char a, b, c, d;

	ABCD(a, b, c, d, &ip);

	TESTIP_WALK(map, a, btree, fullbitmap_b);
	TESTIP_WALK(btree, b, ctree, fullbitmap_c);
	TESTIP_WALK(ctree, c, dtree, fullbitmap_d);

	return !!test_bit(d, (void *) dtree->bitmap);
}

#define KADT_CONDITION

UADT(iptreemap, test)
KADT(iptreemap, test, ipaddr)

static inline int
__addip_single(struct ip_set *set, ip_set_ip_t ip)
{
	struct ip_set_iptreemap *map = (struct ip_set_iptreemap *) set->data;
	struct ip_set_iptreemap_b *btree;
	struct ip_set_iptreemap_c *ctree;
	struct ip_set_iptreemap_d *dtree;
	unsigned char a, b, c, d;

	ABCD(a, b, c, d, &ip);

	ADDIP_WALK(map, a, btree, struct ip_set_iptreemap_b, cachep_b, fullbitmap_b);
	ADDIP_WALK(btree, b, ctree, struct ip_set_iptreemap_c, cachep_c, fullbitmap_c);
	ADDIP_WALK(ctree, c, dtree, struct ip_set_iptreemap_d, cachep_d, fullbitmap_d);

	if (__test_and_set_bit(d, (void *) dtree->bitmap))
		return -EEXIST;

	__set_bit(b, (void *) btree->dirty);

	return 0;
}

static inline int
iptreemap_add(struct ip_set *set, ip_set_ip_t start, ip_set_ip_t end)
{
	struct ip_set_iptreemap *map = set->data;
	struct ip_set_iptreemap_b *btree;
	struct ip_set_iptreemap_c *ctree;
	struct ip_set_iptreemap_d *dtree;
	unsigned int a, b, c, d;
	unsigned char a1, b1, c1, d1;
	unsigned char a2, b2, c2, d2;

	if (start == end)
		return __addip_single(set, start);

	ABCD(a1, b1, c1, d1, &start);
	ABCD(a2, b2, c2, d2, &end);

	/* This is sooo ugly... */
	ADDIP_RANGE_LOOP(map, a, a1, a2, CHECK1(a, a1, a2, b1, b2, c1, c2, d1, d2), btree, fullbitmap_b, cachep_b, free_b) {
		ADDIP_RANGE_LOOP(btree, b, GETVALUE1(a, a1, b1, 0), GETVALUE1(a, a2, b2, 255), CHECK2(a, b, a1, a2, b1, b2, c1, c2, d1, d2), ctree, fullbitmap_c, cachep_c, free_c) {
			ADDIP_RANGE_LOOP(ctree, c, GETVALUE2(a, b, a1, b1, c1, 0), GETVALUE2(a, b, a2, b2, c2, 255), CHECK3(a, b, c, a1, a2, b1, b2, c1, c2, d1, d2), dtree, fullbitmap_d, cachep_d, free_d) {
				for (d = GETVALUE3(a, b, c, a1, b1, c1, d1, 0); d <= GETVALUE3(a, b, c, a2, b2, c2, d2, 255); d++)
					__set_bit(d, (void *) dtree->bitmap);
				__set_bit(b, (void *) btree->dirty);
			} ADDIP_RANGE_LOOP_END();
		} ADDIP_RANGE_LOOP_END();
	} ADDIP_RANGE_LOOP_END();

	return 0;
}

UADT0(iptreemap, add, min(req->ip, req->end), max(req->ip, req->end))
KADT(iptreemap, add, ipaddr, ip)

static inline int
__delip_single(struct ip_set *set, ip_set_ip_t ip)
{
	struct ip_set_iptreemap *map = set->data;
	struct ip_set_iptreemap_b *btree;
	struct ip_set_iptreemap_c *ctree;
	struct ip_set_iptreemap_d *dtree;
	unsigned char a,b,c,d;

	ABCD(a, b, c, d, &ip);

	DELIP_WALK(map, a, btree, cachep_b, fullbitmap_b);
	DELIP_WALK(btree, b, ctree, cachep_c, fullbitmap_c);
	DELIP_WALK(ctree, c, dtree, cachep_d, fullbitmap_d);

	if (!__test_and_clear_bit(d, (void *) dtree->bitmap))
		return -EEXIST;

	__set_bit(b, (void *) btree->dirty);

	return 0;
}

static inline int
iptreemap_del(struct ip_set *set, ip_set_ip_t start, ip_set_ip_t end)
{
	struct ip_set_iptreemap *map = set->data;
	struct ip_set_iptreemap_b *btree;
	struct ip_set_iptreemap_c *ctree;
	struct ip_set_iptreemap_d *dtree;
	unsigned int a, b, c, d;
	unsigned char a1, b1, c1, d1;
	unsigned char a2, b2, c2, d2;

	if (start == end)
		return __delip_single(set, start);

	ABCD(a1, b1, c1, d1, &start);
	ABCD(a2, b2, c2, d2, &end);

	/* This is sooo ugly... */
	DELIP_RANGE_LOOP(map, a, a1, a2, CHECK1(a, a1, a2, b1, b2, c1, c2, d1, d2), btree, fullbitmap_b, cachep_b, free_b) {
		DELIP_RANGE_LOOP(btree, b, GETVALUE1(a, a1, b1, 0), GETVALUE1(a, a2, b2, 255), CHECK2(a, b, a1, a2, b1, b2, c1, c2, d1, d2), ctree, fullbitmap_c, cachep_c, free_c) {
			DELIP_RANGE_LOOP(ctree, c, GETVALUE2(a, b, a1, b1, c1, 0), GETVALUE2(a, b, a2, b2, c2, 255), CHECK3(a, b, c, a1, a2, b1, b2, c1, c2, d1, d2), dtree, fullbitmap_d, cachep_d, free_d) {
				for (d = GETVALUE3(a, b, c, a1, b1, c1, d1, 0); d <= GETVALUE3(a, b, c, a2, b2, c2, d2, 255); d++)
					__clear_bit(d, (void *) dtree->bitmap);
				__set_bit(b, (void *) btree->dirty);
			} DELIP_RANGE_LOOP_END();
		} DELIP_RANGE_LOOP_END();
	} DELIP_RANGE_LOOP_END();

	return 0;
}

UADT0(iptreemap, del, min(req->ip, req->end), max(req->ip, req->end))
KADT(iptreemap, del, ipaddr, ip)

/* Check the status of the bitmap
 * -1 == all bits cleared
 *  1 == all bits set
 *  0 == anything else
 */
static inline int
bitmap_status(struct ip_set_iptreemap_d *dtree)
{
	unsigned char first = dtree->bitmap[0];
	int a;

	for (a = 1; a < 32; a++)
		if (dtree->bitmap[a] != first)
			return 0;

	return (first == 0 ? -1 : (first == 255 ? 1 : 0));
}

static void
gc(unsigned long addr)
{
	struct ip_set *set = (struct ip_set *) addr;
	struct ip_set_iptreemap *map = set->data;
	struct ip_set_iptreemap_b *btree;
	struct ip_set_iptreemap_c *ctree;
	struct ip_set_iptreemap_d *dtree;
	unsigned int a, b, c;
	int i, j, k;

	write_lock_bh(&set->lock);

	LOOP_WALK_BEGIN_GC(map, a, btree, fullbitmap_b, cachep_b, i) {
		LOOP_WALK_BEGIN_GC(btree, b, ctree, fullbitmap_c, cachep_c, j) {
			if (!__test_and_clear_bit(b, (void *) btree->dirty))
				continue;
			LOOP_WALK_BEGIN_GC(ctree, c, dtree, fullbitmap_d, cachep_d, k) {
				switch (bitmap_status(dtree)) {
					case -1:
						kmem_cache_free(cachep_d, dtree);
						ctree->tree[c] = NULL;
						k--;
					break;
					case 1:
						kmem_cache_free(cachep_d, dtree);
						ctree->tree[c] = fullbitmap_d;
						k++;
					break;
				}
			} LOOP_WALK_END();
		} LOOP_WALK_END_GC(btree, b, ctree, fullbitmap_c, cachep_c, k);
	} LOOP_WALK_END_GC(map, a, btree, fullbitmap_b, cachep_b, j);

	write_unlock_bh(&set->lock);

	map->gc.expires = jiffies + map->gc_interval * HZ;
	add_timer(&map->gc);
}

static inline void
init_gc_timer(struct ip_set *set)
{
	struct ip_set_iptreemap *map = set->data;

	init_timer(&map->gc);
	map->gc.data = (unsigned long) set;
	map->gc.function = gc;
	map->gc.expires = jiffies + map->gc_interval * HZ;
	add_timer(&map->gc);
}

static int
iptreemap_create(struct ip_set *set, const void *data, u_int32_t size)
{
	const struct ip_set_req_iptreemap_create *req = data;
	struct ip_set_iptreemap *map;

	map = kzalloc(sizeof(*map), GFP_KERNEL);
	if (!map)
		return -ENOMEM;

	map->gc_interval = req->gc_interval ? req->gc_interval : IPTREEMAP_DEFAULT_GC_TIME;
	set->data = map;

	init_gc_timer(set);

	return 0;
}

static inline void
__flush(struct ip_set_iptreemap *map)
{
	struct ip_set_iptreemap_b *btree;
	unsigned int a;

	LOOP_WALK_BEGIN(map, a, btree);
		if (btree != fullbitmap_b)
			free_b(btree);
	LOOP_WALK_END();
}

static void
iptreemap_destroy(struct ip_set *set)
{
	struct ip_set_iptreemap *map = set->data;

	while (!del_timer(&map->gc))
		msleep(IPTREEMAP_DESTROY_SLEEP);

	__flush(map);
	kfree(map);

	set->data = NULL;
}

static void
iptreemap_flush(struct ip_set *set)
{
	struct ip_set_iptreemap *map = set->data;
	unsigned int gc_interval = map->gc_interval;

	while (!del_timer(&map->gc))
		msleep(IPTREEMAP_DESTROY_SLEEP);

	__flush(map);

	memset(map, 0, sizeof(*map));
	map->gc_interval = gc_interval;

	init_gc_timer(set);
}

static void
iptreemap_list_header(const struct ip_set *set, void *data)
{
	struct ip_set_iptreemap *map = set->data;
	struct ip_set_req_iptreemap_create *header = data;

	header->gc_interval = map->gc_interval;
}

static int
iptreemap_list_members_size(const struct ip_set *set, char dont_align)
{
	struct ip_set_iptreemap *map = set->data;
	struct ip_set_iptreemap_b *btree;
	struct ip_set_iptreemap_c *ctree;
	struct ip_set_iptreemap_d *dtree;
	unsigned int a, b, c, d, inrange = 0, count = 0;

	LOOP_WALK_BEGIN_COUNT(map, a, btree, inrange, count) {
		LOOP_WALK_BEGIN_COUNT(btree, b, ctree, inrange, count) {
			LOOP_WALK_BEGIN_COUNT(ctree, c, dtree, inrange, count) {
				for (d = 0; d < 256; d++) {
					if (test_bit(d, (void *) dtree->bitmap)) {
						inrange = 1;
					} else if (inrange) {
						count++;
						inrange = 0;
					}
				}
			} LOOP_WALK_END_COUNT();
		} LOOP_WALK_END_COUNT();
	} LOOP_WALK_END_COUNT();

	if (inrange)
		count++;

	return (count * IPSET_VALIGN(sizeof(struct ip_set_req_iptreemap), dont_align));
}

static inline void
add_member(void *data, size_t offset, ip_set_ip_t start, ip_set_ip_t end)
{
	struct ip_set_req_iptreemap *entry = data + offset;

	entry->ip = start;
	entry->end = end;
}

static void
iptreemap_list_members(const struct ip_set *set, void *data, char dont_align)
{
	struct ip_set_iptreemap *map = set->data;
	struct ip_set_iptreemap_b *btree;
	struct ip_set_iptreemap_c *ctree;
	struct ip_set_iptreemap_d *dtree;
	unsigned int a, b, c, d, inrange = 0;
	size_t offset = 0, datasize;
	ip_set_ip_t start = 0, end = 0, ip;

	datasize = IPSET_VALIGN(sizeof(struct ip_set_req_iptreemap), dont_align);
	LOOP_WALK_BEGIN(map, a, btree) {
		LOOP_WALK_BEGIN(btree, b, ctree) {
			LOOP_WALK_BEGIN(ctree, c, dtree) {
				for (d = 0; d < 256; d++) {
					if (test_bit(d, (void *) dtree->bitmap)) {
						ip = ((a << 24) | (b << 16) | (c << 8) | d);
						if (!inrange) {
							inrange = 1;
							start = ip;
						} else if (end < ip - 1) {
							add_member(data, offset, start, end);
							offset += datasize;
							start = ip;
						}
						end = ip;
					} else if (inrange) {
						add_member(data, offset, start, end);
						offset += datasize;
						inrange = 0;
					}
				}
			} LOOP_WALK_END();
		} LOOP_WALK_END();
	} LOOP_WALK_END();

	if (inrange)
		add_member(data, offset, start, end);
}

IP_SET_TYPE(iptreemap, IPSET_TYPE_IP | IPSET_DATA_SINGLE)

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Sven Wegener <sven.wegener@stealer.net>");
MODULE_DESCRIPTION("iptreemap type of IP sets");

static int __init ip_set_iptreemap_init(void)
{
	int ret = -ENOMEM;
	int a;

	cachep_b = KMEM_CACHE_CREATE("ip_set_iptreemap_b",
				     sizeof(struct ip_set_iptreemap_b));
	if (!cachep_b) {
		ip_set_printk("Unable to create ip_set_iptreemap_b slab cache");
		goto out;
	}

	cachep_c = KMEM_CACHE_CREATE("ip_set_iptreemap_c",
				     sizeof(struct ip_set_iptreemap_c));
	if (!cachep_c) {
		ip_set_printk("Unable to create ip_set_iptreemap_c slab cache");
		goto outb;
	}

	cachep_d = KMEM_CACHE_CREATE("ip_set_iptreemap_d",
				     sizeof(struct ip_set_iptreemap_d));
	if (!cachep_d) {
		ip_set_printk("Unable to create ip_set_iptreemap_d slab cache");
		goto outc;
	}

	fullbitmap_d = kmem_cache_alloc(cachep_d, GFP_KERNEL);
	if (!fullbitmap_d)
		goto outd;

	fullbitmap_c = kmem_cache_alloc(cachep_c, GFP_KERNEL);
	if (!fullbitmap_c)
		goto outbitmapd;

	fullbitmap_b = kmem_cache_alloc(cachep_b, GFP_KERNEL);
	if (!fullbitmap_b)
		goto outbitmapc;

	ret = ip_set_register_set_type(&ip_set_iptreemap);
	if (0 > ret)
		goto outbitmapb;

	/* Now init our global bitmaps */
	memset(fullbitmap_d->bitmap, 0xff, sizeof(fullbitmap_d->bitmap));

	for (a = 0; a < 256; a++)
		fullbitmap_c->tree[a] = fullbitmap_d;

	for (a = 0; a < 256; a++)
		fullbitmap_b->tree[a] = fullbitmap_c;
	memset(fullbitmap_b->dirty, 0, sizeof(fullbitmap_b->dirty));

	return 0;

outbitmapb:
	kmem_cache_free(cachep_b, fullbitmap_b);
outbitmapc:
	kmem_cache_free(cachep_c, fullbitmap_c);
outbitmapd:
	kmem_cache_free(cachep_d, fullbitmap_d);
outd:
	kmem_cache_destroy(cachep_d);
outc:
	kmem_cache_destroy(cachep_c);
outb:
	kmem_cache_destroy(cachep_b);
out:

	return ret;
}

static void __exit ip_set_iptreemap_fini(void)
{
	ip_set_unregister_set_type(&ip_set_iptreemap);
	kmem_cache_free(cachep_d, fullbitmap_d);
	kmem_cache_free(cachep_c, fullbitmap_c);
	kmem_cache_free(cachep_b, fullbitmap_b);
	kmem_cache_destroy(cachep_d);
	kmem_cache_destroy(cachep_c);
	kmem_cache_destroy(cachep_b);
}

module_init(ip_set_iptreemap_init);
module_exit(ip_set_iptreemap_fini);
