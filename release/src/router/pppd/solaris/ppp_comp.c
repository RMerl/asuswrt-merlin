/*
 * ppp_comp.c - STREAMS module for kernel-level compression and CCP support.
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
 * $Id: ppp_comp.c,v 1.3 2004/01/17 05:47:55 carlsonj Exp $
 */

/*
 * This file is used under SVR4, Solaris 2, SunOS 4, and Digital UNIX.
 */

#include <sys/types.h>
#include <sys/param.h>
#include <sys/errno.h>
#include <sys/stream.h>

#ifdef SVR4
#include <sys/conf.h>
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

#ifdef __osf__
#include <sys/mbuf.h>
#include <sys/protosw.h>
#endif

#include <netinet/in.h>
#include <netinet/in_systm.h>
#include <netinet/ip.h>
#include <net/vjcompress.h>

#define PACKETPTR	mblk_t *
#include <net/ppp-comp.h>

MOD_OPEN_DECL(ppp_comp_open);
MOD_CLOSE_DECL(ppp_comp_close);
static int ppp_comp_rput __P((queue_t *, mblk_t *));
static int ppp_comp_rsrv __P((queue_t *));
static int ppp_comp_wput __P((queue_t *, mblk_t *));
static int ppp_comp_wsrv __P((queue_t *));
static void ppp_comp_ccp __P((queue_t *, mblk_t *, int));
static int msg_byte __P((mblk_t *, unsigned int));

/* Extract byte i of message mp. */
#define MSG_BYTE(mp, i)	((i) < (mp)->b_wptr - (mp)->b_rptr? (mp)->b_rptr[i]: \
			 msg_byte((mp), (i)))

/* Is this LCP packet one we have to transmit using LCP defaults? */
#define LCP_USE_DFLT(mp)	(1 <= (code = MSG_BYTE((mp), 4)) && code <= 7)

#define PPP_COMP_ID 0xbadf
static struct module_info minfo = {
#ifdef PRIOQ
    PPP_COMP_ID, "ppp_comp", 0, INFPSZ, 16512, 16384,
#else
    PPP_COMP_ID, "ppp_comp", 0, INFPSZ, 16384, 4096,
#endif
};

static struct qinit r_init = {
    ppp_comp_rput, ppp_comp_rsrv, ppp_comp_open, ppp_comp_close,
    NULL, &minfo, NULL
};

static struct qinit w_init = {
    ppp_comp_wput, ppp_comp_wsrv, NULL, NULL, NULL, &minfo, NULL
};

#if defined(SVR4) && !defined(SOL2)
int pcmpdevflag = 0;
#define ppp_compinfo pcmpinfo
#endif
struct streamtab ppp_compinfo = {
    &r_init, &w_init, NULL, NULL
};

int ppp_comp_count;		/* number of module instances in use */

#ifdef __osf__

static void ppp_comp_alloc __P((comp_state_t *));
typedef struct memreq {
    unsigned char comp_opts[20];
    int cmd;
    int thread_status;
    char *returned_mem;
} memreq_t;

#endif

typedef struct comp_state {
    int		flags;
    int		mru;
    int		mtu;
    int		unit;
    struct compressor *xcomp;
    void	*xstate;
    struct compressor *rcomp;
    void	*rstate;
    struct vjcompress vj_comp;
    int		vj_last_ierrors;
    struct pppstat stats;
#ifdef __osf__
    memreq_t	memreq;
    thread_t	thread;
#endif
} comp_state_t;


#ifdef __osf__
extern task_t first_task;
#endif

/* Bits in flags are as defined in pppio.h. */
#define CCP_ERR		(CCP_ERROR | CCP_FATALERROR)
#define LAST_MOD	0x1000000	/* no ppp modules below us */
#define DBGLOG		0x2000000	/* log debugging stuff */

#define MAX_IPHDR	128	/* max TCP/IP header size */
#define MAX_VJHDR	20	/* max VJ compressed header size (?) */

#undef MIN		/* just in case */
#define MIN(a, b)	((a) < (b)? (a): (b))

/*
 * List of compressors we know about.
 */

#if DO_BSD_COMPRESS
extern struct compressor ppp_bsd_compress;
#endif
#if DO_DEFLATE
extern struct compressor ppp_deflate, ppp_deflate_draft;
#endif

struct compressor *ppp_compressors[] = {
#if DO_BSD_COMPRESS
    &ppp_bsd_compress,
#endif
#if DO_DEFLATE
    &ppp_deflate,
    &ppp_deflate_draft,
#endif
    NULL
};

/*
 * STREAMS module entry points.
 */
MOD_OPEN(ppp_comp_open)
{
    comp_state_t *cp;
#ifdef __osf__
    thread_t thread;
#endif

    if (q->q_ptr == NULL) {
	cp = (comp_state_t *) ALLOC_SLEEP(sizeof(comp_state_t));
	if (cp == NULL)
	    OPEN_ERROR(ENOSR);
	bzero((caddr_t)cp, sizeof(comp_state_t));
	WR(q)->q_ptr = q->q_ptr = (caddr_t) cp;
	cp->mru = PPP_MRU;
	cp->mtu = PPP_MTU;
	cp->xstate = NULL;
	cp->rstate = NULL;
	vj_compress_init(&cp->vj_comp, -1);
#ifdef __osf__
	if (!(thread = kernel_thread_w_arg(first_task, ppp_comp_alloc, (void *)cp)))
		OPEN_ERROR(ENOSR);
	cp->thread = thread;
#endif
	++ppp_comp_count;
	qprocson(q);
    }
    return 0;
}

MOD_CLOSE(ppp_comp_close)
{
    comp_state_t *cp;

    qprocsoff(q);
    cp = (comp_state_t *) q->q_ptr;
    if (cp != NULL) {
	if (cp->xstate != NULL)
	    (*cp->xcomp->comp_free)(cp->xstate);
	if (cp->rstate != NULL)
	    (*cp->rcomp->decomp_free)(cp->rstate);
#ifdef __osf__
	if (!cp->thread)
	    printf("ppp_comp_close: NULL thread!\n");
	else
	    thread_terminate(cp->thread);
#endif
	FREE(cp, sizeof(comp_state_t));
	q->q_ptr = NULL;
	OTHERQ(q)->q_ptr = NULL;
	--ppp_comp_count;
    }
    return 0;
}

#ifdef __osf__

/* thread for calling back to a compressor's memory allocator
 * Needed for Digital UNIX since it's VM can't handle requests
 * for large amounts of memory without blocking.  The thread
 * provides a context in which we can call a memory allocator
 * that may block.
 */
static void
ppp_comp_alloc(comp_state_t *cp)
{
    int len, cmd;
    unsigned char *compressor_options;
    thread_t thread;
    void *(*comp_allocator)();


#if defined(MAJOR_VERSION) && (MAJOR_VERSION <= 2)

    /* In 2.x and earlier the argument gets passed
     * in the thread structure itself.  Yuck.
     */
    thread = current_thread();
    cp = thread->reply_port;
    thread->reply_port = PORT_NULL;

#endif

    for (;;) {
	assert_wait((vm_offset_t)&cp->memreq.thread_status, TRUE);
	thread_block();

	if (thread_should_halt(current_thread()))
	    thread_halt_self();
	cmd = cp->memreq.cmd;
	compressor_options = &cp->memreq.comp_opts[0];
	len = compressor_options[1];
	if (cmd == PPPIO_XCOMP) {
	    cp->memreq.returned_mem = cp->xcomp->comp_alloc(compressor_options, len);
	    if (!cp->memreq.returned_mem) {
		cp->memreq.thread_status = ENOSR;
	    } else {
		cp->memreq.thread_status = 0;
	    }
	} else {
	    cp->memreq.returned_mem = cp->rcomp->decomp_alloc(compressor_options, len);
	    if (!cp->memreq.returned_mem) {
	        cp->memreq.thread_status = ENOSR;
	    } else {
		cp->memreq.thread_status = 0;
	    }
	}
    }
}

#endif /* __osf__ */

/* here's the deal with memory allocation under Digital UNIX.
 * Some other may also benefit from this...
 * We can't ask for huge chunks of memory in a context where
 * the caller can't be put to sleep (like, here.)  The alloc
 * is likely to fail.  Instead we do this: the first time we
 * get called, kick off a thread to do the allocation.  Return
 * immediately to the caller with EAGAIN, as an indication that
 * they should send down the ioctl again.  By the time the
 * second call comes in it's likely that the memory allocation
 * thread will have returned with the requested memory.  We will
 * continue to return EAGAIN however until the thread has completed.
 * When it has, we return zero (and the memory) if the allocator
 * was successful and ENOSR otherwise.
 *
 * Callers of the RCOMP and XCOMP ioctls are encouraged (but not
 * required) to loop for some number of iterations with a small
 * delay in the loop body (for instance a 1/10-th second "sleep"
 * via select.)
 */
static int
ppp_comp_wput(q, mp)
    queue_t *q;
    mblk_t *mp;
{
    struct iocblk *iop;
    comp_state_t *cp;
    int error, len, n;
    int flags, mask;
    mblk_t *np;
    struct compressor **comp;
    struct ppp_stats *psp;
    struct ppp_comp_stats *csp;
    unsigned char *opt_data;
    int nxslots, nrslots;

    cp = (comp_state_t *) q->q_ptr;
    if (cp == 0) {
	DPRINT("cp == 0 in ppp_comp_wput\n");
	freemsg(mp);
	return 0;
    }

    switch (mp->b_datap->db_type) {

    case M_DATA:
	putq(q, mp);
	break;

    case M_IOCTL:
	iop = (struct iocblk *) mp->b_rptr;
	error = EINVAL;
	switch (iop->ioc_cmd) {

	case PPPIO_CFLAGS:
	    /* set/get CCP state */
	    if (iop->ioc_count != 2 * sizeof(int))
		break;
	    if (mp->b_cont == 0) {
		DPRINT1("ppp_comp_wput/%d: PPPIO_CFLAGS b_cont = 0!\n", cp->unit);
		break;
	    }
	    flags = ((int *) mp->b_cont->b_rptr)[0];
	    mask = ((int *) mp->b_cont->b_rptr)[1];
	    cp->flags = (cp->flags & ~mask) | (flags & mask);
	    if ((mask & CCP_ISOPEN) && (flags & CCP_ISOPEN) == 0) {
		if (cp->xstate != NULL) {
		    (*cp->xcomp->comp_free)(cp->xstate);
		    cp->xstate = NULL;
		}
		if (cp->rstate != NULL) {
		    (*cp->rcomp->decomp_free)(cp->rstate);
		    cp->rstate = NULL;
		}
		cp->flags &= ~CCP_ISUP;
	    }
	    error = 0;
	    iop->ioc_count = sizeof(int);
	    ((int *) mp->b_cont->b_rptr)[0] = cp->flags;
	    mp->b_cont->b_wptr = mp->b_cont->b_rptr + sizeof(int);
	    break;

	case PPPIO_VJINIT:
	    /*
	     * Initialize VJ compressor/decompressor
	     */
	    if (iop->ioc_count != 2)
		break;
	    if (mp->b_cont == 0) {
		DPRINT1("ppp_comp_wput/%d: PPPIO_VJINIT b_cont = 0!\n", cp->unit);
		break;
	    }
	    nxslots = mp->b_cont->b_rptr[0] + 1;
	    nrslots = mp->b_cont->b_rptr[1] + 1;
	    if (nxslots > MAX_STATES || nrslots > MAX_STATES)
		break;
	    vj_compress_init(&cp->vj_comp, nxslots);
	    cp->vj_last_ierrors = cp->stats.ppp_ierrors;
	    error = 0;
	    iop->ioc_count = 0;
	    break;

	case PPPIO_XCOMP:
	case PPPIO_RCOMP:
	    if (iop->ioc_count <= 0)
		break;
	    if (mp->b_cont == 0) {
		DPRINT1("ppp_comp_wput/%d: PPPIO_[XR]COMP b_cont = 0!\n", cp->unit);
		break;
	    }
	    opt_data = mp->b_cont->b_rptr;
	    len = mp->b_cont->b_wptr - opt_data;
	    if (len > iop->ioc_count)
		len = iop->ioc_count;
	    if (opt_data[1] < 2 || opt_data[1] > len)
		break;
	    for (comp = ppp_compressors; *comp != NULL; ++comp)
		if ((*comp)->compress_proto == opt_data[0]) {
		    /* here's the handler! */
		    error = 0;
#ifndef __osf__
		    if (iop->ioc_cmd == PPPIO_XCOMP) {
			/* A previous call may have fetched memory for a compressor
			 * that's now being retired or reset.  Free it using it's
			 * mechanism for freeing stuff.
			 */
			if (cp->xstate != NULL) {
			    (*cp->xcomp->comp_free)(cp->xstate);
			    cp->xstate = NULL;
			}
			cp->xcomp = *comp;
			cp->xstate = (*comp)->comp_alloc(opt_data, len);
			if (cp->xstate == NULL)
			    error = ENOSR;
		    } else {
			if (cp->rstate != NULL) {
			    (*cp->rcomp->decomp_free)(cp->rstate);
			    cp->rstate = NULL;
			}
			cp->rcomp = *comp;
			cp->rstate = (*comp)->decomp_alloc(opt_data, len);
			if (cp->rstate == NULL)
			    error = ENOSR;
		    }
#else
		    if ((error = cp->memreq.thread_status) != EAGAIN)
		    if (iop->ioc_cmd == PPPIO_XCOMP) {
			if (cp->xstate) {
			    (*cp->xcomp->comp_free)(cp->xstate);
			    cp->xstate = 0;
			}
			/* sanity check for compressor options
			 */
			if (sizeof (cp->memreq.comp_opts) < len) {
			    printf("can't handle options for compressor %d (%d)\n", opt_data[0],
				opt_data[1]);
			    cp->memreq.thread_status = ENOSR;
			    cp->memreq.returned_mem = 0;
			}
			/* fill in request for the thread and kick it off
			 */
			if (cp->memreq.thread_status == 0 && !cp->memreq.returned_mem) {
			    bcopy(opt_data, cp->memreq.comp_opts, len);
			    cp->memreq.cmd = PPPIO_XCOMP;
			    cp->xcomp = *comp;
			    error = cp->memreq.thread_status = EAGAIN;
			    thread_wakeup((vm_offset_t)&cp->memreq.thread_status);
			} else {
			    cp->xstate = cp->memreq.returned_mem;
			    cp->memreq.returned_mem = 0;
			    cp->memreq.thread_status = 0;
			}
		    } else {
			if (cp->rstate) {
			    (*cp->rcomp->decomp_free)(cp->rstate);
			    cp->rstate = NULL;
			}
			if (sizeof (cp->memreq.comp_opts) < len) {
			    printf("can't handle options for compressor %d (%d)\n", opt_data[0],
				opt_data[1]);
			    cp->memreq.thread_status = ENOSR;
			    cp->memreq.returned_mem = 0;
			}
			if (cp->memreq.thread_status == 0 && !cp->memreq.returned_mem) {
			    bcopy(opt_data, cp->memreq.comp_opts, len);
			    cp->memreq.cmd = PPPIO_RCOMP;
			    cp->rcomp = *comp;
			    error = cp->memreq.thread_status = EAGAIN;
			    thread_wakeup((vm_offset_t)&cp->memreq.thread_status);
			} else {
			    cp->rstate = cp->memreq.returned_mem;
			    cp->memreq.returned_mem = 0;
			    cp->memreq.thread_status = 0;
			}
		    }
#endif
		    break;
		}
	    iop->ioc_count = 0;
	    break;

	case PPPIO_GETSTAT:
	    if ((cp->flags & LAST_MOD) == 0) {
		error = -1;	/* let the ppp_ahdl module handle it */
		break;
	    }
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
	    iop->ioc_count = sizeof(struct ppp_stats);
	    psp->p = cp->stats;
	    psp->vj = cp->vj_comp.stats;
	    error = 0;
	    break;

	case PPPIO_GETCSTAT:
	    np = allocb(sizeof(struct ppp_comp_stats), BPRI_HI);
	    if (np == 0) {
		error = ENOSR;
		break;
	    }
	    if (mp->b_cont != 0)
		freemsg(mp->b_cont);
	    mp->b_cont = np;
	    csp = (struct ppp_comp_stats *) np->b_wptr;
	    np->b_wptr += sizeof(struct ppp_comp_stats);
	    iop->ioc_count = sizeof(struct ppp_comp_stats);
	    bzero((caddr_t)csp, sizeof(struct ppp_comp_stats));
	    if (cp->xstate != 0)
		(*cp->xcomp->comp_stat)(cp->xstate, &csp->c);
	    if (cp->rstate != 0)
		(*cp->rcomp->decomp_stat)(cp->rstate, &csp->d);
	    error = 0;
	    break;

	case PPPIO_DEBUG:
	    if (iop->ioc_count != sizeof(int))
		break;
	    if (mp->b_cont == 0) {
		DPRINT1("ppp_comp_wput/%d: PPPIO_DEBUG b_cont = 0!\n", cp->unit);
		break;
	    }
	    n = *(int *)mp->b_cont->b_rptr;
	    if (n == PPPDBG_LOG + PPPDBG_COMP) {
		DPRINT1("ppp_comp%d: debug log enabled\n", cp->unit);
		cp->flags |= DBGLOG;
		error = 0;
		iop->ioc_count = 0;
	    } else {
		error = -1;
	    }
	    break;

	case PPPIO_LASTMOD:
	    cp->flags |= LAST_MOD;
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
	    iop->ioc_error = error;
	    iop->ioc_count = 0;
	    qreply(q, mp);
	}
	break;

    case M_CTL:
	switch (*mp->b_rptr) {
	case PPPCTL_MTU:
	    cp->mtu = ((unsigned short *)mp->b_rptr)[1];
	    break;
	case PPPCTL_MRU:
	    cp->mru = ((unsigned short *)mp->b_rptr)[1];
	    break;
	case PPPCTL_UNIT:
	    cp->unit = mp->b_rptr[1];
	    break;
	}
	putnext(q, mp);
	break;

    default:
	putnext(q, mp);
    }

    return 0;
}

static int
ppp_comp_wsrv(q)
    queue_t *q;
{
    mblk_t *mp, *cmp = NULL;
    comp_state_t *cp;
    int len, proto, type, hlen, code;
    struct ip *ip;
    unsigned char *vjhdr, *dp;

    cp = (comp_state_t *) q->q_ptr;
    if (cp == 0) {
	DPRINT("cp == 0 in ppp_comp_wsrv\n");
	return 0;
    }

    while ((mp = getq(q)) != 0) {
	/* assert(mp->b_datap->db_type == M_DATA) */
#ifdef PRIOQ
        if (!bcanputnext(q,mp->b_band))
#else
        if (!canputnext(q))
#endif /* PRIOQ */
	{
	    putbq(q, mp);
	    break;
	}

	/*
	 * First check the packet length and work out what the protocol is.
	 */
	len = msgdsize(mp);
	if (len < PPP_HDRLEN) {
	    DPRINT1("ppp_comp_wsrv: bogus short packet (%d)\n", len);
	    freemsg(mp);
	    cp->stats.ppp_oerrors++;
	    putctl1(RD(q)->q_next, M_CTL, PPPCTL_OERROR);
	    continue;
	}
	proto = (MSG_BYTE(mp, 2) << 8) + MSG_BYTE(mp, 3);

	/*
	 * Make sure we've got enough data in the first mblk
	 * and that we are its only user.
	 */
	if (proto == PPP_CCP)
	    hlen = len;
	else if (proto == PPP_IP)
	    hlen = PPP_HDRLEN + MAX_IPHDR;
	else
	    hlen = PPP_HDRLEN;
	if (hlen > len)
	    hlen = len;
	if (mp->b_wptr < mp->b_rptr + hlen || mp->b_datap->db_ref > 1) {
	    PULLUP(mp, hlen);
	    if (mp == 0) {
		DPRINT1("ppp_comp_wsrv: pullup failed (%d)\n", hlen);
		cp->stats.ppp_oerrors++;
		putctl1(RD(q)->q_next, M_CTL, PPPCTL_OERROR);
		continue;
	    }
	}

	/*
	 * Do VJ compression if requested.
	 */
	if (proto == PPP_IP && (cp->flags & COMP_VJC)) {
	    ip = (struct ip *) (mp->b_rptr + PPP_HDRLEN);
	    if (ip->ip_p == IPPROTO_TCP) {
		type = vj_compress_tcp(ip, len - PPP_HDRLEN, &cp->vj_comp,
				       (cp->flags & COMP_VJCCID), &vjhdr);
		switch (type) {
		case TYPE_UNCOMPRESSED_TCP:
		    mp->b_rptr[3] = proto = PPP_VJC_UNCOMP;
		    break;
		case TYPE_COMPRESSED_TCP:
		    dp = vjhdr - PPP_HDRLEN;
		    dp[1] = mp->b_rptr[1]; /* copy control field */
		    dp[0] = mp->b_rptr[0]; /* copy address field */
		    dp[2] = 0;		   /* set protocol field */
		    dp[3] = proto = PPP_VJC_COMP;
		    mp->b_rptr = dp;
		    break;
		}
	    }
	}

	/*
	 * Do packet compression if enabled.
	 */
	if (proto == PPP_CCP)
	    ppp_comp_ccp(q, mp, 0);
	else if (proto != PPP_LCP && (cp->flags & CCP_COMP_RUN)
		 && cp->xstate != NULL) {
	    len = msgdsize(mp);
	    (*cp->xcomp->compress)(cp->xstate, &cmp, mp, len,
			(cp->flags & CCP_ISUP? cp->mtu + PPP_HDRLEN: 0));
	    if (cmp != NULL) {
#ifdef PRIOQ
		cmp->b_band=mp->b_band;
#endif /* PRIOQ */
		freemsg(mp);
		mp = cmp;
	    }
	}

	/*
	 * Do address/control and protocol compression if enabled.
	 */
	if ((cp->flags & COMP_AC)
	    && !(proto == PPP_LCP && LCP_USE_DFLT(mp))) {
	    mp->b_rptr += 2;	/* drop the address & ctrl fields */
	    if (proto < 0x100 && (cp->flags & COMP_PROT))
		++mp->b_rptr;	/* drop the high protocol byte */
	} else if (proto < 0x100 && (cp->flags & COMP_PROT)) {
	    /* shuffle up the address & ctrl fields */
	    mp->b_rptr[2] = mp->b_rptr[1];
	    mp->b_rptr[1] = mp->b_rptr[0];
	    ++mp->b_rptr;
	}

	cp->stats.ppp_opackets++;
	cp->stats.ppp_obytes += msgdsize(mp);
	putnext(q, mp);
    }

    return 0;
}

static int
ppp_comp_rput(q, mp)
    queue_t *q;
    mblk_t *mp;
{
    comp_state_t *cp;
    struct iocblk *iop;
    struct ppp_stats *psp;

    cp = (comp_state_t *) q->q_ptr;
    if (cp == 0) {
	DPRINT("cp == 0 in ppp_comp_rput\n");
	freemsg(mp);
	return 0;
    }

    switch (mp->b_datap->db_type) {

    case M_DATA:
	putq(q, mp);
	break;

    case M_IOCACK:
	iop = (struct iocblk *) mp->b_rptr;
	switch (iop->ioc_cmd) {
	case PPPIO_GETSTAT:
	    /*
	     * Catch this on the way back from the ppp_ahdl module
	     * so we can fill in the VJ stats.
	     */
	    if (mp->b_cont == 0 || iop->ioc_count != sizeof(struct ppp_stats))
		break;
	    psp = (struct ppp_stats *) mp->b_cont->b_rptr;
	    psp->vj = cp->vj_comp.stats;
	    break;
	}
	putnext(q, mp);
	break;

    case M_CTL:
	switch (mp->b_rptr[0]) {
	case PPPCTL_IERROR:
	    ++cp->stats.ppp_ierrors;
	    break;
	case PPPCTL_OERROR:
	    ++cp->stats.ppp_oerrors;
	    break;
	}
	putnext(q, mp);
	break;

    default:
	putnext(q, mp);
    }

    return 0;
}

static int
ppp_comp_rsrv(q)
    queue_t *q;
{
    int proto, rv, i;
    mblk_t *mp, *dmp = NULL, *np;
    uchar_t *dp, *iphdr;
    comp_state_t *cp;
    int len, hlen, vjlen;
    u_int iphlen;

    cp = (comp_state_t *) q->q_ptr;
    if (cp == 0) {
	DPRINT("cp == 0 in ppp_comp_rsrv\n");
	return 0;
    }

    while ((mp = getq(q)) != 0) {
	/* assert(mp->b_datap->db_type == M_DATA) */
	if (!canputnext(q)) {
	    putbq(q, mp);
	    break;
	}

	len = msgdsize(mp);
	cp->stats.ppp_ibytes += len;
	cp->stats.ppp_ipackets++;

	/*
	 * First work out the protocol and where the PPP header ends.
	 */
	i = 0;
	proto = MSG_BYTE(mp, 0);
	if (proto == PPP_ALLSTATIONS) {
	    i = 2;
	    proto = MSG_BYTE(mp, 2);
	}
	if ((proto & 1) == 0) {
	    ++i;
	    proto = (proto << 8) + MSG_BYTE(mp, i);
	}
	hlen = i + 1;

	/*
	 * Now reconstruct a complete, contiguous PPP header at the
	 * start of the packet.
	 */
	if (hlen < ((cp->flags & DECOMP_AC)? 0: 2)
	           + ((cp->flags & DECOMP_PROT)? 1: 2)) {
	    /* count these? */
	    goto bad;
	}
	if (mp->b_rptr + hlen > mp->b_wptr) {
	    adjmsg(mp, hlen);	/* XXX check this call */
	    hlen = 0;
	}
	if (hlen != PPP_HDRLEN) {
	    /*
	     * We need to put some bytes on the front of the packet
	     * to make a full-length PPP header.
	     * If we can put them in *mp, we do, otherwise we
	     * tack another mblk on the front.
	     * XXX we really shouldn't need to carry around
	     * the address and control at this stage.
	     */
	    dp = mp->b_rptr + hlen - PPP_HDRLEN;
	    if (dp < mp->b_datap->db_base || mp->b_datap->db_ref > 1) {
		np = allocb(PPP_HDRLEN, BPRI_MED);
		if (np == 0)
		    goto bad;
		np->b_cont = mp;
		mp->b_rptr += hlen;
		mp = np;
		dp = mp->b_wptr;
		mp->b_wptr += PPP_HDRLEN;
	    } else
		mp->b_rptr = dp;

	    dp[0] = PPP_ALLSTATIONS;
	    dp[1] = PPP_UI;
	    dp[2] = proto >> 8;
	    dp[3] = proto;
	}

	/*
	 * Now see if we have a compressed packet to decompress,
	 * or a CCP packet to take notice of.
	 */
	proto = PPP_PROTOCOL(mp->b_rptr);
	if (proto == PPP_CCP) {
	    len = msgdsize(mp);
	    if (mp->b_wptr < mp->b_rptr + len) {
		PULLUP(mp, len);
		if (mp == 0)
		    goto bad;
	    }
	    ppp_comp_ccp(q, mp, 1);
	} else if (proto == PPP_COMP) {
	    if ((cp->flags & CCP_ISUP)
		&& (cp->flags & CCP_DECOMP_RUN) && cp->rstate
		&& (cp->flags & CCP_ERR) == 0) {
		rv = (*cp->rcomp->decompress)(cp->rstate, mp, &dmp);
		switch (rv) {
		case DECOMP_OK:
		    freemsg(mp);
		    mp = dmp;
		    if (mp == NULL) {
			/* no error, but no packet returned either. */
			continue;
		    }
		    break;
		case DECOMP_ERROR:
		    cp->flags |= CCP_ERROR;
		    ++cp->stats.ppp_ierrors;
		    putctl1(q->q_next, M_CTL, PPPCTL_IERROR);
		    break;
		case DECOMP_FATALERROR:
		    cp->flags |= CCP_FATALERROR;
		    ++cp->stats.ppp_ierrors;
		    putctl1(q->q_next, M_CTL, PPPCTL_IERROR);
		    break;
		}
	    }
	} else if (cp->rstate && (cp->flags & CCP_DECOMP_RUN)) {
	    (*cp->rcomp->incomp)(cp->rstate, mp);
	}

	/*
	 * Now do VJ decompression.
	 */
	proto = PPP_PROTOCOL(mp->b_rptr);
	if (proto == PPP_VJC_COMP || proto == PPP_VJC_UNCOMP) {
	    len = msgdsize(mp) - PPP_HDRLEN;
	    if ((cp->flags & DECOMP_VJC) == 0 || len <= 0)
		goto bad;

	    /*
	     * Advance past the ppp header.
	     * Here we assume that the whole PPP header is in the first mblk.
	     */
	    np = mp;
	    dp = np->b_rptr + PPP_HDRLEN;
	    if (dp >= mp->b_wptr) {
		np = np->b_cont;
		dp = np->b_rptr;
	    }

	    /*
	     * Make sure we have sufficient contiguous data at this point.
	     */
	    hlen = (proto == PPP_VJC_COMP)? MAX_VJHDR: MAX_IPHDR;
	    if (hlen > len)
		hlen = len;
	    if (np->b_wptr < dp + hlen || np->b_datap->db_ref > 1) {
		PULLUP(mp, hlen + PPP_HDRLEN);
		if (mp == 0)
		    goto bad;
		np = mp;
		dp = np->b_rptr + PPP_HDRLEN;
	    }

	    if (proto == PPP_VJC_COMP) {
		/*
		 * Decompress VJ-compressed packet.
		 * First reset compressor if an input error has occurred.
		 */
		if (cp->stats.ppp_ierrors != cp->vj_last_ierrors) {
		    if (cp->flags & DBGLOG)
			DPRINT1("ppp%d: resetting VJ\n", cp->unit);
		    vj_uncompress_err(&cp->vj_comp);
		    cp->vj_last_ierrors = cp->stats.ppp_ierrors;
		}

		vjlen = vj_uncompress_tcp(dp, np->b_wptr - dp, len,
					  &cp->vj_comp, &iphdr, &iphlen);
		if (vjlen < 0) {
		    if (cp->flags & DBGLOG)
			DPRINT2("ppp%d: vj_uncomp_tcp failed, pkt len %d\n",
				cp->unit, len);
		    ++cp->vj_last_ierrors;  /* so we don't reset next time */
		    goto bad;
		}

		/* drop ppp and vj headers off */
		if (mp != np) {
		    freeb(mp);
		    mp = np;
		}
		mp->b_rptr = dp + vjlen;

		/* allocate a new mblk for the ppp and ip headers */
		if ((np = allocb(iphlen + PPP_HDRLEN + 4, BPRI_MED)) == 0)
		    goto bad;
		dp = np->b_rptr;	/* prepend mblk with TCP/IP hdr */
		dp[0] = PPP_ALLSTATIONS; /* reconstruct PPP header */
		dp[1] = PPP_UI;
		dp[2] = PPP_IP >> 8;
		dp[3] = PPP_IP;
		bcopy((caddr_t)iphdr, (caddr_t)dp + PPP_HDRLEN, iphlen);
		np->b_wptr = dp + iphlen + PPP_HDRLEN;
		np->b_cont = mp;

		/* XXX there seems to be a bug which causes panics in strread
		   if we make an mbuf with only the IP header in it :-( */
		if (mp->b_wptr - mp->b_rptr > 4) {
		    bcopy((caddr_t)mp->b_rptr, (caddr_t)np->b_wptr, 4);
		    mp->b_rptr += 4;
		    np->b_wptr += 4;
		} else {
		    bcopy((caddr_t)mp->b_rptr, (caddr_t)np->b_wptr,
			  mp->b_wptr - mp->b_rptr);
		    np->b_wptr += mp->b_wptr - mp->b_rptr;
		    np->b_cont = mp->b_cont;
		    freeb(mp);
		}

		mp = np;

	    } else {
		/*
		 * "Decompress" a VJ-uncompressed packet.
		 */
		cp->vj_last_ierrors = cp->stats.ppp_ierrors;
		if (!vj_uncompress_uncomp(dp, hlen, &cp->vj_comp)) {
		    if (cp->flags & DBGLOG)
			DPRINT2("ppp%d: vj_uncomp_uncomp failed, pkt len %d\n",
				cp->unit, len);
		    ++cp->vj_last_ierrors;  /* don't need to reset next time */
		    goto bad;
		}
		mp->b_rptr[3] = PPP_IP;	/* fix up the PPP protocol field */
	    }
	}

	putnext(q, mp);
	continue;

    bad:
	if (mp != 0)
	    freemsg(mp);
	cp->stats.ppp_ierrors++;
	putctl1(q->q_next, M_CTL, PPPCTL_IERROR);
    }

    return 0;
}

/*
 * Handle a CCP packet being sent or received.
 * Here all the data in the packet is in a single mbuf.
 */
static void
ppp_comp_ccp(q, mp, rcvd)
    queue_t *q;
    mblk_t *mp;
    int rcvd;
{
    int len, clen;
    comp_state_t *cp;
    unsigned char *dp;

    len = msgdsize(mp);
    if (len < PPP_HDRLEN + CCP_HDRLEN)
	return;

    cp = (comp_state_t *) q->q_ptr;
    dp = mp->b_rptr + PPP_HDRLEN;
    len -= PPP_HDRLEN;
    clen = CCP_LENGTH(dp);
    if (clen > len)
	return;

    switch (CCP_CODE(dp)) {
    case CCP_CONFREQ:
    case CCP_TERMREQ:
    case CCP_TERMACK:
	cp->flags &= ~CCP_ISUP;
	break;

    case CCP_CONFACK:
	if ((cp->flags & (CCP_ISOPEN | CCP_ISUP)) == CCP_ISOPEN
	    && clen >= CCP_HDRLEN + CCP_OPT_MINLEN
	    && clen >= CCP_HDRLEN + CCP_OPT_LENGTH(dp + CCP_HDRLEN)) {
	    if (!rcvd) {
		if (cp->xstate != NULL
		    && (*cp->xcomp->comp_init)
		        (cp->xstate, dp + CCP_HDRLEN, clen - CCP_HDRLEN,
			 cp->unit, 0, ((cp->flags & DBGLOG) != 0)))
		    cp->flags |= CCP_COMP_RUN;
	    } else {
		if (cp->rstate != NULL
		    && (*cp->rcomp->decomp_init)
		        (cp->rstate, dp + CCP_HDRLEN, clen - CCP_HDRLEN,
			 cp->unit, 0, cp->mru, ((cp->flags & DBGLOG) != 0)))
		    cp->flags = (cp->flags & ~CCP_ERR) | CCP_DECOMP_RUN;
	    }
	}
	break;

    case CCP_RESETACK:
	if (cp->flags & CCP_ISUP) {
	    if (!rcvd) {
		if (cp->xstate && (cp->flags & CCP_COMP_RUN))
		    (*cp->xcomp->comp_reset)(cp->xstate);
	    } else {
		if (cp->rstate && (cp->flags & CCP_DECOMP_RUN)) {
		    (*cp->rcomp->decomp_reset)(cp->rstate);
		    cp->flags &= ~CCP_ERROR;
		}
	    }
	}
	break;
    }
}

#if 0
dump_msg(mp)
    mblk_t *mp;
{
    dblk_t *db;

    while (mp != 0) {
	db = mp->b_datap;
	DPRINT2("mp=%x cont=%x ", mp, mp->b_cont);
	DPRINT3("rptr=%x wptr=%x datap=%x\n", mp->b_rptr, mp->b_wptr, db);
	DPRINT2("  base=%x lim=%x", db->db_base, db->db_lim);
	DPRINT2(" ref=%d type=%d\n", db->db_ref, db->db_type);
	mp = mp->b_cont;
    }
}
#endif

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
