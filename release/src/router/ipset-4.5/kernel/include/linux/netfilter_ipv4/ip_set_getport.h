#ifndef _IP_SET_GETPORT_H
#define _IP_SET_GETPORT_H

#ifdef __KERNEL__

#define INVALID_PORT	(MAX_RANGE + 1)

/* We must handle non-linear skbs */
static inline ip_set_ip_t
get_port(const struct sk_buff *skb, const u_int32_t *flags)
{
	struct iphdr *iph = ip_hdr(skb);
	u_int16_t offset = ntohs(iph->frag_off) & IP_OFFSET;
	switch (iph->protocol) {
	case IPPROTO_TCP: {
		struct tcphdr tcph;
		
		/* See comments at tcp_match in ip_tables.c */
		if (offset)
			return INVALID_PORT;

		if (skb_copy_bits(skb, ip_hdr(skb)->ihl*4, &tcph, sizeof(tcph)) < 0)
			/* No choice either */
			return INVALID_PORT;
	     	
	     	return ntohs(flags[0] & IPSET_SRC ?
			     tcph.source : tcph.dest);
	    }
	case IPPROTO_UDP: {
		struct udphdr udph;

		if (offset)
			return INVALID_PORT;

		if (skb_copy_bits(skb, ip_hdr(skb)->ihl*4, &udph, sizeof(udph)) < 0)
			/* No choice either */
			return INVALID_PORT;
	     	
	     	return ntohs(flags[0] & IPSET_SRC ?
			     udph.source : udph.dest);
	    }
	default:
		return INVALID_PORT;
	}
}
#endif				/* __KERNEL__ */

#endif /*_IP_SET_GETPORT_H*/
