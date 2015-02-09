/*
 *	IPv6 tunneling device
 *	Linux INET6 implementation
 *
 *	Authors:
 *	Ville Nuorvala		<vnuorval@tcs.hut.fi>
 *	Yasuyuki Kozakai	<kozakai@linux-ipv6.org>
 *
 *      Based on:
 *      linux/net/ipv6/sit.c and linux/net/ipv4/ipip.c
 *
 *      RFC 2473
 *
 *	This program is free software; you can redistribute it and/or
 *      modify it under the terms of the GNU General Public License
 *      as published by the Free Software Foundation; either version
 *      2 of the License, or (at your option) any later version.
 *
 */

#include <linux/module.h>
#include <linux/capability.h>
#include <linux/errno.h>
#include <linux/types.h>
#include <linux/sockios.h>
#include <linux/icmp.h>
#include <linux/if.h>
#include <linux/in.h>
#include <linux/ip.h>
#include <linux/if_tunnel.h>
#include <linux/net.h>
#include <linux/in6.h>
#include <linux/netdevice.h>
#include <linux/if_arp.h>
#include <linux/icmpv6.h>
#include <linux/init.h>
#include <linux/route.h>
#include <linux/rtnetlink.h>
#include <linux/netfilter_ipv6.h>
#include <linux/slab.h>

#include <asm/uaccess.h>
#include <asm/atomic.h>

#include <net/icmp.h>
#include <net/ip.h>
#include <net/ipv6.h>
#include <net/ip6_route.h>
#include <net/addrconf.h>
#include <net/ip6_tunnel.h>
#include <net/xfrm.h>
#include <net/dsfield.h>
#include <net/inet_ecn.h>
#include <net/net_namespace.h>
#include <net/netns/generic.h>

MODULE_AUTHOR("Ville Nuorvala");
MODULE_DESCRIPTION("IPv6 tunneling device");
MODULE_LICENSE("GPL");

#define IPV6_TLV_TEL_DST_SIZE 8

#ifdef IP6_TNL_DEBUG
#define IP6_TNL_TRACE(x...) printk(KERN_DEBUG "%s:" x "\n", __func__)
#else
#define IP6_TNL_TRACE(x...) do {;} while(0)
#endif

#define IPV6_TCLASS_MASK (IPV6_FLOWINFO_MASK & ~IPV6_FLOWLABEL_MASK)
#define IPV6_TCLASS_SHIFT 20

#define HASH_SIZE  32

#define HASH(addr) ((__force u32)((addr)->s6_addr32[0] ^ (addr)->s6_addr32[1] ^ \
		     (addr)->s6_addr32[2] ^ (addr)->s6_addr32[3]) & \
		    (HASH_SIZE - 1))

static void ip6_tnl_dev_init(struct net_device *dev);
static void ip6_tnl_dev_setup(struct net_device *dev);

static int ip6_tnl_net_id __read_mostly;
struct ip6_tnl_net {
	/* the IPv6 tunnel fallback device */
	struct net_device *fb_tnl_dev;
	/* lists for storing tunnels in use */
	struct ip6_tnl *tnls_r_l[HASH_SIZE];
	struct ip6_tnl *tnls_wc[1];
	struct ip6_tnl **tnls[2];
};

/*
 * Locking : hash tables are protected by RCU and a spinlock
 */
static DEFINE_SPINLOCK(ip6_tnl_lock);

static inline struct dst_entry *ip6_tnl_dst_check(struct ip6_tnl *t)
{
	struct dst_entry *dst = t->dst_cache;

	if (dst && dst->obsolete &&
	    dst->ops->check(dst, t->dst_cookie) == NULL) {
		t->dst_cache = NULL;
		dst_release(dst);
		return NULL;
	}

	return dst;
}

static inline void ip6_tnl_dst_reset(struct ip6_tnl *t)
{
	dst_release(t->dst_cache);
	t->dst_cache = NULL;
}

static inline void ip6_tnl_dst_store(struct ip6_tnl *t, struct dst_entry *dst)
{
	struct rt6_info *rt = (struct rt6_info *) dst;
	t->dst_cookie = rt->rt6i_node ? rt->rt6i_node->fn_sernum : 0;
	dst_release(t->dst_cache);
	t->dst_cache = dst;
}

/**
 * ip6_tnl_lookup - fetch tunnel matching the end-point addresses
 *   @remote: the address of the tunnel exit-point
 *   @local: the address of the tunnel entry-point
 *
 * Return:
 *   tunnel matching given end-points if found,
 *   else fallback tunnel if its device is up,
 *   else %NULL
 **/

#define for_each_ip6_tunnel_rcu(start) \
	for (t = rcu_dereference(start); t; t = rcu_dereference(t->next))

static struct ip6_tnl *
ip6_tnl_lookup(struct net *net, struct in6_addr *remote, struct in6_addr *local)
{
	unsigned h0 = HASH(remote);
	unsigned h1 = HASH(local);
	struct ip6_tnl *t;
	struct ip6_tnl_net *ip6n = net_generic(net, ip6_tnl_net_id);

	for_each_ip6_tunnel_rcu(ip6n->tnls_r_l[h0 ^ h1]) {
		if (ipv6_addr_equal(local, &t->parms.laddr) &&
		    ipv6_addr_equal(remote, &t->parms.raddr) &&
		    (t->dev->flags & IFF_UP))
			return t;
	}
	t = rcu_dereference(ip6n->tnls_wc[0]);
	if (t && (t->dev->flags & IFF_UP))
		return t;

	return NULL;
}

/**
 * ip6_tnl_bucket - get head of list matching given tunnel parameters
 *   @p: parameters containing tunnel end-points
 *
 * Description:
 *   ip6_tnl_bucket() returns the head of the list matching the
 *   &struct in6_addr entries laddr and raddr in @p.
 *
 * Return: head of IPv6 tunnel list
 **/

static struct ip6_tnl **
ip6_tnl_bucket(struct ip6_tnl_net *ip6n, struct ip6_tnl_parm *p)
{
	struct in6_addr *remote = &p->raddr;
	struct in6_addr *local = &p->laddr;
	unsigned h = 0;
	int prio = 0;

	if (!ipv6_addr_any(remote) || !ipv6_addr_any(local)) {
		prio = 1;
		h = HASH(remote) ^ HASH(local);
	}
	return &ip6n->tnls[prio][h];
}

/**
 * ip6_tnl_link - add tunnel to hash table
 *   @t: tunnel to be added
 **/

static void
ip6_tnl_link(struct ip6_tnl_net *ip6n, struct ip6_tnl *t)
{
	struct ip6_tnl **tp = ip6_tnl_bucket(ip6n, &t->parms);

	spin_lock_bh(&ip6_tnl_lock);
	t->next = *tp;
	rcu_assign_pointer(*tp, t);
	spin_unlock_bh(&ip6_tnl_lock);
}

/**
 * ip6_tnl_unlink - remove tunnel from hash table
 *   @t: tunnel to be removed
 **/

static void
ip6_tnl_unlink(struct ip6_tnl_net *ip6n, struct ip6_tnl *t)
{
	struct ip6_tnl **tp;

	for (tp = ip6_tnl_bucket(ip6n, &t->parms); *tp; tp = &(*tp)->next) {
		if (t == *tp) {
			spin_lock_bh(&ip6_tnl_lock);
			*tp = t->next;
			spin_unlock_bh(&ip6_tnl_lock);
			break;
		}
	}
}

/**
 * ip6_tnl_create() - create a new tunnel
 *   @p: tunnel parameters
 *   @pt: pointer to new tunnel
 *
 * Description:
 *   Create tunnel matching given parameters.
 *
 * Return:
 *   created tunnel or NULL
 **/

static struct ip6_tnl *ip6_tnl_create(struct net *net, struct ip6_tnl_parm *p)
{
	struct net_device *dev;
	struct ip6_tnl *t;
	char name[IFNAMSIZ];
	int err;
	struct ip6_tnl_net *ip6n = net_generic(net, ip6_tnl_net_id);

	if (p->name[0])
		strlcpy(name, p->name, IFNAMSIZ);
	else
		sprintf(name, "ip6tnl%%d");

	dev = alloc_netdev(sizeof (*t), name, ip6_tnl_dev_setup);
	if (dev == NULL)
		goto failed;

	dev_net_set(dev, net);

	if (strchr(name, '%')) {
		if (dev_alloc_name(dev, name) < 0)
			goto failed_free;
	}

	t = netdev_priv(dev);
	t->parms = *p;
	ip6_tnl_dev_init(dev);

	if ((err = register_netdevice(dev)) < 0)
		goto failed_free;

	dev_hold(dev);
	ip6_tnl_link(ip6n, t);
	return t;

failed_free:
	free_netdev(dev);
failed:
	return NULL;
}

/**
 * ip6_tnl_locate - find or create tunnel matching given parameters
 *   @p: tunnel parameters
 *   @create: != 0 if allowed to create new tunnel if no match found
 *
 * Description:
 *   ip6_tnl_locate() first tries to locate an existing tunnel
 *   based on @parms. If this is unsuccessful, but @create is set a new
 *   tunnel device is created and registered for use.
 *
 * Return:
 *   matching tunnel or NULL
 **/

static struct ip6_tnl *ip6_tnl_locate(struct net *net,
		struct ip6_tnl_parm *p, int create)
{
	struct in6_addr *remote = &p->raddr;
	struct in6_addr *local = &p->laddr;
	struct ip6_tnl *t;
	struct ip6_tnl_net *ip6n = net_generic(net, ip6_tnl_net_id);

	for (t = *ip6_tnl_bucket(ip6n, p); t; t = t->next) {
		if (ipv6_addr_equal(local, &t->parms.laddr) &&
		    ipv6_addr_equal(remote, &t->parms.raddr))
			return t;
	}
	if (!create)
		return NULL;
	return ip6_tnl_create(net, p);
}

/**
 * ip6_tnl_dev_uninit - tunnel device uninitializer
 *   @dev: the device to be destroyed
 *
 * Description:
 *   ip6_tnl_dev_uninit() removes tunnel from its list
 **/

static void
ip6_tnl_dev_uninit(struct net_device *dev)
{
	struct ip6_tnl *t = netdev_priv(dev);
	struct net *net = dev_net(dev);
	struct ip6_tnl_net *ip6n = net_generic(net, ip6_tnl_net_id);

	if (dev == ip6n->fb_tnl_dev) {
		spin_lock_bh(&ip6_tnl_lock);
		ip6n->tnls_wc[0] = NULL;
		spin_unlock_bh(&ip6_tnl_lock);
	} else {
		ip6_tnl_unlink(ip6n, t);
	}
	ip6_tnl_dst_reset(t);
	dev_put(dev);
}

/**
 * parse_tvl_tnl_enc_lim - handle encapsulation limit option
 *   @skb: received socket buffer
 *
 * Return:
 *   0 if none was found,
 *   else index to encapsulation limit
 **/

static __u16
parse_tlv_tnl_enc_lim(struct sk_buff *skb, __u8 * raw)
{
	struct ipv6hdr *ipv6h = (struct ipv6hdr *) raw;
	__u8 nexthdr = ipv6h->nexthdr;
	__u16 off = sizeof (*ipv6h);

	while (ipv6_ext_hdr(nexthdr) && nexthdr != NEXTHDR_NONE) {
		__u16 optlen = 0;
		struct ipv6_opt_hdr *hdr;
		if (raw + off + sizeof (*hdr) > skb->data &&
		    !pskb_may_pull(skb, raw - skb->data + off + sizeof (*hdr)))
			break;

		hdr = (struct ipv6_opt_hdr *) (raw + off);
		if (nexthdr == NEXTHDR_FRAGMENT) {
			struct frag_hdr *frag_hdr = (struct frag_hdr *) hdr;
			if (frag_hdr->frag_off)
				break;
			optlen = 8;
		} else if (nexthdr == NEXTHDR_AUTH) {
			optlen = (hdr->hdrlen + 2) << 2;
		} else {
			optlen = ipv6_optlen(hdr);
		}
		if (nexthdr == NEXTHDR_DEST) {
			__u16 i = off + 2;
			while (1) {
				struct ipv6_tlv_tnl_enc_lim *tel;

				/* No more room for encapsulation limit */
				if (i + sizeof (*tel) > off + optlen)
					break;

				tel = (struct ipv6_tlv_tnl_enc_lim *) &raw[i];
				/* return index of option if found and valid */
				if (tel->type == IPV6_TLV_TNL_ENCAP_LIMIT &&
				    tel->length == 1)
					return i;
				/* else jump to next option */
				if (tel->type)
					i += tel->length + 2;
				else
					i++;
			}
		}
		nexthdr = hdr->nexthdr;
		off += optlen;
	}
	return 0;
}

/**
 * ip6_tnl_err - tunnel error handler
 *
 * Description:
 *   ip6_tnl_err() should handle errors in the tunnel according
 *   to the specifications in RFC 2473.
 **/

static int
ip6_tnl_err(struct sk_buff *skb, __u8 ipproto, struct inet6_skb_parm *opt,
	    u8 *type, u8 *code, int *msg, __u32 *info, int offset)
{
	struct ipv6hdr *ipv6h = (struct ipv6hdr *) skb->data;
	struct ip6_tnl *t;
	int rel_msg = 0;
	u8 rel_type = ICMPV6_DEST_UNREACH;
	u8 rel_code = ICMPV6_ADDR_UNREACH;
	__u32 rel_info = 0;
	__u16 len;
	int err = -ENOENT;

	/* If the packet doesn't contain the original IPv6 header we are
	   in trouble since we might need the source address for further
	   processing of the error. */

	rcu_read_lock();
	if ((t = ip6_tnl_lookup(dev_net(skb->dev), &ipv6h->daddr,
					&ipv6h->saddr)) == NULL)
		goto out;

	if (t->parms.proto != ipproto && t->parms.proto != 0)
		goto out;

	err = 0;

	switch (*type) {
		__u32 teli;
		struct ipv6_tlv_tnl_enc_lim *tel;
		__u32 mtu;
	case ICMPV6_DEST_UNREACH:
		if (net_ratelimit())
			printk(KERN_WARNING
			       "%s: Path to destination invalid "
			       "or inactive!\n", t->parms.name);
		rel_msg = 1;
		break;
	case ICMPV6_TIME_EXCEED:
		if ((*code) == ICMPV6_EXC_HOPLIMIT) {
			if (net_ratelimit())
				printk(KERN_WARNING
				       "%s: Too small hop limit or "
				       "routing loop in tunnel!\n",
				       t->parms.name);
			rel_msg = 1;
		}
		break;
	case ICMPV6_PARAMPROB:
		teli = 0;
		if ((*code) == ICMPV6_HDR_FIELD)
			teli = parse_tlv_tnl_enc_lim(skb, skb->data);

		if (teli && teli == *info - 2) {
			tel = (struct ipv6_tlv_tnl_enc_lim *) &skb->data[teli];
			if (tel->encap_limit == 0) {
				if (net_ratelimit())
					printk(KERN_WARNING
					       "%s: Too small encapsulation "
					       "limit or routing loop in "
					       "tunnel!\n", t->parms.name);
				rel_msg = 1;
			}
		} else if (net_ratelimit()) {
			printk(KERN_WARNING
			       "%s: Recipient unable to parse tunneled "
			       "packet!\n ", t->parms.name);
		}
		break;
	case ICMPV6_PKT_TOOBIG:
		mtu = *info - offset;
		if (mtu < IPV6_MIN_MTU)
			mtu = IPV6_MIN_MTU;
		t->dev->mtu = mtu;

		if ((len = sizeof (*ipv6h) + ntohs(ipv6h->payload_len)) > mtu) {
			rel_type = ICMPV6_PKT_TOOBIG;
			rel_code = 0;
			rel_info = mtu;
			rel_msg = 1;
		}
		break;
	}

	*type = rel_type;
	*code = rel_code;
	*info = rel_info;
	*msg = rel_msg;

out:
	rcu_read_unlock();
	return err;
}

static int
ip4ip6_err(struct sk_buff *skb, struct inet6_skb_parm *opt,
	   u8 type, u8 code, int offset, __be32 info)
{
	int rel_msg = 0;
	u8 rel_type = type;
	u8 rel_code = code;
	__u32 rel_info = ntohl(info);
	int err;
	struct sk_buff *skb2;
	struct iphdr *eiph;
	struct flowi fl;
	struct rtable *rt;

	err = ip6_tnl_err(skb, IPPROTO_IPIP, opt, &rel_type, &rel_code,
			  &rel_msg, &rel_info, offset);
	if (err < 0)
		return err;

	if (rel_msg == 0)
		return 0;

	switch (rel_type) {
	case ICMPV6_DEST_UNREACH:
		if (rel_code != ICMPV6_ADDR_UNREACH)
			return 0;
		rel_type = ICMP_DEST_UNREACH;
		rel_code = ICMP_HOST_UNREACH;
		break;
	case ICMPV6_PKT_TOOBIG:
		if (rel_code != 0)
			return 0;
		rel_type = ICMP_DEST_UNREACH;
		rel_code = ICMP_FRAG_NEEDED;
		break;
	default:
		return 0;
	}

	if (!pskb_may_pull(skb, offset + sizeof(struct iphdr)))
		return 0;

	skb2 = skb_clone(skb, GFP_ATOMIC);
	if (!skb2)
		return 0;

	skb_dst_drop(skb2);

	skb_pull(skb2, offset);
	skb_reset_network_header(skb2);
	eiph = ip_hdr(skb2);

	/* Try to guess incoming interface */
	memset(&fl, 0, sizeof(fl));
	fl.fl4_dst = eiph->saddr;
	fl.fl4_tos = RT_TOS(eiph->tos);
	fl.proto = IPPROTO_IPIP;
	if (ip_route_output_key(dev_net(skb->dev), &rt, &fl))
		goto out;

	skb2->dev = rt->dst.dev;

	/* route "incoming" packet */
	if (rt->rt_flags & RTCF_LOCAL) {
		ip_rt_put(rt);
		rt = NULL;
		fl.fl4_dst = eiph->daddr;
		fl.fl4_src = eiph->saddr;
		fl.fl4_tos = eiph->tos;
		if (ip_route_output_key(dev_net(skb->dev), &rt, &fl) ||
		    rt->dst.dev->type != ARPHRD_TUNNEL) {
			ip_rt_put(rt);
			goto out;
		}
		skb_dst_set(skb2, (struct dst_entry *)rt);
	} else {
		ip_rt_put(rt);
		if (ip_route_input(skb2, eiph->daddr, eiph->saddr, eiph->tos,
				   skb2->dev) ||
		    skb_dst(skb2)->dev->type != ARPHRD_TUNNEL)
			goto out;
	}

	/* change mtu on this route */
	if (rel_type == ICMP_DEST_UNREACH && rel_code == ICMP_FRAG_NEEDED) {
		if (rel_info > dst_mtu(skb_dst(skb2)))
			goto out;

		skb_dst(skb2)->ops->update_pmtu(skb_dst(skb2), rel_info);
	}

	icmp_send(skb2, rel_type, rel_code, htonl(rel_info));

out:
	kfree_skb(skb2);
	return 0;
}

static int
ip6ip6_err(struct sk_buff *skb, struct inet6_skb_parm *opt,
	   u8 type, u8 code, int offset, __be32 info)
{
	int rel_msg = 0;
	u8 rel_type = type;
	u8 rel_code = code;
	__u32 rel_info = ntohl(info);
	int err;

	err = ip6_tnl_err(skb, IPPROTO_IPV6, opt, &rel_type, &rel_code,
			  &rel_msg, &rel_info, offset);
	if (err < 0)
		return err;

	if (rel_msg && pskb_may_pull(skb, offset + sizeof(struct ipv6hdr))) {
		struct rt6_info *rt;
		struct sk_buff *skb2 = skb_clone(skb, GFP_ATOMIC);

		if (!skb2)
			return 0;

		skb_dst_drop(skb2);
		skb_pull(skb2, offset);
		skb_reset_network_header(skb2);

		/* Try to guess incoming interface */
		rt = rt6_lookup(dev_net(skb->dev), &ipv6_hdr(skb2)->saddr,
				NULL, 0, 0);

		if (rt && rt->rt6i_dev)
			skb2->dev = rt->rt6i_dev;

		icmpv6_send(skb2, rel_type, rel_code, rel_info);

		if (rt)
			dst_release(&rt->dst);

		kfree_skb(skb2);
	}

	return 0;
}

static void ip4ip6_dscp_ecn_decapsulate(struct ip6_tnl *t,
					struct ipv6hdr *ipv6h,
					struct sk_buff *skb)
{
	__u8 dsfield = ipv6_get_dsfield(ipv6h) & ~INET_ECN_MASK;

	if (t->parms.flags & IP6_TNL_F_RCV_DSCP_COPY)
		ipv4_change_dsfield(ip_hdr(skb), INET_ECN_MASK, dsfield);

	if (INET_ECN_is_ce(dsfield))
		IP_ECN_set_ce(ip_hdr(skb));
}

static void ip6ip6_dscp_ecn_decapsulate(struct ip6_tnl *t,
					struct ipv6hdr *ipv6h,
					struct sk_buff *skb)
{
	if (t->parms.flags & IP6_TNL_F_RCV_DSCP_COPY)
		ipv6_copy_dscp(ipv6_get_dsfield(ipv6h), ipv6_hdr(skb));

	if (INET_ECN_is_ce(ipv6_get_dsfield(ipv6h)))
		IP6_ECN_set_ce(ipv6_hdr(skb));
}

/* called with rcu_read_lock() */
static inline int ip6_tnl_rcv_ctl(struct ip6_tnl *t)
{
	struct ip6_tnl_parm *p = &t->parms;
	int ret = 0;
	struct net *net = dev_net(t->dev);

	if (p->flags & IP6_TNL_F_CAP_RCV) {
		struct net_device *ldev = NULL;

		if (p->link)
			ldev = dev_get_by_index_rcu(net, p->link);

		if ((ipv6_addr_is_multicast(&p->laddr) ||
		     likely(ipv6_chk_addr(net, &p->laddr, ldev, 0))) &&
		    likely(!ipv6_chk_addr(net, &p->raddr, NULL, 0)))
			ret = 1;

	}
	return ret;
}

/**
 * ip6_tnl_rcv - decapsulate IPv6 packet and retransmit it locally
 *   @skb: received socket buffer
 *   @protocol: ethernet protocol ID
 *   @dscp_ecn_decapsulate: the function to decapsulate DSCP code and ECN
 *
 * Return: 0
 **/

static int ip6_tnl_rcv(struct sk_buff *skb, __u16 protocol,
		       __u8 ipproto,
		       void (*dscp_ecn_decapsulate)(struct ip6_tnl *t,
						    struct ipv6hdr *ipv6h,
						    struct sk_buff *skb))
{
	struct ip6_tnl *t;
	struct ipv6hdr *ipv6h = ipv6_hdr(skb);

	rcu_read_lock();

	if ((t = ip6_tnl_lookup(dev_net(skb->dev), &ipv6h->saddr,
					&ipv6h->daddr)) != NULL) {
		if (t->parms.proto != ipproto && t->parms.proto != 0) {
			rcu_read_unlock();
			goto discard;
		}

		if (!xfrm6_policy_check(NULL, XFRM_POLICY_IN, skb)) {
			rcu_read_unlock();
			goto discard;
		}

		if (!ip6_tnl_rcv_ctl(t)) {
			t->dev->stats.rx_dropped++;
			rcu_read_unlock();
			goto discard;
		}
		secpath_reset(skb);
		skb->mac_header = skb->network_header;
		skb_reset_network_header(skb);
		skb->protocol = htons(protocol);
		skb->pkt_type = PACKET_HOST;
		memset(skb->cb, 0, sizeof(struct inet6_skb_parm));

		skb_tunnel_rx(skb, t->dev);

		dscp_ecn_decapsulate(t, ipv6h, skb);
		netif_rx(skb);
		rcu_read_unlock();
		return 0;
	}
	rcu_read_unlock();
	return 1;

discard:
	kfree_skb(skb);
	return 0;
}

static int ip4ip6_rcv(struct sk_buff *skb)
{
	return ip6_tnl_rcv(skb, ETH_P_IP, IPPROTO_IPIP,
			   ip4ip6_dscp_ecn_decapsulate);
}

static int ip6ip6_rcv(struct sk_buff *skb)
{
	return ip6_tnl_rcv(skb, ETH_P_IPV6, IPPROTO_IPV6,
			   ip6ip6_dscp_ecn_decapsulate);
}

struct ipv6_tel_txoption {
	struct ipv6_txoptions ops;
	__u8 dst_opt[8];
};

static void init_tel_txopt(struct ipv6_tel_txoption *opt, __u8 encap_limit)
{
	memset(opt, 0, sizeof(struct ipv6_tel_txoption));

	opt->dst_opt[2] = IPV6_TLV_TNL_ENCAP_LIMIT;
	opt->dst_opt[3] = 1;
	opt->dst_opt[4] = encap_limit;
	opt->dst_opt[5] = IPV6_TLV_PADN;
	opt->dst_opt[6] = 1;

	opt->ops.dst0opt = (struct ipv6_opt_hdr *) opt->dst_opt;
	opt->ops.opt_nflen = 8;
}

/**
 * ip6_tnl_addr_conflict - compare packet addresses to tunnel's own
 *   @t: the outgoing tunnel device
 *   @hdr: IPv6 header from the incoming packet
 *
 * Description:
 *   Avoid trivial tunneling loop by checking that tunnel exit-point
 *   doesn't match source of incoming packet.
 *
 * Return:
 *   1 if conflict,
 *   0 else
 **/

static inline int
ip6_tnl_addr_conflict(struct ip6_tnl *t, struct ipv6hdr *hdr)
{
	return ipv6_addr_equal(&t->parms.raddr, &hdr->saddr);
}

static inline int ip6_tnl_xmit_ctl(struct ip6_tnl *t)
{
	struct ip6_tnl_parm *p = &t->parms;
	int ret = 0;
	struct net *net = dev_net(t->dev);

	if (p->flags & IP6_TNL_F_CAP_XMIT) {
		struct net_device *ldev = NULL;

		rcu_read_lock();
		if (p->link)
			ldev = dev_get_by_index_rcu(net, p->link);

		if (unlikely(!ipv6_chk_addr(net, &p->laddr, ldev, 0)))
			printk(KERN_WARNING
			       "%s xmit: Local address not yet configured!\n",
			       p->name);
		else if (!ipv6_addr_is_multicast(&p->raddr) &&
			 unlikely(ipv6_chk_addr(net, &p->raddr, NULL, 0)))
			printk(KERN_WARNING
			       "%s xmit: Routing loop! "
			       "Remote address found on this node!\n",
			       p->name);
		else
			ret = 1;
		rcu_read_unlock();
	}
	return ret;
}
/**
 * ip6_tnl_xmit2 - encapsulate packet and send
 *   @skb: the outgoing socket buffer
 *   @dev: the outgoing tunnel device
 *   @dsfield: dscp code for outer header
 *   @fl: flow of tunneled packet
 *   @encap_limit: encapsulation limit
 *   @pmtu: Path MTU is stored if packet is too big
 *
 * Description:
 *   Build new header and do some sanity checks on the packet before sending
 *   it.
 *
 * Return:
 *   0 on success
 *   -1 fail
 *   %-EMSGSIZE message too big. return mtu in this case.
 **/

static int ip6_tnl_xmit2(struct sk_buff *skb,
			 struct net_device *dev,
			 __u8 dsfield,
			 struct flowi *fl,
			 int encap_limit,
			 __u32 *pmtu)
{
	struct net *net = dev_net(dev);
	struct ip6_tnl *t = netdev_priv(dev);
	struct net_device_stats *stats = &t->dev->stats;
	struct ipv6hdr *ipv6h = ipv6_hdr(skb);
	struct ipv6_tel_txoption opt;
	struct dst_entry *dst;
	struct net_device *tdev;
	int mtu;
	unsigned int max_headroom = sizeof(struct ipv6hdr);
	u8 proto;
	int err = -1;
	int pkt_len;

	if ((dst = ip6_tnl_dst_check(t)) != NULL)
		dst_hold(dst);
	else {
		dst = ip6_route_output(net, NULL, fl);

		if (dst->error || xfrm_lookup(net, &dst, fl, NULL, 0) < 0)
			goto tx_err_link_failure;
	}

	tdev = dst->dev;

	if (tdev == dev) {
		stats->collisions++;
		if (net_ratelimit())
			printk(KERN_WARNING
			       "%s: Local routing loop detected!\n",
			       t->parms.name);
		goto tx_err_dst_release;
	}
	mtu = dst_mtu(dst) - sizeof (*ipv6h);
	if (encap_limit >= 0) {
		max_headroom += 8;
		mtu -= 8;
	}
	if (mtu < IPV6_MIN_MTU)
		mtu = IPV6_MIN_MTU;
	if (skb_dst(skb))
		skb_dst(skb)->ops->update_pmtu(skb_dst(skb), mtu);
	if (skb->len > mtu) {
		*pmtu = mtu;
		err = -EMSGSIZE;
		goto tx_err_dst_release;
	}

	/*
	 * Okay, now see if we can stuff it in the buffer as-is.
	 */
	max_headroom += LL_RESERVED_SPACE(tdev);

	if (skb_headroom(skb) < max_headroom || skb_shared(skb) ||
	    (skb_cloned(skb) && !skb_clone_writable(skb, 0))) {
		struct sk_buff *new_skb;

		if (!(new_skb = skb_realloc_headroom(skb, max_headroom)))
			goto tx_err_dst_release;

		if (skb->sk)
			skb_set_owner_w(new_skb, skb->sk);
		kfree_skb(skb);
		skb = new_skb;
	}
	skb_dst_drop(skb);
	skb_dst_set(skb, dst_clone(dst));

	skb->transport_header = skb->network_header;

	proto = fl->proto;
	if (encap_limit >= 0) {
		init_tel_txopt(&opt, encap_limit);
		ipv6_push_nfrag_opts(skb, &opt.ops, &proto, NULL);
	}
	skb_push(skb, sizeof(struct ipv6hdr));
	skb_reset_network_header(skb);
	ipv6h = ipv6_hdr(skb);
	*(__be32*)ipv6h = fl->fl6_flowlabel | htonl(0x60000000);
	dsfield = INET_ECN_encapsulate(0, dsfield);
	ipv6_change_dsfield(ipv6h, ~INET_ECN_MASK, dsfield);
	ipv6h->hop_limit = t->parms.hop_limit;
	ipv6h->nexthdr = proto;
	ipv6_addr_copy(&ipv6h->saddr, &fl->fl6_src);
	ipv6_addr_copy(&ipv6h->daddr, &fl->fl6_dst);
	nf_reset(skb);
	pkt_len = skb->len;
	err = ip6_local_out(skb);

	if (net_xmit_eval(err) == 0) {
		stats->tx_bytes += pkt_len;
		stats->tx_packets++;
	} else {
		stats->tx_errors++;
		stats->tx_aborted_errors++;
	}
	ip6_tnl_dst_store(t, dst);
	return 0;
tx_err_link_failure:
	stats->tx_carrier_errors++;
	dst_link_failure(skb);
tx_err_dst_release:
	dst_release(dst);
	return err;
}

static inline int
ip4ip6_tnl_xmit(struct sk_buff *skb, struct net_device *dev)
{
	struct ip6_tnl *t = netdev_priv(dev);
	struct iphdr  *iph = ip_hdr(skb);
	int encap_limit = -1;
	struct flowi fl;
	__u8 dsfield;
	__u32 mtu;
	int err;

	if ((t->parms.proto != IPPROTO_IPIP && t->parms.proto != 0) ||
	    !ip6_tnl_xmit_ctl(t))
		return -1;

	if (!(t->parms.flags & IP6_TNL_F_IGN_ENCAP_LIMIT))
		encap_limit = t->parms.encap_limit;

	memcpy(&fl, &t->fl, sizeof (fl));
	fl.proto = IPPROTO_IPIP;

	dsfield = ipv4_get_dsfield(iph);

	if ((t->parms.flags & IP6_TNL_F_USE_ORIG_TCLASS))
		fl.fl6_flowlabel |= htonl((__u32)iph->tos << IPV6_TCLASS_SHIFT)
					  & IPV6_TCLASS_MASK;

	err = ip6_tnl_xmit2(skb, dev, dsfield, &fl, encap_limit, &mtu);
	if (err != 0) {
		if (err == -EMSGSIZE)
			icmp_send(skb, ICMP_DEST_UNREACH, ICMP_FRAG_NEEDED,
				  htonl(mtu));
		return -1;
	}

	return 0;
}

static inline int
ip6ip6_tnl_xmit(struct sk_buff *skb, struct net_device *dev)
{
	struct ip6_tnl *t = netdev_priv(dev);
	struct ipv6hdr *ipv6h = ipv6_hdr(skb);
	int encap_limit = -1;
	__u16 offset;
	struct flowi fl;
	__u8 dsfield;
	__u32 mtu;
	int err;

	if ((t->parms.proto != IPPROTO_IPV6 && t->parms.proto != 0) ||
	    !ip6_tnl_xmit_ctl(t) || ip6_tnl_addr_conflict(t, ipv6h))
		return -1;

	offset = parse_tlv_tnl_enc_lim(skb, skb_network_header(skb));
	if (offset > 0) {
		struct ipv6_tlv_tnl_enc_lim *tel;
		tel = (struct ipv6_tlv_tnl_enc_lim *)&skb_network_header(skb)[offset];
		if (tel->encap_limit == 0) {
			icmpv6_send(skb, ICMPV6_PARAMPROB,
				    ICMPV6_HDR_FIELD, offset + 2);
			return -1;
		}
		encap_limit = tel->encap_limit - 1;
	} else if (!(t->parms.flags & IP6_TNL_F_IGN_ENCAP_LIMIT))
		encap_limit = t->parms.encap_limit;

	memcpy(&fl, &t->fl, sizeof (fl));
	fl.proto = IPPROTO_IPV6;

	dsfield = ipv6_get_dsfield(ipv6h);
	if ((t->parms.flags & IP6_TNL_F_USE_ORIG_TCLASS))
		fl.fl6_flowlabel |= (*(__be32 *) ipv6h & IPV6_TCLASS_MASK);
	if ((t->parms.flags & IP6_TNL_F_USE_ORIG_FLOWLABEL))
		fl.fl6_flowlabel |= (*(__be32 *) ipv6h & IPV6_FLOWLABEL_MASK);

	err = ip6_tnl_xmit2(skb, dev, dsfield, &fl, encap_limit, &mtu);
	if (err != 0) {
		if (err == -EMSGSIZE)
			icmpv6_send(skb, ICMPV6_PKT_TOOBIG, 0, mtu);
		return -1;
	}

	return 0;
}

static netdev_tx_t
ip6_tnl_xmit(struct sk_buff *skb, struct net_device *dev)
{
	struct ip6_tnl *t = netdev_priv(dev);
	struct net_device_stats *stats = &t->dev->stats;
	int ret;

	switch (skb->protocol) {
	case htons(ETH_P_IP):
		ret = ip4ip6_tnl_xmit(skb, dev);
		break;
	case htons(ETH_P_IPV6):
		ret = ip6ip6_tnl_xmit(skb, dev);
		break;
	default:
		goto tx_err;
	}

	if (ret < 0)
		goto tx_err;

	return NETDEV_TX_OK;

tx_err:
	stats->tx_errors++;
	stats->tx_dropped++;
	kfree_skb(skb);
	return NETDEV_TX_OK;
}

static void ip6_tnl_set_cap(struct ip6_tnl *t)
{
	struct ip6_tnl_parm *p = &t->parms;
	int ltype = ipv6_addr_type(&p->laddr);
	int rtype = ipv6_addr_type(&p->raddr);

	p->flags &= ~(IP6_TNL_F_CAP_XMIT|IP6_TNL_F_CAP_RCV);

	if (ltype & (IPV6_ADDR_UNICAST|IPV6_ADDR_MULTICAST) &&
	    rtype & (IPV6_ADDR_UNICAST|IPV6_ADDR_MULTICAST) &&
	    !((ltype|rtype) & IPV6_ADDR_LOOPBACK) &&
	    (!((ltype|rtype) & IPV6_ADDR_LINKLOCAL) || p->link)) {
		if (ltype&IPV6_ADDR_UNICAST)
			p->flags |= IP6_TNL_F_CAP_XMIT;
		if (rtype&IPV6_ADDR_UNICAST)
			p->flags |= IP6_TNL_F_CAP_RCV;
	}
}

static void ip6_tnl_link_config(struct ip6_tnl *t)
{
	struct net_device *dev = t->dev;
	struct ip6_tnl_parm *p = &t->parms;
	struct flowi *fl = &t->fl;

	memcpy(dev->dev_addr, &p->laddr, sizeof(struct in6_addr));
	memcpy(dev->broadcast, &p->raddr, sizeof(struct in6_addr));

	/* Set up flowi template */
	ipv6_addr_copy(&fl->fl6_src, &p->laddr);
	ipv6_addr_copy(&fl->fl6_dst, &p->raddr);
	fl->oif = p->link;
	fl->fl6_flowlabel = 0;

	if (!(p->flags&IP6_TNL_F_USE_ORIG_TCLASS))
		fl->fl6_flowlabel |= IPV6_TCLASS_MASK & p->flowinfo;
	if (!(p->flags&IP6_TNL_F_USE_ORIG_FLOWLABEL))
		fl->fl6_flowlabel |= IPV6_FLOWLABEL_MASK & p->flowinfo;

	ip6_tnl_set_cap(t);

	if (p->flags&IP6_TNL_F_CAP_XMIT && p->flags&IP6_TNL_F_CAP_RCV)
		dev->flags |= IFF_POINTOPOINT;
	else
		dev->flags &= ~IFF_POINTOPOINT;

	dev->iflink = p->link;

	if (p->flags & IP6_TNL_F_CAP_XMIT) {
		int strict = (ipv6_addr_type(&p->raddr) &
			      (IPV6_ADDR_MULTICAST|IPV6_ADDR_LINKLOCAL));

		struct rt6_info *rt = rt6_lookup(dev_net(dev),
						 &p->raddr, &p->laddr,
						 p->link, strict);

		if (rt == NULL)
			return;

		if (rt->rt6i_dev) {
			dev->hard_header_len = rt->rt6i_dev->hard_header_len +
				sizeof (struct ipv6hdr);

			dev->mtu = rt->rt6i_dev->mtu - sizeof (struct ipv6hdr);

			if (dev->mtu < IPV6_MIN_MTU)
				dev->mtu = IPV6_MIN_MTU;
		}
		dst_release(&rt->dst);
	}
}

/**
 * ip6_tnl_change - update the tunnel parameters
 *   @t: tunnel to be changed
 *   @p: tunnel configuration parameters
 *
 * Description:
 *   ip6_tnl_change() updates the tunnel parameters
 **/

static int
ip6_tnl_change(struct ip6_tnl *t, struct ip6_tnl_parm *p)
{
	ipv6_addr_copy(&t->parms.laddr, &p->laddr);
	ipv6_addr_copy(&t->parms.raddr, &p->raddr);
	t->parms.flags = p->flags;
	t->parms.hop_limit = p->hop_limit;
	t->parms.encap_limit = p->encap_limit;
	t->parms.flowinfo = p->flowinfo;
	t->parms.link = p->link;
	t->parms.proto = p->proto;
	ip6_tnl_dst_reset(t);
	ip6_tnl_link_config(t);
	return 0;
}

/**
 * ip6_tnl_ioctl - configure ipv6 tunnels from userspace
 *   @dev: virtual device associated with tunnel
 *   @ifr: parameters passed from userspace
 *   @cmd: command to be performed
 *
 * Description:
 *   ip6_tnl_ioctl() is used for managing IPv6 tunnels
 *   from userspace.
 *
 *   The possible commands are the following:
 *     %SIOCGETTUNNEL: get tunnel parameters for device
 *     %SIOCADDTUNNEL: add tunnel matching given tunnel parameters
 *     %SIOCCHGTUNNEL: change tunnel parameters to those given
 *     %SIOCDELTUNNEL: delete tunnel
 *
 *   The fallback device "ip6tnl0", created during module
 *   initialization, can be used for creating other tunnel devices.
 *
 * Return:
 *   0 on success,
 *   %-EFAULT if unable to copy data to or from userspace,
 *   %-EPERM if current process hasn't %CAP_NET_ADMIN set
 *   %-EINVAL if passed tunnel parameters are invalid,
 *   %-EEXIST if changing a tunnel's parameters would cause a conflict
 *   %-ENODEV if attempting to change or delete a nonexisting device
 **/

static int
ip6_tnl_ioctl(struct net_device *dev, struct ifreq *ifr, int cmd)
{
	int err = 0;
	struct ip6_tnl_parm p;
	struct ip6_tnl *t = NULL;
	struct net *net = dev_net(dev);
	struct ip6_tnl_net *ip6n = net_generic(net, ip6_tnl_net_id);

	switch (cmd) {
	case SIOCGETTUNNEL:
		if (dev == ip6n->fb_tnl_dev) {
			if (copy_from_user(&p, ifr->ifr_ifru.ifru_data, sizeof (p))) {
				err = -EFAULT;
				break;
			}
			t = ip6_tnl_locate(net, &p, 0);
		}
		if (t == NULL)
			t = netdev_priv(dev);
		memcpy(&p, &t->parms, sizeof (p));
		if (copy_to_user(ifr->ifr_ifru.ifru_data, &p, sizeof (p))) {
			err = -EFAULT;
		}
		break;
	case SIOCADDTUNNEL:
	case SIOCCHGTUNNEL:
		err = -EPERM;
		if (!capable(CAP_NET_ADMIN))
			break;
		err = -EFAULT;
		if (copy_from_user(&p, ifr->ifr_ifru.ifru_data, sizeof (p)))
			break;
		err = -EINVAL;
		if (p.proto != IPPROTO_IPV6 && p.proto != IPPROTO_IPIP &&
		    p.proto != 0)
			break;
		t = ip6_tnl_locate(net, &p, cmd == SIOCADDTUNNEL);
		if (dev != ip6n->fb_tnl_dev && cmd == SIOCCHGTUNNEL) {
			if (t != NULL) {
				if (t->dev != dev) {
					err = -EEXIST;
					break;
				}
			} else
				t = netdev_priv(dev);

			ip6_tnl_unlink(ip6n, t);
			err = ip6_tnl_change(t, &p);
			ip6_tnl_link(ip6n, t);
			netdev_state_change(dev);
		}
		if (t) {
			err = 0;
			if (copy_to_user(ifr->ifr_ifru.ifru_data, &t->parms, sizeof (p)))
				err = -EFAULT;

		} else
			err = (cmd == SIOCADDTUNNEL ? -ENOBUFS : -ENOENT);
		break;
	case SIOCDELTUNNEL:
		err = -EPERM;
		if (!capable(CAP_NET_ADMIN))
			break;

		if (dev == ip6n->fb_tnl_dev) {
			err = -EFAULT;
			if (copy_from_user(&p, ifr->ifr_ifru.ifru_data, sizeof (p)))
				break;
			err = -ENOENT;
			if ((t = ip6_tnl_locate(net, &p, 0)) == NULL)
				break;
			err = -EPERM;
			if (t->dev == ip6n->fb_tnl_dev)
				break;
			dev = t->dev;
		}
		err = 0;
		unregister_netdevice(dev);
		break;
	default:
		err = -EINVAL;
	}
	return err;
}

/**
 * ip6_tnl_change_mtu - change mtu manually for tunnel device
 *   @dev: virtual device associated with tunnel
 *   @new_mtu: the new mtu
 *
 * Return:
 *   0 on success,
 *   %-EINVAL if mtu too small
 **/

static int
ip6_tnl_change_mtu(struct net_device *dev, int new_mtu)
{
	if (new_mtu < IPV6_MIN_MTU) {
		return -EINVAL;
	}
	dev->mtu = new_mtu;
	return 0;
}


static const struct net_device_ops ip6_tnl_netdev_ops = {
	.ndo_uninit = ip6_tnl_dev_uninit,
	.ndo_start_xmit = ip6_tnl_xmit,
	.ndo_do_ioctl = ip6_tnl_ioctl,
	.ndo_change_mtu = ip6_tnl_change_mtu,
};

/**
 * ip6_tnl_dev_setup - setup virtual tunnel device
 *   @dev: virtual device associated with tunnel
 *
 * Description:
 *   Initialize function pointers and device parameters
 **/

static void ip6_tnl_dev_setup(struct net_device *dev)
{
	dev->netdev_ops = &ip6_tnl_netdev_ops;
	dev->destructor = free_netdev;

	dev->type = ARPHRD_TUNNEL6;
	dev->hard_header_len = LL_MAX_HEADER + sizeof (struct ipv6hdr);
	dev->mtu = ETH_DATA_LEN - sizeof (struct ipv6hdr);
	dev->flags |= IFF_NOARP;
	dev->addr_len = sizeof(struct in6_addr);
	dev->features |= NETIF_F_NETNS_LOCAL;
}


/**
 * ip6_tnl_dev_init_gen - general initializer for all tunnel devices
 *   @dev: virtual device associated with tunnel
 **/

static inline void
ip6_tnl_dev_init_gen(struct net_device *dev)
{
	struct ip6_tnl *t = netdev_priv(dev);
	t->dev = dev;
	strcpy(t->parms.name, dev->name);
}

/**
 * ip6_tnl_dev_init - initializer for all non fallback tunnel devices
 *   @dev: virtual device associated with tunnel
 **/

static void ip6_tnl_dev_init(struct net_device *dev)
{
	struct ip6_tnl *t = netdev_priv(dev);
	ip6_tnl_dev_init_gen(dev);
	ip6_tnl_link_config(t);
}

/**
 * ip6_fb_tnl_dev_init - initializer for fallback tunnel device
 *   @dev: fallback device
 *
 * Return: 0
 **/

static void __net_init ip6_fb_tnl_dev_init(struct net_device *dev)
{
	struct ip6_tnl *t = netdev_priv(dev);
	struct net *net = dev_net(dev);
	struct ip6_tnl_net *ip6n = net_generic(net, ip6_tnl_net_id);

	ip6_tnl_dev_init_gen(dev);
	t->parms.proto = IPPROTO_IPV6;
	dev_hold(dev);
	ip6n->tnls_wc[0] = t;
}

static struct xfrm6_tunnel ip4ip6_handler = {
	.handler	= ip4ip6_rcv,
	.err_handler	= ip4ip6_err,
	.priority	=	1,
};

static struct xfrm6_tunnel ip6ip6_handler = {
	.handler	= ip6ip6_rcv,
	.err_handler	= ip6ip6_err,
	.priority	=	1,
};

static void __net_exit ip6_tnl_destroy_tunnels(struct ip6_tnl_net *ip6n)
{
	int h;
	struct ip6_tnl *t;
	LIST_HEAD(list);

	for (h = 0; h < HASH_SIZE; h++) {
		t = ip6n->tnls_r_l[h];
		while (t != NULL) {
			unregister_netdevice_queue(t->dev, &list);
			t = t->next;
		}
	}

	t = ip6n->tnls_wc[0];
	unregister_netdevice_queue(t->dev, &list);
	unregister_netdevice_many(&list);
}

static int __net_init ip6_tnl_init_net(struct net *net)
{
	struct ip6_tnl_net *ip6n = net_generic(net, ip6_tnl_net_id);
	int err;

	ip6n->tnls[0] = ip6n->tnls_wc;
	ip6n->tnls[1] = ip6n->tnls_r_l;

	err = -ENOMEM;
	ip6n->fb_tnl_dev = alloc_netdev(sizeof(struct ip6_tnl), "ip6tnl0",
				      ip6_tnl_dev_setup);

	if (!ip6n->fb_tnl_dev)
		goto err_alloc_dev;
	dev_net_set(ip6n->fb_tnl_dev, net);

	ip6_fb_tnl_dev_init(ip6n->fb_tnl_dev);

	err = register_netdev(ip6n->fb_tnl_dev);
	if (err < 0)
		goto err_register;
	return 0;

err_register:
	free_netdev(ip6n->fb_tnl_dev);
err_alloc_dev:
	return err;
}

static void __net_exit ip6_tnl_exit_net(struct net *net)
{
	struct ip6_tnl_net *ip6n = net_generic(net, ip6_tnl_net_id);

	rtnl_lock();
	ip6_tnl_destroy_tunnels(ip6n);
	rtnl_unlock();
}

static struct pernet_operations ip6_tnl_net_ops = {
	.init = ip6_tnl_init_net,
	.exit = ip6_tnl_exit_net,
	.id   = &ip6_tnl_net_id,
	.size = sizeof(struct ip6_tnl_net),
};

/**
 * ip6_tunnel_init - register protocol and reserve needed resources
 *
 * Return: 0 on success
 **/

static int __init ip6_tunnel_init(void)
{
	int  err;

	err = register_pernet_device(&ip6_tnl_net_ops);
	if (err < 0)
		goto out_pernet;

	err = xfrm6_tunnel_register(&ip4ip6_handler, AF_INET);
	if (err < 0) {
		printk(KERN_ERR "ip6_tunnel init: can't register ip4ip6\n");
		goto out_ip4ip6;
	}

	err = xfrm6_tunnel_register(&ip6ip6_handler, AF_INET6);
	if (err < 0) {
		printk(KERN_ERR "ip6_tunnel init: can't register ip6ip6\n");
		goto out_ip6ip6;
	}

	return 0;

out_ip6ip6:
	xfrm6_tunnel_deregister(&ip4ip6_handler, AF_INET);
out_ip4ip6:
	unregister_pernet_device(&ip6_tnl_net_ops);
out_pernet:
	return err;
}

/**
 * ip6_tunnel_cleanup - free resources and unregister protocol
 **/

static void __exit ip6_tunnel_cleanup(void)
{
	if (xfrm6_tunnel_deregister(&ip4ip6_handler, AF_INET))
		printk(KERN_INFO "ip6_tunnel close: can't deregister ip4ip6\n");

	if (xfrm6_tunnel_deregister(&ip6ip6_handler, AF_INET6))
		printk(KERN_INFO "ip6_tunnel close: can't deregister ip6ip6\n");

	unregister_pernet_device(&ip6_tnl_net_ops);
}

module_init(ip6_tunnel_init);
module_exit(ip6_tunnel_cleanup);
