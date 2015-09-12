/*
 * Kernel module to match cone target.
 *
 * Copyright (C) 1999-2015, Broadcom Corporation
 * 
 *      Unless you and Broadcom execute a separate written software license
 * agreement governing use of this software, this software is licensed to you
 * under the terms of the GNU General Public License version 2 (the "GPL"),
 * available at http://www.broadcom.com/licenses/GPLv2.php, with the
 * following added to such license:
 * 
 *      As a special exception, the copyright holders of this software give you
 * permission to link this software with independent modules, and to copy and
 * distribute the resulting executable under terms of your choice, provided that
 * you also meet, for each linked independent module, the terms and conditions of
 * the license of that module.  An independent module is a module which is not
 * derived from this software.  The special exception does not apply to any
 * modifications of the software.
 * 
 *      Notwithstanding the above, under no circumstances may you combine this
 * software in any way with any other Broadcom software provided under a license
 * other than the GPL, without Broadcom's express prior written consent.
 *
 * $Id:  $
 */
#include <linux/module.h>
#include <linux/types.h>
#include <linux/timer.h>
#include <linux/skbuff.h>
#include <linux/gfp.h>
#include <net/checksum.h>
#include <net/icmp.h>
#include <net/ip.h>
#include <net/tcp.h>  /* For tcp_prot in getorigdst */
#include <linux/icmp.h>
#include <linux/udp.h>
#include <linux/jhash.h>

#include <linux/netfilter_ipv4.h>
#include <net/netfilter/nf_conntrack.h>
#include <net/netfilter/nf_conntrack_core.h>
#include <net/netfilter/nf_nat.h>
#include <net/netfilter/nf_nat_protocol.h>
#include <net/netfilter/nf_nat_core.h>
#include <net/netfilter/nf_nat_helper.h>
#include <net/netfilter/nf_conntrack_helper.h>
#include <net/netfilter/nf_conntrack_l3proto.h>
#include <net/netfilter/nf_conntrack_l4proto.h>
#include <net/netfilter/nf_conntrack_zones.h>

#define DEBUGP(format, args...)

static DEFINE_SPINLOCK(cone_lock);

/* Calculated at init based on memory size */
static unsigned int ipt_cone_htable_size __read_mostly;
static struct hlist_head *bycone __read_mostly;
static int ipt_cone_vmalloc;

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
find_appropriate_cone(struct net *net, const struct nf_conntrack_tuple *tuple)
{
	unsigned int h = hash_by_cone(tuple->dst.u3.ip,
		tuple->dst.u.udp.port, tuple->dst.protonum);

	struct nf_conn_nat *nat;
	struct nf_conn *ct;
	const struct hlist_node *n;

	rcu_read_lock();
	hlist_for_each_entry_rcu(nat, n, &bycone[h], bycone) {
		ct = nat->ct;
		if (cone_cmp(ct, tuple)) {
			rcu_read_unlock();
			return ct;
		}
	}
	rcu_read_unlock();
	return 0;
}

void
ipt_cone_place_in_hashes(struct nf_conn *ct)
{
	struct nf_conn_nat *nat = nfct_nat(ct);
	struct nf_conntrack_tuple *tuple = &ct->tuplehash[IP_CT_DIR_REPLY].tuple;
	struct net *net = nf_ct_net(ct);
	unsigned int conehash;

	/* Make sure it's IPV4 */
	if (tuple->src.l3num != PF_INET)
		return;

	if ((nat->nat_type & NFC_IP_CONE_NAT)) {
		if (!find_appropriate_cone(net, tuple)) {
			conehash = hash_by_cone(tuple->dst.u3.ip,
				tuple->dst.u.udp.port, tuple->dst.protonum);
			spin_lock_bh(&cone_lock);
			hlist_add_head_rcu(&nat->bycone, &bycone[conehash]);
			spin_unlock_bh(&cone_lock);
		}
	}
}

void
ipt_cone_cleanup_conntrack(struct nf_conn_nat *nat)
{
	if (nat->bycone.next) {
		spin_lock_bh(&cone_lock);
		hlist_del_rcu(&nat->bycone);
		spin_unlock_bh(&cone_lock);
	}
}


unsigned int
ipt_cone_target(struct sk_buff *skb, const struct xt_action_param *par)
{
	struct nf_conn *ct;
	struct nf_conn_nat *nat;
	enum ip_conntrack_info ctinfo;
	struct nf_nat_range newrange;
	const struct nf_nat_multi_range_compat *mr = par->targinfo;
	__be32 newdst;
	__be16 newport;
	struct nf_conntrack_tuple *tuple;
	struct nf_conn *cone;
	struct net *net;

	/* Care about only new created one */
	ct = nf_ct_get(skb, &ctinfo);
	if (ct == 0 || (ctinfo != IP_CT_NEW && ctinfo != IP_CT_RELATED))
		return XT_CONTINUE;

	nat = nfct_nat(ct);
	if (nat == NULL)
		return XT_CONTINUE;

	/* As a matter of fact, CONE NAT should only apply on IPPROTO_UDP */
	tuple = &ct->tuplehash[IP_CT_DIR_ORIGINAL].tuple;
	if (tuple->src.l3num != PF_INET || tuple->dst.protonum != IPPROTO_UDP)
		return XT_CONTINUE;

	/* Handle forwarding from WAN to LAN */
	if (par->hooknum == NF_INET_FORWARD) {
		if (nat->nat_type & NFC_IP_CONE_NAT_ALTERED)
			return NF_ACCEPT;
		else
			return XT_CONTINUE;
	}

	/* Make sure it is pre routing */
	if (par->hooknum != NF_INET_PRE_ROUTING)
		return XT_CONTINUE;

	if (nat->nat_type & NFC_IP_CONE_NAT_ALTERED)
		return NF_ACCEPT;

	net = nf_ct_net(ct);
	/* Get cone dst */
	cone = find_appropriate_cone(net, tuple);
	if (cone == NULL)
		return XT_CONTINUE;

	/* Mark it's a CONE_NAT_TYPE, so NF_IP_FORWARD can accept it */
	nat->nat_type |= NFC_IP_CONE_NAT_ALTERED;

	/* Setup new dst ip and port */
	newdst = cone->tuplehash[IP_CT_DIR_ORIGINAL].tuple.src.u3.ip;
	newport = cone->tuplehash[IP_CT_DIR_ORIGINAL].tuple.src.u.udp.port;

	/* Transfer from original range. */
	newrange = ((struct nf_nat_range)
		{ mr->range[0].flags | IP_NAT_RANGE_MAP_IPS,
		newdst, newdst,
		{newport}, {newport} });
	/* Hand modified range to generic setup. */
	return nf_nat_setup_info(ct, &newrange, IP_NAT_MANIP_DST);
}
EXPORT_SYMBOL_GPL(ipt_cone_target);

/* Init/cleanup */
static int __init ipt_cone_init(void)
{
	/* Leave them the same for the moment. */
	ipt_cone_htable_size = nf_conntrack_htable_size;

	bycone = nf_ct_alloc_hashtable(&ipt_cone_htable_size, &ipt_cone_vmalloc, 0);
	if (!bycone)
		return -ENOMEM;

	return 0;
}

static void __exit ipt_cone_cleanup(void)
{
	nf_ct_free_hashtable(bycone, ipt_cone_vmalloc, ipt_cone_htable_size);
}

module_init(ipt_cone_init);
module_exit(ipt_cone_cleanup);
MODULE_LICENSE("Proprietary");
