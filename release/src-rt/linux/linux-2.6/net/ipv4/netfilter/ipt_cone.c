/*
 * Kernel module to match cone target.
 *
 * Copyright (C) 2009, Broadcom Corporation
 * All Rights Reserved.
 * 
 * THIS SOFTWARE IS OFFERED "AS IS", AND BROADCOM GRANTS NO WARRANTIES OF ANY
 * KIND, EXPRESS OR IMPLIED, BY STATUTE, COMMUNICATION OR OTHERWISE. BROADCOM
 * SPECIFICALLY DISCLAIMS ANY IMPLIED WARRANTIES OF MERCHANTABILITY, FITNESS
 * FOR A SPECIFIC PURPOSE OR NONINFRINGEMENT CONCERNING THIS SOFTWARE.
 *
 * $Id: ipt_cone.c,v 1.1 2010/02/07 01:22:09 Exp $
 */
#include <linux/module.h>
#include <linux/types.h>
#include <linux/timer.h>
#include <linux/skbuff.h>
#include <linux/vmalloc.h>
#include <net/ip.h>
#include <linux/udp.h>
#include <net/netfilter/nf_nat_rule.h>
#include <linux/netfilter_ipv4.h>
#include <linux/netfilter/x_tables.h>

#define DEBUGP(format, args...)

static DEFINE_RWLOCK(cone_lock);

/* Calculated at init based on memory size */
static unsigned int ipt_cone_htable_size;
static struct list_head *bycone;

static inline size_t
hash_by_cone(__be32 ip, __be16 port, u_int8_t protonum)
{
	/* Original src, to ensure we map it consistently if poss. */
	return (ip + port + protonum) % ipt_cone_htable_size;
}

static inline int
cone_cmp(const struct nf_conn *ct,
	const struct nf_conntrack_tuple *tuple)
{
	return (ct->tuplehash[IP_CT_DIR_REPLY].tuple.dst.protonum
		== tuple->dst.protonum &&
		ct->tuplehash[IP_CT_DIR_REPLY].tuple.dst.u3.ip
		== tuple->dst.u3.ip &&
		ct->tuplehash[IP_CT_DIR_REPLY].tuple.dst.u.udp.port
		== tuple->dst.u.udp.port);
}

/* Only called for SRC manip */
static struct nf_conn *
find_appropriate_cone(const struct nf_conntrack_tuple *tuple)
{
	unsigned int h = hash_by_cone(tuple->dst.u3.ip,
		tuple->dst.u.udp.port, tuple->dst.protonum);

	struct nf_conn_nat *nat;
	struct nf_conn *ct;

	read_lock_bh(&cone_lock);
	list_for_each_entry(nat, &bycone[h], info.bycone) {
		ct = (struct nf_conn *)((char *)nat - offsetof(struct nf_conn, data));
		if (cone_cmp(ct, tuple)) {
			read_unlock_bh(&cone_lock);
			return ct;
		}
	}
	read_unlock_bh(&cone_lock);
	return 0;
}

void
ipt_cone_place_in_hashes(struct nf_conn *ct)
{
	struct nf_conn_nat *nat = nfct_nat(ct);
	struct nf_conntrack_tuple *tuple = &ct->tuplehash[IP_CT_DIR_REPLY].tuple;

	/* Make sure it's IPV4 */
	if (tuple->src.l3num != PF_INET)
		return;

	if ((nat->info.nat_type & NFC_IP_CONE_NAT)) {
		if (!find_appropriate_cone(tuple)) {
			unsigned int conehash = hash_by_cone(
				tuple->dst.u3.ip, tuple->dst.u.udp.port, tuple->dst.protonum);

			write_lock_bh(&cone_lock);
			list_add(&nat->info.bycone, &bycone[conehash]);
			write_unlock_bh(&cone_lock);
		}
	}
}

void
ipt_cone_cleanup_conntrack(struct nf_conn_nat *nat)
{
	if (nat->info.bycone.next) {
		write_lock_bh(&cone_lock);
		list_del(&nat->info.bycone);
		write_unlock_bh(&cone_lock);
	}
}

unsigned int
ipt_cone_target(struct sk_buff **pskb,
	unsigned int hooknum,
	const struct net_device *in,
	const struct net_device *out,
	const struct xt_target *target,
	const void *targinfo)
{
	struct nf_conn *ct;
	struct nf_conn_nat *nat;
	enum ip_conntrack_info ctinfo;
	struct nf_nat_range newrange;
	const struct nf_nat_multi_range_compat *mr = targinfo;
	__be32 newdst;
	__be16 newport;
	struct nf_conntrack_tuple *tuple;
	struct nf_conn *cone;


	/* Care about only new created one */
	ct = nf_ct_get(*pskb, &ctinfo);
	if (ct == 0 || (ctinfo != IP_CT_NEW && ctinfo != IP_CT_RELATED))
		return XT_CONTINUE;

	nat = nfct_nat(ct);

	/* As a matter of fact, CONE NAT should only apply on IPPROTO_UDP */
	tuple = &ct->tuplehash[IP_CT_DIR_ORIGINAL].tuple;
	if (tuple->src.l3num != PF_INET || tuple->dst.protonum != IPPROTO_UDP)
		return XT_CONTINUE;

	/* Handle forwarding from WAN to LAN */
	if (hooknum == NF_IP_FORWARD) {
		if (nat->info.nat_type & NFC_IP_CONE_NAT_ALTERED)
			return NF_ACCEPT;
		else
			return XT_CONTINUE;
	}

	/* Make sure it is pre routing */
	if (hooknum != NF_IP_PRE_ROUTING)
		return XT_CONTINUE;

	/* Get cone dst */
	cone = find_appropriate_cone(tuple);
	if (cone == NULL)
		return XT_CONTINUE;

	/* Mark it's a CONE_NAT_TYPE, so NF_IP_FORWARD can accept it */
	nat->info.nat_type |= NFC_IP_CONE_NAT_ALTERED;

	/* Setup new dst ip and port */
	newdst = cone->tuplehash[IP_CT_DIR_ORIGINAL].tuple.src.u3.ip;
	newport = cone->tuplehash[IP_CT_DIR_ORIGINAL].tuple.src.u.udp.port;

	/* Transfer from original range. */
	newrange = ((struct nf_nat_range)
		{ mr->range[0].flags | IP_NAT_RANGE_MAP_IPS,
		newdst, newdst,
		{newport}, {newport} });

	/* Hand modified range to generic setup. */
	return nf_nat_setup_info(ct, &newrange, hooknum);
}

/* Init/cleanup */
static int __init ipt_cone_init(void)
{
	size_t i;

	/* Leave them the same for the moment. */
	ipt_cone_htable_size = nf_conntrack_htable_size;

	/* vmalloc for hash table */
	bycone = vmalloc(sizeof(struct list_head) * ipt_cone_htable_size);
	if (!bycone)
		return -ENOMEM;

	for (i = 0; i < ipt_cone_htable_size; i++) {
		INIT_LIST_HEAD(&bycone[i]);
	}

	return 0;
}

static void __exit ipt_cone_cleanup(void)
{
	vfree(bycone);
}

module_init(ipt_cone_init);
module_exit(ipt_cone_cleanup);
MODULE_LICENSE("Proprietary");
