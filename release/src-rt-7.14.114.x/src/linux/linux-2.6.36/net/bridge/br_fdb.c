/* Modified by Broadcom Corp. Portions Copyright (c) Broadcom Corp, 2012. */
/*
 *	Forwarding database
 *	Linux ethernet bridge
 *
 *	Authors:
 *	Lennert Buytenhek		<buytenh@gnu.org>
 *
 *	This program is free software; you can redistribute it and/or
 *	modify it under the terms of the GNU General Public License
 *	as published by the Free Software Foundation; either version
 *	2 of the License, or (at your option) any later version.
 */

#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/rculist.h>
#include <linux/spinlock.h>
#include <linux/times.h>
#include <linux/netdevice.h>
#include <linux/etherdevice.h>
#include <linux/jhash.h>
#include <linux/random.h>
#include <linux/slab.h>
#include <asm/atomic.h>
#include <asm/unaligned.h>
#include "br_private.h"
#ifdef HNDCTF
#include <linux/if.h>
#include <linux/if_vlan.h>
#include <linux/if_ether.h>
#include <net/arp.h>
#include <typedefs.h>
#include <osl.h>
#include <ctf/hndctf.h>
#else
#define BCMFASTPATH_HOST
#endif	/* HNDCTF */

#ifdef HNDCTF
static void
br_brc_init(ctf_brc_t *brc, unsigned char *ea, struct net_device *rxdev, unsigned char *sip)
{
	memset(brc, 0, sizeof(ctf_brc_t));

	memcpy(brc->dhost.octet, ea, ETH_ALEN);

        if (rxdev->priv_flags & IFF_802_1Q_VLAN) {
		brc->txifp = (void *)vlan_dev_real_dev(rxdev);
		brc->vid = vlan_dev_vlan_id(rxdev);
		brc->action = ((vlan_dev_vlan_flags(rxdev) & 1) ?
		                     CTF_ACTION_TAG : CTF_ACTION_UNTAG);
	} else {
		brc->txifp = (void *)rxdev;
		brc->action = CTF_ACTION_UNTAG;
	}

	if (sip)
		memcpy(&brc->ip, sip, IPV4_ADDR_LEN);

#ifdef DEBUG
	printk("mac %02x:%02x:%02x:%02x:%02x:%02x\n",
	       brc->dhost.octet[0], brc->dhost.octet[1],
	       brc->dhost.octet[2], brc->dhost.octet[3],
	       brc->dhost.octet[4], brc->dhost.octet[5]);
	printk("vid: %d action %x\n", brc->vid, brc->action);
	printk("txif: %s\n", ((struct net_device *)brc->txifp)->name);
#endif

	return;
}

/*
 * Add bridge cache entry.
 */
void
br_brc_add(unsigned char *ea, struct net_device *rxdev, struct sk_buff *skb)
{
	ctf_brc_t brc_entry;
	struct ethhdr *eth;
	struct arphdr *arp;
	unsigned char *arp_ptr = NULL;

	/* Add brc entry only if packet is received on ctf 
	 * enabled interface
	 */
	if (!ctf_isenabled(kcih, ((rxdev->priv_flags & IFF_802_1Q_VLAN) ?
	                   vlan_dev_real_dev(rxdev) : rxdev)))
		return;

	if (skb) {
		/* Only handle ARP Reply */
		eth = (struct ethhdr *)skb_mac_header(skb);
		if (eth->h_proto == htons(ETHER_TYPE_ARP)) {
			arp = (struct arphdr *)skb_network_header(skb);
			if (arp->ar_hrd == htons(ARPHRD_ETHER) &&
			    arp->ar_pro == htons(ETH_P_IP) &&
			    arp->ar_op == htons(ARPOP_REPLY)) {
			    /* Extract source ip fields */
				arp_ptr = (unsigned char *)(arp+1);
				arp_ptr += skb->dev->addr_len;
			}
		}
	}

	br_brc_init(&brc_entry, ea, rxdev, arp_ptr);

#ifdef DEBUG
	printk("%s: Adding brc entry\n", __FUNCTION__);
#endif

	/* Add the bridge cache entry */
	ctf_brc_add(kcih, &brc_entry);

	return;
}

#endif /* HNDCTF */

static struct kmem_cache *br_fdb_cache __read_mostly;
static int fdb_insert(struct net_bridge *br, struct net_bridge_port *source,
		      const unsigned char *addr);

static u32 fdb_salt __read_mostly;

int __init br_fdb_init(void)
{
	br_fdb_cache = kmem_cache_create("bridge_fdb_cache",
					 sizeof(struct net_bridge_fdb_entry),
					 0,
					 SLAB_HWCACHE_ALIGN, NULL);
	if (!br_fdb_cache)
		return -ENOMEM;

	get_random_bytes(&fdb_salt, sizeof(fdb_salt));
	return 0;
}

void br_fdb_fini(void)
{
	kmem_cache_destroy(br_fdb_cache);
}


/* if topology_changing then use forward_delay (default 15 sec)
 * otherwise keep longer (default 5 minutes)
 */
static inline unsigned long hold_time(const struct net_bridge *br)
{
	return br->topology_change ? br->forward_delay : br->ageing_time;
}

static inline int has_expired(const struct net_bridge *br,
				  const struct net_bridge_fdb_entry *fdb)
{
	return !fdb->is_static &&
		time_before_eq(fdb->ageing_timer + hold_time(br), jiffies);
}

static inline int br_mac_hash(const unsigned char *mac)
{
	/* use 1 byte of OUI cnd 3 bytes of NIC */
	u32 key = get_unaligned((u32 *)(mac + 2));
	return jhash_1word(key, fdb_salt) & (BR_HASH_SIZE - 1);
}

static void fdb_rcu_free(struct rcu_head *head)
{
	struct net_bridge_fdb_entry *ent
		= container_of(head, struct net_bridge_fdb_entry, rcu);
	kmem_cache_free(br_fdb_cache, ent);
}

static inline void fdb_delete(struct net_bridge_fdb_entry *f)
{
	hlist_del_rcu(&f->hlist);

#ifdef HNDCTF
	/* Delete the corresponding brc entry when it expires
	 * or deleted by user.
	 */
	ctf_brc_delete(kcih, f->addr.addr);
#endif /* HNDCTF */

	call_rcu(&f->rcu, fdb_rcu_free);
}

void br_fdb_changeaddr(struct net_bridge_port *p, const unsigned char *newaddr)
{
	struct net_bridge *br = p->br;
	int i;

	spin_lock_bh(&br->hash_lock);

	/* Search all chains since old address/hash is unknown */
	for (i = 0; i < BR_HASH_SIZE; i++) {
		struct hlist_node *h;
		hlist_for_each(h, &br->hash[i]) {
			struct net_bridge_fdb_entry *f;

			f = hlist_entry(h, struct net_bridge_fdb_entry, hlist);
			if (f->dst == p && f->is_local) {
				/* maybe another port has same hw addr? */
				struct net_bridge_port *op;
				list_for_each_entry(op, &br->port_list, list) {
					if (op != p &&
					    !compare_ether_addr(op->dev->dev_addr,
								f->addr.addr)) {
						f->dst = op;
						goto insert;
					}
				}

				/* delete old one */
				fdb_delete(f);
				goto insert;
			}
		}
	}
 insert:
	/* insert new address,  may fail if invalid address or dup. */
	fdb_insert(br, p, newaddr);

	spin_unlock_bh(&br->hash_lock);
}

void br_fdb_cleanup(unsigned long _data)
{
	struct net_bridge *br = (struct net_bridge *)_data;
	unsigned long delay = hold_time(br);
	unsigned long next_timer = jiffies + br->ageing_time;
	int i;

	spin_lock_bh(&br->hash_lock);
	for (i = 0; i < BR_HASH_SIZE; i++) {
		struct net_bridge_fdb_entry *f;
		struct hlist_node *h, *n;

		hlist_for_each_entry_safe(f, h, n, &br->hash[i], hlist) {
			unsigned long this_timer;
			if (f->is_static)
				continue;
			this_timer = f->ageing_timer + delay;
			if (time_before_eq(this_timer, jiffies)) {
#ifdef HNDCTF
				ctf_brc_t *brcp;

				ctf_brc_acquire(kcih);

				/* Before expiring the fdb entry check the brc
				 * live counter to make sure there are no frames
				 * on this connection for timeout period.
				 */
				brcp = ctf_brc_lkup(kcih, f->addr.addr, TRUE);
				if (brcp != NULL) {
					uint32 arpip = 0;

					if (brcp->live > 0) {
						brcp->live = 0;
						brcp->hitting = 0;
						ctf_brc_release(kcih);
						f->ageing_timer = jiffies;
						continue;
					} else if (brcp->hitting > 0) {
						/* When bridge deletes a CTF hitting cache entry,
						 * we use DHCP "probes" (ARP Request) to trigger
						 * the CTF fast path restoration.
						 */
						brcp->hitting = 0;
						if (brcp->ip != 0)
							arpip = brcp->ip;
					}
					ctf_brc_release(kcih);
					if (arpip != 0)
						arp_send(ARPOP_REQUEST, ETH_P_ARP,
							arpip, br->dev, 0, NULL,
							NULL, NULL);
				} else {
					ctf_brc_release(kcih);
				}
#endif /* HNDCTF */
				fdb_delete(f);
			} else if (time_before(this_timer, next_timer))
				next_timer = this_timer;
		}
	}
	spin_unlock_bh(&br->hash_lock);

	mod_timer(&br->gc_timer, round_jiffies_up(next_timer));
}

/* Completely flush all dynamic entries in forwarding database.*/
void br_fdb_flush(struct net_bridge *br)
{
	int i;

	spin_lock_bh(&br->hash_lock);
	for (i = 0; i < BR_HASH_SIZE; i++) {
		struct net_bridge_fdb_entry *f;
		struct hlist_node *h, *n;
		hlist_for_each_entry_safe(f, h, n, &br->hash[i], hlist) {
			if (!f->is_static)
				fdb_delete(f);
		}
	}
	spin_unlock_bh(&br->hash_lock);
}

/* Flush all entries refering to a specific port.
 * if do_all is set also flush static entries
 */
void br_fdb_delete_by_port(struct net_bridge *br,
			   const struct net_bridge_port *p,
			   int do_all)
{
	int i;

	spin_lock_bh(&br->hash_lock);
	for (i = 0; i < BR_HASH_SIZE; i++) {
		struct hlist_node *h, *g;

		hlist_for_each_safe(h, g, &br->hash[i]) {
			struct net_bridge_fdb_entry *f
				= hlist_entry(h, struct net_bridge_fdb_entry, hlist);
			if (f->dst != p)
				continue;

			if (f->is_static && !do_all)
				continue;
			/*
			 * if multiple ports all have the same device address
			 * then when one port is deleted, assign
			 * the local entry to other port
			 */
			if (f->is_local) {
				struct net_bridge_port *op;
				list_for_each_entry(op, &br->port_list, list) {
					if (op != p &&
					    !compare_ether_addr(op->dev->dev_addr,
								f->addr.addr)) {
						f->dst = op;
						goto skip_delete;
					}
				}
			}

			fdb_delete(f);
		skip_delete: ;
		}
	}
	spin_unlock_bh(&br->hash_lock);
}

/* No locking or refcounting, assumes caller has rcu_read_lock */
struct net_bridge_fdb_entry * BCMFASTPATH_HOST __br_fdb_get(struct net_bridge *br,
					  const unsigned char *addr)
{
	struct hlist_node *h;
	struct net_bridge_fdb_entry *fdb;

	hlist_for_each_entry_rcu(fdb, h, &br->hash[br_mac_hash(addr)], hlist) {
		if (!compare_ether_addr(fdb->addr.addr, addr)) {
			if (unlikely(has_expired(br, fdb)))
				break;
			return fdb;
		}
	}

	return NULL;
}

#if defined(CONFIG_ATM_LANE) || defined(CONFIG_ATM_LANE_MODULE)
/* Interface used by ATM LANE hook to test
 * if an addr is on some other bridge port */
int br_fdb_test_addr(struct net_device *dev, unsigned char *addr)
{
	struct net_bridge_fdb_entry *fdb;
	int ret;

	if (!br_port_exists(dev))
		return 0;

	rcu_read_lock();
	fdb = __br_fdb_get(br_port_get_rcu(dev)->br, addr);
	ret = fdb && fdb->dst->dev != dev &&
		fdb->dst->state == BR_STATE_FORWARDING;
	rcu_read_unlock();

	return ret;
}
#endif /* CONFIG_ATM_LANE */

/*
 * Fill buffer with forwarding table records in
 * the API format.
 */
int br_fdb_fillbuf(struct net_bridge *br, void *buf,
		   unsigned long maxnum, unsigned long skip)
{
	struct __fdb_entry *fe = buf;
	int i, num = 0;
	struct hlist_node *h;
	struct net_bridge_fdb_entry *f;

	memset(buf, 0, maxnum*sizeof(struct __fdb_entry));

	rcu_read_lock();
	for (i = 0; i < BR_HASH_SIZE; i++) {
		hlist_for_each_entry_rcu(f, h, &br->hash[i], hlist) {
			if (num >= maxnum)
				goto out;

			if (has_expired(br, f))
				continue;

			if (skip) {
				--skip;
				continue;
			}

			/* convert from internal format to API */
			memcpy(fe->mac_addr, f->addr.addr, ETH_ALEN);

			/* due to ABI compat need to split into hi/lo */
			fe->port_no = f->dst->port_no;
			fe->port_hi = f->dst->port_no >> 8;

			fe->is_local = f->is_local;
			if (!f->is_static)
				fe->ageing_timer_value = jiffies_to_clock_t(jiffies - f->ageing_timer);
			++fe;
			++num;
		}
	}

 out:
	rcu_read_unlock();

	return num;
}

static inline struct net_bridge_fdb_entry *fdb_find(struct hlist_head *head,
						    const unsigned char *addr)
{
	struct hlist_node *h;
	struct net_bridge_fdb_entry *fdb;

	hlist_for_each_entry_rcu(fdb, h, head, hlist) {
		if (!compare_ether_addr(fdb->addr.addr, addr))
			return fdb;
	}
	return NULL;
}

#ifdef HNDCTF
static struct net_bridge_fdb_entry *fdb_create(struct hlist_head *head,
					       struct net_bridge_port *source,
					       const unsigned char *addr,
					       int is_local,
					       struct sk_buff *skb)
#else
static struct net_bridge_fdb_entry *fdb_create(struct hlist_head *head,
					       struct net_bridge_port *source,
					       const unsigned char *addr,
					       int is_local)
#endif
{
	struct net_bridge_fdb_entry *fdb;

	fdb = kmem_cache_alloc(br_fdb_cache, GFP_ATOMIC);
	if (fdb) {
		memcpy(fdb->addr.addr, addr, ETH_ALEN);
		hlist_add_head_rcu(&fdb->hlist, head);

		fdb->dst = source;
		fdb->is_local = is_local;
		fdb->is_static = is_local;
		fdb->ageing_timer = jiffies;

		/* Add bridge cache entry for non local hosts */
#ifdef HNDCTF
		if (!is_local && (source->state == BR_STATE_FORWARDING))
			br_brc_add((unsigned char *)addr, source->dev, skb);
#endif /* HNDCTF */
	}
	return fdb;
}

static int fdb_insert(struct net_bridge *br, struct net_bridge_port *source,
		  const unsigned char *addr)
{
	struct hlist_head *head = &br->hash[br_mac_hash(addr)];
	struct net_bridge_fdb_entry *fdb;

	if (!is_valid_ether_addr(addr))
		return -EINVAL;

	fdb = fdb_find(head, addr);
	if (fdb) {
		/* it is okay to have multiple ports with same
		 * address, just use the first one.
		 */
		if (fdb->is_local)
			return 0;
		br_warn(br, "adding interface %s with same address "
		       "as a received packet\n",
		       source->dev->name);
		fdb_delete(fdb);
	}

#ifdef HNDCTF
	if (!fdb_create(head, source, addr, 1, NULL))
#else
	if (!fdb_create(head, source, addr, 1))
#endif
		return -ENOMEM;

	return 0;
}

int br_fdb_insert(struct net_bridge *br, struct net_bridge_port *source,
		  const unsigned char *addr)
{
	int ret;

	spin_lock_bh(&br->hash_lock);
	ret = fdb_insert(br, source, addr);
	spin_unlock_bh(&br->hash_lock);
	return ret;
}

#ifdef HNDCTF
void BCMFASTPATH_HOST br_fdb_update(struct net_bridge *br, struct net_bridge_port *source,
		   const unsigned char *addr, struct sk_buff *skb)
#else
void BCMFASTPATH_HOST br_fdb_update(struct net_bridge *br, struct net_bridge_port *source,
		   const unsigned char *addr)
#endif
{
	struct hlist_head *head = &br->hash[br_mac_hash(addr)];
	struct net_bridge_fdb_entry *fdb;

	/* some users want to always flood. */
	if (hold_time(br) == 0)
		return;

	/* ignore packets unless we are using this port */
	if (!(source->state == BR_STATE_LEARNING ||
	      source->state == BR_STATE_FORWARDING))
		return;

	fdb = fdb_find(head, addr);
	if (likely(fdb)) {
		/* attempt to update an entry for a local interface */
		if (unlikely(fdb->is_local)) {
			if (net_ratelimit())
				br_warn(br, "received packet on %s with "
					"own address as source address\n",
					source->dev->name);
		} else {
			/* fastpath: update of existing entry */
#ifdef HNDCTF
			/* Add the entry if the addr is new, or
			 * update the brc entry incase the host moved from
			 * one bridge to another or to a different port under
			 * the same bridge.
			 */
			if (source->state == BR_STATE_FORWARDING)
				br_brc_add((unsigned char *)addr, source->dev, skb);
#endif /* HNDCTF */

			fdb->dst = source;
			fdb->ageing_timer = jiffies;
		}
	} else {
		spin_lock(&br->hash_lock);
		if (!fdb_find(head, addr))
#ifdef HNDCTF
			fdb_create(head, source, addr, 0, skb);
#else
			fdb_create(head, source, addr, 0);
#endif
		/* else  we lose race and someone else inserts
		 * it first, don't bother updating
		 */
		spin_unlock(&br->hash_lock);
	}
}
