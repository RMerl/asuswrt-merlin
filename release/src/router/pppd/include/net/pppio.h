/*
 * pppio.h - ioctl and other misc. definitions for STREAMS modules.
 *
 * Copyright (c) 1994 Paul Mackerras. All rights reserved.
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
 *
 * $Id: pppio.h,v 1.9 2002/12/06 09:49:15 paulus Exp $
 */

#define _PPPIO(n)	(('p' << 8) + (n))

#define PPPIO_NEWPPA	_PPPIO(130)	/* allocate a new PPP unit */
#define PPPIO_GETSTAT	_PPPIO(131)	/* get PPP statistics */
#define PPPIO_GETCSTAT	_PPPIO(132)	/* get PPP compression stats */
#define PPPIO_MTU	_PPPIO(133)	/* set max transmission unit */
#define PPPIO_MRU	_PPPIO(134)	/* set max receive unit */
#define PPPIO_CFLAGS	_PPPIO(135)	/* set/clear/get compression flags */
#define PPPIO_XCOMP	_PPPIO(136)	/* alloc transmit compressor */
#define PPPIO_RCOMP	_PPPIO(137)	/* alloc receive decompressor */
#define PPPIO_XACCM	_PPPIO(138)	/* set transmit asyncmap */
#define PPPIO_RACCM	_PPPIO(139)	/* set receive asyncmap */
#define PPPIO_VJINIT	_PPPIO(140)	/* initialize VJ comp/decomp */
#define PPPIO_ATTACH	_PPPIO(141)	/* attach to a ppa (without putmsg) */
#define PPPIO_LASTMOD	_PPPIO(142)	/* mark last ppp module */
#define PPPIO_GCLEAN	_PPPIO(143)	/* get 8-bit-clean flags */
#define PPPIO_DEBUG	_PPPIO(144)	/* request debug information */
#define PPPIO_BIND	_PPPIO(145)	/* bind to SAP */
#define PPPIO_NPMODE	_PPPIO(146)	/* set mode for handling data pkts */
#define PPPIO_GIDLE	_PPPIO(147)	/* get time since last data pkt */
#define PPPIO_PASSFILT	_PPPIO(148)	/* set filter for packets to pass */
#define PPPIO_ACTIVEFILT _PPPIO(149)	/* set filter for "link active" pkts */

/*
 * Values for PPPIO_CFLAGS
 */
#define COMP_AC		0x1		/* compress address/control */
#define DECOMP_AC	0x2		/* decompress address/control */
#define COMP_PROT	0x4		/* compress PPP protocol */
#define DECOMP_PROT	0x8		/* decompress PPP protocol */

#define COMP_VJC	0x10		/* compress TCP/IP headers */
#define COMP_VJCCID	0x20		/* compress connection ID as well */
#define DECOMP_VJC	0x40		/* decompress TCP/IP headers */
#define DECOMP_VJCCID	0x80		/* accept compressed connection ID */

#define CCP_ISOPEN	0x100		/* look at CCP packets */
#define CCP_ISUP	0x200		/* do packet comp/decomp */
#define CCP_ERROR	0x400		/* (status) error in packet decomp */
#define CCP_FATALERROR	0x800		/* (status) fatal error ditto */
#define CCP_COMP_RUN	0x1000		/* (status) seen CCP ack sent */
#define CCP_DECOMP_RUN	0x2000		/* (status) seen CCP ack rcvd */

/*
 * Values for 8-bit-clean flags.
 */
#define RCV_B7_0	1		/* have rcvd char with bit 7 = 0 */
#define RCV_B7_1	2		/* have rcvd char with bit 7 = 1 */
#define RCV_EVNP	4		/* have rcvd char with even parity */
#define RCV_ODDP	8		/* have rcvd char with odd parity */

/*
 * Values for the first byte of M_CTL messages passed between
 * PPP modules.
 */
#define PPPCTL_OERROR	0xe0		/* output error [up] */
#define PPPCTL_IERROR	0xe1		/* input error (e.g. FCS) [up] */
#define PPPCTL_MTU	0xe2		/* set MTU [down] */
#define PPPCTL_MRU	0xe3		/* set MRU [down] */
#define PPPCTL_UNIT	0xe4		/* note PPP unit number [down] */

/*
 * Values for the integer argument to PPPIO_DEBUG.
 */
#define PPPDBG_DUMP	0x10000		/* print out debug info now */
#define PPPDBG_LOG	0x100		/* log various things */
#define PPPDBG_DRIVER	0		/* identifies ppp driver as target */
#define PPPDBG_IF	1		/* identifies ppp network i/f target */
#define PPPDBG_COMP	2		/* identifies ppp compression target */
#define PPPDBG_AHDLC	3		/* identifies ppp async hdlc target */
