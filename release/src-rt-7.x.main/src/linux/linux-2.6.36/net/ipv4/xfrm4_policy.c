/*
 * xfrm4_policy.c
 *
 * Changes:
 *	Kazunori MIYAZAWA @USAGI
 * 	YOSHIFUJI Hideaki @USAGI
 *		Split up af-specific portion
 *
 */

#include <linux/err.h>
#include <linux/kernel.h>
#include <linux/inetdevice.h>
#include <net/dst.h>
#include <net/xfrm.h>
#include <net/ip.h>

static struct xfrm_policy_afinfo xfrm4_policy_afinfo;

static struct dst_entry *xfrm4_dst_lookup(struct net *net, int tos,
					  xfrm_address_t *saddr,
					  xfrm_address_t *daddr)
{
	struct flowi fl = {
		.nl_u = {
			.ip4_u = {
				.tos = tos,
				.daddr = daddr->a4,
			},
		},
	};
	struct dst_entry *dst;
	struct rtable *rt;
	int err;

	if (saddr)
		fl.fl4_src = saddr->a4;

	err = __ip_route_output_key(net, &rt, &fl);
	dst = &rt->dst;
	if (err)
		dst = ERR_PTR(err);
	return dst;
}

static int xfrm4_get_saddr(struct net *net,
			   xfrm_address_t *saddr, xfrm_address_t *daddr)
{
	struct dst_entry *dst;
	struct rtable *rt;

	dst = xfrm4_dst_lookup(net, 0, NULL, daddr);
	if (IS_ERR(dst))
		return -EHOSTUNREACH;

	rt = (struct rtable *)dst;
	saddr->a4 = rt->rt_src;
	dst_release(dst);
	return 0;
}

static int xfrm4_get_tos(struct flowi *fl)
{
	return IPTOS_RT_MASK & fl->fl4_tos; /* Strip ECN bits */
}

static int xfrm4_init_path(struct xfrm_dst *path, struct dst_entry *dst,
			   int nfheader_len)
{
	return 0;
}

static int xfrm4_fill_dst(struct xfrm_dst *xdst, struct net_device *dev,
			  struct flowi *fl)
{
	struct rtable *rt = (struct rtable *)xdst->route;

	xdst->u.rt.fl = *fl;

	xdst->u.dst.dev = dev;
	dev_hold(dev);

	xdst->u.rt.idev = in_dev_get(dev);
	if (!xdst->u.rt.idev)
		return -ENODEV;

	xdst->u.rt.peer = rt->peer;
	if (rt->peer)
		atomic_inc(&rt->peer->refcnt);

	/* Sheit... I remember I did this right. Apparently,
	 * it was magically lost, so this code needs audit */
	xdst->u.rt.rt_flags = rt->rt_flags & (RTCF_BROADCAST | RTCF_MULTICAST |
					      RTCF_LOCAL);
	xdst->u.rt.rt_type = rt->rt_type;
	xdst->u.rt.rt_src = rt->rt_src;
	xdst->u.rt.rt_dst = rt->rt_dst;
	xdst->u.rt.rt_gateway = rt->rt_gateway;
	xdst->u.rt.rt_spec_dst = rt->rt_spec_dst;

	return 0;
}

static void
_decode_session4(struct sk_buff *skb, struct flowi *fl, int reverse)
{
	struct iphdr *iph = ip_hdr(skb);
	u8 *xprth = skb_network_header(skb) + iph->ihl * 4;

	memset(fl, 0, sizeof(struct flowi));
	fl->mark = skb->mark;

	if (!(iph->frag_off & htons(IP_MF | IP_OFFSET))) {
		switch (iph->protocol) {
		case IPPROTO_UDP:
		case IPPROTO_UDPLITE:
		case IPPROTO_TCP:
		case IPPROTO_SCTP:
		case IPPROTO_DCCP:
			if (xprth + 4 < skb->data ||
			    pskb_may_pull(skb, xprth + 4 - skb->data)) {
				__be16 *ports = (__be16 *)xprth;

				fl->fl_ip_sport = ports[!!reverse];
				fl->fl_ip_dport = ports[!reverse];
			}
			break;

		case IPPROTO_ICMP:
			if (pskb_may_pull(skb, xprth + 2 - skb->data)) {
				u8 *icmp = xprth;

				fl->fl_icmp_type = icmp[0];
				fl->fl_icmp_code = icmp[1];
			}
			break;

		case IPPROTO_ESP:
			if (pskb_may_pull(skb, xprth + 4 - skb->data)) {
				__be32 *ehdr = (__be32 *)xprth;

				fl->fl_ipsec_spi = ehdr[0];
			}
			break;

		case IPPROTO_AH:
			if (pskb_may_pull(skb, xprth + 8 - skb->data)) {
				__be32 *ah_hdr = (__be32*)xprth;

				fl->fl_ipsec_spi = ah_hdr[1];
			}
			break;

		case IPPROTO_COMP:
			if (pskb_may_pull(skb, xprth + 4 - skb->data)) {
				__be16 *ipcomp_hdr = (__be16 *)xprth;

				fl->fl_ipsec_spi = htonl(ntohs(ipcomp_hdr[1]));
			}
			break;
		default:
			fl->fl_ipsec_spi = 0;
			break;
		}
	}
	fl->proto = iph->protocol;
	fl->fl4_dst = reverse ? iph->saddr : iph->daddr;
	fl->fl4_src = reverse ? iph->daddr : iph->saddr;
	fl->fl4_tos = iph->tos;
}

static inline int xfrm4_garbage_collect(struct dst_ops *ops)
{
	struct net *net = container_of(ops, struct net, xfrm.xfrm4_dst_ops);

	xfrm4_policy_afinfo.garbage_collect(net);
	return (atomic_read(&ops->entries) > ops->gc_thresh * 2);
}

static void xfrm4_update_pmtu(struct dst_entry *dst, u32 mtu)
{
	struct xfrm_dst *xdst = (struct xfrm_dst *)dst;
	struct dst_entry *path = xdst->route;

	path->ops->update_pmtu(path, mtu);
}

static void xfrm4_dst_destroy(struct dst_entry *dst)
{
	struct xfrm_dst *xdst = (struct xfrm_dst *)dst;

	if (likely(xdst->u.rt.idev))
		in_dev_put(xdst->u.rt.idev);
	if (likely(xdst->u.rt.peer))
		inet_putpeer(xdst->u.rt.peer);
	xfrm_dst_destroy(xdst);
}

static void xfrm4_dst_ifdown(struct dst_entry *dst, struct net_device *dev,
			     int unregister)
{
	struct xfrm_dst *xdst;

	if (!unregister)
		return;

	xdst = (struct xfrm_dst *)dst;
	if (xdst->u.rt.idev->dev == dev) {
		struct in_device *loopback_idev =
			in_dev_get(dev_net(dev)->loopback_dev);
		BUG_ON(!loopback_idev);

		do {
			in_dev_put(xdst->u.rt.idev);
			xdst->u.rt.idev = loopback_idev;
			in_dev_hold(loopback_idev);
			xdst = (struct xfrm_dst *)xdst->u.dst.child;
		} while (xdst->u.dst.xfrm);

		__in_dev_put(loopback_idev);
	}

	xfrm_dst_ifdown(dst, dev);
}

static struct dst_ops xfrm4_dst_ops = {
	.family =		AF_INET,
	.protocol =		cpu_to_be16(ETH_P_IP),
	.gc =			xfrm4_garbage_collect,
	.update_pmtu =		xfrm4_update_pmtu,
	.destroy =		xfrm4_dst_destroy,
	.ifdown =		xfrm4_dst_ifdown,
	.local_out =		__ip_local_out,
	.gc_thresh =		1024,
	.entries =		ATOMIC_INIT(0),
};

static struct xfrm_policy_afinfo xfrm4_policy_afinfo = {
	.family = 		AF_INET,
	.dst_ops =		&xfrm4_dst_ops,
	.dst_lookup =		xfrm4_dst_lookup,
	.get_saddr =		xfrm4_get_saddr,
	.decode_session =	_decode_session4,
	.get_tos =		xfrm4_get_tos,
	.init_path =		xfrm4_init_path,
	.fill_dst =		xfrm4_fill_dst,
};

#ifdef CONFIG_SYSCTL
static struct ctl_table xfrm4_policy_table[] = {
	{
		.procname       = "xfrm4_gc_thresh",
		.data           = &init_net.xfrm.xfrm4_dst_ops.gc_thresh,
		.maxlen         = sizeof(int),
		.mode           = 0644,
		.proc_handler   = proc_dointvec,
	},
	{ }
};

static struct ctl_table_header *sysctl_hdr;
#endif

static void __init xfrm4_policy_init(void)
{
	xfrm_policy_register_afinfo(&xfrm4_policy_afinfo);
}

static void __exit xfrm4_policy_fini(void)
{
#ifdef CONFIG_SYSCTL
	if (sysctl_hdr)
		unregister_net_sysctl_table(sysctl_hdr);
#endif
	xfrm_policy_unregister_afinfo(&xfrm4_policy_afinfo);
}

void __init xfrm4_init(int rt_max_size)
{
	/*
	 * Select a default value for the gc_thresh based on the main route
	 * table hash size.  It seems to me the worst case scenario is when
	 * we have ipsec operating in transport mode, in which we create a
	 * dst_entry per socket.  The xfrm gc algorithm starts trying to remove
	 * entries at gc_thresh, and prevents new allocations as 2*gc_thresh
	 * so lets set an initial xfrm gc_thresh value at the rt_max_size/2.
	 * That will let us store an ipsec connection per route table entry,
	 * and start cleaning when were 1/2 full
	 */
	xfrm4_dst_ops.gc_thresh = rt_max_size/2;

	xfrm4_state_init();
	xfrm4_policy_init();
#ifdef CONFIG_SYSCTL
	sysctl_hdr = register_net_sysctl_table(&init_net, net_ipv4_ctl_path,
						xfrm4_policy_table);
#endif
}
