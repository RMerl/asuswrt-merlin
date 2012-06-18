/*
 * Routines to compress and uncompess tcp packets (for transmission
 * over low speed serial lines.
 *
 * Copyright (c) 1989 Regents of the University of California.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms are permitted
 * provided that the above copyright notice and this paragraph are
 * duplicated in all such forms and that any documentation,
 * advertising materials, and other materials related to such
 * distribution and use acknowledge that the software was developed
 * by the University of California, Berkeley.  The name of the
 * University may not be used to endorse or promote products derived
 * from this software without specific prior written permission.
 * THIS SOFTWARE IS PROVIDED ``AS IS'' AND WITHOUT ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, WITHOUT LIMITATION, THE IMPLIED
 * WARRANTIES OF MERCHANTIBILITY AND FITNESS FOR A PARTICULAR PURPOSE.
 *
 *	Van Jacobson (van@helios.ee.lbl.gov), Dec 31, 1989:
 *	- Initial distribution.
 *
 * Modified June 1993 by Paul Mackerras, paulus@cs.anu.edu.au,
 * so that the entire packet being decompressed doesn't have
 * to be in contiguous memory (just the compressed header).
 */

/*
 * This version is used under SunOS 4.x, Digital UNIX, AIX 4.x,
 * and SVR4 systems including Solaris 2.
 *
 * $Id: vjcompress.c,v 1.11 2004/01/17 05:47:55 carlsonj Exp $
 */

#include <sys/types.h>
#include <sys/param.h>

#ifdef SVR4
#ifndef __GNUC__
#include <sys/byteorder.h>	/* for ntohl, etc. */
#else
/* make sure we don't get the gnu "fixed" one! */
#include "/usr/include/sys/byteorder.h"
#endif
#endif

#ifdef __osf__
#include <net/net_globals.h>
#endif
#include <netinet/in.h>

#ifdef AIX4
#define _NETINET_IN_SYSTM_H_
typedef u_long  n_long;
#else
#include <netinet/in_systm.h>
#endif

#ifdef SOL2
#include <sys/sunddi.h>
#endif

#include <netinet/ip.h>
#include <netinet/tcp.h>

#include <net/ppp_defs.h>
#include <net/vjcompress.h>

#ifndef VJ_NO_STATS
#define INCR(counter) ++comp->stats.counter
#else
#define INCR(counter)
#endif

#define BCMP(p1, p2, n) bcmp((char *)(p1), (char *)(p2), (int)(n))
#undef  BCOPY
#define BCOPY(p1, p2, n) bcopy((char *)(p1), (char *)(p2), (int)(n))
#ifndef KERNEL
#define ovbcopy bcopy
#endif

#ifdef __osf__
#define getip_hl(base)	(((base).ip_vhl)&0xf)
#define getth_off(base)	((((base).th_xoff)&0xf0)>>4)

#else
#define getip_hl(base)	((base).ip_hl)
#define getth_off(base)	((base).th_off)
#endif

void
vj_compress_init(comp, max_state)
    struct vjcompress *comp;
    int max_state;
{
    register u_int i;
    register struct cstate *tstate = comp->tstate;

    if (max_state == -1)
	max_state = MAX_STATES - 1;
    bzero((char *)comp, sizeof(*comp));
    for (i = max_state; i > 0; --i) {
	tstate[i].cs_id = i;
	tstate[i].cs_next = &tstate[i - 1];
    }
    tstate[0].cs_next = &tstate[max_state];
    tstate[0].cs_id = 0;
    comp->last_cs = &tstate[0];
    comp->last_recv = 255;
    comp->last_xmit = 255;
    comp->flags = VJF_TOSS;
}


/* ENCODE encodes a number that is known to be non-zero.  ENCODEZ
 * checks for zero (since zero has to be encoded in the long, 3 byte
 * form).
 */
#define ENCODE(n) { \
	if ((u_short)(n) >= 256) { \
		*cp++ = 0; \
		cp[1] = (n); \
		cp[0] = (n) >> 8; \
		cp += 2; \
	} else { \
		*cp++ = (n); \
	} \
}
#define ENCODEZ(n) { \
	if ((u_short)(n) >= 256 || (u_short)(n) == 0) { \
		*cp++ = 0; \
		cp[1] = (n); \
		cp[0] = (n) >> 8; \
		cp += 2; \
	} else { \
		*cp++ = (n); \
	} \
}

#define DECODEL(f) { \
	if (*cp == 0) {\
		u_int32_t tmp = ntohl(f) + ((cp[1] << 8) | cp[2]); \
		(f) = htonl(tmp); \
		cp += 3; \
	} else { \
		u_int32_t tmp = ntohl(f) + (u_int32_t)*cp++; \
		(f) = htonl(tmp); \
	} \
}

#define DECODES(f) { \
	if (*cp == 0) {\
		u_short tmp = ntohs(f) + ((cp[1] << 8) | cp[2]); \
		(f) = htons(tmp); \
		cp += 3; \
	} else { \
		u_short tmp = ntohs(f) + (u_int32_t)*cp++; \
		(f) = htons(tmp); \
	} \
}

#define DECODEU(f) { \
	if (*cp == 0) {\
		(f) = htons((cp[1] << 8) | cp[2]); \
		cp += 3; \
	} else { \
		(f) = htons((u_int32_t)*cp++); \
	} \
}

u_int
vj_compress_tcp(ip, mlen, comp, compress_cid, vjhdrp)
    register struct ip *ip;
    u_int mlen;
    struct vjcompress *comp;
    int compress_cid;
    u_char **vjhdrp;
{
    register struct cstate *cs = comp->last_cs->cs_next;
    register u_int hlen = getip_hl(*ip);
    register struct tcphdr *oth;
    register struct tcphdr *th;
    register u_int deltaS, deltaA;
    register u_int changes = 0;
    u_char new_seq[16];
    register u_char *cp = new_seq;

    /*
     * Bail if this is an IP fragment or if the TCP packet isn't
     * `compressible' (i.e., ACK isn't set or some other control bit is
     * set).  (We assume that the caller has already made sure the
     * packet is IP proto TCP).
     */
    if ((ip->ip_off & htons(0x3fff)) || mlen < 40)
	return (TYPE_IP);

    th = (struct tcphdr *)&((int *)ip)[hlen];
    if ((th->th_flags & (TH_SYN|TH_FIN|TH_RST|TH_ACK)) != TH_ACK)
	return (TYPE_IP);
    /*
     * Packet is compressible -- we're going to send either a
     * COMPRESSED_TCP or UNCOMPRESSED_TCP packet.  Either way we need
     * to locate (or create) the connection state.  Special case the
     * most recently used connection since it's most likely to be used
     * again & we don't have to do any reordering if it's used.
     */
    INCR(vjs_packets);
    if (ip->ip_src.s_addr != cs->cs_ip.ip_src.s_addr ||
	ip->ip_dst.s_addr != cs->cs_ip.ip_dst.s_addr ||
	*(int *)th != ((int *)&cs->cs_ip)[getip_hl(cs->cs_ip)]) {
	/*
	 * Wasn't the first -- search for it.
	 *
	 * States are kept in a circularly linked list with
	 * last_cs pointing to the end of the list.  The
	 * list is kept in lru order by moving a state to the
	 * head of the list whenever it is referenced.  Since
	 * the list is short and, empirically, the connection
	 * we want is almost always near the front, we locate
	 * states via linear search.  If we don't find a state
	 * for the datagram, the oldest state is (re-)used.
	 */
	register struct cstate *lcs;
	register struct cstate *lastcs = comp->last_cs;

	do {
	    lcs = cs; cs = cs->cs_next;
	    INCR(vjs_searches);
	    if (ip->ip_src.s_addr == cs->cs_ip.ip_src.s_addr
		&& ip->ip_dst.s_addr == cs->cs_ip.ip_dst.s_addr
		&& *(int *)th == ((int *)&cs->cs_ip)[getip_hl(cs->cs_ip)])
		goto found;
	} while (cs != lastcs);

	/*
	 * Didn't find it -- re-use oldest cstate.  Send an
	 * uncompressed packet that tells the other side what
	 * connection number we're using for this conversation.
	 * Note that since the state list is circular, the oldest
	 * state points to the newest and we only need to set
	 * last_cs to update the lru linkage.
	 */
	INCR(vjs_misses);
	comp->last_cs = lcs;
	hlen += getth_off(*th);
	hlen <<= 2;
	if (hlen > mlen)
	    return (TYPE_IP);
	goto uncompressed;

    found:
	/*
	 * Found it -- move to the front on the connection list.
	 */
	if (cs == lastcs)
	    comp->last_cs = lcs;
	else {
	    lcs->cs_next = cs->cs_next;
	    cs->cs_next = lastcs->cs_next;
	    lastcs->cs_next = cs;
	}
    }

    /*
     * Make sure that only what we expect to change changed. The first
     * line of the `if' checks the IP protocol version, header length &
     * type of service.  The 2nd line checks the "Don't fragment" bit.
     * The 3rd line checks the time-to-live and protocol (the protocol
     * check is unnecessary but costless).  The 4th line checks the TCP
     * header length.  The 5th line checks IP options, if any.  The 6th
     * line checks TCP options, if any.  If any of these things are
     * different between the previous & current datagram, we send the
     * current datagram `uncompressed'.
     */
    oth = (struct tcphdr *)&((int *)&cs->cs_ip)[hlen];
    deltaS = hlen;
    hlen += getth_off(*th);
    hlen <<= 2;
    if (hlen > mlen)
	return (TYPE_IP);

    if (((u_short *)ip)[0] != ((u_short *)&cs->cs_ip)[0] ||
	((u_short *)ip)[3] != ((u_short *)&cs->cs_ip)[3] ||
	((u_short *)ip)[4] != ((u_short *)&cs->cs_ip)[4] ||
	getth_off(*th) != getth_off(*oth) ||
	(deltaS > 5 && BCMP(ip + 1, &cs->cs_ip + 1, (deltaS - 5) << 2)) ||
	(getth_off(*th) > 5 && BCMP(th + 1, oth + 1, (getth_off(*th) - 5) << 2)))
	goto uncompressed;

    /*
     * Figure out which of the changing fields changed.  The
     * receiver expects changes in the order: urgent, window,
     * ack, seq (the order minimizes the number of temporaries
     * needed in this section of code).
     */
    if (th->th_flags & TH_URG) {
	deltaS = ntohs(th->th_urp);
	ENCODEZ(deltaS);
	changes |= NEW_U;
    } else if (th->th_urp != oth->th_urp)
	/* argh! URG not set but urp changed -- a sensible
	 * implementation should never do this but RFC793
	 * doesn't prohibit the change so we have to deal
	 * with it. */
	goto uncompressed;

    if ((deltaS = (u_short)(ntohs(th->th_win) - ntohs(oth->th_win))) > 0) {
	ENCODE(deltaS);
	changes |= NEW_W;
    }

    if ((deltaA = ntohl(th->th_ack) - ntohl(oth->th_ack)) > 0) {
	if (deltaA > 0xffff)
	    goto uncompressed;
	ENCODE(deltaA);
	changes |= NEW_A;
    }

    if ((deltaS = ntohl(th->th_seq) - ntohl(oth->th_seq)) > 0) {
	if (deltaS > 0xffff)
	    goto uncompressed;
	ENCODE(deltaS);
	changes |= NEW_S;
    }

    switch(changes) {

    case 0:
	/*
	 * Nothing changed. If this packet contains data and the
	 * last one didn't, this is probably a data packet following
	 * an ack (normal on an interactive connection) and we send
	 * it compressed.  Otherwise it's probably a retransmit,
	 * retransmitted ack or window probe.  Send it uncompressed
	 * in case the other side missed the compressed version.
	 */
	if (ip->ip_len != cs->cs_ip.ip_len &&
	    ntohs(cs->cs_ip.ip_len) == hlen)
	    break;

	/* (fall through) */

    case SPECIAL_I:
    case SPECIAL_D:
	/*
	 * actual changes match one of our special case encodings --
	 * send packet uncompressed.
	 */
	goto uncompressed;

    case NEW_S|NEW_A:
	if (deltaS == deltaA && deltaS == ntohs(cs->cs_ip.ip_len) - hlen) {
	    /* special case for echoed terminal traffic */
	    changes = SPECIAL_I;
	    cp = new_seq;
	}
	break;

    case NEW_S:
	if (deltaS == ntohs(cs->cs_ip.ip_len) - hlen) {
	    /* special case for data xfer */
	    changes = SPECIAL_D;
	    cp = new_seq;
	}
	break;
    }

    deltaS = ntohs(ip->ip_id) - ntohs(cs->cs_ip.ip_id);
    if (deltaS != 1) {
	ENCODEZ(deltaS);
	changes |= NEW_I;
    }
    if (th->th_flags & TH_PUSH)
	changes |= TCP_PUSH_BIT;
    /*
     * Grab the cksum before we overwrite it below.  Then update our
     * state with this packet's header.
     */
    deltaA = ntohs(th->th_sum);
    BCOPY(ip, &cs->cs_ip, hlen);

    /*
     * We want to use the original packet as our compressed packet.
     * (cp - new_seq) is the number of bytes we need for compressed
     * sequence numbers.  In addition we need one byte for the change
     * mask, one for the connection id and two for the tcp checksum.
     * So, (cp - new_seq) + 4 bytes of header are needed.  hlen is how
     * many bytes of the original packet to toss so subtract the two to
     * get the new packet size.
     */
    deltaS = cp - new_seq;
    cp = (u_char *)ip;
    if (compress_cid == 0 || comp->last_xmit != cs->cs_id) {
	comp->last_xmit = cs->cs_id;
	hlen -= deltaS + 4;
	*vjhdrp = (cp += hlen);
	*cp++ = changes | NEW_C;
	*cp++ = cs->cs_id;
    } else {
	hlen -= deltaS + 3;
	*vjhdrp = (cp += hlen);
	*cp++ = changes;
    }
    *cp++ = deltaA >> 8;
    *cp++ = deltaA;
    BCOPY(new_seq, cp, deltaS);
    INCR(vjs_compressed);
    return (TYPE_COMPRESSED_TCP);

    /*
     * Update connection state cs & send uncompressed packet (that is,
     * a regular ip/tcp packet but with the 'conversation id' we hope
     * to use on future compressed packets in the protocol field).
     */
 uncompressed:
    BCOPY(ip, &cs->cs_ip, hlen);
    ip->ip_p = cs->cs_id;
    comp->last_xmit = cs->cs_id;
    return (TYPE_UNCOMPRESSED_TCP);
}

/*
 * Called when we may have missed a packet.
 */
void
vj_uncompress_err(comp)
    struct vjcompress *comp;
{
    comp->flags |= VJF_TOSS;
    INCR(vjs_errorin);
}

/*
 * "Uncompress" a packet of type TYPE_UNCOMPRESSED_TCP.
 */
int
vj_uncompress_uncomp(buf, buflen, comp)
    u_char *buf;
    int buflen;
    struct vjcompress *comp;
{
    register u_int hlen;
    register struct cstate *cs;
    register struct ip *ip;

    ip = (struct ip *) buf;
    hlen = getip_hl(*ip) << 2;
    if (ip->ip_p >= MAX_STATES
	|| hlen + sizeof(struct tcphdr) > buflen
	|| (hlen += getth_off(*((struct tcphdr *)&((char *)ip)[hlen])) << 2)
	    > buflen
	|| hlen > MAX_HDR) {
	comp->flags |= VJF_TOSS;
	INCR(vjs_errorin);
	return (0);
    }
    cs = &comp->rstate[comp->last_recv = ip->ip_p];
    comp->flags &=~ VJF_TOSS;
    ip->ip_p = IPPROTO_TCP;
    BCOPY(ip, &cs->cs_ip, hlen);
    cs->cs_hlen = hlen;
    INCR(vjs_uncompressedin);
    return (1);
}

/*
 * Uncompress a packet of type TYPE_COMPRESSED_TCP.
 * The packet starts at buf and is of total length total_len.
 * The first buflen bytes are at buf; this must include the entire
 * compressed TCP/IP header.  This procedure returns the length
 * of the VJ header, with a pointer to the uncompressed IP header
 * in *hdrp and its length in *hlenp.
 */
int
vj_uncompress_tcp(buf, buflen, total_len, comp, hdrp, hlenp)
    u_char *buf;
    int buflen, total_len;
    struct vjcompress *comp;
    u_char **hdrp;
    u_int *hlenp;
{
    register u_char *cp;
    register u_int hlen, changes;
    register struct tcphdr *th;
    register struct cstate *cs;
    register u_short *bp;
    register u_int vjlen;
    register u_int32_t tmp;

    INCR(vjs_compressedin);
    cp = buf;
    changes = *cp++;
    if (changes & NEW_C) {
	/* Make sure the state index is in range, then grab the state.
	 * If we have a good state index, clear the 'discard' flag. */
	if (*cp >= MAX_STATES)
	    goto bad;

	comp->flags &=~ VJF_TOSS;
	comp->last_recv = *cp++;
    } else {
	/* this packet has an implicit state index.  If we've
	 * had a line error since the last time we got an
	 * explicit state index, we have to toss the packet. */
	if (comp->flags & VJF_TOSS) {
	    INCR(vjs_tossed);
	    return (-1);
	}
    }
    cs = &comp->rstate[comp->last_recv];
    hlen = getip_hl(cs->cs_ip) << 2;
    th = (struct tcphdr *)&((u_char *)&cs->cs_ip)[hlen];
    th->th_sum = htons((*cp << 8) | cp[1]);
    cp += 2;
    if (changes & TCP_PUSH_BIT)
	th->th_flags |= TH_PUSH;
    else
	th->th_flags &=~ TH_PUSH;

    switch (changes & SPECIALS_MASK) {
    case SPECIAL_I:
	{
	    register u_int32_t i = ntohs(cs->cs_ip.ip_len) - cs->cs_hlen;
	    /* some compilers can't nest inline assembler.. */
	    tmp = ntohl(th->th_ack) + i;
	    th->th_ack = htonl(tmp);
	    tmp = ntohl(th->th_seq) + i;
	    th->th_seq = htonl(tmp);
	}
	break;

    case SPECIAL_D:
	/* some compilers can't nest inline assembler.. */
	tmp = ntohl(th->th_seq) + ntohs(cs->cs_ip.ip_len) - cs->cs_hlen;
	th->th_seq = htonl(tmp);
	break;

    default:
	if (changes & NEW_U) {
	    th->th_flags |= TH_URG;
	    DECODEU(th->th_urp);
	} else
	    th->th_flags &=~ TH_URG;
	if (changes & NEW_W)
	    DECODES(th->th_win);
	if (changes & NEW_A)
	    DECODEL(th->th_ack);
	if (changes & NEW_S)
	    DECODEL(th->th_seq);
	break;
    }
    if (changes & NEW_I) {
	DECODES(cs->cs_ip.ip_id);
    } else {
	cs->cs_ip.ip_id = ntohs(cs->cs_ip.ip_id) + 1;
	cs->cs_ip.ip_id = htons(cs->cs_ip.ip_id);
    }

    /*
     * At this point, cp points to the first byte of data in the
     * packet.  Fill in the IP total length and update the IP
     * header checksum.
     */
    vjlen = cp - buf;
    buflen -= vjlen;
    if (buflen < 0)
	/* we must have dropped some characters (crc should detect
	 * this but the old slip framing won't) */
	goto bad;

    total_len += cs->cs_hlen - vjlen;
    cs->cs_ip.ip_len = htons(total_len);

    /* recompute the ip header checksum */
    bp = (u_short *) &cs->cs_ip;
    cs->cs_ip.ip_sum = 0;
    for (changes = 0; hlen > 0; hlen -= 2)
	changes += *bp++;
    changes = (changes & 0xffff) + (changes >> 16);
    changes = (changes & 0xffff) + (changes >> 16);
    cs->cs_ip.ip_sum = ~ changes;

    *hdrp = (u_char *) &cs->cs_ip;
    *hlenp = cs->cs_hlen;
    return vjlen;

 bad:
    comp->flags |= VJF_TOSS;
    INCR(vjs_errorin);
    return (-1);
}
