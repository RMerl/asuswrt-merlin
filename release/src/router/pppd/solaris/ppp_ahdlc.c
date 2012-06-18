/*
 * ppp_ahdlc.c - STREAMS module for doing PPP asynchronous HDLC.
 *
 * Re-written by Adi Masputra <adi.masputra@sun.com>, based on 
 * the original ppp_ahdlc.c
 *
 * Copyright (c) 2000 by Sun Microsystems, Inc.
 * All rights reserved.
 *
 * Permission to use, copy, modify, and distribute this software and its
 * documentation is hereby granted, provided that the above copyright
 * notice appears in all copies.  
 *
 * SUN MAKES NO REPRESENTATION OR WARRANTIES ABOUT THE SUITABILITY OF
 * THE SOFTWARE, EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED
 * TO THE IMPLIED WARRANTIES OF MERCHANTABILITY, FITNESS FOR A
 * PARTICULAR PURPOSE, OR NON-INFRINGEMENT.  SUN SHALL NOT BE LIABLE FOR
 * ANY DAMAGES SUFFERED BY LICENSEE AS A RESULT OF USING, MODIFYING OR
 * DISTRIBUTING THIS SOFTWARE OR ITS DERIVATIVES
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
 * $Id: ppp_ahdlc.c,v 1.5 2005/06/27 00:59:57 carlsonj Exp $
 */

/*
 * This file is used under Solaris 2, SVR4, SunOS 4, and Digital UNIX.
 */
#include <sys/types.h>
#include <sys/param.h>
#include <sys/stream.h>
#include <sys/stropts.h>
#include <sys/errno.h>

#ifdef SVR4
#include <sys/conf.h>
#include <sys/kmem.h>
#include <sys/cmn_err.h>
#include <sys/ddi.h>
#else
#include <sys/user.h>
#ifdef __osf__
#include <sys/cmn_err.h>
#endif
#endif /* SVR4 */

#include <net/ppp_defs.h>
#include <net/pppio.h>
#include "ppp_mod.h"

/*
 * Right now, mutex is only enabled for Solaris 2.x
 */
#if defined(SOL2)
#define USE_MUTEX
#endif /* SOL2 */

#ifdef USE_MUTEX
#define	MUTEX_ENTER(x)	mutex_enter(x)
#define	MUTEX_EXIT(x)	mutex_exit(x)
#else
#define	MUTEX_ENTER(x)
#define	MUTEX_EXIT(x)
#endif

/*
 * intpointer_t and uintpointer_t are signed and unsigned integer types 
 * large enough to hold any data pointer; that is, data pointers can be 
 * assigned into or from these integer types without losing precision.
 * On recent Solaris releases, these types are defined in sys/int_types.h,
 * but not on SunOS 4.x or the earlier Solaris versions.
 */
#if defined(_LP64) || defined(_I32LPx)
typedef long                    intpointer_t;
typedef unsigned long           uintpointer_t;
#else
typedef int                     intpointer_t;
typedef unsigned int            uintpointer_t;
#endif

MOD_OPEN_DECL(ahdlc_open);
MOD_CLOSE_DECL(ahdlc_close);
static int ahdlc_wput __P((queue_t *, mblk_t *));
static int ahdlc_rput __P((queue_t *, mblk_t *));
static void ahdlc_encode __P((queue_t *, mblk_t *));
static void ahdlc_decode __P((queue_t *, mblk_t *));
static int msg_byte __P((mblk_t *, unsigned int));

#if defined(SOL2)
/*
 * Don't send HDLC start flag is last transmit is within 1.5 seconds -
 * FLAG_TIME is defined is microseconds
 */
#define FLAG_TIME   1500
#define ABS(x)	    (x >= 0 ? x : (-x))
#endif /* SOL2 */

/*
 * Extract byte i of message mp 
 */
#define MSG_BYTE(mp, i)	((i) < (mp)->b_wptr - (mp)->b_rptr? (mp)->b_rptr[i]: \
			 msg_byte((mp), (i)))

/* 
 * Is this LCP packet one we have to transmit using LCP defaults? 
 */
#define LCP_USE_DFLT(mp)	(1 <= (code = MSG_BYTE((mp), 4)) && code <= 7)

/*
 * Standard STREAMS declarations
 */
static struct module_info minfo = {
    0x7d23, "ppp_ahdl", 0, INFPSZ, 32768, 512
};

static struct qinit rinit = {
    ahdlc_rput, NULL, ahdlc_open, ahdlc_close, NULL, &minfo, NULL
};

static struct qinit winit = {
    ahdlc_wput, NULL, NULL, NULL, NULL, &minfo, NULL
};

#if defined(SVR4) && !defined(SOL2)
int phdldevflag = 0;
#define ppp_ahdlcinfo phdlinfo
#endif /* defined(SVR4) && !defined(SOL2) */

struct streamtab ppp_ahdlcinfo = {
    &rinit,			    /* ptr to st_rdinit */
    &winit,			    /* ptr to st_wrinit */
    NULL,			    /* ptr to st_muxrinit */
    NULL,			    /* ptr to st_muxwinit */
#if defined(SUNOS4)
    NULL			    /* ptr to ptr to st_modlist */
#endif /* SUNOS4 */
};

#if defined(SUNOS4)
int ppp_ahdlc_count = 0;	    /* open counter */
#endif /* SUNOS4 */

/*
 * Per-stream state structure
 */
typedef struct ahdlc_state {
#if defined(USE_MUTEX)
    kmutex_t	    lock;		    /* lock for this structure */
#endif /* USE_MUTEX */
    int		    flags;		    /* link flags */
    mblk_t	    *rx_buf;		    /* ptr to receive buffer */
    int		    rx_buf_size;	    /* receive buffer size */
    ushort_t	    infcs;		    /* calculated rx HDLC FCS */
    u_int32_t	    xaccm[8];		    /* 256-bit xmit ACCM */
    u_int32_t	    raccm;		    /* 32-bit rcv ACCM */
    int		    mtu;		    /* interface MTU */
    int		    mru;		    /* link MRU */
    int		    unit;		    /* current PPP unit number */
    struct pppstat  stats;		    /* statistic structure */
#if defined(SOL2)
    clock_t	    flag_time;		    /* time in usec between flags */
    clock_t	    lbolt;		    /* last updated lbolt */
#endif /* SOL2 */
} ahdlc_state_t;

/*
 * Values for flags 
 */
#define ESCAPED		0x100	/* last saw escape char on input */
#define IFLUSH		0x200	/* flushing input due to error */

/* 
 * RCV_B7_1, etc., defined in net/pppio.h, are stored in flags also. 
 */
#define RCV_FLAGS	(RCV_B7_1|RCV_B7_0|RCV_ODDP|RCV_EVNP)

/*
 * FCS lookup table as calculated by genfcstab.
 */
static u_short fcstab[256] = {
	0x0000,	0x1189,	0x2312,	0x329b,	0x4624,	0x57ad,	0x6536,	0x74bf,
	0x8c48,	0x9dc1,	0xaf5a,	0xbed3,	0xca6c,	0xdbe5,	0xe97e,	0xf8f7,
	0x1081,	0x0108,	0x3393,	0x221a,	0x56a5,	0x472c,	0x75b7,	0x643e,
	0x9cc9,	0x8d40,	0xbfdb,	0xae52,	0xdaed,	0xcb64,	0xf9ff,	0xe876,
	0x2102,	0x308b,	0x0210,	0x1399,	0x6726,	0x76af,	0x4434,	0x55bd,
	0xad4a,	0xbcc3,	0x8e58,	0x9fd1,	0xeb6e,	0xfae7,	0xc87c,	0xd9f5,
	0x3183,	0x200a,	0x1291,	0x0318,	0x77a7,	0x662e,	0x54b5,	0x453c,
	0xbdcb,	0xac42,	0x9ed9,	0x8f50,	0xfbef,	0xea66,	0xd8fd,	0xc974,
	0x4204,	0x538d,	0x6116,	0x709f,	0x0420,	0x15a9,	0x2732,	0x36bb,
	0xce4c,	0xdfc5,	0xed5e,	0xfcd7,	0x8868,	0x99e1,	0xab7a,	0xbaf3,
	0x5285,	0x430c,	0x7197,	0x601e,	0x14a1,	0x0528,	0x37b3,	0x263a,
	0xdecd,	0xcf44,	0xfddf,	0xec56,	0x98e9,	0x8960,	0xbbfb,	0xaa72,
	0x6306,	0x728f,	0x4014,	0x519d,	0x2522,	0x34ab,	0x0630,	0x17b9,
	0xef4e,	0xfec7,	0xcc5c,	0xddd5,	0xa96a,	0xb8e3,	0x8a78,	0x9bf1,
	0x7387,	0x620e,	0x5095,	0x411c,	0x35a3,	0x242a,	0x16b1,	0x0738,
	0xffcf,	0xee46,	0xdcdd,	0xcd54,	0xb9eb,	0xa862,	0x9af9,	0x8b70,
	0x8408,	0x9581,	0xa71a,	0xb693,	0xc22c,	0xd3a5,	0xe13e,	0xf0b7,
	0x0840,	0x19c9,	0x2b52,	0x3adb,	0x4e64,	0x5fed,	0x6d76,	0x7cff,
	0x9489,	0x8500,	0xb79b,	0xa612,	0xd2ad,	0xc324,	0xf1bf,	0xe036,
	0x18c1,	0x0948,	0x3bd3,	0x2a5a,	0x5ee5,	0x4f6c,	0x7df7,	0x6c7e,
	0xa50a,	0xb483,	0x8618,	0x9791,	0xe32e,	0xf2a7,	0xc03c,	0xd1b5,
	0x2942,	0x38cb,	0x0a50,	0x1bd9,	0x6f66,	0x7eef,	0x4c74,	0x5dfd,
	0xb58b,	0xa402,	0x9699,	0x8710,	0xf3af,	0xe226,	0xd0bd,	0xc134,
	0x39c3,	0x284a,	0x1ad1,	0x0b58,	0x7fe7,	0x6e6e,	0x5cf5,	0x4d7c,
	0xc60c,	0xd785,	0xe51e,	0xf497,	0x8028,	0x91a1,	0xa33a,	0xb2b3,
	0x4a44,	0x5bcd,	0x6956,	0x78df,	0x0c60,	0x1de9,	0x2f72,	0x3efb,
	0xd68d,	0xc704,	0xf59f,	0xe416,	0x90a9,	0x8120,	0xb3bb,	0xa232,
	0x5ac5,	0x4b4c,	0x79d7,	0x685e,	0x1ce1,	0x0d68,	0x3ff3,	0x2e7a,
	0xe70e,	0xf687,	0xc41c,	0xd595,	0xa12a,	0xb0a3,	0x8238,	0x93b1,
	0x6b46,	0x7acf,	0x4854,	0x59dd,	0x2d62,	0x3ceb,	0x0e70,	0x1ff9,
	0xf78f,	0xe606,	0xd49d,	0xc514,	0xb1ab,	0xa022,	0x92b9,	0x8330,
	0x7bc7,	0x6a4e,	0x58d5,	0x495c,	0x3de3,	0x2c6a,	0x1ef1,	0x0f78
};

static u_int32_t paritytab[8] =
{
	0x96696996, 0x69969669, 0x69969669, 0x96696996,
	0x69969669, 0x96696996, 0x96696996, 0x69969669
};

/*
 * STREAMS module open (entry) point
 */
MOD_OPEN(ahdlc_open)
{
    ahdlc_state_t   *state;
    mblk_t *mp;

    /*
     * Return if it's already opened
     */
    if (q->q_ptr) {
	return 0;
    }

    /*
     * This can only be opened as a module
     */
    if (sflag != MODOPEN) {
	OPEN_ERROR(EINVAL);
    }

    state = (ahdlc_state_t *) ALLOC_NOSLEEP(sizeof(ahdlc_state_t));
    if (state == 0)
	OPEN_ERROR(ENOSR);
    bzero((caddr_t) state, sizeof(ahdlc_state_t));

    q->q_ptr	 = (caddr_t) state;
    WR(q)->q_ptr = (caddr_t) state;

#if defined(USE_MUTEX)
    mutex_init(&state->lock, NULL, MUTEX_DEFAULT, NULL);
#endif /* USE_MUTEX */

    state->xaccm[0] = ~0;	    /* escape 0x00 through 0x1f */
    state->xaccm[3] = 0x60000000;   /* escape 0x7d and 0x7e */
    state->mru	    = PPP_MRU;	    /* default of 1500 bytes */
#if defined(SOL2)
    state->flag_time = drv_usectohz(FLAG_TIME);
#endif /* SOL2 */

#if defined(SUNOS4)
    ppp_ahdlc_count++;
#endif /* SUNOS4 */

    qprocson(q);

    if ((mp = allocb(1, BPRI_HI)) != NULL) {
	    mp->b_datap->db_type = M_FLUSH;
	    *mp->b_wptr++ = FLUSHR;
	    putnext(q, mp);
    }

    return 0;
}

/*
 * STREAMS module close (exit) point
 */
MOD_CLOSE(ahdlc_close)
{
    ahdlc_state_t   *state;

    qprocsoff(q);

    state = (ahdlc_state_t *) q->q_ptr;

    if (state == 0) {
	DPRINT("state == 0 in ahdlc_close\n");
	return 0;
    }

    if (state->rx_buf != 0) {
	freemsg(state->rx_buf);
	state->rx_buf = 0;
    }

#if defined(USE_MUTEX)
    mutex_destroy(&state->lock);
#endif /* USE_MUTEX */

    FREE(q->q_ptr, sizeof(ahdlc_state_t));
    q->q_ptr	     = NULL;
    OTHERQ(q)->q_ptr = NULL;

#if defined(SUNOS4)
    if (ppp_ahdlc_count)
	ppp_ahdlc_count--;
#endif /* SUNOS4 */
    
    return 0;
}

/*
 * Write side put routine
 */
static int
ahdlc_wput(q, mp)
    queue_t	*q;
    mblk_t	*mp;
{
    ahdlc_state_t  	*state;
    struct iocblk  	*iop;
    int		   	error;
    mblk_t	   	*np;
    struct ppp_stats	*psp;

    state = (ahdlc_state_t *) q->q_ptr;
    if (state == 0) {
	DPRINT("state == 0 in ahdlc_wput\n");
	freemsg(mp);
	return 0;
    }

    switch (mp->b_datap->db_type) {
    case M_DATA:
	/*
	 * A data packet - do character-stuffing and FCS, and
	 * send it onwards.
	 */
	ahdlc_encode(q, mp);
	freemsg(mp);
	break;

    case M_IOCTL:
	iop = (struct iocblk *) mp->b_rptr;
	error = EINVAL;
	switch (iop->ioc_cmd) {
	case PPPIO_XACCM:
	    if ((iop->ioc_count < sizeof(u_int32_t)) || 
		(iop->ioc_count > sizeof(ext_accm))) {
		break;
	    }
	    if (mp->b_cont == 0) {
		DPRINT1("ahdlc_wput/%d: PPPIO_XACCM b_cont = 0!\n", state->unit);
		break;
	    }
	    MUTEX_ENTER(&state->lock);
	    bcopy((caddr_t)mp->b_cont->b_rptr, (caddr_t)state->xaccm,
		  iop->ioc_count);
	    state->xaccm[2] &= ~0x40000000;	/* don't escape 0x5e */
	    state->xaccm[3] |= 0x60000000;	/* do escape 0x7d, 0x7e */
	    MUTEX_EXIT(&state->lock);
	    iop->ioc_count = 0;
	    error = 0;
	    break;

	case PPPIO_RACCM:
	    if (iop->ioc_count != sizeof(u_int32_t))
		break;
	    if (mp->b_cont == 0) {
		DPRINT1("ahdlc_wput/%d: PPPIO_RACCM b_cont = 0!\n", state->unit);
		break;
	    }
	    MUTEX_ENTER(&state->lock);
	    bcopy((caddr_t)mp->b_cont->b_rptr, (caddr_t)&state->raccm,
		  sizeof(u_int32_t));
	    MUTEX_EXIT(&state->lock);
	    iop->ioc_count = 0;
	    error = 0;
	    break;

	case PPPIO_GCLEAN:
	    np = allocb(sizeof(int), BPRI_HI);
	    if (np == 0) {
		error = ENOSR;
		break;
	    }
	    if (mp->b_cont != 0)
		freemsg(mp->b_cont);
	    mp->b_cont = np;
	    MUTEX_ENTER(&state->lock);
	    *(int *)np->b_wptr = state->flags & RCV_FLAGS;
	    MUTEX_EXIT(&state->lock);
	    np->b_wptr += sizeof(int);
	    iop->ioc_count = sizeof(int);
	    error = 0;
	    break;

	case PPPIO_GETSTAT:
	    np = allocb(sizeof(struct ppp_stats), BPRI_HI);
	    if (np == 0) {
		error = ENOSR;
		break;
	    }
	    if (mp->b_cont != 0)
		freemsg(mp->b_cont);
	    mp->b_cont = np;
	    psp = (struct ppp_stats *) np->b_wptr;
	    np->b_wptr += sizeof(struct ppp_stats);
	    bzero((caddr_t)psp, sizeof(struct ppp_stats));
	    psp->p = state->stats;
	    iop->ioc_count = sizeof(struct ppp_stats);
	    error = 0;
	    break;

	case PPPIO_LASTMOD:
	    /* we knew this anyway */
	    error = 0;
	    break;

	default:
	    error = -1;
	    break;
	}

	if (error < 0)
	    putnext(q, mp);
	else if (error == 0) {
	    mp->b_datap->db_type = M_IOCACK;
	    qreply(q, mp);
	} else {
	    mp->b_datap->db_type = M_IOCNAK;
	    iop->ioc_count = 0;
	    iop->ioc_error = error;
	    qreply(q, mp);
	}
	break;

    case M_CTL:
	switch (*mp->b_rptr) {
	case PPPCTL_MTU:
	    MUTEX_ENTER(&state->lock);
	    state->mtu = ((unsigned short *)mp->b_rptr)[1];
	    MUTEX_EXIT(&state->lock);
	    break;
	case PPPCTL_MRU:
	    MUTEX_ENTER(&state->lock);
	    state->mru = ((unsigned short *)mp->b_rptr)[1];
	    MUTEX_EXIT(&state->lock);
	    break;
	case PPPCTL_UNIT:
	    MUTEX_ENTER(&state->lock);
	    state->unit = mp->b_rptr[1];
	    MUTEX_EXIT(&state->lock);
	    break;
	default:
	    putnext(q, mp);
	    return 0;
	}
	freemsg(mp);
	break;

    default:
	putnext(q, mp);
    }

    return 0;
}

/*
 * Read side put routine
 */
static int
ahdlc_rput(q, mp)
    queue_t *q;
    mblk_t  *mp;
{
    ahdlc_state_t *state;

    state = (ahdlc_state_t *) q->q_ptr;
    if (state == 0) {
	DPRINT("state == 0 in ahdlc_rput\n");
	freemsg(mp);
	return 0;
    }

    switch (mp->b_datap->db_type) {
    case M_DATA:
	ahdlc_decode(q, mp);
	break;

    case M_HANGUP:
	MUTEX_ENTER(&state->lock);
	if (state->rx_buf != 0) {
	    /* XXX would like to send this up for debugging */
	    freemsg(state->rx_buf);
	    state->rx_buf = 0;
	}
	state->flags = IFLUSH;
	MUTEX_EXIT(&state->lock);
	putnext(q, mp);
	break;

    default:
	putnext(q, mp);
    }
    return 0;
}

/*
 * Extract bit c from map m, to determine if c needs to be escaped
 */
#define IN_TX_MAP(c, m)	((m)[(c) >> 5] & (1 << ((c) & 0x1f)))

static void
ahdlc_encode(q, mp)
    queue_t	*q;
    mblk_t	*mp;
{
    ahdlc_state_t	*state;
    u_int32_t		*xaccm, loc_xaccm[8];
    ushort_t		fcs;
    size_t		outmp_len;
    mblk_t		*outmp, *tmp;
    uchar_t		*dp, fcs_val;
    int			is_lcp, code;
#if defined(SOL2)
    clock_t		lbolt;
#endif /* SOL2 */

    if (msgdsize(mp) < 4) {
	return;
    }

    state = (ahdlc_state_t *)q->q_ptr;
    MUTEX_ENTER(&state->lock);

    /*
     * Allocate an output buffer large enough to handle a case where all
     * characters need to be escaped
     */
    outmp_len = (msgdsize(mp)	 << 1) +		/* input block x 2 */
		(sizeof(fcs)	 << 2) +		/* HDLC FCS x 4 */
		(sizeof(uchar_t) << 1);			/* HDLC flags x 2 */

    outmp = allocb(outmp_len, BPRI_MED);
    if (outmp == NULL) {
	state->stats.ppp_oerrors++;
	MUTEX_EXIT(&state->lock);
	putctl1(RD(q)->q_next, M_CTL, PPPCTL_OERROR);
	return;
    }

#if defined(SOL2)
    /*
     * Check if our last transmit happenned within flag_time, using
     * the system's LBOLT value in clock ticks
     */
    if (drv_getparm(LBOLT, &lbolt) != -1) {
	if (ABS((clock_t)lbolt - state->lbolt) > state->flag_time) {
	    *outmp->b_wptr++ = PPP_FLAG;
	} 
	state->lbolt = lbolt;
    } else {
	*outmp->b_wptr++ = PPP_FLAG;
    }
#else
    /*
     * If the driver below still has a message to process, skip the
     * HDLC flag, otherwise, put one in the beginning
     */
    if (qsize(q->q_next) == 0) {
	*outmp->b_wptr++ = PPP_FLAG;
    }
#endif

    /*
     * All control characters must be escaped for LCP packets with code
     * values between 1 (Conf-Req) and 7 (Code-Rej).
     */
    is_lcp = ((MSG_BYTE(mp, 0) == PPP_ALLSTATIONS) && 
	      (MSG_BYTE(mp, 1) == PPP_UI) && 
	      (MSG_BYTE(mp, 2) == (PPP_LCP >> 8)) &&
	      (MSG_BYTE(mp, 3) == (PPP_LCP & 0xff)) &&
	      LCP_USE_DFLT(mp));

    xaccm = state->xaccm;
    if (is_lcp) {
	bcopy((caddr_t)state->xaccm, (caddr_t)loc_xaccm, sizeof(loc_xaccm));
	loc_xaccm[0] = ~0;	/* force escape on 0x00 through 0x1f */
	xaccm = loc_xaccm;
    }

    fcs = PPP_INITFCS;		/* Initial FCS is 0xffff */

    /*
     * Process this block and the rest (if any) attached to the this one
     */
    for (tmp = mp; tmp; tmp = tmp->b_cont) {
	if (tmp->b_datap->db_type == M_DATA) {
	    for (dp = tmp->b_rptr; dp < tmp->b_wptr; dp++) {
		fcs = PPP_FCS(fcs, *dp);
		if (IN_TX_MAP(*dp, xaccm)) {
		    *outmp->b_wptr++ = PPP_ESCAPE;
		    *outmp->b_wptr++ = *dp ^ PPP_TRANS;
		} else {
		    *outmp->b_wptr++ = *dp;
		}
	    }
	} else {
	    continue;	/* skip if db_type is something other than M_DATA */
	}
    }

    /*
     * Append the HDLC FCS, making sure that escaping is done on any
     * necessary bytes
     */
    fcs_val = (fcs ^ 0xffff) & 0xff;
    if (IN_TX_MAP(fcs_val, xaccm)) {
	*outmp->b_wptr++ = PPP_ESCAPE;
	*outmp->b_wptr++ = fcs_val ^ PPP_TRANS;
    } else {
	*outmp->b_wptr++ = fcs_val;
    }

    fcs_val = ((fcs ^ 0xffff) >> 8) & 0xff;
    if (IN_TX_MAP(fcs_val, xaccm)) {
	*outmp->b_wptr++ = PPP_ESCAPE;
	*outmp->b_wptr++ = fcs_val ^ PPP_TRANS;
    } else {
	*outmp->b_wptr++ = fcs_val;
    }

    /*
     * And finally, append the HDLC flag, and send it away
     */
    *outmp->b_wptr++ = PPP_FLAG;

    state->stats.ppp_obytes += msgdsize(outmp);
    state->stats.ppp_opackets++;

    MUTEX_EXIT(&state->lock);

    putnext(q, outmp);
}

/*
 * Checks the 32-bit receive ACCM to see if the byte needs un-escaping
 */
#define IN_RX_MAP(c, m)	((((unsigned int) (uchar_t) (c)) < 0x20) && \
			(m) & (1 << (c)))


/*
 * Process received characters.
 */
static void
ahdlc_decode(q, mp)
    queue_t *q;
    mblk_t  *mp;
{
    ahdlc_state_t   *state;
    mblk_t	    *om;
    uchar_t	    *dp;

    state = (ahdlc_state_t *) q->q_ptr;

    MUTEX_ENTER(&state->lock);

    state->stats.ppp_ibytes += msgdsize(mp);

    for (; mp != 0; om = mp->b_cont, freeb(mp), mp = om)
    for (dp = mp->b_rptr; dp < mp->b_wptr; dp++) {

	/*
	 * This should detect the lack of 8-bit communication channel
	 * which is necessary for PPP to work. In addition, it also
	 * checks on the parity.
	 */
	if (*dp & 0x80)
	    state->flags |= RCV_B7_1;
	else
	    state->flags |= RCV_B7_0;

	if (paritytab[*dp >> 5] & (1 << (*dp & 0x1f)))
	    state->flags |= RCV_ODDP;
	else
	    state->flags |= RCV_EVNP;

	/*
	 * So we have a HDLC flag ...
	 */
	if (*dp == PPP_FLAG) {

	    /*
	     * If we think that it marks the beginning of the frame,
	     * then continue to process the next octects
	     */
	    if ((state->flags & IFLUSH) ||
		(state->rx_buf == 0) ||
		(msgdsize(state->rx_buf) == 0)) {

		state->flags &= ~IFLUSH;
		continue;
	    }

	    /*
	     * We get here because the above condition isn't true,
	     * in which case the HDLC flag was there to mark the end
	     * of the frame (or so we think)
	     */
	    om = state->rx_buf;

	    if (state->infcs == PPP_GOODFCS) {
		state->stats.ppp_ipackets++;
		adjmsg(om, -PPP_FCSLEN);
		putnext(q, om);
	    } else {
		DPRINT2("ppp%d: bad fcs (len=%d)\n",
                    state->unit, msgdsize(state->rx_buf));
		freemsg(state->rx_buf);
		state->flags &= ~(IFLUSH | ESCAPED);
		state->stats.ppp_ierrors++;
		putctl1(q->q_next, M_CTL, PPPCTL_IERROR);
	    }

	    state->rx_buf = 0;
	    continue;
	}

	if (state->flags & IFLUSH) {
	    continue;
	}

	/*
	 * Allocate a receive buffer, large enough to store a frame (after
	 * un-escaping) of at least 1500 octets. If MRU is negotiated to
	 * be more than the default, then allocate that much. In addition,
	 * we add an extra 32-bytes for a fudge factor
	 */ 
	if (state->rx_buf == 0) {
	    state->rx_buf_size  = (state->mru < PPP_MRU ? PPP_MRU : state->mru);
	    state->rx_buf_size += (sizeof(u_int32_t) << 3);
	    state->rx_buf = allocb(state->rx_buf_size, BPRI_MED);

	    /*
	     * If allocation fails, try again on the next frame
	     */
	    if (state->rx_buf == 0) {
		state->flags |= IFLUSH;
		continue;
	    }
	    state->flags &= ~(IFLUSH | ESCAPED);
	    state->infcs  = PPP_INITFCS;
	}

	if (*dp == PPP_ESCAPE) {
	    state->flags |= ESCAPED;
	    continue;
	}

	/*
	 * Make sure we un-escape the necessary characters, as well as the
	 * ones in our receive async control character map
	 */
	if (state->flags & ESCAPED) {
	    *dp ^= PPP_TRANS;
	    state->flags &= ~ESCAPED;
	} else if (IN_RX_MAP(*dp, state->raccm)) 
	    continue;

	/*
	 * Unless the peer lied to us about the negotiated MRU, we should
	 * never get a frame which is too long. If it happens, toss it away
	 * and grab the next incoming one
	 */
	if (msgdsize(state->rx_buf) < state->rx_buf_size) {
	    state->infcs = PPP_FCS(state->infcs, *dp);
	    *state->rx_buf->b_wptr++ = *dp;
	} else {
	    DPRINT2("ppp%d: frame too long (%d)\n",
		state->unit, msgdsize(state->rx_buf));
	    freemsg(state->rx_buf);
	    state->rx_buf     = 0;
	    state->flags     |= IFLUSH;
	}
    }

    MUTEX_EXIT(&state->lock);
}

static int
msg_byte(mp, i)
    mblk_t *mp;
    unsigned int i;
{
    while (mp != 0 && i >= mp->b_wptr - mp->b_rptr)
	mp = mp->b_cont;
    if (mp == 0)
	return -1;
    return mp->b_rptr[i];
}
