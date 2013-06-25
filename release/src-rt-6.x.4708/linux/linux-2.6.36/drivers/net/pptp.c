/*
 *  Point-to-Point Tunneling Protocol for Linux
 *
 *	Authors: Kozlov D. (xeb@mail.ru)
 *
 *	This program is free software; you can redistribute it and/or
 *	modify it under the terms of the GNU General Public License
 *	as published by the Free Software Foundation; either version
 *	2 of the License, or (at your option) any later version.
 *
 */

#include <linux/string.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/vmalloc.h>
#include <linux/errno.h>
#include <linux/netdevice.h>
#include <linux/net.h>
#include <linux/skbuff.h>
#include <linux/init.h>
#include <linux/ppp_channel.h>
#include <linux/ppp_defs.h>
#include <linux/if_pppox.h>
#include <linux/if_ppp.h>
#include <linux/notifier.h>
#include <linux/file.h>
#include <linux/in.h>
#include <linux/ip.h>
#include <linux/netfilter.h>
#include <linux/netfilter_ipv4.h>
#include <linux/version.h>

#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,0)
#include <asm/bitops.h>
#endif

#include <net/sock.h>
#include <net/protocol.h>
#include <net/ip.h>
#include <net/icmp.h>
#include <net/route.h>

#include <asm/uaccess.h>

//#define DEBUG
//#define CONFIG_NET_IPGRE_DEMUX
#define HAVE_FIND_NEXT_BIT
#define HAVE_SKB_CLONE_WRITABLE

#if defined(CONFIG_NET_IPGRE_DEMUX) || defined(CONFIG_NET_IPGRE_DEMUX_MODULE)
#include <net/gre.h>
#endif

#define PPTP_DRIVER_VERSION "0.8.5"

#ifdef DEBUG
static int log_level=0;
static int log_packets=10;
#endif

#define MAX_CALLID 65535
#define PPP_LCP_ECHOREQ 0x09
#define PPP_LCP_ECHOREP 0x0A

#if LINUX_VERSION_CODE <= KERNEL_VERSION(2,4,20)
#define BITS_TO_LONGS(bits) \
	(((bits)+BITS_PER_LONG-1)/BITS_PER_LONG)
#define DECLARE_BITMAP(name,bits) \
	unsigned long name[BITS_TO_LONGS(bits)]
#define CLEAR_BITMAP(name,bits) \
	memset(name, 0, BITS_TO_LONGS(bits)*sizeof(unsigned long))
#endif

static DECLARE_BITMAP(callid_bitmap, MAX_CALLID + 1);
static struct pppox_sock **callid_sock;

#define SC_RCV_BITS	(SC_RCV_B7_1|SC_RCV_B7_0|SC_RCV_ODDP|SC_RCV_EVNP)

#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,0)
#define INIT_TIMER(_timer,_routine,_data) \
do { \
	(_timer)->function=_routine; \
	(_timer)->data=_data; \
	init_timer(_timer); \
} while (0);

static inline void *kzalloc(size_t size,int gfp)
{
	void *p=kmalloc(size,gfp);
	memset(p,0,size);
	return p;
}

#if LINUX_VERSION_CODE <= KERNEL_VERSION(2,4,20)
static inline void nf_reset(struct sk_buff *skb)
{
#ifdef CONFIG_NETFILTER
	nf_conntrack_put(skb->nfct);
	skb->nfct=NULL;
#ifdef CONFIG_NETFILTER_DEBUG
	skb->nf_debug=0;
#endif
#endif
}
#define __user
#endif
#endif

#ifndef HAVE_FIND_NEXT_BIT
/**
 * __ffs - find first bit in word.
 * @word: The word to search
 *
 * Undefined if no bit exists, so code should check against 0 first.
 */
static inline unsigned long __ffs(unsigned long word)
{
	int num = 0;

#if BITS_PER_LONG == 64
	if ((word & 0xffffffff) == 0) {
		num += 32;
		word >>= 32;
	}
#endif
	if ((word & 0xffff) == 0) {
		num += 16;
		word >>= 16;
	}
	if ((word & 0xff) == 0) {
		num += 8;
		word >>= 8;
	}
	if ((word & 0xf) == 0) {
		num += 4;
		word >>= 4;
	}
	if ((word & 0x3) == 0) {
		num += 2;
		word >>= 2;
	}
	if ((word & 0x1) == 0)
		num += 1;
	return num;
}

#define BITOP_WORD(nr)		((nr) / BITS_PER_LONG)
/*
 * Find the next set bit in a memory region.
 */
static unsigned long find_next_bit(const unsigned long *addr, unsigned long size,
			    unsigned long offset)
{
	const unsigned long *p = addr + BITOP_WORD(offset);
	unsigned long result = offset & ~(BITS_PER_LONG-1);
	unsigned long tmp;

	if (offset >= size)
		return size;
	size -= result;
	offset %= BITS_PER_LONG;
	if (offset) {
		tmp = *(p++);
		tmp &= (~0UL << offset);
		if (size < BITS_PER_LONG)
			goto found_first;
		if (tmp)
			goto found_middle;
		size -= BITS_PER_LONG;
		result += BITS_PER_LONG;
	}
	while (size & ~(BITS_PER_LONG-1)) {
		if ((tmp = *(p++)))
			goto found_middle;
		result += BITS_PER_LONG;
		size -= BITS_PER_LONG;
	}
	if (!size)
		return result;
	tmp = *p;

found_first:
	tmp &= (~0UL >> (BITS_PER_LONG - size));
	if (tmp == 0UL)		/* Are any bits set? */
		return result + size;	/* Nope. */
found_middle:
	return result + __ffs(tmp);
}
#endif


#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,0)
static rwlock_t chan_lock=RW_LOCK_UNLOCKED;
#define SK_STATE(sk) (sk)->state
#else
static DEFINE_SPINLOCK(chan_lock);
#define SK_STATE(sk) (sk)->sk_state
#endif

static int pptp_xmit(struct ppp_channel *chan, struct sk_buff *skb);
static int pptp_ppp_ioctl(struct ppp_channel *chan, unsigned int cmd,
			   unsigned long arg);
static int pptp_rcv_core(struct sock *sk,struct sk_buff *skb);

static struct ppp_channel_ops pptp_chan_ops= {
	.start_xmit = pptp_xmit,
	.ioctl=pptp_ppp_ioctl,
};


#define MISSING_WINDOW 20
#define WRAPPED( curseq, lastseq) \
    ((((curseq) & 0xffffff00) == 0) && \
     (((lastseq) & 0xffffff00 ) == 0xffffff00))

/* gre header structure: -------------------------------------------- */

#define PPTP_GRE_PROTO  0x880B
#define PPTP_GRE_VER    0x1

#define PPTP_GRE_FLAG_C	0x80
#define PPTP_GRE_FLAG_R	0x40
#define PPTP_GRE_FLAG_K	0x20
#define PPTP_GRE_FLAG_S	0x10
#define PPTP_GRE_FLAG_A	0x80

#define PPTP_GRE_IS_C(f) ((f)&PPTP_GRE_FLAG_C)
#define PPTP_GRE_IS_R(f) ((f)&PPTP_GRE_FLAG_R)
#define PPTP_GRE_IS_K(f) ((f)&PPTP_GRE_FLAG_K)
#define PPTP_GRE_IS_S(f) ((f)&PPTP_GRE_FLAG_S)
#define PPTP_GRE_IS_A(f) ((f)&PPTP_GRE_FLAG_A)

struct pptp_gre_header {
  u8 flags;		/* bitfield */
  u8 ver;			/* should be PPTP_GRE_VER (enhanced GRE) */
  u16 protocol;		/* should be PPTP_GRE_PROTO (ppp-encaps) */
  u16 payload_len;	/* size of ppp payload, not inc. gre header */
  u16 call_id;		/* peer's call_id for this session */
  u32 seq;		/* sequence number.  Present if S==1 */
  u32 ack;		/* seq number of highest packet recieved by */
  				/*  sender in this session */
} __attribute__ ((__packed__));
#define PPTP_HEADER_OVERHEAD (2+sizeof(struct pptp_gre_header))

#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,0)
static struct pppox_sock * lookup_chan(u16 call_id, u32 s_addr)
#else
static struct pppox_sock * lookup_chan(u16 call_id, __be32 s_addr)
#endif
{
	struct pppox_sock *sock;
	struct pptp_opt *opt;

#if LINUX_VERSION_CODE > KERNEL_VERSION(2,6,0)
	rcu_read_lock();
	sock = rcu_dereference(callid_sock[call_id]);
#else
	read_lock(&chan_lock);
	sock = callid_sock[call_id];
#endif
	if (sock) {
		opt=&sock->proto.pptp;
		if (opt->dst_addr.sin_addr.s_addr!=s_addr) sock=NULL;
		else sock_hold(sk_pppox(sock));
	}
#if LINUX_VERSION_CODE > KERNEL_VERSION(2,6,0)
	rcu_read_unlock();
#else
	read_unlock(&chan_lock);
#endif
	
	return sock;
}

#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,0)
static int lookup_chan_dst(u16 call_id, u32 d_addr)
#else
static int lookup_chan_dst(u16 call_id, __be32 d_addr)
#endif
{
	struct pppox_sock *sock;
	struct pptp_opt *opt;
	int i;
	
#if LINUX_VERSION_CODE > KERNEL_VERSION(2,6,0)
	rcu_read_lock();
#else
	read_lock(&chan_lock);
#endif
	for(i = find_next_bit(callid_bitmap,MAX_CALLID,1); i < MAX_CALLID; 
	                i = find_next_bit(callid_bitmap, MAX_CALLID, i + 1)){
#if LINUX_VERSION_CODE > KERNEL_VERSION(2,6,0)
	    sock = rcu_dereference(callid_sock[i]);
#else
	    sock = callid_sock[i];
#endif
	    if (!sock)
		continue;
	    opt = &sock->proto.pptp;
	    if (opt->dst_addr.call_id == call_id && opt->dst_addr.sin_addr.s_addr == d_addr) break;
	}
#if LINUX_VERSION_CODE > KERNEL_VERSION(2,6,0)
	rcu_read_unlock();
#else
	read_unlock(&chan_lock);
#endif
	
	return i<MAX_CALLID;
}

static int add_chan(struct pppox_sock *sock)
{
	static int call_id=0;
	int res=-1;

#if LINUX_VERSION_CODE > KERNEL_VERSION(2,6,0)
	spin_lock(&chan_lock);
#else
	write_lock_bh(&chan_lock);
#endif
	
	if (!sock->proto.pptp.src_addr.call_id)
	{
	    call_id=find_next_zero_bit(callid_bitmap,MAX_CALLID,call_id+1);
	    if (call_id==MAX_CALLID)
				call_id=find_next_zero_bit(callid_bitmap,MAX_CALLID,1);
	    sock->proto.pptp.src_addr.call_id=call_id;
	}
	else if (test_bit(sock->proto.pptp.src_addr.call_id,callid_bitmap))
		goto exit;
	
#if LINUX_VERSION_CODE > KERNEL_VERSION(2,6,0)
	rcu_assign_pointer(callid_sock[sock->proto.pptp.src_addr.call_id],sock);
#else
	callid_sock[sock->proto.pptp.src_addr.call_id] = sock;
#endif
	set_bit(sock->proto.pptp.src_addr.call_id,callid_bitmap);
	res=0;

exit:	
	#if LINUX_VERSION_CODE > KERNEL_VERSION(2,6,0)
	spin_unlock(&chan_lock);
	#else
	write_unlock_bh(&chan_lock);
	#endif

	return res;
}

static void del_chan(struct pppox_sock *sock)
{
#if LINUX_VERSION_CODE > KERNEL_VERSION(2,6,0)
	spin_lock(&chan_lock);
#else
	write_lock_bh(&chan_lock);
#endif
	clear_bit(sock->proto.pptp.src_addr.call_id,callid_bitmap);
#if LINUX_VERSION_CODE > KERNEL_VERSION(2,6,0)
	rcu_assign_pointer(callid_sock[sock->proto.pptp.src_addr.call_id],NULL);
	spin_unlock(&chan_lock);
	synchronize_rcu();
#else
	callid_sock[sock->proto.pptp.src_addr.call_id] = NULL;
	write_unlock_bh(&chan_lock);
#endif
}

static int pptp_xmit(struct ppp_channel *chan, struct sk_buff *skb)
{
	struct sock *sk = (struct sock *) chan->private;
	struct pppox_sock *po = pppox_sk(sk);
	struct pptp_opt *opt=&po->proto.pptp;
	struct pptp_gre_header *hdr;
	unsigned int header_len=sizeof(*hdr);
	int err=0;
	int islcp;
	int len;
	unsigned char *data;
	u32 seq_recv;
	
	
	struct rtable *rt;     			/* Route to the other host */
	struct net_device *tdev;			/* Device to other host */
	struct iphdr  *iph;			/* Our new IP header */
	int    max_headroom;			/* The extra header space needed */

	if (SK_STATE(sk_pppox(po)) & PPPOX_DEAD)
	    goto tx_error;

#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,0)
	{
		struct rt_key key = {
			.dst=opt->dst_addr.sin_addr.s_addr,
			.src=opt->src_addr.sin_addr.s_addr,
			.tos=RT_TOS(0),
		};
		if ((err=ip_route_output_key(&rt, &key))) {
			goto tx_error;
		}
	}
#else
	{
		struct flowi fl = { .oif = 0,
				    .nl_u = { .ip4_u =
					      { .daddr = opt->dst_addr.sin_addr.s_addr,
						.saddr = opt->src_addr.sin_addr.s_addr,
						.tos = RT_TOS(0) } },
				    .proto = IPPROTO_GRE };
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,25)
		if ((err=ip_route_output_key(&rt, &fl))) {
#else
		if ((err=ip_route_output_key(&init_net,&rt, &fl))) {
#endif
			goto tx_error;
		}
	}
#endif
	tdev = rt->dst.dev;

#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,0)
	max_headroom = ((tdev->hard_header_len+15)&~15) + sizeof(*iph)+sizeof(*hdr)+2;
#else
	max_headroom = LL_RESERVED_SPACE(tdev) + sizeof(*iph)+sizeof(*hdr)+2;
#endif

#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,23) && !defined(HAVE_SKB_CLONE_WRITABLE)
	if (skb_headroom(skb) < max_headroom || skb_cloned(skb) || skb_shared(skb)) {
#else
	if (skb_headroom(skb) < max_headroom || skb_shared(skb) ||
		  (skb_cloned(skb) && !skb_clone_writable(skb,0))) {
#endif
 		struct sk_buff *new_skb = skb_realloc_headroom(skb, max_headroom);
		if (!new_skb) {
 			ip_rt_put(rt);
			goto tx_error;
		}
		if (skb->sk)
		skb_set_owner_w(new_skb, skb->sk);
		kfree_skb(skb);
		skb = new_skb;
	}

	data=skb->data;
	islcp=((data[0] << 8) + data[1])== PPP_LCP && 1 <= data[2] && data[2] <= 7;

	/* compress protocol field */
	if ((opt->ppp_flags & SC_COMP_PROT) && data[0]==0 && !islcp)
		skb_pull(skb,1);

	/*
		* Put in the address/control bytes if necessary
		*/
	if ((opt->ppp_flags & SC_COMP_AC) == 0 || islcp) {
		data=skb_push(skb,2);
		data[0]=PPP_ALLSTATIONS;
		data[1]=PPP_UI;
	}
	
	len=skb->len;
  
	seq_recv = opt->seq_recv;
  
	if (opt->ack_sent == seq_recv) header_len-=sizeof(hdr->ack);

	// Push down and install GRE header
	skb_push(skb,header_len);
	hdr=(struct pptp_gre_header *)(skb->data);

	hdr->flags       = PPTP_GRE_FLAG_K;
	hdr->ver         = PPTP_GRE_VER;
	hdr->protocol    = htons(PPTP_GRE_PROTO);
	hdr->call_id     = htons(opt->dst_addr.call_id);

	hdr->flags |= PPTP_GRE_FLAG_S;
	hdr->seq    = htonl(++opt->seq_sent);
#ifdef DEBUG
	if (log_level>=3 && opt->seq_sent<=log_packets)
		printk(KERN_INFO"PPTP[%i]: send packet: seq=%i",opt->src_addr.call_id,opt->seq_sent);
#endif
	if (opt->ack_sent != seq_recv)	{
	/* send ack with this message */
		hdr->ver |= PPTP_GRE_FLAG_A;
		hdr->ack  = htonl(seq_recv);
		opt->ack_sent = seq_recv;
#ifdef DEBUG
		if (log_level>=3 && opt->seq_sent<=log_packets)
			printk(" ack=%i",seq_recv);
#endif
	}
	hdr->payload_len = htons(len);
#ifdef DEBUG
	if (log_level>=3 && opt->seq_sent<=log_packets)
		printk("\n");
#endif

	/*
	 *	Push down and install the IP header.
	 */

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,31)
	skb_reset_transport_header(skb);
	skb_push(skb, sizeof(*iph));
	skb_reset_network_header(skb);
#elif LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,22)
	skb->transport_header = skb->network_header;
	skb_push(skb, sizeof(*iph));
	skb_reset_network_header(skb);
#else
	skb->h.raw = skb->nh.raw;
	skb->nh.raw = skb_push(skb, sizeof(*iph));
#endif
	memset(&(IPCB(skb)->opt), 0, sizeof(IPCB(skb)->opt));
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,16)
	IPCB(skb)->flags &= ~(IPSKB_XFRM_TUNNEL_SIZE | IPSKB_XFRM_TRANSFORMED |
			      IPSKB_REROUTED);
#endif

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,22)
	iph 			=	ip_hdr(skb);
#else
	iph 			=	skb->nh.iph;
#endif
	iph->version		=	4;
	iph->ihl		=	sizeof(struct iphdr) >> 2;
	if (ip_dont_fragment(sk, &rt->dst))
		iph->frag_off	=	htons(IP_DF);
	else
		iph->frag_off	=	0;
	iph->protocol		=	IPPROTO_GRE;
	iph->tos		=	0;
	iph->daddr		=	rt->rt_dst;
	iph->saddr		=	rt->rt_src;
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,0)
	iph->ttl = sk->protinfo.af_inet.ttl;
#else
	iph->ttl = dst_metric(&rt->dst, RTAX_HOPLIMIT);
#endif
	iph->tot_len = htons(skb->len);

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,31)
	skb_dst_drop(skb);
	skb_dst_set(skb,&rt->dst);
#else
	dst_release(skb->dst);
	skb->dst = &rt->dst;
#endif

	nf_reset(skb);

	skb->ip_summed = CHECKSUM_NONE;
	ip_select_ident(iph, &rt->dst, NULL);
	ip_send_check(iph);

#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,0)
 	err = NF_HOOK(PF_INET, NF_IP_LOCAL_OUT, skb, NULL, rt->dst.dev, ip_send);
#elif LINUX_VERSION_CODE < KERNEL_VERSION(2,6,25)
 	err = NF_HOOK(PF_INET, NF_IP_LOCAL_OUT, skb, NULL, rt->dst.dev, dst_output);
#else
 	err = ip_local_out(skb);
#endif

tx_error:
	return 1;
}

static int pptp_rcv_core(struct sock *sk,struct sk_buff *skb)
{
	struct pppox_sock *po = pppox_sk(sk);
	struct pptp_opt *opt=&po->proto.pptp;
	int headersize,payload_len,seq;
	u8 *payload;
	struct pptp_gre_header *header;

	if (!(SK_STATE(sk) & PPPOX_CONNECTED)) {
		if (sock_queue_rcv_skb(sk, skb))
			goto drop;
		return NET_RX_SUCCESS;
	}
	
	header = (struct pptp_gre_header *)(skb->data);

	/* test if acknowledgement present */
	if (PPTP_GRE_IS_A(header->ver)){
			u32 ack = (PPTP_GRE_IS_S(header->flags))?
					header->ack:header->seq; /* ack in different place if S = 0 */

			ack = ntohl( ack);

			if (ack > opt->ack_recv) opt->ack_recv = ack;
			/* also handle sequence number wrap-around  */
			if (WRAPPED(ack,opt->ack_recv)) opt->ack_recv = ack;
	}

	/* test if payload present */
	if (!PPTP_GRE_IS_S(header->flags)){
		goto drop;
	}

	headersize  = sizeof(*header);
	payload_len = ntohs(header->payload_len);
	seq         = ntohl(header->seq);

	/* no ack present? */
	if (!PPTP_GRE_IS_A(header->ver)) headersize -= sizeof(header->ack);
	/* check for incomplete packet (length smaller than expected) */
	if (skb->len - headersize < payload_len){
#ifdef DEBUG
		if (log_level>=1)
			printk(KERN_INFO"PPTP: discarding truncated packet (expected %d, got %d bytes)\n",
						payload_len, skb->len - headersize);
#endif
		goto drop;
	}

	payload=skb->data+headersize;
	/* check for expected sequence number */
	if ( seq < opt->seq_recv + 1 || WRAPPED(opt->seq_recv, seq) ){
		if ( (payload[0] == PPP_ALLSTATIONS) && (payload[1] == PPP_UI) &&
		     (PPP_PROTOCOL(payload) == PPP_LCP) &&
		     ((payload[4] == PPP_LCP_ECHOREQ) || (payload[4] == PPP_LCP_ECHOREP)) ){
#ifdef DEBUG
			if ( log_level >= 1)
				printk(KERN_INFO"PPTP[%i]: allowing old LCP Echo packet %d (expecting %d)\n", opt->src_addr.call_id,
							seq, opt->seq_recv + 1);
#endif
			goto allow_packet;
		}
#ifdef DEBUG
		if ( log_level >= 1)
			printk(KERN_INFO"PPTP[%i]: discarding duplicate or old packet %d (expecting %d)\n",opt->src_addr.call_id,
							seq, opt->seq_recv + 1);
#endif
	}else{
		opt->seq_recv = seq;
allow_packet:
#ifdef DEBUG
		if ( log_level >= 3 && opt->seq_sent<=log_packets)
			printk(KERN_INFO"PPTP[%i]: accepting packet %d size=%i (%02x %02x %02x %02x %02x %02x)\n",opt->src_addr.call_id, seq,payload_len,
				*(payload +0),
				*(payload +1),
				*(payload +2),
				*(payload +3),
				*(payload +4),
				*(payload +5));
#endif

		skb_pull(skb,headersize);

		if (payload[0] == PPP_ALLSTATIONS && payload[1] == PPP_UI){
			/* chop off address/control */
			if (skb->len < 3)
				goto drop;
			skb_pull(skb,2);
		}

		if ((*skb->data) & 1){
			/* protocol is compressed */
			skb_push(skb, 1)[0] = 0;
		}

		skb->ip_summed=CHECKSUM_NONE;
#if LINUX_VERSION_CODE > KERNEL_VERSION(2,6,21)
		skb_set_network_header(skb,skb->head-skb->data);
#endif
		ppp_input(&po->chan,skb);

		return NET_RX_SUCCESS;
	}
drop:
	kfree_skb(skb);
	return NET_RX_DROP;
}

static int pptp_rcv(struct sk_buff *skb)
{
	struct pppox_sock *po;
	struct pptp_gre_header *header;
	struct iphdr *iph;
#if LINUX_VERSION_CODE <= KERNEL_VERSION(2,6,0)
	int ret;
	struct sock *sk;
#endif

	if (skb->pkt_type != PACKET_HOST)
		goto drop;

#if !defined(CONFIG_NET_IPGRE_DEMUX) && !defined(CONFIG_NET_IPGRE_DEMUX_MODULE)
	if (!pskb_may_pull(skb, 12))
		goto drop;
#endif

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,22)
	iph = ip_hdr(skb);
#else
	iph = skb->nh.iph;
#endif

	header = (struct pptp_gre_header *)skb->data;

	if (    /* version should be 1 */
					((header->ver & 0x7F) != PPTP_GRE_VER) ||
					/* PPTP-GRE protocol for PPTP */
					(ntohs(header->protocol) != PPTP_GRE_PROTO)||
					/* flag C should be clear   */
					PPTP_GRE_IS_C(header->flags) ||
					/* flag R should be clear   */
					PPTP_GRE_IS_R(header->flags) ||
					/* flag K should be set     */
					(!PPTP_GRE_IS_K(header->flags)) ||
					/* routing and recursion ctrl = 0  */
					((header->flags&0xF) != 0)){
		/* if invalid, discard this packet */
#ifdef DEBUG
		if (log_level>=1)
			printk(KERN_INFO"PPTP: Discarding GRE: %X %X %X %X %X %X\n",
							header->ver&0x7F, ntohs(header->protocol),
							PPTP_GRE_IS_C(header->flags),
							PPTP_GRE_IS_R(header->flags),
							PPTP_GRE_IS_K(header->flags),
							header->flags & 0xF);
#endif
		goto drop;
	}


	if ((po=lookup_chan(htons(header->call_id),iph->saddr))) {
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,31)
		skb_dst_drop(skb);
#else
		dst_release(skb->dst);
		skb->dst = NULL;
#endif
		nf_reset(skb);
#if LINUX_VERSION_CODE <= KERNEL_VERSION(2,6,0)
		sk=sk_pppox(po);
    		bh_lock_sock(sk);
		/* Socket state is unknown, must put skb into backlog. */
		if (sk->lock.users != 0) {
		    sk_add_backlog(sk, skb);
		    ret = NET_RX_SUCCESS;
		} else {
		    ret = pptp_rcv_core(sk, skb);
		}
		bh_unlock_sock(sk);
		sock_put(sk);
		return ret;
		
#else /* LINUX_VERSION_CODE <= KERNEL_VERSION(2,6,0) */
		
#if LINUX_VERSION_CODE <= KERNEL_VERSION(2,6,19)
		return sk_receive_skb(sk_pppox(po), skb);
#else
		return sk_receive_skb(sk_pppox(po), skb, 0);
#endif
		
#endif /* LINUX_VERSION_CODE <= KERNEL_VERSION(2,6,0) */
	}else {
#ifdef DEBUG
		if (log_level>=1)
			printk(KERN_INFO"PPTP: Discarding packet from unknown call_id %i\n",htons(header->call_id));
#endif
	}

drop:
	kfree_skb(skb);
	return NET_RX_DROP;
}

static int pptp_bind(struct socket *sock,struct sockaddr *uservaddr,int sockaddr_len)
{
	struct sock *sk = sock->sk;
	struct sockaddr_pppox *sp = (struct sockaddr_pppox *) uservaddr;
	struct pppox_sock *po = pppox_sk(sk);
	struct pptp_opt *opt=&po->proto.pptp;
	int error=0;

#ifdef DEBUG	
	if (log_level>=1)
		printk(KERN_INFO"PPTP: bind: addr=%X call_id=%i\n",sp->sa_addr.pptp.sin_addr.s_addr,
						sp->sa_addr.pptp.call_id);
#endif
	lock_sock(sk);

	opt->src_addr=sp->sa_addr.pptp;
	if (add_chan(po))
		error=-EBUSY;
#ifdef DEBUG
	if (log_level>=1)
		printk(KERN_INFO"PPTP: using call_id %i\n",opt->src_addr.call_id);
#endif

	release_sock(sk);
	return error;
}

static int pptp_connect(struct socket *sock, struct sockaddr *uservaddr,
		  int sockaddr_len, int flags)
{
	struct sock *sk = sock->sk;
	struct sockaddr_pppox *sp = (struct sockaddr_pppox *) uservaddr;
	struct pppox_sock *po = pppox_sk(sk);
	struct pptp_opt *opt = &po->proto.pptp;
	struct rtable *rt;     			/* Route to the other host */
	int error=0;

	if (sp->sa_protocol != PX_PROTO_PPTP)
		return -EINVAL;
	
#ifdef DEBUG
	if (log_level>=1)
		printk(KERN_INFO"PPTP[%i]: connect: addr=%X call_id=%i\n",opt->src_addr.call_id,
						sp->sa_addr.pptp.sin_addr.s_addr,sp->sa_addr.pptp.call_id);
#endif
	
	if (lookup_chan_dst(sp->sa_addr.pptp.call_id,sp->sa_addr.pptp.sin_addr.s_addr))
		return -EALREADY;

	lock_sock(sk);
	/* Check for already bound sockets */
	if (SK_STATE(sk) & PPPOX_CONNECTED){
		error = -EBUSY;
		goto end;
	}

	/* Check for already disconnected sockets, on attempts to disconnect */
	if (SK_STATE(sk) & PPPOX_DEAD){
		error = -EALREADY;
		goto end;
	}

	if (!opt->src_addr.sin_addr.s_addr || !sp->sa_addr.pptp.sin_addr.s_addr){
		error = -EINVAL;
		goto end;
	}

	po->chan.private=sk;
	po->chan.ops=&pptp_chan_ops;

#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,0)
	{
		struct rt_key key = {
			.dst=opt->dst_addr.sin_addr.s_addr,
			.src=opt->src_addr.sin_addr.s_addr,
			.tos=RT_TOS(0),
		};
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,25)
		if (ip_route_output_key(&rt, &key)) {
#else
		if (ip_route_output_key(&init_net, &rt, &key)) {
#endif
			error = -EHOSTUNREACH;
			goto end;
		}
	}
#else
	{
		struct flowi fl = {
				    .nl_u = { .ip4_u =
					      { .daddr = opt->dst_addr.sin_addr.s_addr,
						.saddr = opt->src_addr.sin_addr.s_addr,
						.tos = RT_CONN_FLAGS(sk) } },
				    .proto = IPPROTO_GRE };
#if LINUX_VERSION_CODE > KERNEL_VERSION(2,6,18)
		security_sk_classify_flow(sk, &fl);
#endif
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,25)
		if (ip_route_output_key(&rt, &fl)){
#else
		if (ip_route_output_key(&init_net, &rt, &fl)){
#endif
			error = -EHOSTUNREACH;
			goto end;
		}
		sk_setup_caps(sk, &rt->dst);
	}
#endif
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,0)
	po->chan.mtu=PPP_MTU;
#else
	po->chan.mtu=dst_mtu(&rt->dst);
	if (!po->chan.mtu) po->chan.mtu=PPP_MTU;
#endif
	ip_rt_put(rt);
	po->chan.mtu-=PPTP_HEADER_OVERHEAD;

	po->chan.hdrlen=2+sizeof(struct pptp_gre_header);
	po->chan.hdrlen += LL_MAX_HEADER + sizeof(struct iphdr);
	error = ppp_register_channel(&po->chan);
	if (error){
		printk(KERN_ERR "PPTP: failed to register PPP channel (%d)\n",error);
		goto end;
	}

	opt->dst_addr=sp->sa_addr.pptp;
	SK_STATE(sk) = PPPOX_CONNECTED;

 end:
	release_sock(sk);
	return error;
}

static int pptp_getname(struct socket *sock, struct sockaddr *uaddr,
		  int *usockaddr_len, int peer)
{
	int len = sizeof(struct sockaddr_pppox);
	struct sockaddr_pppox sp;

	sp.sa_family	= AF_PPPOX;
	sp.sa_protocol	= PX_PROTO_PPTP;
	sp.sa_addr.pptp=pppox_sk(sock->sk)->proto.pptp.src_addr;

	memcpy(uaddr, &sp, len);

	*usockaddr_len = len;

	return 0;
}

static int pptp_release(struct socket *sock)
{
	struct sock *sk = sock->sk;
	struct pppox_sock *po;
	struct pptp_opt *opt;
	int error = 0;

	if (!sk)
	    return 0;

	lock_sock(sk);

#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,0)
	if (sk->dead)
#else
	if (sock_flag(sk, SOCK_DEAD))
#endif
	{
	    release_sock(sk);
	    return -EBADF;
	}
		
	po = pppox_sk(sk);
	opt=&po->proto.pptp;
	del_chan(po);

	pppox_unbind_sock(sk);
	SK_STATE(sk) = PPPOX_DEAD;

#ifdef DEBUG
	if (log_level>=1)
		printk(KERN_INFO"PPTP[%i]: release\n",opt->src_addr.call_id);
#endif

	sock_orphan(sk);
	sock->sk = NULL;

	release_sock(sk);
	sock_put(sk);

	return error;
}


#if LINUX_VERSION_CODE > KERNEL_VERSION(2,6,0)
static struct proto pptp_sk_proto = {
	.name	  = "PPTP",
	.owner	  = THIS_MODULE,
	.obj_size = sizeof(struct pppox_sock),
};
#endif

static struct proto_ops pptp_ops = {
    .family		= AF_PPPOX,
#if LINUX_VERSION_CODE > KERNEL_VERSION(2,6,0)
    .owner		= THIS_MODULE,
#endif
    .release		= pptp_release,
    .bind		=  pptp_bind,
    .connect		= pptp_connect,
    .socketpair		= sock_no_socketpair,
    .accept		= sock_no_accept,
    .getname		= pptp_getname,
    .poll		= sock_no_poll,
    .listen		= sock_no_listen,
    .shutdown		= sock_no_shutdown,
    .setsockopt		= sock_no_setsockopt,
    .getsockopt		= sock_no_getsockopt,
    .sendmsg		= sock_no_sendmsg,
    .recvmsg		= sock_no_recvmsg,
    .mmap		= sock_no_mmap,
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,16)
    .ioctl		= pppox_ioctl,
#endif
};


#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,0)
static void pptp_sock_destruct(struct sock *sk)
{
	skb_queue_purge(&sk->receive_queue);
	if (!(SK_STATE(sk) & PPPOX_DEAD)) {
		del_chan(pppox_sk(sk));
		pppox_unbind_sock(sk);
	}
	if (sk->protinfo.destruct_hook)
		kfree(sk->protinfo.destruct_hook);

	MOD_DEC_USE_COUNT;
}

static int pptp_create(struct socket *sock)
{
	int error = -ENOMEM;
	struct sock *sk;
	struct pppox_sock *po;
	struct pptp_opt *opt;

	MOD_INC_USE_COUNT;

	sk = sk_alloc(PF_PPPOX, GFP_KERNEL, 1);
	if (!sk)
		goto out;

	sock_init_data(sock, sk);

	sock->state = SS_UNCONNECTED;
	sock->ops   = &pptp_ops;

	//sk->sk_backlog_rcv = pppoe_rcv_core;
	sk->state	   = PPPOX_NONE;
	sk->type	   = SOCK_STREAM;
	sk->family	   = PF_PPPOX;
	sk->protocol	   = PX_PROTO_PPTP;

	sk->protinfo.pppox=kzalloc(sizeof(struct pppox_sock),GFP_KERNEL);
	sk->destruct=pptp_sock_destruct;
	sk->protinfo.destruct_hook=sk->protinfo.pppox;

	po = pppox_sk(sk);
	po->sk=sk;
	opt=&po->proto.pptp;

	opt->seq_sent=0; opt->seq_recv=0;
	opt->ack_recv=0; opt->ack_sent=0;

	error = 0;
out:
	return error;
}
#else
static void pptp_sock_destruct(struct sock *sk)
{
    if (!(SK_STATE(sk) & PPPOX_DEAD)){
	    del_chan(pppox_sk(sk));
	    pppox_unbind_sock(sk);
    }
    skb_queue_purge(&sk->sk_receive_queue);
}
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,24)
static int pptp_create(struct socket *sock)
#else
static int pptp_create(struct net *net, struct socket *sock)
#endif
{
	int error = -ENOMEM;
	struct sock *sk;
	struct pppox_sock *po;
	struct pptp_opt *opt;

#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,24)
	sk = sk_alloc(PF_PPPOX, GFP_KERNEL, &pptp_sk_proto, 1);
#else
	sk = sk_alloc(net,PF_PPPOX, GFP_KERNEL, &pptp_sk_proto);
#endif
	if (!sk)
		goto out;

	sock_init_data(sock, sk);

	sock->state = SS_UNCONNECTED;
	sock->ops   = &pptp_ops;

	sk->sk_backlog_rcv = pptp_rcv_core;
	sk->sk_state	   = PPPOX_NONE;
	sk->sk_type	   = SOCK_STREAM;
	sk->sk_family	   = PF_PPPOX;
	sk->sk_protocol	   = PX_PROTO_PPTP;
	sk->sk_destruct	   = pptp_sock_destruct;

	po = pppox_sk(sk);
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,0)
	po->sk=sk;
#endif
	opt=&po->proto.pptp;

	opt->seq_sent = 0; opt->seq_recv = 0xffffffff;
	opt->ack_recv = 0; opt->ack_sent = 0xffffffff;

	error = 0;
out:
	return error;
}
#endif


static int pptp_ppp_ioctl(struct ppp_channel *chan, unsigned int cmd,
			   unsigned long arg)
{
	struct sock *sk = (struct sock *) chan->private;
	struct pppox_sock *po = pppox_sk(sk);
	struct pptp_opt *opt=&po->proto.pptp;
	void __user *argp = (void __user *)arg;
	int __user *p = argp;
	int err, val;

	err = -EFAULT;
	switch (cmd) {
	case PPPIOCGFLAGS:
		val = opt->ppp_flags;
		if (put_user(val, p))
			break;
		err = 0;
		break;
	case PPPIOCSFLAGS:
		if (get_user(val, p))
			break;
		opt->ppp_flags = val & ~SC_RCV_BITS;
		err = 0;
		break;
	default:
		err = -ENOTTY;
	}

	return err;
}


static struct pppox_proto pppox_pptp_proto = {
    .create	= pptp_create,
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,15)
    .owner	= THIS_MODULE,
#endif
};

#if defined(CONFIG_NET_IPGRE_DEMUX) || defined(CONFIG_NET_IPGRE_DEMUX_MODULE)
static struct gre_protocol gre_pptp_protocol = {
	.handler	= pptp_rcv,
};
#elif LINUX_VERSION_CODE < KERNEL_VERSION(2,6,0)
static struct inet_protocol net_pptp_protocol = {
	.handler	= pptp_rcv,
	.protocol = IPPROTO_GRE,
	.name     = "PPTP",
};
#else
static struct net_protocol net_pptp_protocol = {
	.handler	= pptp_rcv,
};
#endif

static int __init pptp_init_module(void)
{
	int err=0;
	printk(KERN_INFO "PPTP driver version " PPTP_DRIVER_VERSION "\n");
printk("=== PPTP init ===\n");
#if LINUX_VERSION_CODE > KERNEL_VERSION(2,6,0)
	callid_sock = __vmalloc((MAX_CALLID + 1) * sizeof(void *),
	                        GFP_KERNEL | __GFP_ZERO, PAGE_KERNEL);
#else
	callid_sock = __vmalloc((MAX_CALLID + 1) * sizeof(void *),
	                        GFP_KERNEL, PAGE_KERNEL);
	memset(callid_sock, 0, (MAX_CALLID + 1) * sizeof(void *));
#endif
	if (!callid_sock) {
		printk(KERN_ERR "PPTP: cann't allocate memory\n");
		return -ENOMEM;
	}

#if defined(CONFIG_NET_IPGRE_DEMUX) || defined(CONFIG_NET_IPGRE_DEMUX_MODULE)
	if (gre_add_protocol(&gre_pptp_protocol, GREPROTO_PPTP) < 0) {
		printk(KERN_INFO "PPTP: can't add protocol\n");
		goto out_free_mem;
	}
#elif LINUX_VERSION_CODE < KERNEL_VERSION(2,6,0)
	inet_add_protocol(&net_pptp_protocol);
#else
	if (inet_add_protocol(&net_pptp_protocol, IPPROTO_GRE) < 0) {
		printk(KERN_INFO "PPTP: can't add protocol\n");
		goto out_free_mem;
	}
#endif

#if LINUX_VERSION_CODE > KERNEL_VERSION(2,6,0)
	err = proto_register(&pptp_sk_proto, 0);
	if (err){
		printk(KERN_INFO "PPTP: can't register sk_proto\n");
		goto out_inet_del_protocol;
	}
#endif

 	err = register_pppox_proto(PX_PROTO_PPTP, &pppox_pptp_proto);
	if (err){
		printk(KERN_INFO "PPTP: can't register pppox_proto\n");
		goto out_unregister_sk_proto;
	}
	
	return 0;
out_unregister_sk_proto:
#if LINUX_VERSION_CODE > KERNEL_VERSION(2,6,0)
	proto_unregister(&pptp_sk_proto);
#endif

#if LINUX_VERSION_CODE > KERNEL_VERSION(2,6,0)
out_inet_del_protocol:
#endif

#if defined(CONFIG_NET_IPGRE_DEMUX) || defined(CONFIG_NET_IPGRE_DEMUX_MODULE)
	gre_del_protocol(&gre_pptp_protocol, GREPROTO_PPTP);
#elif LINUX_VERSION_CODE < KERNEL_VERSION(2,6,0)
	inet_del_protocol(&net_pptp_protocol);
#else
	inet_del_protocol(&net_pptp_protocol, IPPROTO_GRE);
#endif
out_free_mem:
	vfree(callid_sock);
	
	return err;
}

static void __exit pptp_exit_module(void)
{
	unregister_pppox_proto(PX_PROTO_PPTP);
#if defined(CONFIG_NET_IPGRE_DEMUX) || defined(CONFIG_NET_IPGRE_DEMUX_MODULE)
#if LINUX_VERSION_CODE > KERNEL_VERSION(2,6,0)
	proto_unregister(&pptp_sk_proto);
#endif
	gre_del_protocol(&gre_pptp_protocol, GREPROTO_PPTP);
#elif LINUX_VERSION_CODE < KERNEL_VERSION(2,6,0)
	inet_del_protocol(&net_pptp_protocol);
#else
	proto_unregister(&pptp_sk_proto);
	inet_del_protocol(&net_pptp_protocol, IPPROTO_GRE);
#endif
	vfree(callid_sock);
}

module_init(pptp_init_module);
module_exit(pptp_exit_module);

MODULE_DESCRIPTION("Point-to-Point Tunneling Protocol for Linux");
MODULE_AUTHOR("Kozlov D. (xeb@mail.ru)");
MODULE_LICENSE("GPL");

#ifdef DEBUG
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,0)
MODULE_PARM(log_level,"i");
MODULE_PARM(log_packets,"i");
#else
module_param(log_level,int,0);
module_param(log_packets,int,0);
#endif
MODULE_PARM_DESC(log_level,"Logging level (default=0)");
#endif
