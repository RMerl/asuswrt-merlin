/*
 *      broadcast connection tracking helper
 *
 *      (c) 2005 Patrick McHardy <kaber@trash.net>
 *
 *      This program is free software; you can redistribute it and/or
 *      modify it under the terms of the GNU General Public License
 *      as published by the Free Software Foundation; either version
 *      2 of the License, or (at your option) any later version.
 */

#include <linux/module.h>
#include <linux/ip.h>
#include <net/route.h>
#include <linux/inetdevice.h>
#include <linux/skbuff.h>

#include <net/netfilter/nf_conntrack.h>
#include <net/netfilter/nf_conntrack_helper.h>
#include <net/netfilter/nf_conntrack_expect.h>

int nf_conntrack_broadcast_help(struct sk_buff *skb,
				unsigned int protoff,
				struct nf_conn *ct,
				enum ip_conntrack_info ctinfo,
				unsigned int timeout)
{
	struct nf_conntrack_expect *exp;
	struct iphdr *iph = ip_hdr(skb);
	struct rtable *rt = skb_rtable(skb);
	struct in_device *in_dev;
	struct nf_conn_help *help = nfct_help(ct);
	__be32 mask = 0;

	/* we're only interested in locally generated packets */
	if (skb->sk == NULL)
		goto out;
	if (rt == NULL || !(rt->rt_flags & RTCF_BROADCAST))
		goto out;
	if (CTINFO2DIR(ctinfo) != IP_CT_DIR_ORIGINAL)
		goto out;

	rcu_read_lock();
	in_dev = __in_dev_get_rcu(rt->dst.dev);
	if (in_dev != NULL) {
		for_primary_ifa(in_dev) {
			if (ifa->ifa_broadcast == iph->daddr) {
				mask = ifa->ifa_mask;
				break;
			}
		} endfor_ifa(in_dev);
	}
	rcu_read_unlock();

	if (mask == 0)
		goto out;

	exp = nf_ct_expect_alloc(ct);
	if (exp == NULL)
		goto out;

	exp->tuple                = ct->tuplehash[IP_CT_DIR_REPLY].tuple;
	exp->tuple.src.u.udp.port = help->helper->tuple.src.u.udp.port;

	exp->mask.src.u3.ip       = mask;
	exp->mask.src.u.udp.port  = htons(0xFFFF);

	exp->expectfn             = NULL;
	exp->flags                = NF_CT_EXPECT_PERMANENT;
	exp->class		  = NF_CT_EXPECT_CLASS_DEFAULT;
	exp->helper               = NULL;

	nf_ct_expect_related(exp);
	nf_ct_expect_put(exp);

	nf_ct_refresh(ct, skb, timeout * HZ);
out:
	return NF_ACCEPT;
}
EXPORT_SYMBOL_GPL(nf_conntrack_broadcast_help);

MODULE_LICENSE("GPL");
