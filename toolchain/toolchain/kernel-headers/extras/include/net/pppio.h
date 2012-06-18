/*
 * pppio.h - ioctl and other misc. definitions for STREAMS modules.
 *
 * Copyright (c) 1994 The Australian National University.
 * All rights reserved.
 *
 * Permission to use, copy, modify, and distribute this software and its
 * documentation is hereby granted, provided that the above copyright
 * notice appears in all copies.  This software is provided without any
 * warranty, express or implied. The Australian National University
 * makes no representations about the suitability of this software for
 * any purpose.
 *
 * IN NO EVENT SHALL THE AUSTRALIAN NATIONAL UNIVERSITY BE LIABLE TO ANY
 * PARTY FOR DIRECT, INDIRECT, SPECIAL, INCIDENTAL, OR CONSEQUENTIAL DAMAGES
 * ARISING OUT OF THE USE OF THIS SOFTWARE AND ITS DOCUMENTATION, EVEN IF
 * THE AUSTRALIAN NATIONAL UNIVERSITY HAVE BEEN ADVISED OF THE POSSIBILITY
 * OF SUCH DAMAGE.
 *
 * THE AUSTRALIAN NATIONAL UNIVERSITY SPECIFICALLY DISCLAIMS ANY WARRANTIES,
 * INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY
 * AND FITNESS FOR A PARTICULAR PURPOSE.  THE SOFTWARE PROVIDED HEREUNDER IS
 * ON AN "AS IS" BASIS, AND THE AUSTRALIAN NATIONAL UNIVERSITY HAS NO
 * OBLIGATION TO PROVIDE MAINTENANCE, SUPPORT, UPDATES, ENHANCEMENTS,
 * OR MODIFICATIONS.
 *
 * $Id: pppio.h,v 1.2 2001/09/27 05:21:12 andersen Exp $
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
