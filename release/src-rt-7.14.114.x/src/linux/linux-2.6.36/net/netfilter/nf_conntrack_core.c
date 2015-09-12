/* Modified by Broadcom Corp. Portions Copyright (c) Broadcom Corp, 2012. */
/* Connection state tracking for netfilter.  This is separated from,
   but required by, the NAT layer; it can also be used by an iptables
   extension. */

/* (C) 1999-2001 Paul `Rusty' Russell
 * (C) 2002-2006 Netfilter Core Team <coreteam@netfilter.org>
 * (C) 2003,2004 USAGI/WIDE Project <http://www.linux-ipv6.org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/types.h>
#include <linux/netfilter.h>
#include <linux/module.h>
#include <linux/sched.h>
#include <linux/skbuff.h>
#include <linux/proc_fs.h>
#include <linux/vmalloc.h>
#include <linux/stddef.h>
#include <linux/slab.h>
#include <linux/random.h>
#include <linux/jhash.h>
#include <linux/err.h>
#include <linux/percpu.h>
#include <linux/moduleparam.h>
#include <linux/notifier.h>
#include <linux/kernel.h>
#include <linux/netdevice.h>
#include <linux/socket.h>
#include <linux/mm.h>
#include <linux/nsproxy.h>
#include <linux/rculist_nulls.h>

#include <net/netfilter/nf_conntrack.h>
#include <net/netfilter/nf_conntrack_l3proto.h>
#include <net/netfilter/nf_conntrack_l4proto.h>
#include <net/netfilter/nf_conntrack_expect.h>
#include <net/netfilter/nf_conntrack_helper.h>
#include <net/netfilter/nf_conntrack_core.h>
#include <net/netfilter/nf_conntrack_extend.h>
#include <net/netfilter/nf_conntrack_acct.h>
#include <net/netfilter/nf_conntrack_ecache.h>
#include <net/netfilter/nf_conntrack_zones.h>
#include <net/netfilter/nf_nat.h>
#include <net/netfilter/nf_nat_core.h>

#define NF_CONNTRACK_VERSION	"0.5.0"

#ifdef HNDCTF
#include <linux/if.h>
#include <linux/if_vlan.h>
#include <linux/if_pppox.h>
#include <linux/in.h>
#include <linux/ip.h>
#include <linux/tcp.h>

#ifdef CONFIG_IPV6
#include <linux/ipv6.h>
#include <net/ipv6.h>
#include <net/ip6_route.h>
#define IPVERSION_IS_4(ipver)		((ipver) == 4)
#else
#define IPVERSION_IS_4(ipver)		1
#endif /* CONFIG_IPV6 */

#include <net/ip.h>
#include <net/route.h>
#include <typedefs.h>
#include <osl.h>
#include <ctf/hndctf.h>
#include <ctf/ctf_cfg.h>

#define NFC_CTF_ENABLED	(1 << 31)
#else
#define BCMFASTPATH_HOST
#endif /* HNDCTF */

int (*nfnetlink_parse_nat_setup_hook)(struct nf_conn *ct,
				      enum nf_nat_manip_type manip,
				      const struct nlattr *attr) __read_mostly;
EXPORT_SYMBOL_GPL(nfnetlink_parse_nat_setup_hook);

DEFINE_SPINLOCK(nf_conntrack_lock);
EXPORT_SYMBOL_GPL(nf_conntrack_lock);

unsigned int nf_conntrack_htable_size __read_mostly;
EXPORT_SYMBOL_GPL(nf_conntrack_htable_size);

unsigned int nf_conntrack_max __read_mostly;
EXPORT_SYMBOL_GPL(nf_conntrack_max);

DEFINE_PER_CPU(struct nf_conn, nf_conntrack_untracked);
EXPORT_PER_CPU_SYMBOL(nf_conntrack_untracked);

#ifdef HNDCTF
/*
 *      Display an IP address in readable format.
 */
/* Returns the number of 1-bits in x */
static int
_popcounts(uint32 x)
{
    x = x - ((x >> 1) & 0x55555555);
    x = ((x >> 2) & 0x33333333) + (x & 0x33333333);
    x = (x + (x >> 4)) & 0x0F0F0F0F;
    x = (x + (x >> 16));
    return (x + (x >> 8)) & 0x0000003F;
}

bool
ip_conntrack_is_ipc_allowed(struct sk_buff *skb, u_int32_t hooknum)
{
	struct net_device *dev;

	if (!CTF_ENAB(kcih))
		return FALSE;

	if (hooknum == NF_INET_PRE_ROUTING || hooknum == NF_INET_POST_ROUTING) {
		dev = skb->dev;
		if (dev->priv_flags & IFF_802_1Q_VLAN)
			dev = vlan_dev_real_dev(dev);

		/* Add ipc entry if packet is received on ctf enabled interface
		 * and the packet is not a defrag'd one.
		 */
		if (ctf_isenabled(kcih, dev) && (skb->len <= dev->mtu)) {
			skb->nfcache |= NFC_CTF_ENABLED;
		}
	}

	/* Add the cache entries only if the device has registered and
	 * enabled ctf.
	 */
	if (skb->nfcache & NFC_CTF_ENABLED)
		return TRUE;

	return FALSE;
}

void
ip_conntrack_ipct_add(struct sk_buff *skb, u_int32_t hooknum,
                      struct nf_conn *ct, enum ip_conntrack_info ci,
                      struct nf_conntrack_tuple *manip)
{
	ctf_ipc_t ipc_entry;
	struct hh_cache *hh;
	struct ethhdr *eth;
	struct iphdr *iph;
	struct tcphdr *tcph;
	struct rtable *rt;
	struct nf_conn_help *help;
	enum ip_conntrack_dir dir;
	uint8 ipver, protocol;
#ifdef CONFIG_IPV6
	struct ipv6hdr *ip6h = NULL;
#endif /* CONFIG_IPV6 */
	uint32 nud_flags;

	if ((skb == NULL) || (ct == NULL))
		return;

	/* Check CTF enabled */
	if (!ip_conntrack_is_ipc_allowed(skb, hooknum))
		return;
	/* We only add cache entires for non-helper connections and at
	 * pre or post routing hooks.
	 */
	help = nfct_help(ct);
	if ((help && help->helper) || (ct->ctf_flags & CTF_FLAGS_EXCLUDED) ||
	    ((hooknum != NF_INET_PRE_ROUTING) && (hooknum != NF_INET_POST_ROUTING)))
		return;

	iph = ip_hdr(skb);
	ipver = iph->version;

	/* Support both IPv4 and IPv6 */
	if (ipver == 4) {
		tcph = ((struct tcphdr *)(((__u8 *)iph) + (iph->ihl << 2)));
		protocol = iph->protocol;
	}
#ifdef CONFIG_IPV6
	else if (ipver == 6) {
		ip6h = (struct ipv6hdr *)iph;
		tcph = (struct tcphdr *)ctf_ipc_lkup_l4proto(kcih, ip6h, &protocol);
		if (tcph == NULL)
			return;
	}
#endif /* CONFIG_IPV6 */
	else
		return;

	/* Only TCP and UDP are supported */
	if (protocol == IPPROTO_TCP) {
		/* Add ipc entries for connections in established state only */
		if ((ci != IP_CT_ESTABLISHED) && (ci != (IP_CT_ESTABLISHED+IP_CT_IS_REPLY)))
			return;

		if (ct->proto.tcp.state >= TCP_CONNTRACK_FIN_WAIT &&
			ct->proto.tcp.state <= TCP_CONNTRACK_TIME_WAIT)
			return;
	}
	else if (protocol != IPPROTO_UDP)
		return;

	dir = CTINFO2DIR(ci);
	if (ct->ctf_flags & (1 << dir))
		return;

	/* Do route lookup for alias address if we are doing DNAT in this
	 * direction.
	 */
	if (skb_dst(skb) == NULL) {
		/* Find the destination interface */
		if (IPVERSION_IS_4(ipver)) {
			u_int32_t daddr;

			if ((manip != NULL) && (HOOK2MANIP(hooknum) == IP_NAT_MANIP_DST))
				daddr = manip->dst.u3.ip;
			else
				daddr = iph->daddr;
			ip_route_input(skb, daddr, iph->saddr, iph->tos, skb->dev);
		}
#ifdef CONFIG_IPV6
		else
			ip6_route_input(skb);
#endif /* CONFIG_IPV6 */
	}

	/* Ensure the packet belongs to a forwarding connection and it is
	 * destined to an unicast address.
	 */
	rt = (struct rtable *)skb_dst(skb);

	nud_flags = NUD_PERMANENT | NUD_REACHABLE | NUD_STALE | NUD_DELAY | NUD_PROBE;
#if defined(CTF_PPPOE) || defined(CTF_PPTP) || defined(CTF_L2TP)
	if ((skb_dst(skb) != NULL) && (skb_dst(skb)->dev != NULL) &&
	    (skb_dst(skb)->dev->flags & IFF_POINTOPOINT))
		nud_flags |= NUD_NOARP;
#endif /* CTF_PPPOE | CTF_PPTP | CTF_L2TP */

	if ((rt == NULL) || (
#ifdef CONFIG_IPV6
			!IPVERSION_IS_4(ipver) ?
			 ((rt->dst.input != ip6_forward) ||
			  !(ipv6_addr_type(&ip6h->daddr) & IPV6_ADDR_UNICAST)) :
#endif /* CONFIG_IPV6 */
			 ((rt->dst.input != ip_forward) || (rt->rt_type != RTN_UNICAST))) ||
			(rt->dst.neighbour == NULL) ||
			((rt->dst.neighbour->nud_state & nud_flags) == 0))
		return;

	memset(&ipc_entry, 0, sizeof(ipc_entry));

	/* Init the neighboring sender address */
	memcpy(ipc_entry.sa.octet, eth_hdr(skb)->h_source, ETH_ALEN);

	/* If the packet is received on a bridge device then save
	 * the bridge cache entry pointer in the ip cache entry.
	 * This will be referenced in the data path to update the
	 * live counter of brc entry whenever a received packet
	 * matches corresponding ipc entry matches.
	 */
	if ((skb->dev != NULL) && ctf_isbridge(kcih, skb->dev)) {
		ipc_entry.brcp = ctf_brc_lkup(kcih, eth_hdr(skb)->h_source, FALSE);
	}

	hh = skb_dst(skb)->hh;
	if (hh != NULL) {
		eth = (struct ethhdr *)(((unsigned char *)hh->hh_data) + 2);
		memcpy(ipc_entry.dhost.octet, eth->h_dest, ETH_ALEN);
		memcpy(ipc_entry.shost.octet, eth->h_source, ETH_ALEN);
	} else {
		memcpy(ipc_entry.dhost.octet, rt->dst.neighbour->ha, ETH_ALEN);
		memcpy(ipc_entry.shost.octet, skb_dst(skb)->dev->dev_addr, ETH_ALEN);
	}

	/* Add ctf ipc entry for this direction */
	if (IPVERSION_IS_4(ipver)) {
		ipc_entry.tuple.sip[0] = iph->saddr;
		ipc_entry.tuple.dip[0] = iph->daddr;
#ifdef CONFIG_IPV6
	}	else {
		memcpy(ipc_entry.tuple.sip, &ip6h->saddr, sizeof(ipc_entry.tuple.sip));
		memcpy(ipc_entry.tuple.dip, &ip6h->daddr, sizeof(ipc_entry.tuple.dip));
#endif /* CONFIG_IPV6 */
	}
	ipc_entry.tuple.proto = protocol;
	ipc_entry.tuple.sp = tcph->source;
	ipc_entry.tuple.dp = tcph->dest;

	ipc_entry.next = NULL;

	/* For vlan interfaces fill the vlan id and the tag/untag actions */
	if (skb_dst(skb)->dev->priv_flags & IFF_802_1Q_VLAN) {
		ipc_entry.txif = (void *)vlan_dev_real_dev(skb_dst(skb)->dev);
		ipc_entry.vid = vlan_dev_vlan_id(skb_dst(skb)->dev);
		ipc_entry.action = ((vlan_dev_vlan_flags(skb_dst(skb)->dev) & 1) ?
		                    CTF_ACTION_TAG : CTF_ACTION_UNTAG);
	} else {
		ipc_entry.txif = skb_dst(skb)->dev;
		ipc_entry.action = CTF_ACTION_UNTAG;
	}

#if defined(CTF_PPTP) || defined(CTF_L2TP)
	if (((skb_dst(skb)->dev->flags & IFF_POINTOPOINT) || (skb->dev->flags & IFF_POINTOPOINT) )) {
		int pppunit = 0;
		struct net_device  *pppox_tx_dev=NULL;
		ctf_ppp_t ctfppp;
		
		/* For pppoe interfaces fill the session id and header add/del actions */
		if (skb_dst(skb)->dev->flags & IFF_POINTOPOINT) {
			/* Transmit interface and sid will be populated by pppoe module */
			ipc_entry.ppp_ifp = skb_dst(skb)->dev;
		} else if (skb->dev->flags & IFF_POINTOPOINT) {
			ipc_entry.ppp_ifp = skb->dev;
		} else{
			ipc_entry.ppp_ifp = NULL;
			ipc_entry.pppoe_sid = 0xffff;
		}

		if (ipc_entry.ppp_ifp){
			struct net_device  *pppox_tx_dev=NULL;
			ctf_ppp_t ctfppp;
			if (ppp_get_conn_pkt_info(ipc_entry.ppp_ifp,&ctfppp)){
				return;
			}
			else {
				if(ctfppp.psk.pppox_protocol == PX_PROTO_OE){
					goto PX_PROTO_PPPOE;
				}
#ifdef CTF_PPTP
				else if (ctfppp.psk.pppox_protocol == PX_PROTO_PPTP){
					struct pptp_opt *opt;
					if(ctfppp.psk.po == NULL) 
						return;
					opt=&ctfppp.psk.po->proto.pptp;
					if (skb_dst(skb)->dev->flags & IFF_POINTOPOINT){
						ipc_entry.action |= CTF_ACTION_PPTP_ADD;

						/* For PPTP, to get rt information*/
						if ((manip != NULL) && (HOOK2MANIP(hooknum) == IP_NAT_MANIP_SRC)){
							struct flowi fl = { .oif = 0,
							    .nl_u = { .ip4_u =
								      { .daddr = opt->dst_addr.sin_addr.s_addr,
									.saddr = opt->src_addr.sin_addr.s_addr,
									.tos = RT_TOS(0) } },
							    .proto = IPPROTO_GRE };
							if (ip_route_output_key(&init_net,&rt, &fl) ) {
								return;
							}
							if (rt==NULL ) 
								return;

							if (skb_dst(skb)->hh == NULL) {
								memcpy(ipc_entry.dhost.octet, rt->dst.neighbour->ha, ETH_ALEN);
							}

						}

						pppox_tx_dev = rt->dst.dev;
						memcpy(ipc_entry.shost.octet, rt->dst.dev->dev_addr, ETH_ALEN);
						ctf_pptp_cache(kcih,
							dst_metric(&rt->dst, RTAX_LOCK)&(1<<RTAX_MTU),
							dst_metric(&rt->dst, RTAX_HOPLIMIT));
						
					}
					else{
						ipc_entry.action |= CTF_ACTION_PPTP_DEL;
					}

					ipc_entry.pppox_opt = &ctfppp.psk.po->proto.pptp;
				}
#endif
#ifdef CTF_L2TP
				else if (ctfppp.psk.pppox_protocol == PX_PROTO_OL2TP){
					struct l2tp_opt *opt;
					if(ctfppp.psk.po == NULL) 
						return;
					opt=&ctfppp.psk.po->proto.l2tp;
					if (skb_dst(skb)->dev->flags & IFF_POINTOPOINT){
						ipc_entry.action |= CTF_ACTION_L2TP_ADD;

						/* For PPTP, to get rt information*/
						if ((manip != NULL) && (HOOK2MANIP(hooknum) == IP_NAT_MANIP_SRC)){
							struct flowi fl = { .oif = 0,
							    .nl_u = { .ip4_u =
								      { .daddr = opt->inet.daddr,
									.saddr = opt->inet.saddr,
									.tos = RT_TOS(0) } },
							    .proto = IPPROTO_UDP };
							if (ip_route_output_key(&init_net,&rt, &fl) ) {
								return;
							}
							if (rt==NULL ) 
								return;

							if (skb_dst(skb)->hh == NULL) {
								memcpy(ipc_entry.dhost.octet, rt->dst.neighbour->ha, ETH_ALEN);
							}
							

						}

						opt->inet.ttl = dst_metric(&rt->dst, RTAX_HOPLIMIT);
						pppox_tx_dev = rt->dst.dev;
						memcpy(ipc_entry.shost.octet, rt->dst.dev->dev_addr, ETH_ALEN);
						
					}
					else{
						ipc_entry.action |= CTF_ACTION_L2TP_DEL;
					}
					
					ipc_entry.pppox_opt = &ctfppp.psk.po->proto.l2tp;
				}
#endif
				else
					return;
				
				/* For vlan interfaces fill the vlan id and the tag/untag actions */
				if(pppox_tx_dev){
					if (pppox_tx_dev ->priv_flags & IFF_802_1Q_VLAN) {
						ipc_entry.txif = (void *)vlan_dev_real_dev(pppox_tx_dev);
						ipc_entry.vid = vlan_dev_vlan_id(pppox_tx_dev);
						ipc_entry.action |= ((vlan_dev_vlan_flags(pppox_tx_dev) & 1) ?
						                    CTF_ACTION_TAG : CTF_ACTION_UNTAG);
					} else {
						ipc_entry.txif = pppox_tx_dev;
						ipc_entry.action |= CTF_ACTION_UNTAG;
					}
				}
			}
		}	
	}

	if (ipc_entry.action & 
		(CTF_ACTION_L2TP_ADD | CTF_ACTION_L2TP_DEL | CTF_ACTION_PPTP_ADD | CTF_ACTION_PPTP_DEL)) {
		goto PX_PROTO_PPTP_L2TP;
	}
PX_PROTO_PPPOE:
#endif /* CTF_PPTP | CTF_L2TP */

#ifdef CTF_PPPOE
	/* For pppoe interfaces fill the session id and header add/del actions */
	ipc_entry.pppoe_sid = -1;
	if (skb_dst(skb)->dev->flags & IFF_POINTOPOINT) {
		/* Transmit interface and sid will be populated by pppoe module */
		ipc_entry.action |= CTF_ACTION_PPPOE_ADD;
		skb->ctf_pppoe_cb[0] = 2;
		ipc_entry.ppp_ifp = skb_dst(skb)->dev;
	} else if ((skb->dev->flags & IFF_POINTOPOINT) && (skb->ctf_pppoe_cb[0] == 1)) {
		ipc_entry.action |= CTF_ACTION_PPPOE_DEL;
		ipc_entry.pppoe_sid = *(uint16 *)&skb->ctf_pppoe_cb[2];
		ipc_entry.ppp_ifp = skb->dev;
	}
#endif

#if defined(CTF_PPTP) || defined(CTF_L2TP)
PX_PROTO_PPTP_L2TP:
#endif

	if (((ipc_entry.tuple.proto == IPPROTO_TCP) && (kcih->ipc_suspend & CTF_SUSPEND_TCP_MASK)) ||
	    ((ipc_entry.tuple.proto == IPPROTO_UDP) && (kcih->ipc_suspend & CTF_SUSPEND_UDP_MASK))) {
		/* The default action is suspend */
		ipc_entry.action |= CTF_ACTION_SUSPEND;
		ipc_entry.susp_cnt = ((ipc_entry.tuple.proto == IPPROTO_TCP) ? 
			_popcounts(kcih->ipc_suspend & CTF_SUSPEND_TCP_MASK) : 
			_popcounts(kcih->ipc_suspend & CTF_SUSPEND_UDP_MASK));
	}

	/* Copy the DSCP value. ECN bits must be cleared. */
	if (IPVERSION_IS_4(ipver))
		ipc_entry.tos = IPV4_TOS(iph);
#ifdef CONFIG_IPV6
	else
		ipc_entry.tos = IPV6_TRAFFIC_CLASS(ip6h);
#endif /* CONFIG_IPV6 */
	ipc_entry.tos &= IPV4_TOS_DSCP_MASK;
	if (ipc_entry.tos)
		ipc_entry.action |= CTF_ACTION_TOS;

#ifdef CONFIG_NF_CONNTRACK_MARK
	/* Initialize the mark for this connection */
	if (ct->mark != 0) {
		ipc_entry.mark.value = ct->mark;
		ipc_entry.action |= CTF_ACTION_MARK;
	}
#endif /* CONFIG_NF_CONNTRACK_MARK */

	/* Update the manip ip and port */
	if (manip != NULL) {
		if (HOOK2MANIP(hooknum) == IP_NAT_MANIP_SRC) {
			ipc_entry.nat.ip = manip->src.u3.ip;
			ipc_entry.nat.port = manip->src.u.tcp.port;
			ipc_entry.action |= CTF_ACTION_SNAT;
		} else {
			ipc_entry.nat.ip = manip->dst.u3.ip;
			ipc_entry.nat.port = manip->dst.u.tcp.port;
			ipc_entry.action |= CTF_ACTION_DNAT;
		}
	}

	/* Do bridge cache lookup to determine outgoing interface
	 * and any vlan tagging actions if needed.
	 */
	if (ctf_isbridge(kcih, ipc_entry.txif)) {
		ctf_brc_t *brcp;

		ctf_brc_acquire(kcih);

		if ((brcp = ctf_brc_lkup(kcih, ipc_entry.dhost.octet, TRUE)) != NULL) {
			ipc_entry.txbif = ipc_entry.txif;
			ipc_entry.action |= brcp->action;
			ipc_entry.txif = brcp->txifp;
			ipc_entry.vid = brcp->vid;
		}
                else {
                        ctf_brc_release(kcih);
                        return;
                }


		ctf_brc_release(kcih);
	}

#ifdef DEBUG
	if (IPVERSION_IS_4(ipver))
		printk("%s: Adding ipc entry for [%d]%u.%u.%u.%u:%u - %u.%u.%u.%u:%u\n", __FUNCTION__,
			ipc_entry.tuple.proto,
			NIPQUAD(ipc_entry.tuple.sip[0]), ntohs(ipc_entry.tuple.sp),
			NIPQUAD(ipc_entry.tuple.dip[0]), ntohs(ipc_entry.tuple.dp));
#ifdef CONFIG_IPV6
	else
		printk("\n%s: Adding ipc entry for [%d]\n"
			"%08x.%08x.%08x.%08x:%u => %08x.%08x.%08x.%08x:%u\n",
			__FUNCTION__, ipc_entry.tuple.proto,
			ntohl(ipc_entry.tuple.sip[0]), ntohl(ipc_entry.tuple.sip[1]),
			ntohl(ipc_entry.tuple.sip[2]), ntohl(ipc_entry.tuple.sip[3]),
			ntohs(ipc_entry.tuple.sp),
			ntohl(ipc_entry.tuple.dip[0]), ntohl(ipc_entry.tuple.dip[1]),
			ntohl(ipc_entry.tuple.dip[2]), ntohl(ipc_entry.tuple.dip[3]),
			ntohs(ipc_entry.tuple.dp));
#endif /* CONFIG_IPV6 */
	printk("sa %02x:%02x:%02x:%02x:%02x:%02x\n",
			ipc_entry.shost.octet[0], ipc_entry.shost.octet[1],
			ipc_entry.shost.octet[2], ipc_entry.shost.octet[3],
			ipc_entry.shost.octet[4], ipc_entry.shost.octet[5]);
	printk("da %02x:%02x:%02x:%02x:%02x:%02x\n",
			ipc_entry.dhost.octet[0], ipc_entry.dhost.octet[1],
			ipc_entry.dhost.octet[2], ipc_entry.dhost.octet[3],
			ipc_entry.dhost.octet[4], ipc_entry.dhost.octet[5]);
	printk("[%d] vid: %d action %x\n", hooknum, ipc_entry.vid, ipc_entry.action);
	if (manip != NULL)
		printk("manip_ip: %u.%u.%u.%u manip_port %u\n",
			NIPQUAD(ipc_entry.nat.ip), ntohs(ipc_entry.nat.port));
	printk("txif: %s\n", ((struct net_device *)ipc_entry.txif)->name);
#endif

	ctf_ipc_add(kcih, &ipc_entry, !IPVERSION_IS_4(ipver));

#ifdef CTF_PPPOE
	if (skb->ctf_pppoe_cb[0] == 2) {
		ctf_ipc_t *ipct;
		ipct = ctf_ipc_lkup(kcih, &ipc_entry, ipver == 6);
		*(uint32 *)&skb->ctf_pppoe_cb[4] = (uint32)ipct;
		if (ipct != NULL)
			ctf_ipc_release(kcih, ipct);
	}
#endif

	/* Update the attributes flag to indicate a CTF conn */
	ct->ctf_flags |= (CTF_FLAGS_CACHED | (1 << dir));
}

int
ip_conntrack_ipct_delete(struct nf_conn *ct, int ct_timeout)
{
	ctf_ipc_t *ipct;
	struct nf_conntrack_tuple *orig, *repl;
	ctf_ipc_t orig_ipct, repl_ipct;
	int ipaddr_sz;
	bool v6;

	if (!CTF_ENAB(kcih))
		return (0);

       if (!(ct->ctf_flags & CTF_FLAGS_CACHED)) {
                 return (0);
       }

	orig = &ct->tuplehash[IP_CT_DIR_ORIGINAL].tuple;

	if ((orig->dst.protonum != IPPROTO_TCP) && (orig->dst.protonum != IPPROTO_UDP))
		return (0);

	repl = &ct->tuplehash[IP_CT_DIR_REPLY].tuple;

#ifdef CONFIG_IPV6
	v6 = (orig->src.l3num == AF_INET6);
	ipaddr_sz = (v6) ? sizeof(struct in6_addr) : sizeof(struct in_addr);
#else
	v6 = FALSE;
	ipaddr_sz = sizeof(struct in_addr);
#endif /* CONFIG_IPV6 */

	memset(&orig_ipct, 0, sizeof(orig_ipct));
	memcpy(orig_ipct.tuple.sip, &orig->src.u3.ip, ipaddr_sz);
	memcpy(orig_ipct.tuple.dip, &orig->dst.u3.ip, ipaddr_sz);
	orig_ipct.tuple.proto = orig->dst.protonum;
	orig_ipct.tuple.sp = orig->src.u.tcp.port;
	orig_ipct.tuple.dp = orig->dst.u.tcp.port;

	memset(&repl_ipct, 0, sizeof(repl_ipct));
	memcpy(repl_ipct.tuple.sip, &repl->src.u3.ip, ipaddr_sz);
	memcpy(repl_ipct.tuple.dip, &repl->dst.u3.ip, ipaddr_sz);
	repl_ipct.tuple.proto = repl->dst.protonum;
	repl_ipct.tuple.sp = repl->src.u.tcp.port;
	repl_ipct.tuple.dp = repl->dst.u.tcp.port;

	/* If the refresh counter of ipc entry is non zero, it indicates
	 * that the packet transfer is active and we should not delete
	 * the conntrack entry.
	 */
	if (ct_timeout) {
		ipct = ctf_ipc_lkup(kcih, &orig_ipct, v6);

		/* Postpone the deletion of ct entry if there are frames
		 * flowing in this direction.
		 */
		if (ipct != NULL) {
#ifdef BCMFA
			ctf_live(kcih, ipct, v6);
#endif
			if (ipct->live > 0) {
				ipct->live = 0;
				ctf_ipc_release(kcih, ipct);
				ct->timeout.expires = jiffies + ct->expire_jiffies;
				add_timer(&ct->timeout);
				return (-1);
			}
			ctf_ipc_release(kcih, ipct);
		}

		ipct = ctf_ipc_lkup(kcih, &repl_ipct, v6);

		if (ipct != NULL) {
#ifdef BCMFA
			ctf_live(kcih, ipct, v6);
#endif
			if (ipct->live > 0) {
				ipct->live = 0;
				ctf_ipc_release(kcih, ipct);
				ct->timeout.expires = jiffies + ct->expire_jiffies;
				add_timer(&ct->timeout);
				return (-1);
			}
			ctf_ipc_release(kcih, ipct);
		}
	}

	/* If there are no packets over this connection for timeout period
	 * delete the entries.
	 */
	ctf_ipc_delete(kcih, &orig_ipct, v6);

	ctf_ipc_delete(kcih, &repl_ipct, v6);

#ifdef DEBUG
	printk("%s: Deleting the tuple %x %x %d %d %d\n",
	       __FUNCTION__, orig->src.u3.ip, orig->dst.u3.ip, orig->dst.protonum,
	       orig->src.u.tcp.port, orig->dst.u.tcp.port);
	printk("%s: Deleting the tuple %x %x %d %d %d\n",
	       __FUNCTION__, repl->dst.u3.ip, repl->src.u3.ip, repl->dst.protonum,
	       repl->dst.u.tcp.port, repl->src.u.tcp.port);
#endif

	return (0);
}

void
ip_conntrack_ipct_default_fwd_set(uint8 protocol, ctf_fwd_t fwd, uint8 userid)
{
	ctf_cfg_request_t req;
	ctf_fwd_t *f;
	uint8 *p;
	uint8 *uid;

	memset(&req, '\0', sizeof(req));
	req.command_id = CTFCFG_CMD_DEFAULT_FWD_SET;
	req.size = sizeof(ctf_fwd_t) + sizeof(uint8) + sizeof(uint8);
	f = (ctf_fwd_t *) req.arg;
	*f = fwd;
	p = (req.arg + sizeof(ctf_fwd_t));
	*p = protocol;
	uid = (req.arg + sizeof(ctf_fwd_t) + sizeof(uint8));
	*uid = userid;

	ctf_cfg_req_process(kcih, &req);
}
EXPORT_SYMBOL(ip_conntrack_ipct_default_fwd_set);


uint32
ip_conntrack_ipct_resume(struct sk_buff *skb, u_int32_t hooknum,
                      struct nf_conn *ct, enum ip_conntrack_info ci)
{
	struct iphdr *iph;
	struct tcphdr *tcph;
	struct nf_conn_help *help;
	uint8 ipver, protocol;
#ifdef CONFIG_IPV6
	struct ipv6hdr *ip6h = NULL;
#endif /* CONFIG_IPV6 */
	uint32 *ct_mark_p;

	ctf_cfg_request_t req;
	ctf_tuple_t tuple, *tp = NULL;

	if ((skb == NULL) || (ct == NULL))
		return 0;

	/* Check CTF enabled */
	if (!ip_conntrack_is_ipc_allowed(skb, hooknum))
		return 0;

	/* We only add cache entires for non-helper connections and at
	 * pre or post routing hooks.
	 */
	help = nfct_help(ct);
	if ((help && help->helper) || (ct->ctf_flags & CTF_FLAGS_EXCLUDED) ||
	    ((hooknum != NF_INET_PRE_ROUTING) && (hooknum != NF_INET_POST_ROUTING)))
		return 0;

	iph = ip_hdr(skb);
	ipver = iph->version;

	/* Support both IPv4 and IPv6 */
	if (ipver == 4) {
		tcph = ((struct tcphdr *)(((__u8 *)iph) + (iph->ihl << 2)));
		protocol = iph->protocol;
	}
#ifdef CONFIG_IPV6
	else if (ipver == 6) {
		ip6h = (struct ipv6hdr *)iph;
		tcph = (struct tcphdr *)ctf_ipc_lkup_l4proto(kcih, ip6h, &protocol);
		if (tcph == NULL)
			return 0;
	}
#endif /* CONFIG_IPV6 */
	else
		return 0;

	/* Only TCP and UDP are supported */
	if (protocol == IPPROTO_TCP) {
		/* Add ipc entries for connections in established state only */
		if ((ci != IP_CT_ESTABLISHED) && (ci != (IP_CT_ESTABLISHED+IP_CT_IS_REPLY)))
			return 0;

		if (ct->proto.tcp.state >= TCP_CONNTRACK_FIN_WAIT &&
			ct->proto.tcp.state <= TCP_CONNTRACK_TIME_WAIT)
			return 0;
	}
	else if (protocol != IPPROTO_UDP)
		return 0;

	memset(&tuple, '\0', sizeof(tuple));
	if (IPVERSION_IS_4(ipver)) {
		memcpy(&tuple.src_addr, &iph->saddr, sizeof(uint32));
		memcpy(&tuple.dst_addr, &iph->daddr, sizeof(uint32));
		tuple.family = AF_INET;
#ifdef CONFIG_IPV6
	}	else {
		memcpy(&tuple.src_addr, &ip6h->saddr, IPV6_ADDR_LEN);
		memcpy(&tuple.dst_addr, &ip6h->daddr, IPV6_ADDR_LEN);
		tuple.family = AF_INET6;
#endif /* CONFIG_IPV6 */
	}
	tuple.src_port = tcph->source;
	tuple.dst_port = tcph->dest;
	tuple.protocol = protocol;

#ifdef CONFIG_NF_CONNTRACK_MARK
	if (ct->mark != 0) {
		/* To Update Mark */
		memset(&req, '\0', sizeof(req));
		req.command_id = CTFCFG_CMD_UPD_MARK;
		req.size = sizeof(ctf_tuple_t) + sizeof(uint32);
		tp = (ctf_tuple_t *) req.arg;
		*tp = tuple;
		ct_mark_p = (uint32 *)(req.arg + sizeof(ctf_tuple_t));
		*ct_mark_p =  ct->mark;
		ctf_cfg_req_process(kcih, &req);

		/* To Update ipct txif */
		memset(&req, '\0', sizeof(req));
		req.command_id = CTFCFG_CMD_CHANGE_TXIF_TO_BR;
		req.size = sizeof(ctf_tuple_t);
		tp = (ctf_tuple_t *) req.arg;
		*tp = tuple;
		ctf_cfg_req_process(kcih, &req);
	}
#endif /* CONFIG_NF_CONNTRACK_MARK */

	/* To Resume */
	memset(&req, '\0', sizeof(req));
	req.command_id = CTFCFG_CMD_RESUME;
	req.size = sizeof(ctf_tuple_t);
	tp = (ctf_tuple_t *) req.arg;
	*tp = tuple;
	ctf_cfg_req_process(kcih, &req);
	return req.status;
}
EXPORT_SYMBOL(ip_conntrack_ipct_resume);
#endif /* HNDCTF */


static int nf_conntrack_hash_rnd_initted;
static unsigned int nf_conntrack_hash_rnd;

static u_int32_t BCMFASTPATH_HOST __hash_conntrack(const struct nf_conntrack_tuple *tuple,
				  u16 zone, unsigned int size, unsigned int rnd)
{
	unsigned int n;
	u_int32_t h;

	/* The direction must be ignored, so we hash everything up to the
	 * destination ports (which is a multiple of 4) and treat the last
	 * three bytes manually.
	 */
	n = (sizeof(tuple->src) + sizeof(tuple->dst.u3)) / sizeof(u32);
	h = jhash2((u32 *)tuple, n,
		   zone ^ rnd ^ (((__force __u16)tuple->dst.u.all << 16) |
				 tuple->dst.protonum));

	return ((u64)h * size) >> 32;
}

static inline u_int32_t hash_conntrack(const struct net *net, u16 zone,
				       const struct nf_conntrack_tuple *tuple)
{
	return __hash_conntrack(tuple, zone, net->ct.htable_size,
				nf_conntrack_hash_rnd);
}

bool
nf_ct_get_tuple(const struct sk_buff *skb,
		unsigned int nhoff,
		unsigned int dataoff,
		u_int16_t l3num,
		u_int8_t protonum,
		struct nf_conntrack_tuple *tuple,
		const struct nf_conntrack_l3proto *l3proto,
		const struct nf_conntrack_l4proto *l4proto)
{
	memset(tuple, 0, sizeof(*tuple));

	tuple->src.l3num = l3num;
	if (l3proto->pkt_to_tuple(skb, nhoff, tuple) == 0)
		return false;

	tuple->dst.protonum = protonum;
	tuple->dst.dir = IP_CT_DIR_ORIGINAL;

	return l4proto->pkt_to_tuple(skb, dataoff, tuple);
}
EXPORT_SYMBOL_GPL(nf_ct_get_tuple);

bool nf_ct_get_tuplepr(const struct sk_buff *skb, unsigned int nhoff,
		       u_int16_t l3num, struct nf_conntrack_tuple *tuple)
{
	struct nf_conntrack_l3proto *l3proto;
	struct nf_conntrack_l4proto *l4proto;
	unsigned int protoff;
	u_int8_t protonum;
	int ret;

	rcu_read_lock();

	l3proto = __nf_ct_l3proto_find(l3num);
	ret = l3proto->get_l4proto(skb, nhoff, &protoff, &protonum);
	if (ret != NF_ACCEPT) {
		rcu_read_unlock();
		return false;
	}

	l4proto = __nf_ct_l4proto_find(l3num, protonum);

	ret = nf_ct_get_tuple(skb, nhoff, protoff, l3num, protonum, tuple,
			      l3proto, l4proto);

	rcu_read_unlock();
	return ret;
}
EXPORT_SYMBOL_GPL(nf_ct_get_tuplepr);

bool
nf_ct_invert_tuple(struct nf_conntrack_tuple *inverse,
		   const struct nf_conntrack_tuple *orig,
		   const struct nf_conntrack_l3proto *l3proto,
		   const struct nf_conntrack_l4proto *l4proto)
{
	memset(inverse, 0, sizeof(*inverse));

	if (l3proto->invert_tuple(inverse, orig) == 0)
		return false;

        inverse->src.l3num = orig->src.l3num;
	inverse->dst.dir = !orig->dst.dir;

	inverse->dst.protonum = orig->dst.protonum;
	return l4proto->invert_tuple(inverse, orig);
}
EXPORT_SYMBOL_GPL(nf_ct_invert_tuple);

static void
clean_from_lists(struct nf_conn *ct)
{
	pr_debug("clean_from_lists(%p)\n", ct);
	hlist_nulls_del_rcu(&ct->tuplehash[IP_CT_DIR_ORIGINAL].hnnode);
	hlist_nulls_del_rcu(&ct->tuplehash[IP_CT_DIR_REPLY].hnnode);

	/* Destroy all pending expectations */
	nf_ct_remove_expectations(ct);
}

static void
destroy_conntrack(struct nf_conntrack *nfct)
{
	struct nf_conn *ct = (struct nf_conn *)nfct;
	struct net *net = nf_ct_net(ct);
	struct nf_conntrack_l4proto *l4proto;

	pr_debug("destroy_conntrack(%p)\n", ct);
	NF_CT_ASSERT(atomic_read(&nfct->use) == 0);
	NF_CT_ASSERT(!timer_pending(&ct->timeout));

#ifdef HNDCTF
	ip_conntrack_ipct_delete(ct, 0);
#endif /* HNDCTF*/
	/* To make sure we don't get any weird locking issues here:
	 * destroy_conntrack() MUST NOT be called with a write lock
	 * to nf_conntrack_lock!!! -HW */
	rcu_read_lock();
	l4proto = __nf_ct_l4proto_find(nf_ct_l3num(ct), nf_ct_protonum(ct));
	if (l4proto && l4proto->destroy)
		l4proto->destroy(ct);

	rcu_read_unlock();

	spin_lock_bh(&nf_conntrack_lock);
	/* Expectations will have been removed in clean_from_lists,
	 * except TFTP can create an expectation on the first packet,
	 * before connection is in the list, so we need to clean here,
	 * too. */
	nf_ct_remove_expectations(ct);

	/* We overload first tuple to link into unconfirmed list. */
	if (!nf_ct_is_confirmed(ct)) {
		BUG_ON(hlist_nulls_unhashed(&ct->tuplehash[IP_CT_DIR_ORIGINAL].hnnode));
		hlist_nulls_del_rcu(&ct->tuplehash[IP_CT_DIR_ORIGINAL].hnnode);
	}

	NF_CT_STAT_INC(net, delete);
	spin_unlock_bh(&nf_conntrack_lock);

	if (ct->master)
		nf_ct_put(ct->master);

	pr_debug("destroy_conntrack: returning ct=%p to slab\n", ct);
	nf_conntrack_free(ct);
}

void nf_ct_delete_from_lists(struct nf_conn *ct)
{
	struct net *net = nf_ct_net(ct);

	nf_ct_helper_destroy(ct);
	spin_lock_bh(&nf_conntrack_lock);
	/* Inside lock so preempt is disabled on module removal path.
	 * Otherwise we can get spurious warnings. */
	NF_CT_STAT_INC(net, delete_list);
	clean_from_lists(ct);
	spin_unlock_bh(&nf_conntrack_lock);
}
EXPORT_SYMBOL_GPL(nf_ct_delete_from_lists);

static void death_by_event(unsigned long ul_conntrack)
{
	struct nf_conn *ct = (void *)ul_conntrack;
	struct net *net = nf_ct_net(ct);

	if (nf_conntrack_event(IPCT_DESTROY, ct) < 0) {
		/* bad luck, let's retry again */
		ct->timeout.expires = jiffies +
			(random32() % net->ct.sysctl_events_retry_timeout);
		add_timer(&ct->timeout);
		return;
	}
	/* we've got the event delivered, now it's dying */
	set_bit(IPS_DYING_BIT, &ct->status);
	spin_lock(&nf_conntrack_lock);
	hlist_nulls_del(&ct->tuplehash[IP_CT_DIR_ORIGINAL].hnnode);
	spin_unlock(&nf_conntrack_lock);
	nf_ct_put(ct);
}

void nf_ct_insert_dying_list(struct nf_conn *ct)
{
	struct net *net = nf_ct_net(ct);

	/* add this conntrack to the dying list */
	spin_lock_bh(&nf_conntrack_lock);
	hlist_nulls_add_head(&ct->tuplehash[IP_CT_DIR_ORIGINAL].hnnode,
			     &net->ct.dying);
	spin_unlock_bh(&nf_conntrack_lock);
	/* set a new timer to retry event delivery */
	setup_timer(&ct->timeout, death_by_event, (unsigned long)ct);
	ct->timeout.expires = jiffies +
		(random32() % net->ct.sysctl_events_retry_timeout);
	add_timer(&ct->timeout);
}
EXPORT_SYMBOL_GPL(nf_ct_insert_dying_list);

static void death_by_timeout(unsigned long ul_conntrack)
{
	struct nf_conn *ct = (void *)ul_conntrack;
#ifdef HNDCTF
	/* If negative error is returned it means the entry hasn't
	 * timed out yet.
	 */
	if (ip_conntrack_ipct_delete(ct, jiffies >= ct->timeout.expires ? 1 : 0) != 0)
		return;
#endif /* HNDCTF */

	if (!test_bit(IPS_DYING_BIT, &ct->status) &&
	    unlikely(nf_conntrack_event(IPCT_DESTROY, ct) < 0)) {
		/* destroy event was not delivered */
		nf_ct_delete_from_lists(ct);
		nf_ct_insert_dying_list(ct);
		return;
	}
	set_bit(IPS_DYING_BIT, &ct->status);
	nf_ct_delete_from_lists(ct);
	nf_ct_put(ct);
}

/*
 * Warning :
 * - Caller must take a reference on returned object
 *   and recheck nf_ct_tuple_equal(tuple, &h->tuple)
 * OR
 * - Caller must lock nf_conntrack_lock before calling this function
 */
struct nf_conntrack_tuple_hash * BCMFASTPATH_HOST
__nf_conntrack_find(struct net *net, u16 zone,
		    const struct nf_conntrack_tuple *tuple)
{
	struct nf_conntrack_tuple_hash *h;
	struct hlist_nulls_node *n;
	unsigned int hash = hash_conntrack(net, zone, tuple);

	/* Disable BHs the entire time since we normally need to disable them
	 * at least once for the stats anyway.
	 */
	local_bh_disable();
begin:
	hlist_nulls_for_each_entry_rcu(h, n, &net->ct.hash[hash], hnnode) {
		if (nf_ct_tuple_equal(tuple, &h->tuple) &&
		    nf_ct_zone(nf_ct_tuplehash_to_ctrack(h)) == zone) {
			NF_CT_STAT_INC(net, found);
			local_bh_enable();
			return h;
		}
		NF_CT_STAT_INC(net, searched);
	}
	/*
	 * if the nulls value we got at the end of this lookup is
	 * not the expected one, we must restart lookup.
	 * We probably met an item that was moved to another chain.
	 */
	if (get_nulls_value(n) != hash) {
		NF_CT_STAT_INC(net, search_restart);
		goto begin;
	}
	local_bh_enable();

	return NULL;
}
EXPORT_SYMBOL_GPL(__nf_conntrack_find);

/* Find a connection corresponding to a tuple. */
struct nf_conntrack_tuple_hash * BCMFASTPATH_HOST
nf_conntrack_find_get(struct net *net, u16 zone,
		      const struct nf_conntrack_tuple *tuple)
{
	struct nf_conntrack_tuple_hash *h;
	struct nf_conn *ct;

	rcu_read_lock();
begin:
	h = __nf_conntrack_find(net, zone, tuple);
	if (h) {
		ct = nf_ct_tuplehash_to_ctrack(h);
		if (unlikely(nf_ct_is_dying(ct) ||
			     !atomic_inc_not_zero(&ct->ct_general.use)))
			h = NULL;
		else {
			if (unlikely(!nf_ct_tuple_equal(tuple, &h->tuple) ||
				     nf_ct_zone(ct) != zone)) {
				nf_ct_put(ct);
				goto begin;
			}
		}
	}
	rcu_read_unlock();

	return h;
}
EXPORT_SYMBOL_GPL(nf_conntrack_find_get);

static void __nf_conntrack_hash_insert(struct nf_conn *ct,
				       unsigned int hash,
				       unsigned int repl_hash)
{
	struct net *net = nf_ct_net(ct);

	hlist_nulls_add_head_rcu(&ct->tuplehash[IP_CT_DIR_ORIGINAL].hnnode,
			   &net->ct.hash[hash]);
	hlist_nulls_add_head_rcu(&ct->tuplehash[IP_CT_DIR_REPLY].hnnode,
			   &net->ct.hash[repl_hash]);
}

void nf_conntrack_hash_insert(struct nf_conn *ct)
{
	struct net *net = nf_ct_net(ct);
	unsigned int hash, repl_hash;
	u16 zone;

	zone = nf_ct_zone(ct);
	hash = hash_conntrack(net, zone, &ct->tuplehash[IP_CT_DIR_ORIGINAL].tuple);
	repl_hash = hash_conntrack(net, zone, &ct->tuplehash[IP_CT_DIR_REPLY].tuple);

	__nf_conntrack_hash_insert(ct, hash, repl_hash);
}
EXPORT_SYMBOL_GPL(nf_conntrack_hash_insert);

/* Confirm a connection given skb; places it in hash table */
int
__nf_conntrack_confirm(struct sk_buff *skb)
{
	unsigned int hash, repl_hash;
	struct nf_conntrack_tuple_hash *h;
	struct nf_conn *ct;
	struct nf_conn_help *help;
	struct hlist_nulls_node *n;
	enum ip_conntrack_info ctinfo;
	struct net *net;
	u16 zone;

	ct = nf_ct_get(skb, &ctinfo);
	net = nf_ct_net(ct);

	/* ipt_REJECT uses nf_conntrack_attach to attach related
	   ICMP/TCP RST packets in other direction.  Actual packet
	   which created connection will be IP_CT_NEW or for an
	   expected connection, IP_CT_RELATED. */
	if (CTINFO2DIR(ctinfo) != IP_CT_DIR_ORIGINAL)
		return NF_ACCEPT;

	zone = nf_ct_zone(ct);
	hash = hash_conntrack(net, zone, &ct->tuplehash[IP_CT_DIR_ORIGINAL].tuple);
	repl_hash = hash_conntrack(net, zone, &ct->tuplehash[IP_CT_DIR_REPLY].tuple);

	/* We're not in hash table, and we refuse to set up related
	   connections for unconfirmed conns.  But packet copies and
	   REJECT will give spurious warnings here. */
	/* NF_CT_ASSERT(atomic_read(&ct->ct_general.use) == 1); */

	/* No external references means noone else could have
	   confirmed us. */
	NF_CT_ASSERT(!nf_ct_is_confirmed(ct));
	pr_debug("Confirming conntrack %p\n", ct);

	spin_lock_bh(&nf_conntrack_lock);

	/* We have to check the DYING flag inside the lock to prevent
	   a race against nf_ct_get_next_corpse() possibly called from
	   user context, else we insert an already 'dead' hash, blocking
	   further use of that particular connection -JM */

	if (unlikely(nf_ct_is_dying(ct))) {
		spin_unlock_bh(&nf_conntrack_lock);
		return NF_ACCEPT;
	}

	/* See if there's one in the list already, including reverse:
	   NAT could have grabbed it without realizing, since we're
	   not in the hash.  If there is, we lost race. */
	hlist_nulls_for_each_entry(h, n, &net->ct.hash[hash], hnnode)
		if (nf_ct_tuple_equal(&ct->tuplehash[IP_CT_DIR_ORIGINAL].tuple,
				      &h->tuple) &&
		    zone == nf_ct_zone(nf_ct_tuplehash_to_ctrack(h)))
			goto out;
	hlist_nulls_for_each_entry(h, n, &net->ct.hash[repl_hash], hnnode)
		if (nf_ct_tuple_equal(&ct->tuplehash[IP_CT_DIR_REPLY].tuple,
				      &h->tuple) &&
		    zone == nf_ct_zone(nf_ct_tuplehash_to_ctrack(h)))
			goto out;

	/* Remove from unconfirmed list */
	hlist_nulls_del_rcu(&ct->tuplehash[IP_CT_DIR_ORIGINAL].hnnode);

	/* Timer relative to confirmation time, not original
	   setting time, otherwise we'd get timer wrap in
	   weird delay cases. */
	ct->timeout.expires += jiffies;
	add_timer(&ct->timeout);
	atomic_inc(&ct->ct_general.use);
	set_bit(IPS_CONFIRMED_BIT, &ct->status);

	/* Since the lookup is lockless, hash insertion must be done after
	 * starting the timer and setting the CONFIRMED bit. The RCU barriers
	 * guarantee that no other CPU can find the conntrack before the above
	 * stores are visible.
	 */
	__nf_conntrack_hash_insert(ct, hash, repl_hash);
	NF_CT_STAT_INC(net, insert);
	spin_unlock_bh(&nf_conntrack_lock);

	help = nfct_help(ct);
	if (help && help->helper)
		nf_conntrack_event_cache(IPCT_HELPER, ct);

	nf_conntrack_event_cache(master_ct(ct) ?
				 IPCT_RELATED : IPCT_NEW, ct);
	return NF_ACCEPT;

out:
	NF_CT_STAT_INC(net, insert_failed);
	spin_unlock_bh(&nf_conntrack_lock);
	return NF_DROP;
}
EXPORT_SYMBOL_GPL(__nf_conntrack_confirm);

/* Returns true if a connection correspondings to the tuple (required
   for NAT). */
int
nf_conntrack_tuple_taken(const struct nf_conntrack_tuple *tuple,
			 const struct nf_conn *ignored_conntrack)
{
	struct net *net = nf_ct_net(ignored_conntrack);
	struct nf_conntrack_tuple_hash *h;
	struct hlist_nulls_node *n;
	struct nf_conn *ct;
	u16 zone = nf_ct_zone(ignored_conntrack);
	unsigned int hash = hash_conntrack(net, zone, tuple);

	/* Disable BHs the entire time since we need to disable them at
	 * least once for the stats anyway.
	 */
	rcu_read_lock_bh();
	hlist_nulls_for_each_entry_rcu(h, n, &net->ct.hash[hash], hnnode) {
		ct = nf_ct_tuplehash_to_ctrack(h);
		if (ct != ignored_conntrack &&
		    nf_ct_tuple_equal(tuple, &h->tuple) &&
		    nf_ct_zone(ct) == zone) {
			NF_CT_STAT_INC(net, found);
			rcu_read_unlock_bh();
			return 1;
		}
		NF_CT_STAT_INC(net, searched);
	}
	rcu_read_unlock_bh();

	return 0;
}
EXPORT_SYMBOL_GPL(nf_conntrack_tuple_taken);

#define NF_CT_EVICTION_RANGE	8

/* There's a small race here where we may free a just-assured
   connection.  Too bad: we're in trouble anyway. */
static noinline int early_drop(struct net *net, unsigned int hash)
{
	/* Use oldest entry, which is roughly LRU */
	struct nf_conntrack_tuple_hash *h;
	struct nf_conn *ct = NULL, *tmp;
	struct hlist_nulls_node *n;
	unsigned int i, cnt = 0;
	int dropped = 0;

	rcu_read_lock();
	for (i = 0; i < net->ct.htable_size; i++) {
		hlist_nulls_for_each_entry_rcu(h, n, &net->ct.hash[hash],
					 hnnode) {
			tmp = nf_ct_tuplehash_to_ctrack(h);
			if (!test_bit(IPS_ASSURED_BIT, &tmp->status))
				ct = tmp;
			cnt++;
		}

		if (ct != NULL) {
			if (likely(!nf_ct_is_dying(ct) &&
				   atomic_inc_not_zero(&ct->ct_general.use)))
				break;
			else
				ct = NULL;
		}

		if (cnt >= NF_CT_EVICTION_RANGE)
			break;

		hash = (hash + 1) % net->ct.htable_size;
	}
	rcu_read_unlock();

	if (!ct)
		return dropped;

#ifdef HNDCTF
	ip_conntrack_ipct_delete(ct, 0);
#endif /* HNDCTF */

	if (del_timer(&ct->timeout)) {
		death_by_timeout((unsigned long)ct);
		dropped = 1;
		NF_CT_STAT_INC_ATOMIC(net, early_drop);
	}
	nf_ct_put(ct);
	return dropped;
}

struct nf_conn *nf_conntrack_alloc(struct net *net, u16 zone,
				   const struct nf_conntrack_tuple *orig,
				   const struct nf_conntrack_tuple *repl,
				   gfp_t gfp)
{
	struct nf_conn *ct;

	if (unlikely(!nf_conntrack_hash_rnd_initted)) {
		get_random_bytes(&nf_conntrack_hash_rnd,
				sizeof(nf_conntrack_hash_rnd));
		nf_conntrack_hash_rnd_initted = 1;
	}

	/* We don't want any race condition at early drop stage */
	atomic_inc(&net->ct.count);

	if (nf_conntrack_max &&
	    unlikely(atomic_read(&net->ct.count) > nf_conntrack_max)) {
		unsigned int hash = hash_conntrack(net, zone, orig);
		if (!early_drop(net, hash)) {
			atomic_dec(&net->ct.count);
			if (net_ratelimit())
				printk(KERN_WARNING
				       "nf_conntrack: table full, dropping"
				       " packet.\n");
			return ERR_PTR(-ENOMEM);
		}
	}

	/*
	 * Do not use kmem_cache_zalloc(), as this cache uses
	 * SLAB_DESTROY_BY_RCU.
	 */
	ct = kmem_cache_alloc(net->ct.nf_conntrack_cachep, gfp);
	if (ct == NULL) {
		pr_debug("nf_conntrack_alloc: Can't alloc conntrack.\n");
		atomic_dec(&net->ct.count);
		return ERR_PTR(-ENOMEM);
	}
	/*
	 * Let ct->tuplehash[IP_CT_DIR_ORIGINAL].hnnode.next
	 * and ct->tuplehash[IP_CT_DIR_REPLY].hnnode.next unchanged.
	 */
	memset(&ct->tuplehash[IP_CT_DIR_MAX], 0,
	       sizeof(*ct) - offsetof(struct nf_conn, tuplehash[IP_CT_DIR_MAX]));
	spin_lock_init(&ct->lock);
	ct->tuplehash[IP_CT_DIR_ORIGINAL].tuple = *orig;
	ct->tuplehash[IP_CT_DIR_ORIGINAL].hnnode.pprev = NULL;
	ct->tuplehash[IP_CT_DIR_REPLY].tuple = *repl;
	ct->tuplehash[IP_CT_DIR_REPLY].hnnode.pprev = NULL;
	/* Don't set timer yet: wait for confirmation */
	setup_timer(&ct->timeout, death_by_timeout, (unsigned long)ct);
	write_pnet(&ct->ct_net, net);
#ifdef CONFIG_NF_CONNTRACK_ZONES
	if (zone) {
		struct nf_conntrack_zone *nf_ct_zone;

		nf_ct_zone = nf_ct_ext_add(ct, NF_CT_EXT_ZONE, GFP_ATOMIC);
		if (!nf_ct_zone)
			goto out_free;
		nf_ct_zone->id = zone;
	}
#endif
	/*
	 * changes to lookup keys must be done before setting refcnt to 1
	 */
	smp_wmb();
	atomic_set(&ct->ct_general.use, 1);
	return ct;

#ifdef CONFIG_NF_CONNTRACK_ZONES
out_free:
	kmem_cache_free(net->ct.nf_conntrack_cachep, ct);
	return ERR_PTR(-ENOMEM);
#endif
}
EXPORT_SYMBOL_GPL(nf_conntrack_alloc);

void nf_conntrack_free(struct nf_conn *ct)
{
	struct net *net = nf_ct_net(ct);

	nf_ct_ext_destroy(ct);
	atomic_dec(&net->ct.count);
	nf_ct_ext_free(ct);
	kmem_cache_free(net->ct.nf_conntrack_cachep, ct);
}
EXPORT_SYMBOL_GPL(nf_conntrack_free);

/* Allocate a new conntrack: we return -ENOMEM if classification
   failed due to stress.  Otherwise it really is unclassifiable. */
static struct nf_conntrack_tuple_hash *
init_conntrack(struct net *net, struct nf_conn *tmpl,
	       const struct nf_conntrack_tuple *tuple,
	       struct nf_conntrack_l3proto *l3proto,
	       struct nf_conntrack_l4proto *l4proto,
	       struct sk_buff *skb,
	       unsigned int dataoff)
{
	struct nf_conn *ct;
	struct nf_conn_help *help;
	struct nf_conntrack_tuple repl_tuple;
	struct nf_conntrack_ecache *ecache;
	struct nf_conntrack_expect *exp;
	u16 zone = tmpl ? nf_ct_zone(tmpl) : NF_CT_DEFAULT_ZONE;

	if (!nf_ct_invert_tuple(&repl_tuple, tuple, l3proto, l4proto)) {
		pr_debug("Can't invert tuple.\n");
		return NULL;
	}

	ct = nf_conntrack_alloc(net, zone, tuple, &repl_tuple, GFP_ATOMIC);
	if (IS_ERR(ct)) {
		pr_debug("Can't allocate conntrack.\n");
		return (struct nf_conntrack_tuple_hash *)ct;
	}

	if (!l4proto->new(ct, skb, dataoff)) {
		nf_conntrack_free(ct);
		pr_debug("init conntrack: can't track with proto module\n");
		return NULL;
	}

	nf_ct_acct_ext_add(ct, GFP_ATOMIC);

	ecache = tmpl ? nf_ct_ecache_find(tmpl) : NULL;
	nf_ct_ecache_ext_add(ct, ecache ? ecache->ctmask : 0,
				 ecache ? ecache->expmask : 0,
			     GFP_ATOMIC);

	spin_lock_bh(&nf_conntrack_lock);
	exp = nf_ct_find_expectation(net, zone, tuple);
	if (exp) {
		pr_debug("conntrack: expectation arrives ct=%p exp=%p\n",
			 ct, exp);
		/* Welcome, Mr. Bond.  We've been expecting you... */
		__set_bit(IPS_EXPECTED_BIT, &ct->status);
		ct->master = exp->master;
		if (exp->helper) {
			help = nf_ct_helper_ext_add(ct, GFP_ATOMIC);
			if (help)
				rcu_assign_pointer(help->helper, exp->helper);
		}

#ifdef CONFIG_NF_CONNTRACK_MARK
		ct->mark = exp->master->mark;
#endif
#ifdef CONFIG_NF_CONNTRACK_SECMARK
		ct->secmark = exp->master->secmark;
#endif
		nf_conntrack_get(&ct->master->ct_general);
		NF_CT_STAT_INC(net, expect_new);
	} else {
		__nf_ct_try_assign_helper(ct, tmpl, GFP_ATOMIC);
		NF_CT_STAT_INC(net, new);
	}

	/* Overload tuple linked list to put us in unconfirmed list. */
	hlist_nulls_add_head_rcu(&ct->tuplehash[IP_CT_DIR_ORIGINAL].hnnode,
		       &net->ct.unconfirmed);

	spin_unlock_bh(&nf_conntrack_lock);

	if (exp) {
		if (exp->expectfn)
			exp->expectfn(ct, exp);
		nf_ct_expect_put(exp);
	}

	return &ct->tuplehash[IP_CT_DIR_ORIGINAL];
}

/* On success, returns conntrack ptr, sets skb->nfct and ctinfo */
static inline struct nf_conn *
resolve_normal_ct(struct net *net, struct nf_conn *tmpl,
		  struct sk_buff *skb,
		  unsigned int dataoff,
		  u_int16_t l3num,
		  u_int8_t protonum,
		  struct nf_conntrack_l3proto *l3proto,
		  struct nf_conntrack_l4proto *l4proto,
		  int *set_reply,
		  enum ip_conntrack_info *ctinfo)
{
	struct nf_conntrack_tuple tuple;
	struct nf_conntrack_tuple_hash *h;
	struct nf_conn *ct;
	u16 zone = tmpl ? nf_ct_zone(tmpl) : NF_CT_DEFAULT_ZONE;

	if (!nf_ct_get_tuple(skb, skb_network_offset(skb),
			     dataoff, l3num, protonum, &tuple, l3proto,
			     l4proto)) {
		pr_debug("resolve_normal_ct: Can't get tuple\n");
		return NULL;
	}

	/* look for tuple match */
	h = nf_conntrack_find_get(net, zone, &tuple);
	if (!h) {
		h = init_conntrack(net, tmpl, &tuple, l3proto, l4proto,
				   skb, dataoff);
		if (!h)
			return NULL;
		if (IS_ERR(h))
			return (void *)h;
	}
	ct = nf_ct_tuplehash_to_ctrack(h);

	/* It exists; we have (non-exclusive) reference. */
	if (NF_CT_DIRECTION(h) == IP_CT_DIR_REPLY) {
		*ctinfo = IP_CT_ESTABLISHED + IP_CT_IS_REPLY;
		/* Please set reply bit if this packet OK */
		*set_reply = 1;
	} else {
		/* Once we've had two way comms, always ESTABLISHED. */
		if (test_bit(IPS_SEEN_REPLY_BIT, &ct->status)) {
			pr_debug("nf_conntrack_in: normal packet for %p\n", ct);
			*ctinfo = IP_CT_ESTABLISHED;
		} else if (test_bit(IPS_EXPECTED_BIT, &ct->status)) {
			pr_debug("nf_conntrack_in: related packet for %p\n",
				 ct);
			*ctinfo = IP_CT_RELATED;
		} else {
			pr_debug("nf_conntrack_in: new packet for %p\n", ct);
			*ctinfo = IP_CT_NEW;
		}
		*set_reply = 0;
	}
	skb->nfct = &ct->ct_general;
	skb->nfctinfo = *ctinfo;
	return ct;
}

unsigned int BCMFASTPATH_HOST
nf_conntrack_in(struct net *net, u_int8_t pf, unsigned int hooknum,
		struct sk_buff *skb)
{
	struct nf_conn *ct, *tmpl = NULL;
	enum ip_conntrack_info ctinfo;
	struct nf_conntrack_l3proto *l3proto;
	struct nf_conntrack_l4proto *l4proto;
	unsigned int dataoff;
	u_int8_t protonum;
	int set_reply = 0;
	int ret;

	if (skb->nfct) {
		/* Previously seen (loopback or untracked)?  Ignore. */
		tmpl = (struct nf_conn *)skb->nfct;
		if (!nf_ct_is_template(tmpl)) {
			NF_CT_STAT_INC_ATOMIC(net, ignore);
			return NF_ACCEPT;
		}
		skb->nfct = NULL;
	}

	/* rcu_read_lock()ed by nf_hook_slow */
	l3proto = __nf_ct_l3proto_find(pf);
	ret = l3proto->get_l4proto(skb, skb_network_offset(skb),
				   &dataoff, &protonum);
	if (ret <= 0) {
		pr_debug("not prepared to track yet or error occured\n");
		NF_CT_STAT_INC_ATOMIC(net, error);
		NF_CT_STAT_INC_ATOMIC(net, invalid);
		ret = -ret;
		goto out;
	}

	l4proto = __nf_ct_l4proto_find(pf, protonum);

	/* It may be an special packet, error, unclean...
	 * inverse of the return code tells to the netfilter
	 * core what to do with the packet. */
	if (l4proto->error != NULL) {
		ret = l4proto->error(net, tmpl, skb, dataoff, &ctinfo,
				     pf, hooknum);
		if (ret <= 0) {
			NF_CT_STAT_INC_ATOMIC(net, error);
			NF_CT_STAT_INC_ATOMIC(net, invalid);
			ret = -ret;
			goto out;
		}
	}

	ct = resolve_normal_ct(net, tmpl, skb, dataoff, pf, protonum,
			       l3proto, l4proto, &set_reply, &ctinfo);
	if (!ct) {
		/* Not valid part of a connection */
		NF_CT_STAT_INC_ATOMIC(net, invalid);
		ret = NF_ACCEPT;
		goto out;
	}

	if (IS_ERR(ct)) {
		/* Too stressed to deal. */
		NF_CT_STAT_INC_ATOMIC(net, drop);
		ret = NF_DROP;
		goto out;
	}

	NF_CT_ASSERT(skb->nfct);

	ret = l4proto->packet(ct, skb, dataoff, ctinfo, pf, hooknum);
	if (ret <= 0) {
		/* Invalid: inverse of the return code tells
		 * the netfilter core what to do */
		pr_debug("nf_conntrack_in: Can't track with proto module\n");
		nf_conntrack_put(skb->nfct);
		skb->nfct = NULL;
		NF_CT_STAT_INC_ATOMIC(net, invalid);
		if (ret == -NF_DROP)
			NF_CT_STAT_INC_ATOMIC(net, drop);
		ret = -ret;
		goto out;
	}

	if (set_reply && !test_and_set_bit(IPS_SEEN_REPLY_BIT, &ct->status))
		nf_conntrack_event_cache(IPCT_REPLY, ct);
out:
	if (tmpl)
		nf_ct_put(tmpl);

	return ret;
}
EXPORT_SYMBOL_GPL(nf_conntrack_in);

bool nf_ct_invert_tuplepr(struct nf_conntrack_tuple *inverse,
			  const struct nf_conntrack_tuple *orig)
{
	bool ret;

	rcu_read_lock();
	ret = nf_ct_invert_tuple(inverse, orig,
				 __nf_ct_l3proto_find(orig->src.l3num),
				 __nf_ct_l4proto_find(orig->src.l3num,
						      orig->dst.protonum));
	rcu_read_unlock();
	return ret;
}
EXPORT_SYMBOL_GPL(nf_ct_invert_tuplepr);

/* Alter reply tuple (maybe alter helper).  This is for NAT, and is
   implicitly racy: see __nf_conntrack_confirm */
void nf_conntrack_alter_reply(struct nf_conn *ct,
			      const struct nf_conntrack_tuple *newreply)
{
	struct nf_conn_help *help = nfct_help(ct);

	/* Should be unconfirmed, so not in hash table yet */
	NF_CT_ASSERT(!nf_ct_is_confirmed(ct));

	pr_debug("Altering reply tuple of %p to ", ct);
	nf_ct_dump_tuple(newreply);

	ct->tuplehash[IP_CT_DIR_REPLY].tuple = *newreply;
	if (ct->master || (help && !hlist_empty(&help->expectations)))
		return;

	rcu_read_lock();
	__nf_ct_try_assign_helper(ct, NULL, GFP_ATOMIC);
	rcu_read_unlock();
}
EXPORT_SYMBOL_GPL(nf_conntrack_alter_reply);

/* Refresh conntrack for this many jiffies and do accounting if do_acct is 1 */
void __nf_ct_refresh_acct(struct nf_conn *ct,
			  enum ip_conntrack_info ctinfo,
			  const struct sk_buff *skb,
			  unsigned long extra_jiffies,
			  int do_acct)
{
	NF_CT_ASSERT(ct->timeout.data == (unsigned long)ct);
	NF_CT_ASSERT(skb);

	/* Only update if this is not a fixed timeout */
	if (test_bit(IPS_FIXED_TIMEOUT_BIT, &ct->status))
		goto acct;

	/* If not in hash table, timer will not be active yet */
	if (!nf_ct_is_confirmed(ct)) {
#ifdef HNDCTF
		ct->expire_jiffies = extra_jiffies;
#endif /* HNDCTF */
		ct->timeout.expires = extra_jiffies;
	} else {
		unsigned long newtime = jiffies + extra_jiffies;

		/* Only update the timeout if the new timeout is at least
		   HZ jiffies from the old timeout. Need del_timer for race
		   avoidance (may already be dying). */
		if (newtime - ct->timeout.expires >= HZ)
		{
#ifdef HNDCTF
			ct->expire_jiffies = extra_jiffies;
#endif /* HNDCTF */
			mod_timer_pending(&ct->timeout, newtime);
		}
	}

acct:
	if (do_acct) {
		struct nf_conn_counter *acct;

		acct = nf_conn_acct_find(ct);
		if (acct) {
			spin_lock_bh(&ct->lock);
			acct[CTINFO2DIR(ctinfo)].packets++;
			acct[CTINFO2DIR(ctinfo)].bytes += skb->len;
			spin_unlock_bh(&ct->lock);
		}
	}
}
EXPORT_SYMBOL_GPL(__nf_ct_refresh_acct);

bool __nf_ct_kill_acct(struct nf_conn *ct,
		       enum ip_conntrack_info ctinfo,
		       const struct sk_buff *skb,
		       int do_acct)
{
	if (do_acct) {
		struct nf_conn_counter *acct;

		acct = nf_conn_acct_find(ct);
		if (acct) {
			spin_lock_bh(&ct->lock);
			acct[CTINFO2DIR(ctinfo)].packets++;
			acct[CTINFO2DIR(ctinfo)].bytes +=
				skb->len - skb_network_offset(skb);
			spin_unlock_bh(&ct->lock);
		}
	}

	if (del_timer(&ct->timeout)) {
		ct->timeout.function((unsigned long)ct);
		return true;
	}
	return false;
}
EXPORT_SYMBOL_GPL(__nf_ct_kill_acct);

#ifdef CONFIG_NF_CONNTRACK_ZONES
static struct nf_ct_ext_type nf_ct_zone_extend __read_mostly = {
	.len	= sizeof(struct nf_conntrack_zone),
	.align	= __alignof__(struct nf_conntrack_zone),
	.id	= NF_CT_EXT_ZONE,
};
#endif

#if defined(CONFIG_NF_CT_NETLINK) || defined(CONFIG_NF_CT_NETLINK_MODULE)

#include <linux/netfilter/nfnetlink.h>
#include <linux/netfilter/nfnetlink_conntrack.h>
#include <linux/mutex.h>

/* Generic function for tcp/udp/sctp/dccp and alike. This needs to be
 * in ip_conntrack_core, since we don't want the protocols to autoload
 * or depend on ctnetlink */
int nf_ct_port_tuple_to_nlattr(struct sk_buff *skb,
			       const struct nf_conntrack_tuple *tuple)
{
	NLA_PUT_BE16(skb, CTA_PROTO_SRC_PORT, tuple->src.u.tcp.port);
	NLA_PUT_BE16(skb, CTA_PROTO_DST_PORT, tuple->dst.u.tcp.port);
	return 0;

nla_put_failure:
	return -1;
}
EXPORT_SYMBOL_GPL(nf_ct_port_tuple_to_nlattr);

const struct nla_policy nf_ct_port_nla_policy[CTA_PROTO_MAX+1] = {
	[CTA_PROTO_SRC_PORT]  = { .type = NLA_U16 },
	[CTA_PROTO_DST_PORT]  = { .type = NLA_U16 },
};
EXPORT_SYMBOL_GPL(nf_ct_port_nla_policy);

int nf_ct_port_nlattr_to_tuple(struct nlattr *tb[],
			       struct nf_conntrack_tuple *t)
{
	if (!tb[CTA_PROTO_SRC_PORT] || !tb[CTA_PROTO_DST_PORT])
		return -EINVAL;

	t->src.u.tcp.port = nla_get_be16(tb[CTA_PROTO_SRC_PORT]);
	t->dst.u.tcp.port = nla_get_be16(tb[CTA_PROTO_DST_PORT]);

	return 0;
}
EXPORT_SYMBOL_GPL(nf_ct_port_nlattr_to_tuple);

int nf_ct_port_nlattr_tuple_size(void)
{
	return nla_policy_len(nf_ct_port_nla_policy, CTA_PROTO_MAX + 1);
}
EXPORT_SYMBOL_GPL(nf_ct_port_nlattr_tuple_size);
#endif

/* Used by ipt_REJECT and ip6t_REJECT. */
static void nf_conntrack_attach(struct sk_buff *nskb, struct sk_buff *skb)
{
	struct nf_conn *ct;
	enum ip_conntrack_info ctinfo;

	/* This ICMP is in reverse direction to the packet which caused it */
	ct = nf_ct_get(skb, &ctinfo);
	if (CTINFO2DIR(ctinfo) == IP_CT_DIR_ORIGINAL)
		ctinfo = IP_CT_RELATED + IP_CT_IS_REPLY;
	else
		ctinfo = IP_CT_RELATED;

	/* Attach to new skbuff, and increment count */
	nskb->nfct = &ct->ct_general;
	nskb->nfctinfo = ctinfo;
	nf_conntrack_get(nskb->nfct);
}

/* Bring out ya dead! */
static struct nf_conn *
get_next_corpse(struct net *net, int (*iter)(struct nf_conn *i, void *data),
		void *data, unsigned int *bucket)
{
	struct nf_conntrack_tuple_hash *h;
	struct nf_conn *ct;
	struct hlist_nulls_node *n;

	spin_lock_bh(&nf_conntrack_lock);
	for (; *bucket < net->ct.htable_size; (*bucket)++) {
		hlist_nulls_for_each_entry(h, n, &net->ct.hash[*bucket], hnnode) {
			ct = nf_ct_tuplehash_to_ctrack(h);
			if (iter(ct, data))
				goto found;
		}
	}
	hlist_nulls_for_each_entry(h, n, &net->ct.unconfirmed, hnnode) {
		ct = nf_ct_tuplehash_to_ctrack(h);
		if (iter(ct, data))
			set_bit(IPS_DYING_BIT, &ct->status);
	}
	spin_unlock_bh(&nf_conntrack_lock);
	return NULL;
found:
	atomic_inc(&ct->ct_general.use);
	spin_unlock_bh(&nf_conntrack_lock);
	return ct;
}

void nf_ct_iterate_cleanup(struct net *net,
			   int (*iter)(struct nf_conn *i, void *data),
			   void *data)
{
	struct nf_conn *ct;
	unsigned int bucket = 0;

	while ((ct = get_next_corpse(net, iter, data, &bucket)) != NULL) {
#ifdef HNDCTF
		ip_conntrack_ipct_delete(ct, 0);
#endif /* HNDCTF */
		/* Time to push up daises... */
		if (del_timer(&ct->timeout))
			death_by_timeout((unsigned long)ct);
		/* ... else the timer will get him soon. */

		nf_ct_put(ct);
	}
}
EXPORT_SYMBOL_GPL(nf_ct_iterate_cleanup);

struct __nf_ct_flush_report {
	u32 pid;
	int report;
};

static int kill_report(struct nf_conn *i, void *data)
{
	struct __nf_ct_flush_report *fr = (struct __nf_ct_flush_report *)data;

	/* If we fail to deliver the event, death_by_timeout() will retry */
	if (nf_conntrack_event_report(IPCT_DESTROY, i,
				      fr->pid, fr->report) < 0)
		return 1;

	/* Avoid the delivery of the destroy event in death_by_timeout(). */
	set_bit(IPS_DYING_BIT, &i->status);
	return 1;
}

static int kill_all(struct nf_conn *i, void *data)
{
	return 1;
}

void nf_ct_free_hashtable(void *hash, int vmalloced, unsigned int size)
{
	if (vmalloced)
		vfree(hash);
	else
		free_pages((unsigned long)hash,
			   get_order(sizeof(struct hlist_head) * size));
}
EXPORT_SYMBOL_GPL(nf_ct_free_hashtable);

void nf_conntrack_flush_report(struct net *net, u32 pid, int report)
{
	struct __nf_ct_flush_report fr = {
		.pid 	= pid,
		.report = report,
	};
	nf_ct_iterate_cleanup(net, kill_report, &fr);
}
EXPORT_SYMBOL_GPL(nf_conntrack_flush_report);

static void nf_ct_release_dying_list(struct net *net)
{
	struct nf_conntrack_tuple_hash *h;
	struct nf_conn *ct;
	struct hlist_nulls_node *n;

	spin_lock_bh(&nf_conntrack_lock);
	hlist_nulls_for_each_entry(h, n, &net->ct.dying, hnnode) {
		ct = nf_ct_tuplehash_to_ctrack(h);
		/* never fails to remove them, no listeners at this point */
		nf_ct_kill(ct);
	}
	spin_unlock_bh(&nf_conntrack_lock);
}

static int untrack_refs(void)
{
	int cnt = 0, cpu;

	for_each_possible_cpu(cpu) {
		struct nf_conn *ct = &per_cpu(nf_conntrack_untracked, cpu);

		cnt += atomic_read(&ct->ct_general.use) - 1;
	}
	return cnt;
}

static void nf_conntrack_cleanup_init_net(void)
{
	while (untrack_refs() > 0)
		schedule();

	nf_conntrack_helper_fini();
	nf_conntrack_proto_fini();
#ifdef CONFIG_NF_CONNTRACK_ZONES
	nf_ct_extend_unregister(&nf_ct_zone_extend);
#endif
}

static void nf_conntrack_cleanup_net(struct net *net)
{
 i_see_dead_people:
	nf_ct_iterate_cleanup(net, kill_all, NULL);
	nf_ct_release_dying_list(net);
	if (atomic_read(&net->ct.count) != 0) {
		schedule();
		goto i_see_dead_people;
	}

	nf_ct_free_hashtable(net->ct.hash, net->ct.hash_vmalloc,
			     net->ct.htable_size);
	nf_conntrack_ecache_fini(net);
	nf_conntrack_acct_fini(net);
	nf_conntrack_expect_fini(net);
	kmem_cache_destroy(net->ct.nf_conntrack_cachep);
	kfree(net->ct.slabname);
	free_percpu(net->ct.stat);
}

/* Mishearing the voices in his head, our hero wonders how he's
   supposed to kill the mall. */
void nf_conntrack_cleanup(struct net *net)
{
	if (net_eq(net, &init_net))
		rcu_assign_pointer(ip_ct_attach, NULL);

	/* This makes sure all current packets have passed through
	   netfilter framework.  Roll on, two-stage module
	   delete... */
	synchronize_net();

	nf_conntrack_cleanup_net(net);

	if (net_eq(net, &init_net)) {
		rcu_assign_pointer(nf_ct_destroy, NULL);
		nf_conntrack_cleanup_init_net();
	}
}

void *nf_ct_alloc_hashtable(unsigned int *sizep, int *vmalloced, int nulls)
{
	struct hlist_nulls_head *hash;
	unsigned int nr_slots, i;
	size_t sz;

	*vmalloced = 0;

	BUILD_BUG_ON(sizeof(struct hlist_nulls_head) != sizeof(struct hlist_head));
	nr_slots = *sizep = roundup(*sizep, PAGE_SIZE / sizeof(struct hlist_nulls_head));
	sz = nr_slots * sizeof(struct hlist_nulls_head);
	hash = (void *)__get_free_pages(GFP_KERNEL | __GFP_NOWARN | __GFP_ZERO,
					get_order(sz));
	if (!hash) {
		*vmalloced = 1;
		printk(KERN_WARNING "nf_conntrack: falling back to vmalloc.\n");
		hash = __vmalloc(sz, GFP_KERNEL | __GFP_HIGHMEM | __GFP_ZERO,
				 PAGE_KERNEL);
	}

	if (hash && nulls)
		for (i = 0; i < nr_slots; i++)
			INIT_HLIST_NULLS_HEAD(&hash[i], i);

	return hash;
}
EXPORT_SYMBOL_GPL(nf_ct_alloc_hashtable);

int nf_conntrack_set_hashsize(const char *val, struct kernel_param *kp)
{
	int i, bucket, vmalloced, old_vmalloced;
	unsigned int hashsize, old_size;
	struct hlist_nulls_head *hash, *old_hash;
	struct nf_conntrack_tuple_hash *h;
	struct nf_conn *ct;

	if (current->nsproxy->net_ns != &init_net)
		return -EOPNOTSUPP;

	/* On boot, we can set this without any fancy locking. */
	if (!nf_conntrack_htable_size)
		return param_set_uint(val, kp);

	hashsize = simple_strtoul(val, NULL, 0);
	if (!hashsize)
		return -EINVAL;

	hash = nf_ct_alloc_hashtable(&hashsize, &vmalloced, 1);
	if (!hash)
		return -ENOMEM;

	/* Lookups in the old hash might happen in parallel, which means we
	 * might get false negatives during connection lookup. New connections
	 * created because of a false negative won't make it into the hash
	 * though since that required taking the lock.
	 */
	spin_lock_bh(&nf_conntrack_lock);
	for (i = 0; i < init_net.ct.htable_size; i++) {
		while (!hlist_nulls_empty(&init_net.ct.hash[i])) {
			h = hlist_nulls_entry(init_net.ct.hash[i].first,
					struct nf_conntrack_tuple_hash, hnnode);
			ct = nf_ct_tuplehash_to_ctrack(h);
			hlist_nulls_del_rcu(&h->hnnode);
			bucket = __hash_conntrack(&h->tuple, nf_ct_zone(ct),
						  hashsize,
						  nf_conntrack_hash_rnd);
			hlist_nulls_add_head_rcu(&h->hnnode, &hash[bucket]);
		}
	}
	old_size = init_net.ct.htable_size;
	old_vmalloced = init_net.ct.hash_vmalloc;
	old_hash = init_net.ct.hash;

	init_net.ct.htable_size = nf_conntrack_htable_size = hashsize;
	init_net.ct.hash_vmalloc = vmalloced;
	init_net.ct.hash = hash;
	spin_unlock_bh(&nf_conntrack_lock);

	nf_ct_free_hashtable(old_hash, old_vmalloced, old_size);
	return 0;
}
EXPORT_SYMBOL_GPL(nf_conntrack_set_hashsize);

module_param_call(hashsize, nf_conntrack_set_hashsize, param_get_uint,
		  &nf_conntrack_htable_size, 0600);

void nf_ct_untracked_status_or(unsigned long bits)
{
	int cpu;

	for_each_possible_cpu(cpu)
		per_cpu(nf_conntrack_untracked, cpu).status |= bits;
}
EXPORT_SYMBOL_GPL(nf_ct_untracked_status_or);

static int nf_conntrack_init_init_net(void)
{
	int max_factor = 8;
	int ret, cpu;

	/* Idea from tcp.c: use 1/16384 of memory.  On i386: 32MB
	 * machine has 512 buckets. >= 1GB machines have 16384 buckets. */
	if (!nf_conntrack_htable_size) {
		nf_conntrack_htable_size
			= (((totalram_pages << PAGE_SHIFT) / 16384)
			   / sizeof(struct hlist_head));
		if (totalram_pages > (1024 * 1024 * 1024 / PAGE_SIZE))
			nf_conntrack_htable_size = 16384;
		if (nf_conntrack_htable_size < 32)
			nf_conntrack_htable_size = 32;

		/* Use a max. factor of four by default to get the same max as
		 * with the old struct list_heads. When a table size is given
		 * we use the old value of 8 to avoid reducing the max.
		 * entries. */
		max_factor = 4;
	}
	nf_conntrack_max = max_factor * nf_conntrack_htable_size;

	printk(KERN_INFO "nf_conntrack version %s (%u buckets, %d max)\n",
	       NF_CONNTRACK_VERSION, nf_conntrack_htable_size,
	       nf_conntrack_max);

	ret = nf_conntrack_proto_init();
	if (ret < 0)
		goto err_proto;

	ret = nf_conntrack_helper_init();
	if (ret < 0)
		goto err_helper;

#ifdef CONFIG_NF_CONNTRACK_ZONES
	ret = nf_ct_extend_register(&nf_ct_zone_extend);
	if (ret < 0)
		goto err_extend;
#endif
	/* Set up fake conntrack: to never be deleted, not in any hashes */
	for_each_possible_cpu(cpu) {
		struct nf_conn *ct = &per_cpu(nf_conntrack_untracked, cpu);
		write_pnet(&ct->ct_net, &init_net);
		atomic_set(&ct->ct_general.use, 1);
	}
	/*  - and look it like as a confirmed connection */
	nf_ct_untracked_status_or(IPS_CONFIRMED | IPS_UNTRACKED);
	return 0;

#ifdef CONFIG_NF_CONNTRACK_ZONES
err_extend:
	nf_conntrack_helper_fini();
#endif
err_helper:
	nf_conntrack_proto_fini();
err_proto:
	return ret;
}

/*
 * We need to use special "null" values, not used in hash table
 */
#define UNCONFIRMED_NULLS_VAL	((1<<30)+0)
#define DYING_NULLS_VAL		((1<<30)+1)

static int nf_conntrack_init_net(struct net *net)
{
	int ret;

	atomic_set(&net->ct.count, 0);
	INIT_HLIST_NULLS_HEAD(&net->ct.unconfirmed, UNCONFIRMED_NULLS_VAL);
	INIT_HLIST_NULLS_HEAD(&net->ct.dying, DYING_NULLS_VAL);
	net->ct.stat = alloc_percpu(struct ip_conntrack_stat);
	if (!net->ct.stat) {
		ret = -ENOMEM;
		goto err_stat;
	}

	net->ct.slabname = kasprintf(GFP_KERNEL, "nf_conntrack_%p", net);
	if (!net->ct.slabname) {
		ret = -ENOMEM;
		goto err_slabname;
	}

	net->ct.nf_conntrack_cachep = kmem_cache_create(net->ct.slabname,
							sizeof(struct nf_conn), 0,
							SLAB_DESTROY_BY_RCU, NULL);
	if (!net->ct.nf_conntrack_cachep) {
		printk(KERN_ERR "Unable to create nf_conn slab cache\n");
		ret = -ENOMEM;
		goto err_cache;
	}

	net->ct.htable_size = nf_conntrack_htable_size;
	net->ct.hash = nf_ct_alloc_hashtable(&net->ct.htable_size,
					     &net->ct.hash_vmalloc, 1);
	if (!net->ct.hash) {
		ret = -ENOMEM;
		printk(KERN_ERR "Unable to create nf_conntrack_hash\n");
		goto err_hash;
	}
	ret = nf_conntrack_expect_init(net);
	if (ret < 0)
		goto err_expect;
	ret = nf_conntrack_acct_init(net);
	if (ret < 0)
		goto err_acct;
	ret = nf_conntrack_ecache_init(net);
	if (ret < 0)
		goto err_ecache;

	return 0;

err_ecache:
	nf_conntrack_acct_fini(net);
err_acct:
	nf_conntrack_expect_fini(net);
err_expect:
	nf_ct_free_hashtable(net->ct.hash, net->ct.hash_vmalloc,
			     net->ct.htable_size);
err_hash:
	kmem_cache_destroy(net->ct.nf_conntrack_cachep);
err_cache:
	kfree(net->ct.slabname);
err_slabname:
	free_percpu(net->ct.stat);
err_stat:
	return ret;
}

s16 (*nf_ct_nat_offset)(const struct nf_conn *ct,
			enum ip_conntrack_dir dir,
			u32 seq);
EXPORT_SYMBOL_GPL(nf_ct_nat_offset);

int nf_conntrack_init(struct net *net)
{
	int ret;

	if (net_eq(net, &init_net)) {
		ret = nf_conntrack_init_init_net();
		if (ret < 0)
			goto out_init_net;
	}
	ret = nf_conntrack_init_net(net);
	if (ret < 0)
		goto out_net;

	if (net_eq(net, &init_net)) {
		/* For use by REJECT target */
		rcu_assign_pointer(ip_ct_attach, nf_conntrack_attach);
		rcu_assign_pointer(nf_ct_destroy, destroy_conntrack);

		/* Howto get NAT offsets */
		rcu_assign_pointer(nf_ct_nat_offset, NULL);
	}
	return 0;

out_net:
	if (net_eq(net, &init_net))
		nf_conntrack_cleanup_init_net();
out_init_net:
	return ret;
}
