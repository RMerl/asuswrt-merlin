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
#include <net/ip.h>

#include <net/netfilter/nf_conntrack.h>
#include <net/netfilter/nf_conntrack_l3proto.h>
#include <net/netfilter/nf_conntrack_l4proto.h>
#include <net/netfilter/nf_conntrack_expect.h>
#include <net/netfilter/nf_conntrack_helper.h>
#include <net/netfilter/nf_conntrack_core.h>

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

#define NFC_CTF_ENABLED	(1 << 31)
#endif /* HNDCTF */

#define DEBUGP(format, args...)

DEFINE_RWLOCK(nf_conntrack_lock);
EXPORT_SYMBOL_GPL(nf_conntrack_lock);

/* nf_conntrack_standalone needs this */
atomic_t nf_conntrack_count = ATOMIC_INIT(0);
EXPORT_SYMBOL_GPL(nf_conntrack_count);

void (*nf_conntrack_destroyed)(struct nf_conn *conntrack);
EXPORT_SYMBOL_GPL(nf_conntrack_destroyed);

unsigned int nf_conntrack_htable_size __read_mostly;
EXPORT_SYMBOL_GPL(nf_conntrack_htable_size);

int nf_conntrack_max __read_mostly;
EXPORT_SYMBOL_GPL(nf_conntrack_max);

struct list_head *nf_conntrack_hash __read_mostly;
EXPORT_SYMBOL_GPL(nf_conntrack_hash);

struct nf_conn nf_conntrack_untracked __read_mostly;
EXPORT_SYMBOL_GPL(nf_conntrack_untracked);

unsigned int nf_ct_log_invalid __read_mostly;
LIST_HEAD(unconfirmed);
static int nf_conntrack_vmalloc __read_mostly;

static unsigned int nf_conntrack_next_id;

DEFINE_PER_CPU(struct ip_conntrack_stat, nf_conntrack_stat);
EXPORT_PER_CPU_SYMBOL(nf_conntrack_stat);

#ifdef HNDCTF
bool
ip_conntrack_is_ipc_allowed(struct sk_buff *skb, u_int32_t hooknum)
{
	struct net_device *dev;

	if (!CTF_ENAB(kcih))
		return FALSE;

	if (hooknum == NF_IP_PRE_ROUTING || hooknum == NF_IP_POST_ROUTING) {
		dev = skb->dev;
		if (dev->priv_flags & IFF_802_1Q_VLAN)
			dev = VLAN_DEV_INFO(dev)->real_dev;

		/* Add ipc entry if packet is received on ctf enabled interface
		 * and the packet is not a defrag'd one.
		 */
		if (ctf_isenabled(kcih, dev) && (skb->len <= dev->mtu))
			skb->nfcache |= NFC_CTF_ENABLED;
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
	    ((hooknum != NF_IP_PRE_ROUTING) && (hooknum != NF_IP_POST_ROUTING)))
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
	if (skb->dst == NULL) {
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
	rt = (struct rtable *)skb->dst;

	nud_flags = NUD_PERMANENT | NUD_REACHABLE | NUD_STALE | NUD_DELAY | NUD_PROBE;
#if defined(CTF_PPPOE) || defined(CTF_PPTP) || defined(CTF_L2TP)
	if ((skb->dst != NULL) && (skb->dst->dev != NULL) &&
	    (skb->dst->dev->flags & IFF_POINTOPOINT))
		nud_flags |= NUD_NOARP;
#endif /* CTF_PPPOE | CTF_PPTP | CTF_L2TP */

	if ((rt == NULL) || (
#ifdef CONFIG_IPV6
			!IPVERSION_IS_4(ipver) ?
			 ((rt->u.dst.input != ip6_forward) ||
			  !(ipv6_addr_type(&ip6h->daddr) & IPV6_ADDR_UNICAST)) :
#endif /* CONFIG_IPV6 */
			 ((rt->u.dst.input != ip_forward) || (rt->rt_type != RTN_UNICAST))) ||
			(rt->u.dst.neighbour == NULL) ||
			((rt->u.dst.neighbour->nud_state & nud_flags) == 0))
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
	if ((skb->dev != NULL) && ctf_isbridge(kcih, skb->dev))
		ipc_entry.brcp = ctf_brc_lkup(kcih, eth_hdr(skb)->h_source);

	hh = skb->dst->hh;
	if (hh != NULL) {
		eth = (struct ethhdr *)(((unsigned char *)hh->hh_data) + 2);
		memcpy(ipc_entry.dhost.octet, eth->h_dest, ETH_ALEN);
		memcpy(ipc_entry.shost.octet, eth->h_source, ETH_ALEN);
	} else {
		memcpy(ipc_entry.dhost.octet, rt->u.dst.neighbour->ha, ETH_ALEN);
		memcpy(ipc_entry.shost.octet, skb->dst->dev->dev_addr, ETH_ALEN);
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
	if (skb->dst->dev->priv_flags & IFF_802_1Q_VLAN) {
		ipc_entry.txif = (void *)(VLAN_DEV_INFO(skb->dst->dev)->real_dev);
		ipc_entry.vid = VLAN_DEV_INFO(skb->dst->dev)->vlan_id;
		ipc_entry.action = ((VLAN_DEV_INFO(skb->dst->dev)->flags & 1) ?
		                    CTF_ACTION_TAG : CTF_ACTION_UNTAG);
	} else {
		ipc_entry.txif = skb->dst->dev;
		ipc_entry.action = CTF_ACTION_UNTAG;
	}

#if defined(CTF_PPPOE) || defined(CTF_PPTP) || defined(CTF_L2TP)
	if ((skb->dst->dev->flags & IFF_POINTOPOINT) || (skb->dev->flags & IFF_POINTOPOINT) ){
		int pppunit = 0;
		struct net_device  *pppox_tx_dev=NULL;
		ctf_ppp_t ctfppp;	

		/* For pppoe interfaces fill the session id and header add/del actions */
		if (skb->dst->dev->flags & IFF_POINTOPOINT) {
			ipc_entry.ppp_ifp = skb->dst->dev;

		} else if (skb->dev->flags & IFF_POINTOPOINT)  {
			ipc_entry.ppp_ifp = skb->dev;
		}
		else{
			ipc_entry.ppp_ifp = NULL;
			ipc_entry.pppoe_sid = 0xffff;
		}

		if (ipc_entry.ppp_ifp){
			pppunit = simple_strtol(((struct net_device *)ipc_entry.ppp_ifp)->name + 3, NULL, 10);
			if (ppp_get_conn_pkt_info(pppunit,&ctfppp)){
#if 0
				if(ctfppp.psk.pppox_protocol == PX_PROTO_OL2TP){
					if (skb->dst->dev->flags & IFF_POINTOPOINT) {
						ipc_entry.pppoe_sid = 0xffff;
						//control path for outgoing data will change ipc_entry.pppoe_sid back to zero
					} else if (skb->dev->flags & IFF_POINTOPOINT )  {
						ipc_entry.action |= CTF_ACTION_L2TP_DEL;
					}					

					goto ipcp_add;
				}
				else
#endif
					return;
			}
			else {
				if(ctfppp.psk.pppox_protocol == PX_PROTO_OE){

					if (skb->dst->dev->flags & IFF_POINTOPOINT){
						ipc_entry.action |= CTF_ACTION_PPPOE_ADD;
						pppox_tx_dev = ctfppp.psk.po->pppoe_dev; 
						memcpy(ipc_entry.dhost.octet, ctfppp.psk.dhost.octet, ETH_ALEN);	
						memcpy(ipc_entry.shost.octet, ctfppp.psk.po->pppoe_dev->dev_addr, ETH_ALEN);
					}
					else{
						ipc_entry.action |= CTF_ACTION_PPPOE_DEL;
					}
					ipc_entry.pppoe_sid = ctfppp.pppox_id;
				}
#if 0
#ifdef CTF_PPTP				
				else if (ctfppp.psk.pppox_protocol == PX_PROTO_PPTP){
					struct pptp_opt *opt;
					if(ctfppp.psk.po == NULL) 
						return;
					opt=&ctfppp.psk.po->proto.pptp;
					if (skb->dst->dev->flags & IFF_POINTOPOINT){
						ipc_entry.action |= CTF_ACTION_PPTP_ADD;

						/* For PPTP, to get rt information*/
						if ((manip != NULL) && (HOOK2MANIP(hooknum) == IP_NAT_MANIP_SRC)){
							struct flowi fl = { .oif = 0,
							    .nl_u = { .ip4_u =
								      { .daddr = opt->dst_addr.sin_addr.s_addr,
									.saddr = opt->src_addr.sin_addr.s_addr,
									.tos = RT_TOS(0) } },
							    .proto = IPPROTO_GRE };
							if (ip_route_output_key(&rt, &fl) ) {
								return;
							}
							if (rt==NULL ) 
								return;

							if (skb->dst->hh == NULL) {
								memcpy(ipc_entry.dhost.octet, rt->u.dst.neighbour->ha, ETH_ALEN);
							}
							

						}
						
						pppox_tx_dev = rt->u.dst.dev;
						memcpy(ipc_entry.shost.octet, rt->u.dst.dev->dev_addr, ETH_ALEN);
						ipc_entry.pptp.rt_dst_mtrc_lock_fgoff = dst_metric(&rt->u.dst, RTAX_LOCK)&(1<<RTAX_MTU);//for ip header fragment offset
						ipc_entry.pptp.rt_dst_mtrc_hoplmt = dst_metric(&rt->u.dst, RTAX_HOPLIMIT);//for ip header ttl
					}
					else{
						ipc_entry.action |= CTF_ACTION_PPTP_DEL;
					}
					ipc_entry.pptp.pptpopt = &ctfppp.psk.po->proto.pptp;
				}
#endif /*CTF_PPTP*/
#endif
				else
					return;

ppp_tx_dev:			
			/* For vlan interfaces fill the vlan id and the tag/untag actions */
			if(pppox_tx_dev){
				if (pppox_tx_dev ->priv_flags & IFF_802_1Q_VLAN) {
					ipc_entry.txif = (void *)(VLAN_DEV_INFO(pppox_tx_dev )->real_dev);
					ipc_entry.vid = VLAN_DEV_INFO(pppox_tx_dev )->vlan_id;
					ipc_entry.action |= ((VLAN_DEV_INFO(pppox_tx_dev)->flags & 1) ?
				                   		CTF_ACTION_TAG : CTF_ACTION_UNTAG);
				} else {
					ipc_entry.txif = pppox_tx_dev;
					ipc_entry.action |= CTF_ACTION_UNTAG;
				}
			}
			
			}
		}		
	}
#endif /* CTF_PPPOE | CTF_PPTP | CTF_L2TP */
ipcp_add:
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

		brcp = ctf_brc_lkup(kcih, ipc_entry.dhost.octet);

		if (brcp == NULL)
			return;
		else {
			ipc_entry.action |= brcp->action;
			ipc_entry.txif = brcp->txifp;
			ipc_entry.vid = brcp->vid;
		}
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
	if (ipc_entry.ppp_ifp) printk("pppif: %s\n", ((struct net_device *)ipc_entry.ppp_ifp)->name);
#endif

	ctf_ipc_add(kcih, &ipc_entry, !IPVERSION_IS_4(ipver));

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
		if ((ipct != NULL) && (ipct->live > 0)) {
			ipct->live = 0;
			ct->timeout.expires = jiffies + ct->expire_jiffies;
			add_timer(&ct->timeout);
			return (-1);
		}

		ipct = ctf_ipc_lkup(kcih, &repl_ipct, v6);

		if ((ipct != NULL) && (ipct->live > 0)) {
			ipct->live = 0;
			ct->timeout.expires = jiffies + ct->expire_jiffies;
			add_timer(&ct->timeout);
			return (-1);
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
#ifdef CTF_L2TP
/* 
 * Sniffing outgoing l2tp data packets transmitted by user 
 * space daemon (l2tpd). If skb carried L2TP data packet then
 * function will look whether there is an entry in CTF's ip 
 * connection cache table which corresponds to the
 * encapsulated ip packet. If the entry found then the L2TP 
 * tunnel/session information will be associated with that
 * connection entry.
 */
void
ip_conntrack_l2tp_lookup(struct sk_buff *skb, u_int32_t hooknum,
                      struct nf_conn *ct, enum ip_conntrack_info ci)
{
	struct nf_conntrack_tuple *orig;
	enum ip_conntrack_dir dir;

	if (!CTF_ENAB(kcih))
		return;

	dir = CTINFO2DIR(ci);
	orig = &ct->tuplehash[IP_CT_DIR_ORIGINAL].tuple;

	if (!(hooknum == NF_IP_POST_ROUTING && 
		dir == IP_CT_DIR_ORIGINAL && 
		orig->dst.protonum == IPPROTO_UDP))
		return;

	ctf_l2tp_lookup(kcih, (void*)skb);
}
#endif /*CTF_L2TP */

#endif /* HNDCTF */

/*
 * This scheme offers various size of "struct nf_conn" dependent on
 * features(helper, nat, ...)
 */

#define NF_CT_FEATURES_NAMELEN	256
static struct {
	/* name of slab cache. printed in /proc/slabinfo */
	char *name;

	/* size of slab cache */
	size_t size;

	/* slab cache pointer */
	struct kmem_cache *cachep;

	/* allocated slab cache + modules which uses this slab cache */
	int use;

} nf_ct_cache[NF_CT_F_NUM];

/* protect members of nf_ct_cache except of "use" */
DEFINE_RWLOCK(nf_ct_cache_lock);

/* This avoids calling kmem_cache_create() with same name simultaneously */
static DEFINE_MUTEX(nf_ct_cache_mutex);

static int nf_conntrack_hash_rnd_initted;
static unsigned int nf_conntrack_hash_rnd;

static u_int32_t __hash_conntrack(const struct nf_conntrack_tuple *tuple,
				  unsigned int size, unsigned int rnd)
{
	unsigned int n;
	u_int32_t h;

	/* The direction must be ignored, so we hash everything up to the
	 * destination ports (which is a multiple of 4) and treat the last
	 * three bytes manually.
	 */
	n = (sizeof(tuple->src) + sizeof(tuple->dst.u3)) / sizeof(u32);
	h = jhash2((u32 *)tuple, n,
		   rnd ^ (((__force __u16)tuple->dst.u.all << 16) |
			  tuple->dst.protonum));

	return ((u64)h * size) >> 32;
}

static inline u_int32_t hash_conntrack(const struct nf_conntrack_tuple *tuple)
{
	return __hash_conntrack(tuple, nf_conntrack_htable_size,
				nf_conntrack_hash_rnd);
}

int nf_conntrack_register_cache(u_int32_t features, const char *name,
				size_t size)
{
	int ret = 0;
	char *cache_name;
	struct kmem_cache *cachep;

	DEBUGP("nf_conntrack_register_cache: features=0x%x, name=%s, size=%d\n",
	       features, name, size);

	if (features < NF_CT_F_BASIC || features >= NF_CT_F_NUM) {
		DEBUGP("nf_conntrack_register_cache: invalid features.: 0x%x\n",
			features);
		return -EINVAL;
	}

	mutex_lock(&nf_ct_cache_mutex);

	write_lock_bh(&nf_ct_cache_lock);
	/* e.g: multiple helpers are loaded */
	if (nf_ct_cache[features].use > 0) {
		DEBUGP("nf_conntrack_register_cache: already resisterd.\n");
		if ((!strncmp(nf_ct_cache[features].name, name,
			      NF_CT_FEATURES_NAMELEN))
		    && nf_ct_cache[features].size == size) {
			DEBUGP("nf_conntrack_register_cache: reusing.\n");
			nf_ct_cache[features].use++;
			ret = 0;
		} else
			ret = -EBUSY;

		write_unlock_bh(&nf_ct_cache_lock);
		mutex_unlock(&nf_ct_cache_mutex);
		return ret;
	}
	write_unlock_bh(&nf_ct_cache_lock);

	/*
	 * The memory space for name of slab cache must be alive until
	 * cache is destroyed.
	 */
	cache_name = kmalloc(sizeof(char)*NF_CT_FEATURES_NAMELEN, GFP_ATOMIC);
	if (cache_name == NULL) {
		DEBUGP("nf_conntrack_register_cache: can't alloc cache_name\n");
		ret = -ENOMEM;
		goto out_up_mutex;
	}

	if (strlcpy(cache_name, name, NF_CT_FEATURES_NAMELEN)
						>= NF_CT_FEATURES_NAMELEN) {
		printk("nf_conntrack_register_cache: name too long\n");
		ret = -EINVAL;
		goto out_free_name;
	}

	cachep = kmem_cache_create(cache_name, size, 0, 0,
				   NULL, NULL);
	if (!cachep) {
		printk("nf_conntrack_register_cache: Can't create slab cache "
		       "for the features = 0x%x\n", features);
		ret = -ENOMEM;
		goto out_free_name;
	}

	write_lock_bh(&nf_ct_cache_lock);
	nf_ct_cache[features].use = 1;
	nf_ct_cache[features].size = size;
	nf_ct_cache[features].cachep = cachep;
	nf_ct_cache[features].name = cache_name;
	write_unlock_bh(&nf_ct_cache_lock);

	goto out_up_mutex;

out_free_name:
	kfree(cache_name);
out_up_mutex:
	mutex_unlock(&nf_ct_cache_mutex);
	return ret;
}
EXPORT_SYMBOL_GPL(nf_conntrack_register_cache);

/* FIXME: In the current, only nf_conntrack_cleanup() can call this function. */
void nf_conntrack_unregister_cache(u_int32_t features)
{
	struct kmem_cache *cachep;
	char *name;

	/*
	 * This assures that kmem_cache_create() isn't called before destroying
	 * slab cache.
	 */
	DEBUGP("nf_conntrack_unregister_cache: 0x%04x\n", features);
	mutex_lock(&nf_ct_cache_mutex);

	write_lock_bh(&nf_ct_cache_lock);
	if (--nf_ct_cache[features].use > 0) {
		write_unlock_bh(&nf_ct_cache_lock);
		mutex_unlock(&nf_ct_cache_mutex);
		return;
	}
	cachep = nf_ct_cache[features].cachep;
	name = nf_ct_cache[features].name;
	nf_ct_cache[features].cachep = NULL;
	nf_ct_cache[features].name = NULL;
	nf_ct_cache[features].size = 0;
	write_unlock_bh(&nf_ct_cache_lock);

	synchronize_net();

	kmem_cache_destroy(cachep);
	kfree(name);

	mutex_unlock(&nf_ct_cache_mutex);
}
EXPORT_SYMBOL_GPL(nf_conntrack_unregister_cache);

int
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
		return 0;
	tuple->dst.protonum = protonum;
	tuple->dst.dir = IP_CT_DIR_ORIGINAL;

	return l4proto->pkt_to_tuple(skb, dataoff, tuple);
}
EXPORT_SYMBOL_GPL(nf_ct_get_tuple);

int
nf_ct_invert_tuple(struct nf_conntrack_tuple *inverse,
		   const struct nf_conntrack_tuple *orig,
		   const struct nf_conntrack_l3proto *l3proto,
		   const struct nf_conntrack_l4proto *l4proto)
{
	memset(inverse, 0, sizeof(*inverse));

	inverse->src.l3num = orig->src.l3num;
	if (l3proto->invert_tuple(inverse, orig) == 0)
		return 0;

	inverse->dst.dir = !orig->dst.dir;

	inverse->dst.protonum = orig->dst.protonum;
	return l4proto->invert_tuple(inverse, orig);
}
EXPORT_SYMBOL_GPL(nf_ct_invert_tuple);

static void
clean_from_lists(struct nf_conn *ct)
{
	DEBUGP("clean_from_lists(%p)\n", ct);
	list_del(&ct->tuplehash[IP_CT_DIR_ORIGINAL].list);
	list_del(&ct->tuplehash[IP_CT_DIR_REPLY].list);

	/* Destroy all pending expectations */
	nf_ct_remove_expectations(ct);
}

static void
destroy_conntrack(struct nf_conntrack *nfct)
{
	struct nf_conn *ct = (struct nf_conn *)nfct;
	struct nf_conntrack_l4proto *l4proto;
	typeof(nf_conntrack_destroyed) destroyed;

	DEBUGP("destroy_conntrack(%p)\n", ct);
	NF_CT_ASSERT(atomic_read(&nfct->use) == 0);
	NF_CT_ASSERT(!timer_pending(&ct->timeout));

#ifdef HNDCTF
	ip_conntrack_ipct_delete(ct, 0);
#endif /* HNDCTF*/

	nf_conntrack_event(IPCT_DESTROY, ct);
	set_bit(IPS_DYING_BIT, &ct->status);

	/* To make sure we don't get any weird locking issues here:
	 * destroy_conntrack() MUST NOT be called with a write lock
	 * to nf_conntrack_lock!!! -HW */
	rcu_read_lock();
	l4proto = __nf_ct_l4proto_find(ct->tuplehash[IP_CT_DIR_REPLY].tuple.src.l3num,
				       ct->tuplehash[IP_CT_DIR_REPLY].tuple.dst.protonum);
	if (l4proto && l4proto->destroy)
		l4proto->destroy(ct);

	destroyed = rcu_dereference(nf_conntrack_destroyed);
	if (destroyed)
		destroyed(ct);

	rcu_read_unlock();

	write_lock_bh(&nf_conntrack_lock);
	/* Expectations will have been removed in clean_from_lists,
	 * except TFTP can create an expectation on the first packet,
	 * before connection is in the list, so we need to clean here,
	 * too. */
	nf_ct_remove_expectations(ct);

	#if defined(CONFIG_NETFILTER_XT_MATCH_LAYER7) || defined(CONFIG_NETFILTER_XT_MATCH_LAYER7_MODULE)
	if(ct->layer7.app_proto)
		kfree(ct->layer7.app_proto);
	if(ct->layer7.app_data)
	kfree(ct->layer7.app_data);
	#endif


	/* We overload first tuple to link into unconfirmed list. */
	if (!nf_ct_is_confirmed(ct)) {
		BUG_ON(list_empty(&ct->tuplehash[IP_CT_DIR_ORIGINAL].list));
		list_del(&ct->tuplehash[IP_CT_DIR_ORIGINAL].list);
	}

	NF_CT_STAT_INC(delete);
	write_unlock_bh(&nf_conntrack_lock);

	if (ct->master)
		nf_ct_put(ct->master);

	DEBUGP("destroy_conntrack: returning ct=%p to slab\n", ct);
	nf_conntrack_free(ct);
}

static void death_by_timeout(unsigned long ul_conntrack)
{
	struct nf_conn *ct = (void *)ul_conntrack;
	struct nf_conn_help *help = nfct_help(ct);
	struct nf_conntrack_helper *helper;

#ifdef HNDCTF
	/* If negative error is returned it means the entry hasn't
	 * timed out yet.
	 */
	if (ip_conntrack_ipct_delete(ct, jiffies >= ct->timeout.expires ? 1 : 0) != 0)
		return;
#endif /* HNDCTF */

	if (help) {
		rcu_read_lock();
		helper = rcu_dereference(help->helper);
		if (helper && helper->destroy)
			helper->destroy(ct);
		rcu_read_unlock();
	}

	write_lock_bh(&nf_conntrack_lock);
	/* Inside lock so preempt is disabled on module removal path.
	 * Otherwise we can get spurious warnings. */
	NF_CT_STAT_INC(delete_list);
	clean_from_lists(ct);
	write_unlock_bh(&nf_conntrack_lock);
	nf_ct_put(ct);
}

struct nf_conntrack_tuple_hash *
__nf_conntrack_find(const struct nf_conntrack_tuple *tuple)
{
	struct nf_conntrack_tuple_hash *h;
	unsigned int hash = hash_conntrack(tuple);

	list_for_each_entry(h, &nf_conntrack_hash[hash], list) {
		if (nf_ct_tuple_equal(tuple, &h->tuple)) {
			NF_CT_STAT_INC(found);
			return h;
		}
		NF_CT_STAT_INC(searched);
	}

	return NULL;
}
EXPORT_SYMBOL_GPL(__nf_conntrack_find);

/* Find a connection corresponding to a tuple. */
struct nf_conntrack_tuple_hash *
nf_conntrack_find_get(const struct nf_conntrack_tuple *tuple,
		      const struct nf_conn *ignored_conntrack)
{
	struct nf_conntrack_tuple_hash *h;
	struct nf_conn *ct;

	read_lock_bh(&nf_conntrack_lock);
	h = __nf_conntrack_find(tuple);
	if (h) {
		ct = nf_ct_tuplehash_to_ctrack(h);
		if (unlikely(nf_ct_is_dying(ct) ||
			     !atomic_inc_not_zero(&ct->ct_general.use)))
			h = NULL;
	}
	read_unlock_bh(&nf_conntrack_lock);

	return h;
}
EXPORT_SYMBOL_GPL(nf_conntrack_find_get);

static void __nf_conntrack_hash_insert(struct nf_conn *ct,
				       unsigned int hash,
				       unsigned int repl_hash)
{
	ct->id = ++nf_conntrack_next_id;
	list_add(&ct->tuplehash[IP_CT_DIR_ORIGINAL].list,
		 &nf_conntrack_hash[hash]);
	list_add(&ct->tuplehash[IP_CT_DIR_REPLY].list,
		 &nf_conntrack_hash[repl_hash]);
}

void nf_conntrack_hash_insert(struct nf_conn *ct)
{
	unsigned int hash, repl_hash;

	hash = hash_conntrack(&ct->tuplehash[IP_CT_DIR_ORIGINAL].tuple);
	repl_hash = hash_conntrack(&ct->tuplehash[IP_CT_DIR_REPLY].tuple);

	write_lock_bh(&nf_conntrack_lock);
	__nf_conntrack_hash_insert(ct, hash, repl_hash);
	write_unlock_bh(&nf_conntrack_lock);
}
EXPORT_SYMBOL_GPL(nf_conntrack_hash_insert);

/* Confirm a connection given skb; places it in hash table */
int
__nf_conntrack_confirm(struct sk_buff **pskb)
{
	unsigned int hash, repl_hash;
	struct nf_conntrack_tuple_hash *h;
	struct nf_conn *ct;
	struct nf_conn_help *help;
	enum ip_conntrack_info ctinfo;

	ct = nf_ct_get(*pskb, &ctinfo);

	/* ipt_REJECT uses nf_conntrack_attach to attach related
	   ICMP/TCP RST packets in other direction.  Actual packet
	   which created connection will be IP_CT_NEW or for an
	   expected connection, IP_CT_RELATED. */
	if (CTINFO2DIR(ctinfo) != IP_CT_DIR_ORIGINAL)
		return NF_ACCEPT;

	hash = hash_conntrack(&ct->tuplehash[IP_CT_DIR_ORIGINAL].tuple);
	repl_hash = hash_conntrack(&ct->tuplehash[IP_CT_DIR_REPLY].tuple);

	/* We're not in hash table, and we refuse to set up related
	   connections for unconfirmed conns.  But packet copies and
	   REJECT will give spurious warnings here. */
	/* NF_CT_ASSERT(atomic_read(&ct->ct_general.use) == 1); */

	/* No external references means noone else could have
	   confirmed us. */
	NF_CT_ASSERT(!nf_ct_is_confirmed(ct));
	DEBUGP("Confirming conntrack %p\n", ct);

	write_lock_bh(&nf_conntrack_lock);

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
	list_for_each_entry(h, &nf_conntrack_hash[hash], list)
		if (nf_ct_tuple_equal(&ct->tuplehash[IP_CT_DIR_ORIGINAL].tuple,
				      &h->tuple))
			goto out;
	list_for_each_entry(h, &nf_conntrack_hash[repl_hash], list)
		if (nf_ct_tuple_equal(&ct->tuplehash[IP_CT_DIR_REPLY].tuple,
				      &h->tuple))
			goto out;

	/* Remove from unconfirmed list */
	list_del(&ct->tuplehash[IP_CT_DIR_ORIGINAL].list);

	__nf_conntrack_hash_insert(ct, hash, repl_hash);
	/* Timer relative to confirmation time, not original
	   setting time, otherwise we'd get timer wrap in
	   weird delay cases. */
	ct->timeout.expires += jiffies;
	add_timer(&ct->timeout);
	atomic_inc(&ct->ct_general.use);
	set_bit(IPS_CONFIRMED_BIT, &ct->status);
	NF_CT_STAT_INC(insert);
	write_unlock_bh(&nf_conntrack_lock);
	help = nfct_help(ct);
	if (help && help->helper)
		nf_conntrack_event_cache(IPCT_HELPER, *pskb);
#ifdef CONFIG_NF_NAT_NEEDED
	if (test_bit(IPS_SRC_NAT_DONE_BIT, &ct->status) ||
	    test_bit(IPS_DST_NAT_DONE_BIT, &ct->status))
		nf_conntrack_event_cache(IPCT_NATINFO, *pskb);
#endif
	nf_conntrack_event_cache(master_ct(ct) ?
				 IPCT_RELATED : IPCT_NEW, *pskb);
	return NF_ACCEPT;

out:
	NF_CT_STAT_INC(insert_failed);
	write_unlock_bh(&nf_conntrack_lock);
	return NF_DROP;
}
EXPORT_SYMBOL_GPL(__nf_conntrack_confirm);

/* Returns true if a connection correspondings to the tuple (required
   for NAT). */
int
nf_conntrack_tuple_taken(const struct nf_conntrack_tuple *tuple,
			 const struct nf_conn *ignored_conntrack)
{
	struct nf_conntrack_tuple_hash *h;
	unsigned int hash = hash_conntrack(tuple);

	read_lock_bh(&nf_conntrack_lock);
	list_for_each_entry(h, &nf_conntrack_hash[hash], list) {
		if (nf_ct_tuplehash_to_ctrack(h) != ignored_conntrack &&
		    nf_ct_tuple_equal(tuple, &h->tuple)) {
			NF_CT_STAT_INC(found);
			read_unlock_bh(&nf_conntrack_lock);
			return 1;
		}
		NF_CT_STAT_INC(searched);
	}
	read_unlock_bh(&nf_conntrack_lock);

	return 0;
}
EXPORT_SYMBOL_GPL(nf_conntrack_tuple_taken);

/* There's a small race here where we may free a just-assured
   connection.  Too bad: we're in trouble anyway. */
static noinline int early_drop(struct list_head *chain)
{
	/* Traverse backwards: gives us oldest, which is roughly LRU */
	struct nf_conntrack_tuple_hash *h;
	struct nf_conn *ct = NULL, *tmp;
	int dropped = 0;

	read_lock_bh(&nf_conntrack_lock);
	list_for_each_entry_reverse(h, chain, list) {
		tmp = nf_ct_tuplehash_to_ctrack(h);
		if (!test_bit(IPS_ASSURED_BIT, &tmp->status)) {
			ct = tmp;
			atomic_inc(&ct->ct_general.use);
			break;
		}
	}
	read_unlock_bh(&nf_conntrack_lock);

	if (!ct)
		return dropped;

#ifdef HNDCTF
	ip_conntrack_ipct_delete(ct, 0);
#endif /* HNDCTF */

	if (del_timer(&ct->timeout)) {
		death_by_timeout((unsigned long)ct);
		dropped = 1;
		NF_CT_STAT_INC_ATOMIC(early_drop);
	}
	nf_ct_put(ct);
	return dropped;
}

static struct nf_conn *
__nf_conntrack_alloc(const struct nf_conntrack_tuple *orig,
		     const struct nf_conntrack_tuple *repl,
		     const struct nf_conntrack_l3proto *l3proto,
		     u_int32_t features)
{
	struct nf_conn *conntrack = NULL;
	struct nf_conntrack_helper *helper;

	if (unlikely(!nf_conntrack_hash_rnd_initted)) {
		get_random_bytes(&nf_conntrack_hash_rnd, 4);
		nf_conntrack_hash_rnd_initted = 1;
	}

	/* We don't want any race condition at early drop stage */
	atomic_inc(&nf_conntrack_count);

	if (nf_conntrack_max &&
	    unlikely(atomic_read(&nf_conntrack_count) > nf_conntrack_max)) {
		unsigned int hash = hash_conntrack(orig);
		/* Try dropping from this hash chain. */
		if (!early_drop(&nf_conntrack_hash[hash])) {
			atomic_dec(&nf_conntrack_count);
			if (net_ratelimit())
				printk(KERN_WARNING
				       "nf_conntrack: table full, dropping"
				       " packet.\n");
			return ERR_PTR(-ENOMEM);
		}
	}

	/*  find features needed by this conntrack. */
	features |= l3proto->get_features(orig);

	/* FIXME: protect helper list per RCU */
	read_lock_bh(&nf_conntrack_lock);
	helper = __nf_ct_helper_find(repl);
	/* NAT might want to assign a helper later */
	if (helper || features & NF_CT_F_NAT)
		features |= NF_CT_F_HELP;
	read_unlock_bh(&nf_conntrack_lock);

	DEBUGP("nf_conntrack_alloc: features=0x%x\n", features);

	read_lock_bh(&nf_ct_cache_lock);

	if (unlikely(!nf_ct_cache[features].use)) {
		DEBUGP("nf_conntrack_alloc: not supported features = 0x%x\n",
			features);
		goto out;
	}

	conntrack = kmem_cache_zalloc(nf_ct_cache[features].cachep, GFP_ATOMIC);
	if (conntrack == NULL) {
		DEBUGP("nf_conntrack_alloc: Can't alloc conntrack from cache\n");
		goto out;
	}

	conntrack->features = features;
	atomic_set(&conntrack->ct_general.use, 1);
	conntrack->tuplehash[IP_CT_DIR_ORIGINAL].tuple = *orig;
	conntrack->tuplehash[IP_CT_DIR_REPLY].tuple = *repl;
	/* Don't set timer yet: wait for confirmation */
	setup_timer(&conntrack->timeout, death_by_timeout,
		    (unsigned long)conntrack);
	read_unlock_bh(&nf_ct_cache_lock);

	return conntrack;
out:
	read_unlock_bh(&nf_ct_cache_lock);
	atomic_dec(&nf_conntrack_count);
	return conntrack;
}

struct nf_conn *nf_conntrack_alloc(const struct nf_conntrack_tuple *orig,
				   const struct nf_conntrack_tuple *repl)
{
	struct nf_conntrack_l3proto *l3proto;
	struct nf_conn *ct;

	rcu_read_lock();
	l3proto = __nf_ct_l3proto_find(orig->src.l3num);
	ct = __nf_conntrack_alloc(orig, repl, l3proto, 0);
	rcu_read_unlock();

	return ct;
}
EXPORT_SYMBOL_GPL(nf_conntrack_alloc);

void nf_conntrack_free(struct nf_conn *conntrack)
{
	u_int32_t features = conntrack->features;
	NF_CT_ASSERT(features >= NF_CT_F_BASIC && features < NF_CT_F_NUM);
	DEBUGP("nf_conntrack_free: features = 0x%x, conntrack=%p\n", features,
	       conntrack);
	kmem_cache_free(nf_ct_cache[features].cachep, conntrack);
	atomic_dec(&nf_conntrack_count);
}
EXPORT_SYMBOL_GPL(nf_conntrack_free);

/* Allocate a new conntrack: we return -ENOMEM if classification
   failed due to stress.  Otherwise it really is unclassifiable. */
static struct nf_conntrack_tuple_hash *
init_conntrack(const struct nf_conntrack_tuple *tuple,
	       struct nf_conntrack_l3proto *l3proto,
	       struct nf_conntrack_l4proto *l4proto,
	       struct sk_buff *skb,
	       unsigned int dataoff)
{
	struct nf_conn *conntrack;
	struct nf_conn_help *help;
	struct nf_conntrack_tuple repl_tuple;
	struct nf_conntrack_expect *exp;
	u_int32_t features = 0;

	if (!nf_ct_invert_tuple(&repl_tuple, tuple, l3proto, l4proto)) {
		DEBUGP("Can't invert tuple.\n");
		return NULL;
	}

	read_lock_bh(&nf_conntrack_lock);
	exp = __nf_conntrack_expect_find(tuple);
	if (exp && exp->helper)
		features = NF_CT_F_HELP;
	read_unlock_bh(&nf_conntrack_lock);

	conntrack = __nf_conntrack_alloc(tuple, &repl_tuple, l3proto, features);
	if (conntrack == NULL || IS_ERR(conntrack)) {
		DEBUGP("Can't allocate conntrack.\n");
		return (struct nf_conntrack_tuple_hash *)conntrack;
	}

	if (!l4proto->new(conntrack, skb, dataoff)) {
		nf_conntrack_free(conntrack);
		DEBUGP("init conntrack: can't track with proto module\n");
		return NULL;
	}

	write_lock_bh(&nf_conntrack_lock);

	exp = find_expectation(tuple);

	help = nfct_help(conntrack);
	if (exp) {
		DEBUGP("conntrack: expectation arrives ct=%p exp=%p\n",
			conntrack, exp);
		/* Welcome, Mr. Bond.  We've been expecting you... */
		__set_bit(IPS_EXPECTED_BIT, &conntrack->status);
		conntrack->master = exp->master;
		if (exp->helper)
			rcu_assign_pointer(help->helper, exp->helper);
#ifdef CONFIG_NF_CONNTRACK_MARK
		conntrack->mark = exp->master->mark;
#endif
#ifdef CONFIG_NF_CONNTRACK_SECMARK
		conntrack->secmark = exp->master->secmark;
#endif
		nf_conntrack_get(&conntrack->master->ct_general);
		NF_CT_STAT_INC(expect_new);
	} else {
		if (help) {
			/* not in hash table yet, so not strictly necessary */
			rcu_assign_pointer(help->helper,
					   __nf_ct_helper_find(&repl_tuple));
		}
		NF_CT_STAT_INC(new);
	}

	/* Overload tuple linked list to put us in unconfirmed list. */
	list_add(&conntrack->tuplehash[IP_CT_DIR_ORIGINAL].list, &unconfirmed);

	write_unlock_bh(&nf_conntrack_lock);

	if (exp) {
		if (exp->expectfn)
			exp->expectfn(conntrack, exp);
		nf_conntrack_expect_put(exp);
	}

	return &conntrack->tuplehash[IP_CT_DIR_ORIGINAL];
}

/* On success, returns conntrack ptr, sets skb->nfct and ctinfo */
static inline struct nf_conn *
resolve_normal_ct(struct sk_buff *skb,
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

	if (!nf_ct_get_tuple(skb, skb_network_offset(skb),
			     dataoff, l3num, protonum, &tuple, l3proto,
			     l4proto)) {
		DEBUGP("resolve_normal_ct: Can't get tuple\n");
		return NULL;
	}

	/* look for tuple match */
	h = nf_conntrack_find_get(&tuple, NULL);
	if (!h) {
		h = init_conntrack(&tuple, l3proto, l4proto, skb, dataoff);
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
			DEBUGP("nf_conntrack_in: normal packet for %p\n", ct);
			*ctinfo = IP_CT_ESTABLISHED;
		} else if (test_bit(IPS_EXPECTED_BIT, &ct->status)) {
			DEBUGP("nf_conntrack_in: related packet for %p\n", ct);
			*ctinfo = IP_CT_RELATED;
		} else {
			DEBUGP("nf_conntrack_in: new packet for %p\n", ct);
			*ctinfo = IP_CT_NEW;
		}
		*set_reply = 0;
	}
	skb->nfct = &ct->ct_general;
	skb->nfctinfo = *ctinfo;
	return ct;
}

unsigned int
nf_conntrack_in(int pf, unsigned int hooknum, struct sk_buff **pskb)
{
	struct nf_conn *ct;
	enum ip_conntrack_info ctinfo;
	struct nf_conntrack_l3proto *l3proto;
	struct nf_conntrack_l4proto *l4proto;
	unsigned int dataoff;
	u_int8_t protonum;
	int set_reply = 0;
	int ret;

	/* Previously seen (loopback or untracked)?  Ignore. */
	if ((*pskb)->nfct) {
		NF_CT_STAT_INC_ATOMIC(ignore);
		return NF_ACCEPT;
	}

	/* rcu_read_lock()ed by nf_hook_slow */
	l3proto = __nf_ct_l3proto_find((u_int16_t)pf);

	if ((ret = l3proto->prepare(pskb, hooknum, &dataoff, &protonum)) <= 0) {
		DEBUGP("not prepared to track yet or error occured\n");
		return -ret;
	}

	l4proto = __nf_ct_l4proto_find((u_int16_t)pf, protonum);

	/* It may be an special packet, error, unclean...
	 * inverse of the return code tells to the netfilter
	 * core what to do with the packet. */
	if (l4proto->error != NULL &&
	    (ret = l4proto->error(*pskb, dataoff, &ctinfo, pf, hooknum)) <= 0) {
		NF_CT_STAT_INC_ATOMIC(error);
		NF_CT_STAT_INC_ATOMIC(invalid);
		return -ret;
	}

	ct = resolve_normal_ct(*pskb, dataoff, pf, protonum, l3proto, l4proto,
			       &set_reply, &ctinfo);
	if (!ct) {
		/* Not valid part of a connection */
		NF_CT_STAT_INC_ATOMIC(invalid);
		return NF_ACCEPT;
	}

	if (IS_ERR(ct)) {
		/* Too stressed to deal. */
		NF_CT_STAT_INC_ATOMIC(drop);
		return NF_DROP;
	}

	NF_CT_ASSERT((*pskb)->nfct);

	ret = l4proto->packet(ct, *pskb, dataoff, ctinfo, pf, hooknum);
	if (ret <= 0) {
		/* Invalid: inverse of the return code tells
		 * the netfilter core what to do */
		DEBUGP("nf_conntrack_in: Can't track with proto module\n");
		nf_conntrack_put((*pskb)->nfct);
		(*pskb)->nfct = NULL;
		NF_CT_STAT_INC_ATOMIC(invalid);
		if (ret == -NF_DROP)
			NF_CT_STAT_INC_ATOMIC(drop);
		return -ret;
	}

	if (set_reply && !test_and_set_bit(IPS_SEEN_REPLY_BIT, &ct->status)) {
		nf_conntrack_event_cache(IPCT_STATUS, *pskb);
	}
	return ret;
}
EXPORT_SYMBOL_GPL(nf_conntrack_in);

int nf_ct_invert_tuplepr(struct nf_conntrack_tuple *inverse,
			 const struct nf_conntrack_tuple *orig)
{
	int ret;

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

	write_lock_bh(&nf_conntrack_lock);
	/* Should be unconfirmed, so not in hash table yet */
	NF_CT_ASSERT(!nf_ct_is_confirmed(ct));

	DEBUGP("Altering reply tuple of %p to ", ct);
	NF_CT_DUMP_TUPLE(newreply);

	ct->tuplehash[IP_CT_DIR_REPLY].tuple = *newreply;
	if (!ct->master && help && help->expecting == 0) {
		struct nf_conntrack_helper *helper;
		helper = __nf_ct_helper_find(newreply);
		if (helper)
			memset(&help->help, 0, sizeof(help->help));
		/* not in hash table yet, so not strictly necessary */
		rcu_assign_pointer(help->helper, helper);
	}
	write_unlock_bh(&nf_conntrack_lock);
}
EXPORT_SYMBOL_GPL(nf_conntrack_alter_reply);

/* Refresh conntrack for this many jiffies and do accounting if do_acct is 1 */
void __nf_ct_refresh_acct(struct nf_conn *ct,
			  enum ip_conntrack_info ctinfo,
			  const struct sk_buff *skb,
			  unsigned long extra_jiffies,
			  int do_acct)
{
	int event = 0;

	NF_CT_ASSERT(ct->timeout.data == (unsigned long)ct);
	NF_CT_ASSERT(skb);

	write_lock_bh(&nf_conntrack_lock);

	/* Only update if this is not a fixed timeout */
	if (test_bit(IPS_FIXED_TIMEOUT_BIT, &ct->status))
		goto acct;

	/* If not in hash table, timer will not be active yet */
	if (!nf_ct_is_confirmed(ct)) {
#ifdef HNDCTF
		ct->expire_jiffies = extra_jiffies;
#endif /* HNDCTF */
		ct->timeout.expires = extra_jiffies;
		event = IPCT_REFRESH;
	} else {
		unsigned long newtime = jiffies + extra_jiffies;

		/* Only update the timeout if the new timeout is at least
		   HZ jiffies from the old timeout. Need del_timer for race
		   avoidance (may already be dying). */
		if (newtime - ct->timeout.expires >= HZ
		    && del_timer(&ct->timeout)) {
#ifdef HNDCTF
			ct->expire_jiffies = extra_jiffies;
#endif /* HNDCTF */
			ct->timeout.expires = newtime;
			add_timer(&ct->timeout);
			event = IPCT_REFRESH;
		}
	}

acct:
#ifdef CONFIG_NF_CT_ACCT
	if (do_acct) {
		ct->counters[CTINFO2DIR(ctinfo)].packets++;
		ct->counters[CTINFO2DIR(ctinfo)].bytes +=
			skb->len - skb_network_offset(skb);

		if ((ct->counters[CTINFO2DIR(ctinfo)].packets & 0x80000000)
		    || (ct->counters[CTINFO2DIR(ctinfo)].bytes & 0x80000000))
			event |= IPCT_COUNTER_FILLING;
	}
#endif

	write_unlock_bh(&nf_conntrack_lock);

	/* must be unlocked when calling event cache */
	if (event)
		nf_conntrack_event_cache(event, skb);
}
EXPORT_SYMBOL_GPL(__nf_ct_refresh_acct);

#if defined(CONFIG_NF_CT_NETLINK) || defined(CONFIG_NF_CT_NETLINK_MODULE)

#include <linux/netfilter/nfnetlink.h>
#include <linux/netfilter/nfnetlink_conntrack.h>
#include <linux/mutex.h>


/* Generic function for tcp/udp/sctp/dccp and alike. This needs to be
 * in ip_conntrack_core, since we don't want the protocols to autoload
 * or depend on ctnetlink */
int nf_ct_port_tuple_to_nfattr(struct sk_buff *skb,
			       const struct nf_conntrack_tuple *tuple)
{
	NFA_PUT(skb, CTA_PROTO_SRC_PORT, sizeof(u_int16_t),
		&tuple->src.u.tcp.port);
	NFA_PUT(skb, CTA_PROTO_DST_PORT, sizeof(u_int16_t),
		&tuple->dst.u.tcp.port);
	return 0;

nfattr_failure:
	return -1;
}
EXPORT_SYMBOL_GPL(nf_ct_port_tuple_to_nfattr);

static const size_t cta_min_proto[CTA_PROTO_MAX] = {
	[CTA_PROTO_SRC_PORT-1]  = sizeof(u_int16_t),
	[CTA_PROTO_DST_PORT-1]  = sizeof(u_int16_t)
};

int nf_ct_port_nfattr_to_tuple(struct nfattr *tb[],
			       struct nf_conntrack_tuple *t)
{
	if (!tb[CTA_PROTO_SRC_PORT-1] || !tb[CTA_PROTO_DST_PORT-1])
		return -EINVAL;

	if (nfattr_bad_size(tb, CTA_PROTO_MAX, cta_min_proto))
		return -EINVAL;

	t->src.u.tcp.port = *(__be16 *)NFA_DATA(tb[CTA_PROTO_SRC_PORT-1]);
	t->dst.u.tcp.port = *(__be16 *)NFA_DATA(tb[CTA_PROTO_DST_PORT-1]);

	return 0;
}
EXPORT_SYMBOL_GPL(nf_ct_port_nfattr_to_tuple);
#endif

/* Used by ipt_REJECT and ip6t_REJECT. */
void __nf_conntrack_attach(struct sk_buff *nskb, struct sk_buff *skb)
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
EXPORT_SYMBOL_GPL(__nf_conntrack_attach);

static inline int
do_iter(const struct nf_conntrack_tuple_hash *i,
	int (*iter)(struct nf_conn *i, void *data),
	void *data)
{
	return iter(nf_ct_tuplehash_to_ctrack(i), data);
}

/* Bring out ya dead! */
static struct nf_conn *
get_next_corpse(int (*iter)(struct nf_conn *i, void *data),
		void *data, unsigned int *bucket)
{
	struct nf_conntrack_tuple_hash *h;
	struct nf_conn *ct;

	write_lock_bh(&nf_conntrack_lock);
	for (; *bucket < nf_conntrack_htable_size; (*bucket)++) {
		list_for_each_entry(h, &nf_conntrack_hash[*bucket], list) {
			ct = nf_ct_tuplehash_to_ctrack(h);
			if (iter(ct, data))
				goto found;
		}
	}
	list_for_each_entry(h, &unconfirmed, list) {
		ct = nf_ct_tuplehash_to_ctrack(h);
		if (iter(ct, data))
			set_bit(IPS_DYING_BIT, &ct->status);
	}
	write_unlock_bh(&nf_conntrack_lock);
	return NULL;
found:
	atomic_inc(&ct->ct_general.use);
	write_unlock_bh(&nf_conntrack_lock);
	return ct;
}

void
nf_ct_iterate_cleanup(int (*iter)(struct nf_conn *i, void *data), void *data)
{
	struct nf_conn *ct;
	unsigned int bucket = 0;

	while ((ct = get_next_corpse(iter, data, &bucket)) != NULL) {
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

static int kill_all(struct nf_conn *i, void *data)
{
	return 1;
}

static void free_conntrack_hash(struct list_head *hash, int vmalloced, int size)
{
	if (vmalloced)
		vfree(hash);
	else
		free_pages((unsigned long)hash,
			   get_order(sizeof(struct list_head) * size));
}

void nf_conntrack_flush(void)
{
	nf_ct_iterate_cleanup(kill_all, NULL);
}
EXPORT_SYMBOL_GPL(nf_conntrack_flush);

/* Mishearing the voices in his head, our hero wonders how he's
   supposed to kill the mall. */
void nf_conntrack_cleanup(void)
{
	int i;

	rcu_assign_pointer(ip_ct_attach, NULL);

	/* This makes sure all current packets have passed through
	   netfilter framework.  Roll on, two-stage module
	   delete... */
	synchronize_net();

	nf_ct_event_cache_flush();
 i_see_dead_people:
	nf_conntrack_flush();
	if (atomic_read(&nf_conntrack_count) != 0) {
		schedule();
		goto i_see_dead_people;
	}
	/* wait until all references to nf_conntrack_untracked are dropped */
	while (atomic_read(&nf_conntrack_untracked.ct_general.use) > 1)
		schedule();

	rcu_assign_pointer(nf_ct_destroy, NULL);

	for (i = 0; i < NF_CT_F_NUM; i++) {
		if (nf_ct_cache[i].use == 0)
			continue;

		NF_CT_ASSERT(nf_ct_cache[i].use == 1);
		nf_ct_cache[i].use = 1;
		nf_conntrack_unregister_cache(i);
	}
	kmem_cache_destroy(nf_conntrack_expect_cachep);
	free_conntrack_hash(nf_conntrack_hash, nf_conntrack_vmalloc,
			    nf_conntrack_htable_size);

	nf_conntrack_proto_fini();
}

static struct list_head *alloc_hashtable(int *sizep, int *vmalloced)
{
	struct list_head *hash;
	unsigned int size, i;

	*vmalloced = 0;

	size = *sizep = roundup(*sizep, PAGE_SIZE / sizeof(struct list_head));
	hash = (void*)__get_free_pages(GFP_KERNEL|__GFP_NOWARN,
				       get_order(sizeof(struct list_head)
						 * size));
	if (!hash) {
		*vmalloced = 1;
		printk(KERN_WARNING "nf_conntrack: falling back to vmalloc.\n");
		hash = vmalloc(sizeof(struct list_head) * size);
	}

	if (hash)
		for (i = 0; i < size; i++)
			INIT_LIST_HEAD(&hash[i]);

	return hash;
}

int set_hashsize(const char *val, struct kernel_param *kp)
{
	int i, bucket, hashsize, vmalloced;
	int old_vmalloced, old_size;
	int rnd;
	struct list_head *hash, *old_hash;
	struct nf_conntrack_tuple_hash *h;

	/* On boot, we can set this without any fancy locking. */
	if (!nf_conntrack_htable_size)
		return param_set_uint(val, kp);

	hashsize = simple_strtol(val, NULL, 0);
	if (!hashsize)
		return -EINVAL;

	hash = alloc_hashtable(&hashsize, &vmalloced);
	if (!hash)
		return -ENOMEM;

	/* We have to rehahs for the new table anyway, so we also can
	 * use a newrandom seed */
	get_random_bytes(&rnd, 4);

	write_lock_bh(&nf_conntrack_lock);
	for (i = 0; i < nf_conntrack_htable_size; i++) {
		while (!list_empty(&nf_conntrack_hash[i])) {
			h = list_entry(nf_conntrack_hash[i].next,
				       struct nf_conntrack_tuple_hash, list);
			list_del(&h->list);
			bucket = __hash_conntrack(&h->tuple, hashsize, rnd);
			list_add_tail(&h->list, &hash[bucket]);
		}
	}
	old_size = nf_conntrack_htable_size;
	old_vmalloced = nf_conntrack_vmalloc;
	old_hash = nf_conntrack_hash;

	nf_conntrack_htable_size = hashsize;
	nf_conntrack_vmalloc = vmalloced;
	nf_conntrack_hash = hash;
	nf_conntrack_hash_rnd = rnd;
	write_unlock_bh(&nf_conntrack_lock);

	free_conntrack_hash(old_hash, old_vmalloced, old_size);
	return 0;
}

module_param_call(hashsize, set_hashsize, param_get_uint,
		  &nf_conntrack_htable_size, 0600);

int __init nf_conntrack_init(void)
{
	int ret;

	/* Idea from tcp.c: use 1/16384 of memory.  On i386: 32MB
	 * machine has 256 buckets.  >= 1GB machines have 8192 buckets. */
	if (!nf_conntrack_htable_size) {
		nf_conntrack_htable_size
			= (((num_physpages << PAGE_SHIFT) / 16384)
			   / sizeof(struct list_head));
		if (num_physpages > (1024 * 1024 * 1024 / PAGE_SIZE))
			nf_conntrack_htable_size = 8192;
		if (nf_conntrack_htable_size < 16)
			nf_conntrack_htable_size = 16;
	}

	nf_conntrack_hash = alloc_hashtable(&nf_conntrack_htable_size,
					    &nf_conntrack_vmalloc);
	if (!nf_conntrack_hash) {
		printk(KERN_ERR "Unable to create nf_conntrack_hash\n");
		goto err_out;
	}

	nf_conntrack_max = 8 * nf_conntrack_htable_size;

	printk("nf_conntrack version %s (%u buckets, %d max)\n",
	       NF_CONNTRACK_VERSION, nf_conntrack_htable_size,
	       nf_conntrack_max);

	ret = nf_conntrack_register_cache(NF_CT_F_BASIC, "nf_conntrack:basic",
					  sizeof(struct nf_conn));
	if (ret < 0) {
		printk(KERN_ERR "Unable to create nf_conn slab cache\n");
		goto err_free_hash;
	}

	nf_conntrack_expect_cachep = kmem_cache_create("nf_conntrack_expect",
					sizeof(struct nf_conntrack_expect),
					0, 0, NULL, NULL);
	if (!nf_conntrack_expect_cachep) {
		printk(KERN_ERR "Unable to create nf_expect slab cache\n");
		goto err_free_conntrack_slab;
	}

	ret = nf_conntrack_proto_init();
	if (ret < 0)
		goto out_free_expect_slab;

	/* For use by REJECT target */
	rcu_assign_pointer(ip_ct_attach, __nf_conntrack_attach);
	rcu_assign_pointer(nf_ct_destroy, destroy_conntrack);

	/* Set up fake conntrack:
	    - to never be deleted, not in any hashes */
	atomic_set(&nf_conntrack_untracked.ct_general.use, 1);
	/*  - and look it like as a confirmed connection */
	set_bit(IPS_CONFIRMED_BIT, &nf_conntrack_untracked.status);

	return ret;

out_free_expect_slab:
	kmem_cache_destroy(nf_conntrack_expect_cachep);
err_free_conntrack_slab:
	nf_conntrack_unregister_cache(NF_CT_F_BASIC);
err_free_hash:
	free_conntrack_hash(nf_conntrack_hash, nf_conntrack_vmalloc,
			    nf_conntrack_htable_size);
err_out:
	return -ENOMEM;
}
