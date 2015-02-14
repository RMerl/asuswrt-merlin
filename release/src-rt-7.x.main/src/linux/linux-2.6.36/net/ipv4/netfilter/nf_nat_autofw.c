/*
 * Automatic port forwarding target. When this target is entered, a
 * related connection to a port in the reply direction will be
 * expected. This connection may be mapped to a different port.
 *
 * Copyright (C) 2014, Broadcom Corporation. All Rights Reserved.
 * 
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY
 * SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION
 * OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN
 * CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 *
 * $Id: nf_nat_autofw.c,v 1.1 2008-10-02 03:40:29 $
 */

#include <linux/config.h>
#include <linux/types.h>
#include <linux/ip.h>
#include <linux/timer.h>
#include <linux/module.h>
#include <linux/netfilter.h>
#include <net/protocol.h>
#include <net/checksum.h>
#include <net/tcp.h>

#include <linux/netfilter_ipv4.h>
#include <linux/netfilter_ipv4/ip_tables.h>
#include <net/netfilter/nf_nat_rule.h>
#include <linux/netfilter/x_tables.h>

#include <net/netfilter/nf_conntrack_expect.h>
#include <net/netfilter/nf_conntrack_helper.h>
#include <linux/netfilter_ipv4/ip_autofw.h>

DEFINE_RWLOCK(nf_nat_autofw_lock);

#define DEBUGP(format, args...)

static int
autofw_help(struct sk_buff **pskb,
	unsigned int protoff,
	struct nf_conn *ct,
	enum ip_conntrack_info ctinfo)
{
	return 1;
}

static void
autofw_expect(struct nf_conn *ct, struct nf_conntrack_expect *exp)
{
	struct nf_nat_range pre_range;
	u_int32_t newdstip, newsrcip;
	u_int16_t port;
	int ret;
	struct nf_conn_help *help;
	struct nf_conn *exp_ct = exp->master;
	struct nf_conntrack_expect *newexp;
	int count;

	/* expect has been removed from expect list, but expect isn't free yet. */
	help = nfct_help(exp_ct);
	DEBUGP("autofw_nat_expected: got ");
	NF_CT_DUMP_TUPLE(&ct->tuplehash[IP_CT_DIR_ORIGINAL].tuple);

	spin_lock_bh(&nf_nat_autofw_lock);

	port = ntohs(ct->tuplehash[IP_CT_DIR_ORIGINAL].tuple.dst.u.all);
	newdstip = exp->tuple.dst.u3.ip;
	newsrcip = exp->tuple.src.u3.ip;
	if (port < ntohs(help->help.ct_autofw_info.dport[0]) ||
		port > ntohs(help->help.ct_autofw_info.dport[1])) {
		spin_unlock_bh(&nf_nat_autofw_lock);
			return;
	}

	/* Only need to do PRE_ROUTING */
	port -= ntohs(help->help.ct_autofw_info.dport[0]);
	port += ntohs(help->help.ct_autofw_info.to[0]);
	pre_range.flags = IP_NAT_RANGE_MAP_IPS | IP_NAT_RANGE_PROTO_SPECIFIED;
	pre_range.min_ip = pre_range.max_ip = newdstip;
	pre_range.min.all = pre_range.max.all = htons(port);
	nf_nat_setup_info(ct, &pre_range, NF_IP_PRE_ROUTING);

	spin_unlock_bh(&nf_nat_autofw_lock);

	/* Add expect again */

	/* alloc will set exp->master = exp_ct */
	newexp = nf_conntrack_expect_alloc(exp_ct);
	if (!newexp)
		return;

	newexp->tuple.src.u3.ip = exp->tuple.src.u3.ip;
	newexp->tuple.dst.protonum = exp->tuple.dst.protonum;
	newexp->mask.src.u3.ip = 0xFFFFFFFF;
	newexp->mask.dst.protonum = 0xFF;

	newexp->tuple.dst.u3.ip = exp->tuple.dst.u3.ip;
	newexp->mask.dst.u3.ip = 0x0;

	for (count = 1; count < NF_CT_TUPLE_L3SIZE; count++) {
		newexp->tuple.src.u3.all[count] = 0x0;
		newexp->tuple.dst.u3.all[count] = 0x0;
	}

	newexp->mask.dst.u.all = 0x0;
	newexp->mask.src.u.all = 0x0;
	newexp->mask.src.l3num = 0x0;

	newexp->expectfn = autofw_expect;
	newexp->helper = NULL;
	newexp->flags = 0;

	/*
	 * exp->timeout.expires will set as
	 * (jiffies + helper->timeout * HZ), when insert exp.
	*/
	ret = nf_conntrack_expect_related(newexp);
	if (ret == 0)
		nf_conntrack_expect_put(newexp);
}


static struct nf_conntrack_helper autofw_helper;

static unsigned int
autofw_target(struct sk_buff **pskb,
	const struct net_device *in,
	const struct net_device *out,
	unsigned int hooknum,
	const struct xt_target *target,
	const void *targinfo)
{
	const struct ip_autofw_info *info = targinfo;
	const struct iphdr *iph = ip_hdr(*pskb);
	int ret;
	struct nf_conntrack_helper *helper;
	struct nf_conntrack_expect *exp;
	struct nf_conn *ct;
	enum ip_conntrack_info ctinfo;
	struct nf_conn_help *help;
	int count;

	ct = nf_ct_get(*pskb, &ctinfo);
	if (!ct)
		goto out;

	helper = __nf_conntrack_helper_find_byname("autofw");
	if (!helper)
		goto out;

	help = nfct_help(ct);
	help->helper = helper;

	/* alloc will set exp->master = ct */
	exp = nf_conntrack_expect_alloc(ct);
	if (!exp)
		goto out;

	helper->me = THIS_MODULE;
	helper->timeout = 5 * 60;

	exp->tuple.src.u3.ip = iph->daddr;
	exp->tuple.dst.protonum = info->proto;
	exp->mask.src.u3.ip = 0xFFFFFFFF;
	exp->mask.dst.protonum = 0xFF;

	exp->tuple.dst.u3.ip = iph->saddr;
	exp->mask.dst.u3.ip = 0x0;

	for (count = 1; count < NF_CT_TUPLE_L3SIZE; count++) {
		exp->tuple.src.u3.all[count] = 0x0;
		exp->tuple.dst.u3.all[count] = 0x0;
	}

	exp->mask.dst.u.all = 0x0;
	exp->mask.src.u.all = 0x0;
	exp->mask.src.l3num = 0x0;

	exp->expectfn = autofw_expect;
	exp->helper = NULL;
	exp->flags = 0;

	/*
	 * exp->timeout.expires will set as
	 * (jiffies + helper->timeout * HZ), when insert exp.
	*/
	ret = nf_conntrack_expect_related(exp);
	if (ret != 0)
		goto out;

	nf_conntrack_expect_put(exp);

	help->help.ct_autofw_info.dport[0] = info->dport[0];
	help->help.ct_autofw_info.dport[1] = info->dport[1];
	help->help.ct_autofw_info.to[0] = info->to[0];
	help->help.ct_autofw_info.to[1] = info->to[1];

out:
	return IPT_CONTINUE;
}

static int
autofw_check(const char *tablename,
	const void *e,
	const struct xt_target *target,
	void *targinfo,
	unsigned int hook_mask)
{

	const struct ip_autofw_info *info = targinfo;

	if (info->proto != IPPROTO_TCP && info->proto != IPPROTO_UDP) {
		DEBUGP("autofw_check: bad proto %d.\n", info->proto);
		return 0;
	}

	return 1;
}

static struct xt_target autofw = {
	.name		= "autofw",
	.family		= AF_INET,
	.target		= autofw_target,
	.targetsize	= sizeof(struct ip_autofw_info),
	.table		= "nat",
	.hooks		= 1 << NF_IP_PRE_ROUTING,
	.checkentry	= autofw_check,
	.me		= THIS_MODULE
};

static int __init ip_autofw_init(void)
{
	int ret;

	autofw_helper.name = "autofw";
	autofw_helper.tuple.dst.u3.ip = 0xFFFFFFFF;
	autofw_helper.tuple.dst.protonum = 0xFF;
	autofw_helper.mask.dst.u3.ip = 0xFFFFFFFF;
	autofw_helper.mask.dst.protonum = 0xFF;
	autofw_helper.tuple.src.u3.ip = 0xFFFFFFFF;
	autofw_helper.me = THIS_MODULE;
	autofw_helper.timeout = 5 * 60;
	autofw_helper.help = autofw_help;

	ret = nf_conntrack_helper_register(&autofw_helper);
	if (ret)
		nf_conntrack_helper_unregister(&autofw_helper);

	return xt_register_target(&autofw);
}

static void __exit ip_autofw_fini(void)
{
	xt_unregister_target(&autofw);
}

module_init(ip_autofw_init);
module_exit(ip_autofw_fini);
