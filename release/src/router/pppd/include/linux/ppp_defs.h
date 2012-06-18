/*	$Id: ppp_defs.h,v 1.11 2002/12/06 09:49:15 paulus Exp $	*/

/*
 * ppp_defs.h - PPP definitions.
 *
 * Copyright (c) 1989-2002 Paul Mackerras. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the
 *    distribution.
 *
 * 3. The name(s) of the authors of this software must not be used to
 *    endorse or promote products derived from this software without
 *    prior written permission.
 *
 * 4. Redistributions of any form whatsoever must retain the following
 *    acknowledgment:
 *    "This product includes software developed by Paul Mackerras
 *     <paulus@samba.org>".
 *
 * THE AUTHORS OF THIS SOFTWARE DISCLAIM ALL WARRANTIES WITH REGARD TO
 * THIS SOFTWARE, INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY
 * AND FITNESS, IN NO EVENT SHALL THE AUTHORS BE LIABLE FOR ANY
 * SPECIAL, INDIRECT OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN
 * AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING
 * OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

/*
 *  ==FILEVERSION 20020521==
 *
 *  NOTE TO MAINTAINERS:
 *     If you modify this file at all, please set the above date.
 *     ppp_defs.h is shipped with a PPP distribution as well as with the kernel;
 *     if everyone increases the FILEVERSION number above, then scripts
 *     can do the right thing when deciding whether to install a new ppp_defs.h
 *     file.  Don't change the format of that line otherwise, so the
 *     installation script can recognize it.
 */

#ifndef _PPP_DEFS_H_
#define _PPP_DEFS_H_

/*
 * The basic PPP frame.
 */
#define PPP_HDRLEN	4	/* octets for standard ppp header */
#define PPP_FCSLEN	2	/* octets for FCS */
#define PPP_MRU		1500	/* default MRU = max length of info field */

#define PPP_ADDRESS(p)	(((__u8 *)(p))[0])
#define PPP_CONTROL(p)	(((__u8 *)(p))[1])
#define PPP_PROTOCOL(p)	((((__u8 *)(p))[2] << 8) + ((__u8 *)(p))[3])

/*
 * Significant octet values.
 */
#define	PPP_ALLSTATIONS	0xff	/* All-Stations broadcast address */
#define	PPP_UI		0x03	/* Unnumbered Information */
#define	PPP_FLAG	0x7e	/* Flag Sequence */
#define	PPP_ESCAPE	0x7d	/* Asynchronous Control Escape */
#define	PPP_TRANS	0x20	/* Asynchronous transparency modifier */

/*
 * Protocol field values.
 */
#define PPP_IP		0x21	/* Internet Protocol */
#define PPP_AT		0x29	/* AppleTalk Protocol */
#define PPP_IPX		0x2b	/* IPX protocol */
#define	PPP_VJC_COMP	0x2d	/* VJ compressed TCP */
#define	PPP_VJC_UNCOMP	0x2f	/* VJ uncompressed TCP */
#define PPP_MP		0x3d	/* Multilink protocol */
#define PPP_IPV6	0x57	/* Internet Protocol Version 6 */
#define PPP_COMPFRAG	0xfb	/* fragment compressed below bundle */
#define PPP_COMP	0xfd	/* compressed packet */
#define PPP_IPCP	0x8021	/* IP Control Protocol */
#define PPP_ATCP	0x8029	/* AppleTalk Control Protocol */
#define PPP_IPXCP	0x802b	/* IPX Control Protocol */
#define PPP_IPV6CP	0x8057	/* IPv6 Control Protocol */
#define PPP_CCPFRAG	0x80fb	/* CCP at link level (below MP bundle) */
#define PPP_CCP		0x80fd	/* Compression Control Protocol */
#define PPP_ECPFRAG	0x8055	/* ECP at link level (below MP bundle) */
#define PPP_ECP		0x8053	/* Encryption Control Protocol */
#define PPP_LCP		0xc021	/* Link Control Protocol */
#define PPP_PAP		0xc023	/* Password Authentication Protocol */
#define PPP_LQR		0xc025	/* Link Quality Report protocol */
#define PPP_CHAP	0xc223	/* Cryptographic Handshake Auth. Protocol */
#define PPP_CBCP	0xc029	/* Callback Control Protocol */

/*
 * Values for FCS calculations.
 */

#define PPP_INITFCS	0xffff	/* Initial FCS value */
#define PPP_GOODFCS	0xf0b8	/* Good final FCS value */
#define PPP_FCS(fcs, c)	(((fcs) >> 8) ^ fcstab[((fcs) ^ (c)) & 0xff])

/*
 * Extended asyncmap - allows any character to be escaped.
 */

typedef __u32		ext_accm[8];

/*
 * What to do with network protocol (NP) packets.
 */
enum NPmode {
    NPMODE_PASS,		/* pass the packet through */
    NPMODE_DROP,		/* silently drop the packet */
    NPMODE_ERROR,		/* return an error */
    NPMODE_QUEUE		/* save it up for later. */
};

/*
 * Statistics for LQRP and pppstats
 */
struct pppstat	{
    __u32	ppp_discards;	/* # frames discarded */

    __u32	ppp_ibytes;	/* bytes received */
    __u32	ppp_ioctects;	/* bytes received not in error */
    __u32	ppp_ipackets;	/* packets received */
    __u32	ppp_ierrors;	/* receive errors */
    __u32	ppp_ilqrs;	/* # LQR frames received */

    __u32	ppp_obytes;	/* raw bytes sent */
    __u32	ppp_ooctects;	/* frame bytes sent */
    __u32	ppp_opackets;	/* packets sent */
    __u32	ppp_oerrors;	/* transmit errors */ 
    __u32	ppp_olqrs;	/* # LQR frames sent */
};

struct vjstat {
    __u32	vjs_packets;	/* outbound packets */
    __u32	vjs_compressed;	/* outbound compressed packets */
    __u32	vjs_searches;	/* searches for connection state */
    __u32	vjs_misses;	/* times couldn't find conn. state */
    __u32	vjs_uncompressedin; /* inbound uncompressed packets */
    __u32	vjs_compressedin;   /* inbound compressed packets */
    __u32	vjs_errorin;	/* inbound unknown type packets */
    __u32	vjs_tossed;	/* inbound packets tossed because of error */
};

struct compstat {
    __u32	unc_bytes;	/* total uncompressed bytes */
    __u32	unc_packets;	/* total uncompressed packets */
    __u32	comp_bytes;	/* compressed bytes */
    __u32	comp_packets;	/* compressed packets */
    __u32	inc_bytes;	/* incompressible bytes */
    __u32	inc_packets;	/* incompressible packets */

    /* the compression ratio is defined as in_count / bytes_out */
    __u32       in_count;	/* Bytes received */
    __u32       bytes_out;	/* Bytes transmitted */

    double	ratio;		/* not computed in kernel. */
};

struct ppp_stats {
    struct pppstat	p;	/* basic PPP statistics */
    struct vjstat	vj;	/* VJ header compression statistics */
};

struct ppp_comp_stats {
    struct compstat	c;	/* packet compression statistics */
    struct compstat	d;	/* packet decompression statistics */
};

/*
 * The following structure records the time in seconds since
 * the last NP packet was sent or received.
 */
struct ppp_idle {
    time_t xmit_idle;		/* time since last NP packet sent */
    time_t recv_idle;		/* time since last NP packet received */
};

#ifndef __P
#ifdef __STDC__
#define __P(x)	x
#else
#define __P(x)	()
#endif
#endif

#endif /* _PPP_DEFS_H_ */
