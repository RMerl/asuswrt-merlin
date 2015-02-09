/*
 * Copyright (C)2004 USAGI/WIDE Project
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * Author:
 *	Yasuyuki Kozakai @USAGI <yasuyuki.kozakai@toshiba.co.jp>
 */

#include <linux/types.h>
#include <linux/ipv6.h>
#include <linux/in6.h>
#include <linux/netfilter.h>
#include <linux/module.h>
#include <linux/skbuff.h>
#include <linux/icmp.h>
#include <linux/sysctl.h>
#include <net/ipv6.h>
#include <net/inet_frag.h>

#include <linux/netfilter_bridge.h>
#include <linux/netfilter_ipv6.h>
#include <net/netfilter/nf_conntrack.h>
#include <net/netfilter/nf_conntrack_helper.h>
#include <net/netfilter/nf_conntrack_l4proto.h>
#include <net/netfilter/nf_conntrack_l3proto.h>
#include <net/netfilter/nf_conntrack_core.h>
#include <net/netfilter/nf_conntrack_zones.h>
#include <net/netfilter/ipv6/nf_conntrack_ipv6.h>
#include <net/netfilter/nf_log.h>

#ifdef HNDCTF
extern void ip_conntrack_ipct_add(struct sk_buff *skb, u_int32_t hooknum,
	struct nf_conn *ct, enum ip_conntrack_info ci,
	struct nf_conntrack_tuple *manip);
#endif /* HNDCTF */

static bool ipv6_pkt_to_tuple(const struct sk_buff *skb, unsigned int nhoff,
			      struct nf_conntrack_tuple *tuple)
{
	const u_int32_t *ap;
	u_int32_t _addrs[8];

	ap = skb_header_pointer(skb, nhoff + offsetof(struct ipv6hdr, saddr),
				sizeof(_addrs), _addrs);
	if (ap == NULL)
		return false;

	memcpy(tuple->src.u3.ip6, ap, sizeof(tuple->src.u3.ip6));
	memcpy(tuple->dst.u3.ip6, ap + 4, sizeof(tuple->dst.u3.ip6));

	return true;
}

static bool ipv6_invert_tuple(struct nf_conntrack_tuple *tuple,
			      const struct nf_conntrack_tuple *orig)
{
	memcpy(tuple->src.u3.ip6, orig->dst.u3.ip6, sizeof(tuple->src.u3.ip6));
	memcpy(tuple->dst.u3.ip6, orig->src.u3.ip6, sizeof(tuple->dst.u3.ip6));

	return true;
}

static int ipv6_print_tuple(struct seq_file *s,
			    const struct nf_conntrack_tuple *tuple)
{
	return seq_printf(s, "src=%pI6 dst=%pI6 ",
			  tuple->src.u3.ip6, tuple->dst.u3.ip6);
}

/*
 * Based on ipv6_skip_exthdr() in net/ipv6/exthdr.c
 *
 * This function parses (probably truncated) exthdr set "hdr"
 * of length "len". "nexthdrp" initially points to some place,
 * where type of the first header can be found.
 *
 * It skips all well-known exthdrs, and returns pointer to the start
 * of unparsable area i.e. the first header with unknown type.
 * if success, *nexthdr is updated by type/protocol of this header.
 *
 * NOTES: - it may return pointer pointing beyond end of packet,
 *          if the last recognized header is truncated in the middle.
 *        - if packet is truncated, so that all parsed headers are skipped,
 *          it returns -1.
 *        - if packet is fragmented, return pointer of the fragment header.
 *        - ESP is unparsable for now and considered like
 *          normal payload protocol.
 *        - Note also special handling of AUTH header. Thanks to IPsec wizards.
 */

static int nf_ct_ipv6_skip_exthdr(const struct sk_buff *skb, int start,
				  u8 *nexthdrp, int len)
{
	u8 nexthdr = *nexthdrp;

	while (ipv6_ext_hdr(nexthdr)) {
		struct ipv6_opt_hdr hdr;
		int hdrlen;

		if (len < (int)sizeof(struct ipv6_opt_hdr))
			return -1;
		if (nexthdr == NEXTHDR_NONE)
			break;
		if (nexthdr == NEXTHDR_FRAGMENT)
			break;
		if (skb_copy_bits(skb, start, &hdr, sizeof(hdr)))
			BUG();
		if (nexthdr == NEXTHDR_AUTH)
			hdrlen = (hdr.hdrlen+2)<<2;
		else
			hdrlen = ipv6_optlen(&hdr);

		nexthdr = hdr.nexthdr;
		len -= hdrlen;
		start += hdrlen;
	}

	*nexthdrp = nexthdr;
	return start;
}

static int ipv6_get_l4proto(const struct sk_buff *skb, unsigned int nhoff,
			    unsigned int *dataoff, u_int8_t *protonum)
{
	unsigned int extoff = nhoff + sizeof(struct ipv6hdr);
	unsigned char pnum;
	int protoff;

	if (skb_copy_bits(skb, nhoff + offsetof(struct ipv6hdr, nexthdr),
			  &pnum, sizeof(pnum)) != 0) {
		pr_debug("ip6_conntrack_core: can't get nexthdr\n");
		return -NF_ACCEPT;
	}
	protoff = nf_ct_ipv6_skip_exthdr(skb, extoff, &pnum, skb->len - extoff);
	/*
	 * (protoff == skb->len) mean that the packet doesn't have no data
	 * except of IPv6 & ext headers. but it's tracked anyway. - YK
	 */
	if ((protoff < 0) || (protoff > skb->len)) {
		pr_debug("ip6_conntrack_core: can't find proto in pkt\n");
		return -NF_ACCEPT;
	}

	*dataoff = protoff;
	*protonum = pnum;
	return NF_ACCEPT;
}

static unsigned int ipv6_confirm(unsigned int hooknum,
				 struct sk_buff *skb,
				 const struct net_device *in,
				 const struct net_device *out,
				 int (*okfn)(struct sk_buff *))
{
	struct nf_conn *ct;
	const struct nf_conn_help *help;
	const struct nf_conntrack_helper *helper;
	enum ip_conntrack_info ctinfo;
	unsigned int ret, protoff;
	unsigned int extoff = (u8 *)(ipv6_hdr(skb) + 1) - skb->data;
	unsigned char pnum = ipv6_hdr(skb)->nexthdr;


	/* This is where we call the helper: as the packet goes out. */
	ct = nf_ct_get(skb, &ctinfo);
	if (!ct || ctinfo == IP_CT_RELATED + IP_CT_IS_REPLY)
		goto out;

	help = nfct_help(ct);
	if (!help)
		goto out;
	/* rcu_read_lock()ed by nf_hook_slow */
	helper = rcu_dereference(help->helper);
	if (!helper)
		goto out;

	protoff = nf_ct_ipv6_skip_exthdr(skb, extoff, &pnum,
					 skb->len - extoff);
	if (protoff > skb->len || pnum == NEXTHDR_FRAGMENT) {
		pr_debug("proto header not found\n");
		return NF_ACCEPT;
	}

	ret = helper->help(skb, protoff, ct, ctinfo);
	if (ret != NF_ACCEPT) {
		nf_log_packet(NFPROTO_IPV6, hooknum, skb, in, out, NULL,
			      "nf_ct_%s: dropping packet", helper->name);
		return ret;
	}
out:
	/* We've seen it coming out the other side: confirm it */
	return nf_conntrack_confirm(skb);
}

static enum ip6_defrag_users nf_ct6_defrag_user(unsigned int hooknum,
						struct sk_buff *skb)
{
	u16 zone = NF_CT_DEFAULT_ZONE;

	if (skb->nfct)
		zone = nf_ct_zone((struct nf_conn *)skb->nfct);

#ifdef CONFIG_BRIDGE_NETFILTER
	if (skb->nf_bridge &&
	    skb->nf_bridge->mask & BRNF_NF_BRIDGE_PREROUTING)
		return IP6_DEFRAG_CONNTRACK_BRIDGE_IN + zone;
#endif
	if (hooknum == NF_INET_PRE_ROUTING)
		return IP6_DEFRAG_CONNTRACK_IN + zone;
	else
		return IP6_DEFRAG_CONNTRACK_OUT + zone;

}

static unsigned int ipv6_defrag(unsigned int hooknum,
				struct sk_buff *skb,
				const struct net_device *in,
				const struct net_device *out,
				int (*okfn)(struct sk_buff *))
{
	struct sk_buff *reasm;

	/* Previously seen (loopback)?  */
	if (skb->nfct && !nf_ct_is_template((struct nf_conn *)skb->nfct))
		return NF_ACCEPT;

	reasm = nf_ct_frag6_gather(skb, nf_ct6_defrag_user(hooknum, skb));
	/* queued */
	if (reasm == NULL)
		return NF_STOLEN;

	/* error occured or not fragmented */
	if (reasm == skb)
		return NF_ACCEPT;

	nf_ct_frag6_output(hooknum, reasm, (struct net_device *)in,
			   (struct net_device *)out, okfn);

	return NF_STOLEN;
}

static unsigned int __ipv6_conntrack_in(struct net *net,
					unsigned int hooknum,
					struct sk_buff *skb,
					int (*okfn)(struct sk_buff *))
{
	struct sk_buff *reasm = skb->nfct_reasm;
	unsigned int ret;

	/* This packet is fragmented and has reassembled packet. */
	if (reasm) {
		/* Reassembled packet isn't parsed yet ? */
		if (!reasm->nfct) {
			ret = nf_conntrack_in(net, PF_INET6, hooknum, reasm);
			if (ret != NF_ACCEPT)
				return ret;
		}
		nf_conntrack_get(reasm->nfct);
		skb->nfct = reasm->nfct;
		skb->nfctinfo = reasm->nfctinfo;
		return NF_ACCEPT;
	}

	ret = nf_conntrack_in(net, PF_INET6, hooknum, skb);

#if defined(HNDCTF)
	if (ret == NF_ACCEPT) {
		struct nf_conn *ct;
		enum ip_conntrack_info ctinfo;

		ct = nf_ct_get(skb, &ctinfo);
		ip_conntrack_ipct_add(skb, hooknum, ct, ctinfo, NULL);
	}
#endif /* HNDCTF */

	return ret;
}

static unsigned int ipv6_conntrack_in(unsigned int hooknum,
				      struct sk_buff *skb,
				      const struct net_device *in,
				      const struct net_device *out,
				      int (*okfn)(struct sk_buff *))
{
	return __ipv6_conntrack_in(dev_net(in), hooknum, skb, okfn);
}

static unsigned int ipv6_conntrack_local(unsigned int hooknum,
					 struct sk_buff *skb,
					 const struct net_device *in,
					 const struct net_device *out,
					 int (*okfn)(struct sk_buff *))
{
	/* root is playing with raw sockets. */
	if (skb->len < sizeof(struct ipv6hdr)) {
		if (net_ratelimit())
			pr_notice("ipv6_conntrack_local: packet too short\n");
		return NF_ACCEPT;
	}
	return __ipv6_conntrack_in(dev_net(out), hooknum, skb, okfn);
}

static struct nf_hook_ops ipv6_conntrack_ops[] __read_mostly = {
	{
		.hook		= ipv6_defrag,
		.owner		= THIS_MODULE,
		.pf		= NFPROTO_IPV6,
		.hooknum	= NF_INET_PRE_ROUTING,
		.priority	= NF_IP6_PRI_CONNTRACK_DEFRAG,
	},
	{
		.hook		= ipv6_conntrack_in,
		.owner		= THIS_MODULE,
		.pf		= NFPROTO_IPV6,
		.hooknum	= NF_INET_PRE_ROUTING,
		.priority	= NF_IP6_PRI_CONNTRACK,
	},
	{
		.hook		= ipv6_conntrack_local,
		.owner		= THIS_MODULE,
		.pf		= NFPROTO_IPV6,
		.hooknum	= NF_INET_LOCAL_OUT,
		.priority	= NF_IP6_PRI_CONNTRACK,
	},
	{
		.hook		= ipv6_defrag,
		.owner		= THIS_MODULE,
		.pf		= NFPROTO_IPV6,
		.hooknum	= NF_INET_LOCAL_OUT,
		.priority	= NF_IP6_PRI_CONNTRACK_DEFRAG,
	},
	{
		.hook		= ipv6_confirm,
		.owner		= THIS_MODULE,
		.pf		= NFPROTO_IPV6,
		.hooknum	= NF_INET_POST_ROUTING,
		.priority	= NF_IP6_PRI_LAST,
	},
	{
		.hook		= ipv6_confirm,
		.owner		= THIS_MODULE,
		.pf		= NFPROTO_IPV6,
		.hooknum	= NF_INET_LOCAL_IN,
		.priority	= NF_IP6_PRI_LAST-1,
	},
};

#if defined(CONFIG_NF_CT_NETLINK) || defined(CONFIG_NF_CT_NETLINK_MODULE)

#include <linux/netfilter/nfnetlink.h>
#include <linux/netfilter/nfnetlink_conntrack.h>

static int ipv6_tuple_to_nlattr(struct sk_buff *skb,
				const struct nf_conntrack_tuple *tuple)
{
	NLA_PUT(skb, CTA_IP_V6_SRC, sizeof(u_int32_t) * 4,
		&tuple->src.u3.ip6);
	NLA_PUT(skb, CTA_IP_V6_DST, sizeof(u_int32_t) * 4,
		&tuple->dst.u3.ip6);
	return 0;

nla_put_failure:
	return -1;
}

static const struct nla_policy ipv6_nla_policy[CTA_IP_MAX+1] = {
	[CTA_IP_V6_SRC]	= { .len = sizeof(u_int32_t)*4 },
	[CTA_IP_V6_DST]	= { .len = sizeof(u_int32_t)*4 },
};

static int ipv6_nlattr_to_tuple(struct nlattr *tb[],
				struct nf_conntrack_tuple *t)
{
	if (!tb[CTA_IP_V6_SRC] || !tb[CTA_IP_V6_DST])
		return -EINVAL;

	memcpy(&t->src.u3.ip6, nla_data(tb[CTA_IP_V6_SRC]),
	       sizeof(u_int32_t) * 4);
	memcpy(&t->dst.u3.ip6, nla_data(tb[CTA_IP_V6_DST]),
	       sizeof(u_int32_t) * 4);

	return 0;
}

static int ipv6_nlattr_tuple_size(void)
{
	return nla_policy_len(ipv6_nla_policy, CTA_IP_MAX + 1);
}
#endif

struct nf_conntrack_l3proto nf_conntrack_l3proto_ipv6 __read_mostly = {
	.l3proto		= PF_INET6,
	.name			= "ipv6",
	.pkt_to_tuple		= ipv6_pkt_to_tuple,
	.invert_tuple		= ipv6_invert_tuple,
	.print_tuple		= ipv6_print_tuple,
	.get_l4proto		= ipv6_get_l4proto,
#if defined(CONFIG_NF_CT_NETLINK) || defined(CONFIG_NF_CT_NETLINK_MODULE)
	.tuple_to_nlattr	= ipv6_tuple_to_nlattr,
	.nlattr_tuple_size	= ipv6_nlattr_tuple_size,
	.nlattr_to_tuple	= ipv6_nlattr_to_tuple,
	.nla_policy		= ipv6_nla_policy,
#endif
#ifdef CONFIG_SYSCTL
	.ctl_table_path		= nf_net_netfilter_sysctl_path,
	.ctl_table		= nf_ct_ipv6_sysctl_table,
#endif
	.me			= THIS_MODULE,
};

MODULE_ALIAS("nf_conntrack-" __stringify(AF_INET6));
MODULE_LICENSE("GPL");
MODULE_AUTHOR("Yasuyuki KOZAKAI @USAGI <yasuyuki.kozakai@toshiba.co.jp>");

static int __init nf_conntrack_l3proto_ipv6_init(void)
{
	int ret = 0;

	need_conntrack();

	ret = nf_ct_frag6_init();
	if (ret < 0) {
		pr_err("nf_conntrack_ipv6: can't initialize frag6.\n");
		return ret;
	}
	ret = nf_conntrack_l4proto_register(&nf_conntrack_l4proto_tcp6);
	if (ret < 0) {
		pr_err("nf_conntrack_ipv6: can't register tcp.\n");
		goto cleanup_frag6;
	}

	ret = nf_conntrack_l4proto_register(&nf_conntrack_l4proto_udp6);
	if (ret < 0) {
		pr_err("nf_conntrack_ipv6: can't register udp.\n");
		goto cleanup_tcp;
	}

	ret = nf_conntrack_l4proto_register(&nf_conntrack_l4proto_icmpv6);
	if (ret < 0) {
		pr_err("nf_conntrack_ipv6: can't register icmpv6.\n");
		goto cleanup_udp;
	}

	ret = nf_conntrack_l3proto_register(&nf_conntrack_l3proto_ipv6);
	if (ret < 0) {
		pr_err("nf_conntrack_ipv6: can't register ipv6\n");
		goto cleanup_icmpv6;
	}

	ret = nf_register_hooks(ipv6_conntrack_ops,
				ARRAY_SIZE(ipv6_conntrack_ops));
	if (ret < 0) {
		pr_err("nf_conntrack_ipv6: can't register pre-routing defrag "
		       "hook.\n");
		goto cleanup_ipv6;
	}
	return ret;

 cleanup_ipv6:
	nf_conntrack_l3proto_unregister(&nf_conntrack_l3proto_ipv6);
 cleanup_icmpv6:
	nf_conntrack_l4proto_unregister(&nf_conntrack_l4proto_icmpv6);
 cleanup_udp:
	nf_conntrack_l4proto_unregister(&nf_conntrack_l4proto_udp6);
 cleanup_tcp:
	nf_conntrack_l4proto_unregister(&nf_conntrack_l4proto_tcp6);
 cleanup_frag6:
	nf_ct_frag6_cleanup();
	return ret;
}

static void __exit nf_conntrack_l3proto_ipv6_fini(void)
{
	synchronize_net();
	nf_unregister_hooks(ipv6_conntrack_ops, ARRAY_SIZE(ipv6_conntrack_ops));
	nf_conntrack_l3proto_unregister(&nf_conntrack_l3proto_ipv6);
	nf_conntrack_l4proto_unregister(&nf_conntrack_l4proto_icmpv6);
	nf_conntrack_l4proto_unregister(&nf_conntrack_l4proto_udp6);
	nf_conntrack_l4proto_unregister(&nf_conntrack_l4proto_tcp6);
	nf_ct_frag6_cleanup();
}

module_init(nf_conntrack_l3proto_ipv6_init);
module_exit(nf_conntrack_l3proto_ipv6_fini);
