/*
 * ppp.c - STREAMS multiplexing pseudo-device driver for PPP.
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
 * $Id: ppp.c,v 1.4 2005/06/27 00:59:57 carlsonj Exp $
 */

/*
 * This file is used under Solaris 2, SVR4, SunOS 4, and Digital UNIX.
 */

#include <sys/types.h>
#include <sys/param.h>
#include <sys/stat.h>
#include <sys/stream.h>
#include <sys/stropts.h>
#include <sys/errno.h>
#ifdef __osf__
#include <sys/ioctl.h>
#include <sys/cmn_err.h>
#define queclass(mp)	((mp)->b_band & QPCTL)
#else
#include <sys/ioccom.h>
#endif
#include <sys/time.h>
#ifdef SVR4
#include <sys/cmn_err.h>
#include <sys/conf.h>
#include <sys/dlpi.h>
#include <sys/ddi.h>
#ifdef SOL2
#include <sys/ksynch.h>
#include <sys/kstat.h>
#include <sys/sunddi.h>
#include <sys/ethernet.h>
#else
#include <sys/socket.h>
#include <sys/sockio.h>
#include <net/if.h>
#include <netinet/in.h>
#endif /* SOL2 */
#else /* not SVR4 */
#include <sys/user.h>
#endif /* SVR4 */
#include <net/ppp_defs.h>
#include <net/pppio.h>
#include "ppp_mod.h"

/*
 * Modifications marked with #ifdef PRIOQ are for priority queueing of
 * interactive traffic, and are due to Marko Zec <zec@japa.tel.fer.hr>.
 */
#ifdef PRIOQ
#endif	/* PRIOQ */

#include <netinet/in.h>	/* leave this outside of PRIOQ for htons */

#ifdef __STDC__
#define __P(x)	x
#else
#define __P(x)	()
#endif

/*
 * The IP module may use this SAP value for IP packets.
 */
#ifndef ETHERTYPE_IP
#define ETHERTYPE_IP	0x800
#endif

#if !defined(ETHERTYPE_IPV6) 
#define ETHERTYPE_IPV6	0x86dd
#endif /* !defined(ETHERTYPE_IPV6) */

#if !defined(ETHERTYPE_ALLSAP) && defined(SOL2)
#define ETHERTYPE_ALLSAP   0
#endif /* !defined(ETHERTYPE_ALLSAP) && defined(SOL2) */

#if !defined(PPP_ALLSAP) && defined(SOL2)
#define PPP_ALLSAP  PPP_ALLSTATIONS
#endif /* !defined(PPP_ALLSAP) && defined(SOL2) */

extern time_t time;

#ifdef SOL2
/*
 * We use this reader-writer lock to ensure that the lower streams
 * stay connected to the upper streams while the lower-side put and
 * service procedures are running.  Essentially it is an existence
 * lock for the upper stream associated with each lower stream.
 */
krwlock_t ppp_lower_lock;
#define LOCK_LOWER_W	rw_enter(&ppp_lower_lock, RW_WRITER)
#define LOCK_LOWER_R	rw_enter(&ppp_lower_lock, RW_READER)
#define TRYLOCK_LOWER_R	rw_tryenter(&ppp_lower_lock, RW_READER)
#define UNLOCK_LOWER	rw_exit(&ppp_lower_lock)

#define MT_ENTER(x)	mutex_enter(x)
#define MT_EXIT(x)	mutex_exit(x)

/*
 * Notes on multithreaded implementation for Solaris 2:
 *
 * We use an inner perimeter around each queue pair and an outer
 * perimeter around the whole driver.  The inner perimeter is
 * entered exclusively for all entry points (open, close, put,
 * service).  The outer perimeter is entered exclusively for open
 * and close and shared for put and service.  This is all done for
 * us by the streams framework.
 *
 * I used to think that the perimeters were entered for the lower
 * streams' put and service routines as well as for the upper streams'.
 * Because of problems experienced by people, and after reading the
 * documentation more closely, I now don't think that is true.  So we
 * now use ppp_lower_lock to give us an existence guarantee on the
 * upper stream controlling each lower stream.
 *
 * Shared entry to the outer perimeter protects the existence of all
 * the upper streams and their upperstr_t structures, and guarantees
 * that the following fields of any upperstr_t won't change:
 * nextmn, next, nextppa.  It guarantees that the lowerq field of an
 * upperstr_t won't go from non-zero to zero, that the global `ppas'
 * won't change and that the no lower stream will get unlinked.
 *
 * Shared (reader) access to ppa_lower_lock guarantees that no lower
 * stream will be unlinked and that the lowerq field of all upperstr_t
 * structures won't change.
 */

#else /* SOL2 */
#define LOCK_LOWER_W	0
#define LOCK_LOWER_R	0
#define TRYLOCK_LOWER_R	1
#define UNLOCK_LOWER	0
#define MT_ENTER(x)	0
#define MT_EXIT(x)	0

#endif /* SOL2 */

/*
 * Private information; one per upper stream.
 */
typedef struct upperstr {
    minor_t mn;			/* minor device number */
    struct upperstr *nextmn;	/* next minor device */
    queue_t *q;			/* read q associated with this upper stream */
    int flags;			/* flag bits, see below */
    int state;			/* current DLPI state */
    int sap;			/* service access point */
    int req_sap;		/* which SAP the DLPI client requested */
    struct upperstr *ppa;	/* control stream for our ppa */
    struct upperstr *next;	/* next stream for this ppa */
    uint ioc_id;		/* last ioctl ID for this stream */
    enum NPmode npmode;		/* what to do with packets on this SAP */
    unsigned char rblocked;	/* flow control has blocked upper read strm */
	/* N.B. rblocked is only changed by control stream's put/srv procs */
    /*
     * There is exactly one control stream for each PPA.
     * The following fields are only used for control streams.
     */
    int ppa_id;
    queue_t *lowerq;		/* write queue attached below this PPA */
    struct upperstr *nextppa;	/* next control stream */
    int mru;
    int mtu;
    struct pppstat stats;	/* statistics */
    time_t last_sent;		/* time last NP packet sent */
    time_t last_recv;		/* time last NP packet rcvd */
#ifdef SOL2
    kmutex_t stats_lock;	/* lock for stats updates */
    kstat_t *kstats;		/* stats for netstat */
#endif /* SOL2 */
#ifdef LACHTCP
    int ifflags;
    char ifname[IFNAMSIZ];
    struct ifstats ifstats;
#endif /* LACHTCP */
} upperstr_t;

/* Values for flags */
#define US_PRIV		1	/* stream was opened by superuser */
#define US_CONTROL	2	/* stream is a control stream */
#define US_BLOCKED	4	/* flow ctrl has blocked lower write stream */
#define US_LASTMOD	8	/* no PPP modules below us */
#define US_DBGLOG	0x10	/* log various occurrences */
#define US_RBLOCKED	0x20	/* flow ctrl has blocked upper read stream */

#if defined(SOL2)
#if DL_CURRENT_VERSION >= 2
#define US_PROMISC	0x40	/* stream is promiscuous */
#endif /* DL_CURRENT_VERSION >= 2 */
#define US_RAWDATA	0x80	/* raw M_DATA, no DLPI header */
#endif /* defined(SOL2) */

#ifdef PRIOQ
static u_char max_band=0;
static u_char def_band=0;

#define IPPORT_DEFAULT		65535

/*
 * Port priority table
 * Highest priority ports are listed first, lowest are listed last.
 * ICMP & packets using unlisted ports will be treated as "default".
 * If IPPORT_DEFAULT is not listed here, "default" packets will be 
 * assigned lowest priority.
 * Each line should be terminated with "0".
 * Line containing only "0" marks the end of the list.
 */

static u_short prioq_table[]= {
    113, 53, 0,
    22, 23, 513, 517, 518, 0,
    514, 21, 79, 111, 0,
    25, 109, 110, 0,
    IPPORT_DEFAULT, 0,
    20, 70, 80, 8001, 8008, 8080, 0, /* 8001,8008,8080 - common proxy ports */
0 };

#endif	/* PRIOQ */


static upperstr_t *minor_devs = NULL;
static upperstr_t *ppas = NULL;

#ifdef SVR4
static int pppopen __P((queue_t *, dev_t *, int, int, cred_t *));
static int pppclose __P((queue_t *, int, cred_t *));
#else
static int pppopen __P((queue_t *, int, int, int));
static int pppclose __P((queue_t *, int));
#endif /* SVR4 */
static int pppurput __P((queue_t *, mblk_t *));
static int pppuwput __P((queue_t *, mblk_t *));
static int pppursrv __P((queue_t *));
static int pppuwsrv __P((queue_t *));
static int ppplrput __P((queue_t *, mblk_t *));
static int ppplwput __P((queue_t *, mblk_t *));
static int ppplrsrv __P((queue_t *));
static int ppplwsrv __P((queue_t *));
#ifndef NO_DLPI
static void dlpi_request __P((queue_t *, mblk_t *, upperstr_t *));
static void dlpi_error __P((queue_t *, upperstr_t *, int, int, int));
static void dlpi_ok __P((queue_t *, int));
#endif
static int send_data __P((mblk_t *, upperstr_t *));
static void new_ppa __P((queue_t *, mblk_t *));
static void attach_ppa __P((queue_t *, mblk_t *));
#ifndef NO_DLPI
static void detach_ppa __P((queue_t *, mblk_t *));
#endif
static void detach_lower __P((queue_t *, mblk_t *));
static void debug_dump __P((queue_t *, mblk_t *));
static upperstr_t *find_dest __P((upperstr_t *, int));
#if defined(SOL2)
static upperstr_t *find_promisc __P((upperstr_t *, int));
static mblk_t *prepend_ether __P((upperstr_t *, mblk_t *, int));
static mblk_t *prepend_udind __P((upperstr_t *, mblk_t *, int));
static void promisc_sendup __P((upperstr_t *, mblk_t *, int, int));
#endif /* defined(SOL2) */
static int putctl2 __P((queue_t *, int, int, int));
static int putctl4 __P((queue_t *, int, int, int));
static int pass_packet __P((upperstr_t *ppa, mblk_t *mp, int outbound));
#ifdef FILTER_PACKETS
static int ip_hard_filter __P((upperstr_t *ppa, mblk_t *mp, int outbound));
#endif /* FILTER_PACKETS */

#define PPP_ID 0xb1a6
static struct module_info ppp_info = {
#ifdef PRIOQ
    PPP_ID, "ppp", 0, 512, 512, 384
#else
    PPP_ID, "ppp", 0, 512, 512, 128
#endif	/* PRIOQ */
};

static struct qinit pppurint = {
    pppurput, pppursrv, pppopen, pppclose, NULL, &ppp_info, NULL
};

static struct qinit pppuwint = {
    pppuwput, pppuwsrv, NULL, NULL, NULL, &ppp_info, NULL
};

static struct qinit ppplrint = {
    ppplrput, ppplrsrv, NULL, NULL, NULL, &ppp_info, NULL
};

static struct qinit ppplwint = {
    ppplwput, ppplwsrv, NULL, NULL, NULL, &ppp_info, NULL
};

#ifdef LACHTCP
extern struct ifstats *ifstats;
int pppdevflag = 0;
#endif

struct streamtab pppinfo = {
    &pppurint, &pppuwint,
    &ppplrint, &ppplwint
};

int ppp_count;

/*
 * How we maintain statistics.
 */
#ifdef SOL2
#define INCR_IPACKETS(ppa)				\
	if (ppa->kstats != 0) {				\
	    KSTAT_NAMED_PTR(ppa->kstats)[0].value.ul++;	\
	}
#define INCR_IERRORS(ppa)				\
	if (ppa->kstats != 0) {				\
	    KSTAT_NAMED_PTR(ppa->kstats)[1].value.ul++;	\
	}
#define INCR_OPACKETS(ppa)				\
	if (ppa->kstats != 0) {				\
	    KSTAT_NAMED_PTR(ppa->kstats)[2].value.ul++;	\
	}
#define INCR_OERRORS(ppa)				\
	if (ppa->kstats != 0) {				\
	    KSTAT_NAMED_PTR(ppa->kstats)[3].value.ul++;	\
	}
#endif

#ifdef LACHTCP
#define INCR_IPACKETS(ppa)	ppa->ifstats.ifs_ipackets++;
#define INCR_IERRORS(ppa)	ppa->ifstats.ifs_ierrors++;
#define INCR_OPACKETS(ppa)	ppa->ifstats.ifs_opackets++;
#define INCR_OERRORS(ppa)	ppa->ifstats.ifs_oerrors++;
#endif

/*
 * STREAMS driver entry points.
 */
static int
#ifdef SVR4
pppopen(q, devp, oflag, sflag, credp)
    queue_t *q;
    dev_t *devp;
    int oflag, sflag;
    cred_t *credp;
#else
pppopen(q, dev, oflag, sflag)
    queue_t *q;
    int dev;			/* really dev_t */
    int oflag, sflag;
#endif
{
    upperstr_t *up;
    upperstr_t **prevp;
    minor_t mn;
#ifdef PRIOQ
    u_short *ptr;
    u_char new_band;
#endif	/* PRIOQ */

    if (q->q_ptr)
	DRV_OPEN_OK(dev);	/* device is already open */

#ifdef PRIOQ
    /* Calculate max_bband & def_band from definitions in prioq.h
       This colud be done at some more approtiate time (less often)
       but this way it works well so I'll just leave it here */

    max_band = 1;
    def_band = 0;
    ptr = prioq_table;
    while (*ptr) {
        new_band = 1;
        while (*ptr)
	    if (*ptr++ == IPPORT_DEFAULT) {
		new_band = 0;
		def_band = max_band;
	    }
        max_band += new_band;
        ptr++;
    }
    if (def_band)
        def_band = max_band - def_band;
    --max_band;
#endif	/* PRIOQ */

    if (sflag == CLONEOPEN) {
	mn = 0;
	for (prevp = &minor_devs; (up = *prevp) != 0; prevp = &up->nextmn) {
	    if (up->mn != mn)
		break;
	    ++mn;
	}
    } else {
#ifdef SVR4
	mn = getminor(*devp);
#else
	mn = minor(dev);
#endif
	for (prevp = &minor_devs; (up = *prevp) != 0; prevp = &up->nextmn) {
	    if (up->mn >= mn)
		break;
	}
	if (up->mn == mn) {
	    /* this can't happen */
	    q->q_ptr = WR(q)->q_ptr = (caddr_t) up;
	    DRV_OPEN_OK(dev);
	}
    }

    /*
     * Construct a new minor node.
     */
    up = (upperstr_t *) ALLOC_SLEEP(sizeof(upperstr_t));
    bzero((caddr_t) up, sizeof(upperstr_t));
    if (up == 0) {
	DPRINT("pppopen: out of kernel memory\n");
	OPEN_ERROR(ENXIO);
    }
    up->nextmn = *prevp;
    *prevp = up;
    up->mn = mn;
#ifdef SVR4
    *devp = makedevice(getmajor(*devp), mn);
#endif
    up->q = q;
    if (NOTSUSER() == 0)
	up->flags |= US_PRIV;
#ifndef NO_DLPI
    up->state = DL_UNATTACHED;
#endif
#ifdef LACHTCP
    up->ifflags = IFF_UP | IFF_POINTOPOINT;
#endif
    up->sap = -1;
    up->last_sent = up->last_recv = time;
    up->npmode = NPMODE_DROP;
    q->q_ptr = (caddr_t) up;
    WR(q)->q_ptr = (caddr_t) up;
    noenable(WR(q));
#ifdef SOL2
    mutex_init(&up->stats_lock, NULL, MUTEX_DRIVER, NULL);
#endif
    ++ppp_count;

    qprocson(q);
    DRV_OPEN_OK(makedev(major(dev), mn));
}

static int
#ifdef SVR4
pppclose(q, flag, credp)
    queue_t *q;
    int flag;
    cred_t *credp;
#else
pppclose(q, flag)
    queue_t *q;
    int flag;
#endif
{
    upperstr_t *up, **upp;
    upperstr_t *as, *asnext;
    upperstr_t **prevp;

    qprocsoff(q);

    up = (upperstr_t *) q->q_ptr;
    if (up == 0) {
	DPRINT("pppclose: q_ptr = 0\n");
	return 0;
    }
    if (up->flags & US_DBGLOG)
	DPRINT2("ppp/%d: close, flags=%x\n", up->mn, up->flags);
    if (up->flags & US_CONTROL) {
#ifdef LACHTCP
	struct ifstats *ifp, *pifp;
#endif
	if (up->lowerq != 0) {
	    /* Gack! the lower stream should have be unlinked earlier! */
	    DPRINT1("ppp%d: lower stream still connected on close?\n",
		    up->mn);
	    LOCK_LOWER_W;
	    up->lowerq->q_ptr = 0;
	    RD(up->lowerq)->q_ptr = 0;
	    up->lowerq = 0;
	    UNLOCK_LOWER;
	}

	/*
	 * This stream represents a PPA:
	 * For all streams attached to the PPA, clear their
	 * references to this PPA.
	 * Then remove this PPA from the list of PPAs.
	 */
	for (as = up->next; as != 0; as = asnext) {
	    asnext = as->next;
	    as->next = 0;
	    as->ppa = 0;
	    if (as->flags & US_BLOCKED) {
		as->flags &= ~US_BLOCKED;
		flushq(WR(as->q), FLUSHDATA);
	    }
	}
	for (upp = &ppas; *upp != 0; upp = &(*upp)->nextppa)
	    if (*upp == up) {
		*upp = up->nextppa;
		break;
	    }
#ifdef LACHTCP
	/* Remove the statistics from the active list.  */
	for (ifp = ifstats, pifp = 0; ifp; ifp = ifp->ifs_next) {
	    if (ifp == &up->ifstats) {
		if (pifp)
		    pifp->ifs_next = ifp->ifs_next;
		else
		    ifstats = ifp->ifs_next;
		break;
	    }
	    pifp = ifp;
	}
#endif
    } else {
	/*
	 * If this stream is attached to a PPA,
	 * remove it from the PPA's list.
	 */
	if ((as = up->ppa) != 0) {
	    for (; as->next != 0; as = as->next)
		if (as->next == up) {
		    as->next = up->next;
		    break;
		}
	}
    }

#ifdef SOL2
    if (up->kstats)
	kstat_delete(up->kstats);
    mutex_destroy(&up->stats_lock);
#endif

    q->q_ptr = NULL;
    WR(q)->q_ptr = NULL;

    for (prevp = &minor_devs; *prevp != 0; prevp = &(*prevp)->nextmn) {
	if (*prevp == up) {
	    *prevp = up->nextmn;
	    break;
	}
    }
    FREE(up, sizeof(upperstr_t));
    --ppp_count;

    return 0;
}

/*
 * A message from on high.  We do one of three things:
 *	- qreply()
 *	- put the message on the lower write stream
 *	- queue it for our service routine
 */
static int
pppuwput(q, mp)
    queue_t *q;
    mblk_t *mp;
{
    upperstr_t *us, *ppa, *nps;
    struct iocblk *iop;
    struct linkblk *lb;
#ifdef LACHTCP
    struct ifreq *ifr;
    int i;
#endif
    queue_t *lq;
    int error, n, sap;
    mblk_t *mq;
    struct ppp_idle *pip;
#ifdef PRIOQ
    queue_t *tlq;
#endif	/* PRIOQ */
#ifdef NO_DLPI
    upperstr_t *os;
#endif

    us = (upperstr_t *) q->q_ptr;
    if (us == 0) {
	DPRINT("pppuwput: q_ptr = 0!\n");
	return 0;
    }
    if (mp == 0) {
	DPRINT1("pppuwput/%d: mp = 0!\n", us->mn);
	return 0;
    }
    if (mp->b_datap == 0) {
	DPRINT1("pppuwput/%d: mp->b_datap = 0!\n", us->mn);
	return 0;
    }
    switch (mp->b_datap->db_type) {
#ifndef NO_DLPI
    case M_PCPROTO:
    case M_PROTO:
	dlpi_request(q, mp, us);
	break;
#endif /* NO_DLPI */

    case M_DATA:
	if (us->flags & US_DBGLOG)
	    DPRINT3("ppp/%d: uwput M_DATA len=%d flags=%x\n",
		    us->mn, msgdsize(mp), us->flags);
	if (us->ppa == 0 || msgdsize(mp) > us->ppa->mtu + PPP_HDRLEN
#ifndef NO_DLPI
	    || (us->flags & US_CONTROL) == 0
#endif /* NO_DLPI */
	    ) {
	    DPRINT1("pppuwput: junk data len=%d\n", msgdsize(mp));
	    freemsg(mp);
	    break;
	}
#ifdef NO_DLPI
	/* pass_packet frees the packet on returning 0 */
	if ((us->flags & US_CONTROL) == 0 && !pass_packet(us, mp, 1))
	    break;
#endif
	if (!send_data(mp, us) && !putq(q, mp))
	    freemsg(mp);
	break;

    case M_IOCTL:
	iop = (struct iocblk *) mp->b_rptr;
	error = EINVAL;
	if (us->flags & US_DBGLOG)
	    DPRINT3("ppp/%d: ioctl %x count=%d\n",
		    us->mn, iop->ioc_cmd, iop->ioc_count);
	switch (iop->ioc_cmd) {
#if defined(SOL2)
	case DLIOCRAW:	    /* raw M_DATA mode */
	    us->flags |= US_RAWDATA;
	    error = 0;
	    break;
#endif /* defined(SOL2) */
	case I_LINK:
	    if ((us->flags & US_CONTROL) == 0 || us->lowerq != 0)
		break;
	    if (mp->b_cont == 0) {
		DPRINT1("pppuwput/%d: ioctl I_LINK b_cont = 0!\n", us->mn);
		break;
	    }
	    lb = (struct linkblk *) mp->b_cont->b_rptr;
	    lq = lb->l_qbot;
	    if (lq == 0) {
		DPRINT1("pppuwput/%d: ioctl I_LINK l_qbot = 0!\n", us->mn);
		break;
	    }
	    LOCK_LOWER_W;
	    us->lowerq = lq;
	    lq->q_ptr = (caddr_t) q;
	    RD(lq)->q_ptr = (caddr_t) us->q;
	    UNLOCK_LOWER;
	    iop->ioc_count = 0;
	    error = 0;
	    us->flags &= ~US_LASTMOD;
	    /* Unblock upper streams which now feed this lower stream. */
	    qenable(q);
	    /* Send useful information down to the modules which
	       are now linked below us. */
	    putctl2(lq, M_CTL, PPPCTL_UNIT, us->ppa_id);
	    putctl4(lq, M_CTL, PPPCTL_MRU, us->mru);
	    putctl4(lq, M_CTL, PPPCTL_MTU, us->mtu);
#ifdef PRIOQ
            /* Lower tty driver's queue hiwat/lowat from default 4096/128
               to 256/128 since we don't want queueing of data on
               output to physical device */

            freezestr(lq);
            for (tlq = lq; tlq->q_next != NULL; tlq = tlq->q_next)
		;
            strqset(tlq, QHIWAT, 0, 256);
            strqset(tlq, QLOWAT, 0, 128);
            unfreezestr(lq);
#endif	/* PRIOQ */
	    break;

	case I_UNLINK:
	    if (mp->b_cont == 0) {
		DPRINT1("pppuwput/%d: ioctl I_UNLINK b_cont = 0!\n", us->mn);
		break;
	    }
	    lb = (struct linkblk *) mp->b_cont->b_rptr;
#if DEBUG
	    if (us->lowerq != lb->l_qbot) {
		DPRINT2("ppp unlink: lowerq=%x qbot=%x\n",
			us->lowerq, lb->l_qbot);
		break;
	    }
#endif
	    iop->ioc_count = 0;
	    qwriter(q, mp, detach_lower, PERIM_OUTER);
	    /* mp is now gone */
	    error = -1;
	    break;

	case PPPIO_NEWPPA:
	    if (us->flags & US_CONTROL)
		break;
	    if ((us->flags & US_PRIV) == 0) {
		error = EPERM;
		break;
	    }
	    /* Arrange to return an int */
	    if ((mq = mp->b_cont) == 0
		|| mq->b_datap->db_lim - mq->b_rptr < sizeof(int)) {
		mq = allocb(sizeof(int), BPRI_HI);
		if (mq == 0) {
		    error = ENOSR;
		    break;
		}
		if (mp->b_cont != 0)
		    freemsg(mp->b_cont);
		mp->b_cont = mq;
		mq->b_cont = 0;
	    }
	    iop->ioc_count = sizeof(int);
	    mq->b_wptr = mq->b_rptr + sizeof(int);
	    qwriter(q, mp, new_ppa, PERIM_OUTER);
	    /* mp is now gone */
	    error = -1;
	    break;

	case PPPIO_ATTACH:
	    /* like dlpi_attach, for programs which can't write to
	       the stream (like pppstats) */
	    if (iop->ioc_count != sizeof(int) || us->ppa != 0)
		break;
	    if (mp->b_cont == 0) {
		DPRINT1("pppuwput/%d: ioctl PPPIO_ATTACH b_cont = 0!\n", us->mn);
		break;
	    }
	    n = *(int *)mp->b_cont->b_rptr;
	    for (ppa = ppas; ppa != 0; ppa = ppa->nextppa)
		if (ppa->ppa_id == n)
		    break;
	    if (ppa == 0)
		break;
	    us->ppa = ppa;
	    iop->ioc_count = 0;
	    qwriter(q, mp, attach_ppa, PERIM_OUTER);
	    /* mp is now gone */
	    error = -1;
	    break;

#ifdef NO_DLPI
	case PPPIO_BIND:
	    /* Attach to a given SAP. */
	    if (iop->ioc_count != sizeof(int) || us->ppa == 0)
		break;
	    if (mp->b_cont == 0) {
		DPRINT1("pppuwput/%d: ioctl PPPIO_BIND b_cont = 0!\n", us->mn);
		break;
	    }
	    n = *(int *)mp->b_cont->b_rptr;
	    /* n must be a valid PPP network protocol number. */
	    if (n < 0x21 || n > 0x3fff || (n & 0x101) != 1)
		break;
	    /* check that no other stream is bound to this sap already. */
	    for (os = us->ppa; os != 0; os = os->next)
		if (os->sap == n)
		    break;
	    if (os != 0)
		break;
	    us->sap = n;
	    iop->ioc_count = 0;
	    error = 0;
	    break;
#endif /* NO_DLPI */

	case PPPIO_MRU:
	    if (iop->ioc_count != sizeof(int) || (us->flags & US_CONTROL) == 0)
		break;
	    if (mp->b_cont == 0) {
		DPRINT1("pppuwput/%d: ioctl PPPIO_MRU b_cont = 0!\n", us->mn);
		break;
	    }
	    n = *(int *)mp->b_cont->b_rptr;
	    if (n <= 0 || n > PPP_MAXMRU)
		break;
	    if (n < PPP_MRU)
		n = PPP_MRU;
	    us->mru = n;
	    if (us->lowerq)
		putctl4(us->lowerq, M_CTL, PPPCTL_MRU, n);
	    error = 0;
	    iop->ioc_count = 0;
	    break;

	case PPPIO_MTU:
	    if (iop->ioc_count != sizeof(int) || (us->flags & US_CONTROL) == 0)
		break;
	    if (mp->b_cont == 0) {
		DPRINT1("pppuwput/%d: ioctl PPPIO_MTU b_cont = 0!\n", us->mn);
		break;
	    }
	    n = *(int *)mp->b_cont->b_rptr;
	    if (n <= 0 || n > PPP_MAXMTU)
		break;
	    us->mtu = n;
#ifdef LACHTCP
	    /* The MTU reported in netstat, not used as IP max packet size! */
	    us->ifstats.ifs_mtu = n;
#endif
	    if (us->lowerq)
		putctl4(us->lowerq, M_CTL, PPPCTL_MTU, n);
	    error = 0;
	    iop->ioc_count = 0;
	    break;

	case PPPIO_LASTMOD:
	    us->flags |= US_LASTMOD;
	    error = 0;
	    break;

	case PPPIO_DEBUG:
	    if (iop->ioc_count != sizeof(int))
		break;
	    if (mp->b_cont == 0) {
		DPRINT1("pppuwput/%d: ioctl PPPIO_DEBUG b_cont = 0!\n", us->mn);
		break;
	    }
	    n = *(int *)mp->b_cont->b_rptr;
	    if (n == PPPDBG_DUMP + PPPDBG_DRIVER) {
		qwriter(q, mp, debug_dump, PERIM_OUTER);
		/* mp is now gone */
		error = -1;
	    } else if (n == PPPDBG_LOG + PPPDBG_DRIVER) {
		DPRINT1("ppp/%d: debug log enabled\n", us->mn);
		us->flags |= US_DBGLOG;
		iop->ioc_count = 0;
		error = 0;
	    } else {
		if (us->ppa == 0 || us->ppa->lowerq == 0)
		    break;
		putnext(us->ppa->lowerq, mp);
		/* mp is now gone */
		error = -1;
	    }
	    break;

	case PPPIO_NPMODE:
	    if (iop->ioc_count != 2 * sizeof(int))
		break;
	    if ((us->flags & US_CONTROL) == 0)
		break;
	    if (mp->b_cont == 0) {
		DPRINT1("pppuwput/%d: ioctl PPPIO_NPMODE b_cont = 0!\n", us->mn);
		break;
	    }
	    sap = ((int *)mp->b_cont->b_rptr)[0];
	    for (nps = us->next; nps != 0; nps = nps->next) {
		if (us->flags & US_DBGLOG)
		    DPRINT2("us = 0x%x, us->next->sap = 0x%x\n", nps, nps->sap);
		if (nps->sap == sap)
		    break;
	    }
	    if (nps == 0) {
		if (us->flags & US_DBGLOG)
		    DPRINT2("ppp/%d: no stream for sap %x\n", us->mn, sap);
		break;
	    }
	    /* XXX possibly should use qwriter here */
	    nps->npmode = (enum NPmode) ((int *)mp->b_cont->b_rptr)[1];
	    if (nps->npmode != NPMODE_QUEUE && (nps->flags & US_BLOCKED) != 0)
		qenable(WR(nps->q));
	    iop->ioc_count = 0;
	    error = 0;
	    break;

	case PPPIO_GIDLE:
	    if ((ppa = us->ppa) == 0)
		break;
	    mq = allocb(sizeof(struct ppp_idle), BPRI_HI);
	    if (mq == 0) {
		error = ENOSR;
		break;
	    }
	    if (mp->b_cont != 0)
		freemsg(mp->b_cont);
	    mp->b_cont = mq;
	    mq->b_cont = 0;
	    pip = (struct ppp_idle *) mq->b_wptr;
	    pip->xmit_idle = time - ppa->last_sent;
	    pip->recv_idle = time - ppa->last_recv;
	    mq->b_wptr += sizeof(struct ppp_idle);
	    iop->ioc_count = sizeof(struct ppp_idle);
	    error = 0;
	    break;

#ifdef LACHTCP
	case SIOCSIFNAME:
	    /* Sent from IP down to us.  Attach the ifstats structure.  */
	    if (iop->ioc_count != sizeof(struct ifreq) || us->ppa == 0)
	        break;
	    ifr = (struct ifreq *)mp->b_cont->b_rptr;
	    /* Find the unit number in the interface name.  */
	    for (i = 0; i < IFNAMSIZ; i++) {
		if (ifr->ifr_name[i] == 0 ||
		    (ifr->ifr_name[i] >= '0' &&
		     ifr->ifr_name[i] <= '9'))
		    break;
		else
		    us->ifname[i] = ifr->ifr_name[i];
	    }
	    us->ifname[i] = 0;

	    /* Convert the unit number to binary.  */
	    for (n = 0; i < IFNAMSIZ; i++) {
		if (ifr->ifr_name[i] == 0) {
		    break;
		}
	        else {
		    n = n * 10 + ifr->ifr_name[i] - '0';
		}
	    }

	    /* Verify the ppa.  */
	    if (us->ppa->ppa_id != n)
		break;
	    ppa = us->ppa;

	    /* Set up the netstat block.  */
	    strncpy (ppa->ifname, us->ifname, IFNAMSIZ);

	    ppa->ifstats.ifs_name = ppa->ifname;
	    ppa->ifstats.ifs_unit = n;
	    ppa->ifstats.ifs_active = us->state != DL_UNBOUND;
	    ppa->ifstats.ifs_mtu = ppa->mtu;

	    /* Link in statistics used by netstat.  */
	    ppa->ifstats.ifs_next = ifstats;
	    ifstats = &ppa->ifstats;

	    iop->ioc_count = 0;
	    error = 0;
	    break;

	case SIOCGIFFLAGS:
	    if (!(us->flags & US_CONTROL)) {
		if (us->ppa)
		    us = us->ppa;
	        else
		    break;
	    }
	    ((struct iocblk_in *)iop)->ioc_ifflags = us->ifflags;
	    error = 0;
	    break;

	case SIOCSIFFLAGS:
	    if (!(us->flags & US_CONTROL)) {
		if (us->ppa)
		    us = us->ppa;
		else
		    break;
	    }
	    us->ifflags = ((struct iocblk_in *)iop)->ioc_ifflags;
	    error = 0;
	    break;

	case SIOCSIFADDR:
	    if (!(us->flags & US_CONTROL)) {
		if (us->ppa)
		    us = us->ppa;
		else
		    break;
	    }
	    us->ifflags |= IFF_RUNNING;
	    ((struct iocblk_in *)iop)->ioc_ifflags |= IFF_RUNNING;
	    error = 0;
	    break;

	case SIOCSIFMTU:
	    /*
	     * Vanilla SVR4 systems don't handle SIOCSIFMTU, rather
	     * they take the MTU from the DL_INFO_ACK we sent in response
	     * to their DL_INFO_REQ.  Fortunately, they will update the
	     * MTU if we send an unsolicited DL_INFO_ACK up.
	     */
	    if ((mq = allocb(sizeof(dl_info_req_t), BPRI_HI)) == 0)
		break;		/* should do bufcall */
	    ((union DL_primitives *)mq->b_rptr)->dl_primitive = DL_INFO_REQ;
	    mq->b_wptr = mq->b_rptr + sizeof(dl_info_req_t);
	    dlpi_request(q, mq, us);
	    /* mp is now gone */
	    error = -1;
	    break;

	case SIOCGIFNETMASK:
	case SIOCSIFNETMASK:
	case SIOCGIFADDR:
	case SIOCGIFDSTADDR:
	case SIOCSIFDSTADDR:
	case SIOCGIFMETRIC:
	    error = 0;
	    break;
#endif /* LACHTCP */

	default:
	    if (us->ppa == 0 || us->ppa->lowerq == 0)
		break;
	    us->ioc_id = iop->ioc_id;
	    error = -1;
	    switch (iop->ioc_cmd) {
	    case PPPIO_GETSTAT:
	    case PPPIO_GETCSTAT:
		if (us->flags & US_LASTMOD) {
		    error = EINVAL;
		    break;
		}
		putnext(us->ppa->lowerq, mp);
		break;
	    default:
		if (us->flags & US_PRIV)
		    putnext(us->ppa->lowerq, mp);
		else {
		    DPRINT1("ppp ioctl %x rejected\n", iop->ioc_cmd);
		    error = EPERM;
		}
		break;
	    }
	    break;
	}

	if (error > 0) {
	    iop->ioc_error = error;
	    mp->b_datap->db_type = M_IOCNAK;
	    qreply(q, mp);
	} else if (error == 0) {
	    mp->b_datap->db_type = M_IOCACK;
	    qreply(q, mp);
	}
	break;

    case M_FLUSH:
	if (us->flags & US_DBGLOG)
	    DPRINT2("ppp/%d: flush %x\n", us->mn, *mp->b_rptr);
	if (*mp->b_rptr & FLUSHW)
	    flushq(q, FLUSHDATA);
	if (*mp->b_rptr & FLUSHR) {
	    *mp->b_rptr &= ~FLUSHW;
	    qreply(q, mp);
	} else
	    freemsg(mp);
	break;

    default:
	freemsg(mp);
	break;
    }
    return 0;
}

#ifndef NO_DLPI
static void
dlpi_request(q, mp, us)
    queue_t *q;
    mblk_t *mp;
    upperstr_t *us;
{
    union DL_primitives *d = (union DL_primitives *) mp->b_rptr;
    int size = mp->b_wptr - mp->b_rptr;
    mblk_t *reply, *np;
    upperstr_t *ppa, *os;
    int sap, len;
    dl_info_ack_t *info;
    dl_bind_ack_t *ackp;
#if DL_CURRENT_VERSION >= 2
    dl_phys_addr_ack_t	*paddrack;
    static struct ether_addr eaddr = {0};
#endif

    if (us->flags & US_DBGLOG)
	DPRINT3("ppp/%d: dlpi prim %x len=%d\n", us->mn,
		d->dl_primitive, size);
    switch (d->dl_primitive) {
    case DL_INFO_REQ:
	if (size < sizeof(dl_info_req_t))
	    goto badprim;
	if ((reply = allocb(sizeof(dl_info_ack_t), BPRI_HI)) == 0)
	    break;		/* should do bufcall */
	reply->b_datap->db_type = M_PCPROTO;
	info = (dl_info_ack_t *) reply->b_wptr;
	reply->b_wptr += sizeof(dl_info_ack_t);
	bzero((caddr_t) info, sizeof(dl_info_ack_t));
	info->dl_primitive = DL_INFO_ACK;
	info->dl_max_sdu = us->ppa? us->ppa->mtu: PPP_MAXMTU;
	info->dl_min_sdu = 1;
	info->dl_addr_length = sizeof(uint);
	info->dl_mac_type = DL_ETHER;	/* a bigger lie */
	info->dl_current_state = us->state;
	info->dl_service_mode = DL_CLDLS;
	info->dl_provider_style = DL_STYLE2;
#if DL_CURRENT_VERSION >= 2
	info->dl_sap_length = sizeof(uint);
	info->dl_version = DL_CURRENT_VERSION;
#endif
	qreply(q, reply);
	break;

    case DL_ATTACH_REQ:
	if (size < sizeof(dl_attach_req_t))
	    goto badprim;
	if (us->state != DL_UNATTACHED || us->ppa != 0) {
	    dlpi_error(q, us, DL_ATTACH_REQ, DL_OUTSTATE, 0);
	    break;
	}
	for (ppa = ppas; ppa != 0; ppa = ppa->nextppa)
	    if (ppa->ppa_id == d->attach_req.dl_ppa)
		break;
	if (ppa == 0) {
	    dlpi_error(q, us, DL_ATTACH_REQ, DL_BADPPA, 0);
	    break;
	}
	us->ppa = ppa;
	qwriter(q, mp, attach_ppa, PERIM_OUTER);
	return;

    case DL_DETACH_REQ:
	if (size < sizeof(dl_detach_req_t))
	    goto badprim;
	if (us->state != DL_UNBOUND || us->ppa == 0) {
	    dlpi_error(q, us, DL_DETACH_REQ, DL_OUTSTATE, 0);
	    break;
	}
	qwriter(q, mp, detach_ppa, PERIM_OUTER);
	return;

    case DL_BIND_REQ:
	if (size < sizeof(dl_bind_req_t))
	    goto badprim;
	if (us->state != DL_UNBOUND || us->ppa == 0) {
	    dlpi_error(q, us, DL_BIND_REQ, DL_OUTSTATE, 0);
	    break;
	}
#if 0
	/* apparently this test fails (unnecessarily?) on some systems */
	if (d->bind_req.dl_service_mode != DL_CLDLS) {
	    dlpi_error(q, us, DL_BIND_REQ, DL_UNSUPPORTED, 0);
	    break;
	}
#endif

	/* saps must be valid PPP network protocol numbers,
	   except that we accept ETHERTYPE_IP in place of PPP_IP. */
	sap = d->bind_req.dl_sap;
	us->req_sap = sap;

#if defined(SOL2)
	if (us->flags & US_DBGLOG)
	    DPRINT2("DL_BIND_REQ: ip gives sap = 0x%x, us = 0x%x", sap, us);

	if (sap == ETHERTYPE_IP)	    /* normal IFF_IPV4 */
	    sap = PPP_IP;
	else if (sap == ETHERTYPE_IPV6)	    /* when IFF_IPV6 is set */
	    sap = PPP_IPV6;
	else if (sap == ETHERTYPE_ALLSAP)   /* snoop gives sap of 0 */
	    sap = PPP_ALLSAP;
	else {
	    DPRINT2("DL_BIND_REQ: unrecognized sap = 0x%x, us = 0x%x", sap, us);
	    dlpi_error(q, us, DL_BIND_REQ, DL_BADADDR, 0);
	    break;
	}
#else
	if (sap == ETHERTYPE_IP)
	    sap = PPP_IP;
	if (sap < 0x21 || sap > 0x3fff || (sap & 0x101) != 1) {
	    dlpi_error(q, us, DL_BIND_REQ, DL_BADADDR, 0);
	    break;
	}
#endif /* defined(SOL2) */

	/* check that no other stream is bound to this sap already. */
	for (os = us->ppa; os != 0; os = os->next)
	    if (os->sap == sap)
		break;
	if (os != 0) {
	    dlpi_error(q, us, DL_BIND_REQ, DL_NOADDR, 0);
	    break;
	}

	us->sap = sap;
	us->state = DL_IDLE;

	if ((reply = allocb(sizeof(dl_bind_ack_t) + sizeof(uint),
			    BPRI_HI)) == 0)
	    break;		/* should do bufcall */
	ackp = (dl_bind_ack_t *) reply->b_wptr;
	reply->b_wptr += sizeof(dl_bind_ack_t) + sizeof(uint);
	reply->b_datap->db_type = M_PCPROTO;
	bzero((caddr_t) ackp, sizeof(dl_bind_ack_t));
	ackp->dl_primitive = DL_BIND_ACK;
	ackp->dl_sap = sap;
	ackp->dl_addr_length = sizeof(uint);
	ackp->dl_addr_offset = sizeof(dl_bind_ack_t);
	*(uint *)(ackp+1) = sap;
	qreply(q, reply);
	break;

    case DL_UNBIND_REQ:
	if (size < sizeof(dl_unbind_req_t))
	    goto badprim;
	if (us->state != DL_IDLE) {
	    dlpi_error(q, us, DL_UNBIND_REQ, DL_OUTSTATE, 0);
	    break;
	}
	us->sap = -1;
	us->state = DL_UNBOUND;
#ifdef LACHTCP
	us->ppa->ifstats.ifs_active = 0;
#endif
	dlpi_ok(q, DL_UNBIND_REQ);
	break;

    case DL_UNITDATA_REQ:
	if (size < sizeof(dl_unitdata_req_t))
	    goto badprim;
	if (us->state != DL_IDLE) {
	    dlpi_error(q, us, DL_UNITDATA_REQ, DL_OUTSTATE, 0);
	    break;
	}
	if ((ppa = us->ppa) == 0) {
	    cmn_err(CE_CONT, "ppp: in state dl_idle but ppa == 0?\n");
	    break;
	}
	len = mp->b_cont == 0? 0: msgdsize(mp->b_cont);
	if (len > ppa->mtu) {
	    DPRINT2("dlpi data too large (%d > %d)\n", len, ppa->mtu);
	    break;
	}

#if defined(SOL2)
	/*
	 * Should there be any promiscuous stream(s), send the data
	 * up for each promiscuous stream that we recognize.
	 */
	if (mp->b_cont)
	    promisc_sendup(ppa, mp->b_cont, us->sap, 0);
#endif /* defined(SOL2) */

	mp->b_band = 0;
#ifdef PRIOQ
        /* Extract s_port & d_port from IP-packet, the code is a bit
           dirty here, but so am I, too... */
        if (mp->b_datap->db_type == M_PROTO && us->sap == PPP_IP
	    && mp->b_cont != 0) {
	    u_char *bb, *tlh;
	    int iphlen, len;
	    u_short *ptr;
	    u_char band_unset, cur_band, syn;
	    u_short s_port, d_port;

            bb = mp->b_cont->b_rptr; /* bb points to IP-header*/
	    len = mp->b_cont->b_wptr - mp->b_cont->b_rptr;
            syn = 0;
	    s_port = IPPORT_DEFAULT;
	    d_port = IPPORT_DEFAULT;
	    if (len >= 20) {	/* 20 = minimum length of IP header */
		iphlen = (bb[0] & 0x0f) * 4;
		tlh = bb + iphlen;
		len -= iphlen;
		switch (bb[9]) {
		case IPPROTO_TCP:
		    if (len >= 20) {	      /* min length of TCP header */
			s_port = (tlh[0] << 8) + tlh[1];
			d_port = (tlh[2] << 8) + tlh[3];
			syn = tlh[13] & 0x02;
		    }
		    break;
		case IPPROTO_UDP:
		    if (len >= 8) {	      /* min length of UDP header */
			s_port = (tlh[0] << 8) + tlh[1];
			d_port = (tlh[2] << 8) + tlh[3];
		    }
		    break;
		}
	    }

            /*
	     * Now calculate b_band for this packet from the
	     * port-priority table.
	     */
            ptr = prioq_table;
            cur_band = max_band;
            band_unset = 1;
            while (*ptr) {
                while (*ptr && band_unset)
                    if (s_port == *ptr || d_port == *ptr++) {
                        mp->b_band = cur_band;
                        band_unset = 0;
                        break;
		    }
                ptr++;
                cur_band--;
	    }
            if (band_unset)
		mp->b_band = def_band;
            /* It may be usable to urge SYN packets a bit */
            if (syn)
		mp->b_band++;
	}
#endif	/* PRIOQ */
	/* this assumes PPP_HDRLEN <= sizeof(dl_unitdata_req_t) */
	if (mp->b_datap->db_ref > 1) {
	    np = allocb(PPP_HDRLEN, BPRI_HI);
	    if (np == 0)
		break;		/* gak! */
	    np->b_cont = mp->b_cont;
	    mp->b_cont = 0;
	    freeb(mp);
	    mp = np;
	} else
	    mp->b_datap->db_type = M_DATA;
	/* XXX should use dl_dest_addr_offset/length here,
	   but we would have to translate ETHERTYPE_IP -> PPP_IP */
	mp->b_wptr = mp->b_rptr + PPP_HDRLEN;
	mp->b_rptr[0] = PPP_ALLSTATIONS;
	mp->b_rptr[1] = PPP_UI;
	mp->b_rptr[2] = us->sap >> 8;
	mp->b_rptr[3] = us->sap;
	/* pass_packet frees the packet on returning 0 */
	if (pass_packet(us, mp, 1)) {
	    if (!send_data(mp, us) && !putq(q, mp))
		freemsg(mp);
	}
	return;

#if DL_CURRENT_VERSION >= 2
    case DL_PHYS_ADDR_REQ:
	if (size < sizeof(dl_phys_addr_req_t))
	    goto badprim;

	/*
	 * Don't check state because ifconfig sends this one down too
	 */

	if ((reply = allocb(sizeof(dl_phys_addr_ack_t)+ETHERADDRL, 
			BPRI_HI)) == 0)
	    break;		/* should do bufcall */
	reply->b_datap->db_type = M_PCPROTO;
	paddrack = (dl_phys_addr_ack_t *) reply->b_wptr;
	reply->b_wptr += sizeof(dl_phys_addr_ack_t);
	bzero((caddr_t) paddrack, sizeof(dl_phys_addr_ack_t)+ETHERADDRL);
	paddrack->dl_primitive = DL_PHYS_ADDR_ACK;
	paddrack->dl_addr_length = ETHERADDRL;
	paddrack->dl_addr_offset = sizeof(dl_phys_addr_ack_t);
	bcopy(&eaddr, reply->b_wptr, ETHERADDRL);
	reply->b_wptr += ETHERADDRL;
	qreply(q, reply);
	break;

#if defined(SOL2)
    case DL_PROMISCON_REQ:
	if (size < sizeof(dl_promiscon_req_t))
	    goto badprim;
	us->flags |= US_PROMISC;
	dlpi_ok(q, DL_PROMISCON_REQ);
	break;

    case DL_PROMISCOFF_REQ:
	if (size < sizeof(dl_promiscoff_req_t))
	    goto badprim;
	us->flags &= ~US_PROMISC;
	dlpi_ok(q, DL_PROMISCOFF_REQ);
	break;
#else
    case DL_PROMISCON_REQ:	    /* fall thru */
    case DL_PROMISCOFF_REQ:	    /* fall thru */
#endif /* defined(SOL2) */
#endif /* DL_CURRENT_VERSION >= 2 */

#if DL_CURRENT_VERSION >= 2
    case DL_SET_PHYS_ADDR_REQ:
    case DL_SUBS_BIND_REQ:
    case DL_SUBS_UNBIND_REQ:
    case DL_ENABMULTI_REQ:
    case DL_DISABMULTI_REQ:
    case DL_XID_REQ:
    case DL_TEST_REQ:
    case DL_REPLY_UPDATE_REQ:
    case DL_REPLY_REQ:
    case DL_DATA_ACK_REQ:
#endif
    case DL_CONNECT_REQ:
    case DL_TOKEN_REQ:
	dlpi_error(q, us, d->dl_primitive, DL_NOTSUPPORTED, 0);
	break;

    case DL_CONNECT_RES:
    case DL_DISCONNECT_REQ:
    case DL_RESET_REQ:
    case DL_RESET_RES:
	dlpi_error(q, us, d->dl_primitive, DL_OUTSTATE, 0);
	break;

    case DL_UDQOS_REQ:
	dlpi_error(q, us, d->dl_primitive, DL_BADQOSTYPE, 0);
	break;

#if DL_CURRENT_VERSION >= 2
    case DL_TEST_RES:
    case DL_XID_RES:
	break;
#endif

    default:
	if (us->flags & US_DBGLOG)
	    DPRINT1("ppp: unknown dlpi prim 0x%x\n", d->dl_primitive);
	/* fall through */
    badprim:
	dlpi_error(q, us, d->dl_primitive, DL_BADPRIM, 0);
	break;
    }
    freemsg(mp);
}

static void
dlpi_error(q, us, prim, err, uerr)
    queue_t *q;
    upperstr_t *us;
    int prim, err, uerr;
{
    mblk_t *reply;
    dl_error_ack_t *errp;

    if (us->flags & US_DBGLOG)
        DPRINT3("ppp/%d: dlpi error, prim=%x, err=%x\n", us->mn, prim, err);
    reply = allocb(sizeof(dl_error_ack_t), BPRI_HI);
    if (reply == 0)
	return;			/* XXX should do bufcall */
    reply->b_datap->db_type = M_PCPROTO;
    errp = (dl_error_ack_t *) reply->b_wptr;
    reply->b_wptr += sizeof(dl_error_ack_t);
    errp->dl_primitive = DL_ERROR_ACK;
    errp->dl_error_primitive = prim;
    errp->dl_errno = err;
    errp->dl_unix_errno = uerr;
    qreply(q, reply);
}

static void
dlpi_ok(q, prim)
    queue_t *q;
    int prim;
{
    mblk_t *reply;
    dl_ok_ack_t *okp;

    reply = allocb(sizeof(dl_ok_ack_t), BPRI_HI);
    if (reply == 0)
	return;			/* XXX should do bufcall */
    reply->b_datap->db_type = M_PCPROTO;
    okp = (dl_ok_ack_t *) reply->b_wptr;
    reply->b_wptr += sizeof(dl_ok_ack_t);
    okp->dl_primitive = DL_OK_ACK;
    okp->dl_correct_primitive = prim;
    qreply(q, reply);
}
#endif /* NO_DLPI */

/*
 * If return value is 0, then the packet has already been freed.
 */
static int
pass_packet(us, mp, outbound)
    upperstr_t *us;
    mblk_t *mp;
    int outbound;
{
    int pass;
    upperstr_t *ppa;

    if ((ppa = us->ppa) == 0) {
	freemsg(mp);
	return 0;
    }

#ifdef FILTER_PACKETS
    pass = ip_hard_filter(us, mp, outbound);
#else
    /*
     * Here is where we might, in future, decide whether to pass
     * or drop the packet, and whether it counts as link activity.
     */
    pass = 1;
#endif /* FILTER_PACKETS */

    if (pass < 0) {
	/* pass only if link already up, and don't update time */
	if (ppa->lowerq == 0) {
	    freemsg(mp);
	    return 0;
	}
	pass = 1;
    } else if (pass) {
	if (outbound)
	    ppa->last_sent = time;
	else
	    ppa->last_recv = time;
    }

    return pass;
}

/*
 * We have some data to send down to the lower stream (or up the
 * control stream, if we don't have a lower stream attached).
 * Returns 1 if the message was dealt with, 0 if it wasn't able
 * to be sent on and should therefore be queued up.
 */
static int
send_data(mp, us)
    mblk_t *mp;
    upperstr_t *us;
{
    upperstr_t *ppa;

    if ((us->flags & US_BLOCKED) || us->npmode == NPMODE_QUEUE)
	return 0;
    ppa = us->ppa;
    if (ppa == 0 || us->npmode == NPMODE_DROP || us->npmode == NPMODE_ERROR) {
	if (us->flags & US_DBGLOG)
	    DPRINT2("ppp/%d: dropping pkt (npmode=%d)\n", us->mn, us->npmode);
	freemsg(mp);
	return 1;
    }
    if (ppa->lowerq == 0) {
	/* try to send it up the control stream */
        if (bcanputnext(ppa->q, mp->b_band)) {
	    /*
	     * The message seems to get corrupted for some reason if
	     * we just send the message up as it is, so we send a copy.
	     */
	    mblk_t *np = copymsg(mp);
	    freemsg(mp);
	    if (np != 0)
		putnext(ppa->q, np);
	    return 1;
	}
    } else {
        if (bcanputnext(ppa->lowerq, mp->b_band)) {
	    MT_ENTER(&ppa->stats_lock);
	    ppa->stats.ppp_opackets++;
	    ppa->stats.ppp_obytes += msgdsize(mp);
#ifdef INCR_OPACKETS
	    INCR_OPACKETS(ppa);
#endif
	    MT_EXIT(&ppa->stats_lock);
	    /*
	     * The lower queue is only ever detached while holding an
	     * exclusive lock on the whole driver.  So we can be confident
	     * that the lower queue is still there.
	     */
	    putnext(ppa->lowerq, mp);
	    return 1;
	}
    }
    us->flags |= US_BLOCKED;
    return 0;
}

/*
 * Allocate a new PPA id and link this stream into the list of PPAs.
 * This procedure is called with an exclusive lock on all queues in
 * this driver.
 */
static void
new_ppa(q, mp)
    queue_t *q;
    mblk_t *mp;
{
    upperstr_t *us, *up, **usp;
    int ppa_id;

    us = (upperstr_t *) q->q_ptr;
    if (us == 0) {
	DPRINT("new_ppa: q_ptr = 0!\n");
	return;
    }

    usp = &ppas;
    ppa_id = 0;
    while ((up = *usp) != 0 && ppa_id == up->ppa_id) {
	++ppa_id;
	usp = &up->nextppa;
    }
    us->ppa_id = ppa_id;
    us->ppa = us;
    us->next = 0;
    us->nextppa = *usp;
    *usp = us;
    us->flags |= US_CONTROL;
    us->npmode = NPMODE_PASS;

    us->mtu = PPP_MTU;
    us->mru = PPP_MRU;

#ifdef SOL2
    /*
     * Create a kstats record for our statistics, so netstat -i works.
     */
    if (us->kstats == 0) {
	char unit[32];

	sprintf(unit, "ppp%d", us->ppa->ppa_id);
	us->kstats = kstat_create("ppp", us->ppa->ppa_id, unit,
				  "net", KSTAT_TYPE_NAMED, 4, 0);
	if (us->kstats != 0) {
	    kstat_named_t *kn = KSTAT_NAMED_PTR(us->kstats);

	    strcpy(kn[0].name, "ipackets");
	    kn[0].data_type = KSTAT_DATA_ULONG;
	    strcpy(kn[1].name, "ierrors");
	    kn[1].data_type = KSTAT_DATA_ULONG;
	    strcpy(kn[2].name, "opackets");
	    kn[2].data_type = KSTAT_DATA_ULONG;
	    strcpy(kn[3].name, "oerrors");
	    kn[3].data_type = KSTAT_DATA_ULONG;
	    kstat_install(us->kstats);
	}
    }
#endif /* SOL2 */

    *(int *)mp->b_cont->b_rptr = ppa_id;
    mp->b_datap->db_type = M_IOCACK;
    qreply(q, mp);
}

static void
attach_ppa(q, mp)
    queue_t *q;
    mblk_t *mp;
{
    upperstr_t *us, *t;

    us = (upperstr_t *) q->q_ptr;
    if (us == 0) {
	DPRINT("attach_ppa: q_ptr = 0!\n");
	return;
    }

#ifndef NO_DLPI
    us->state = DL_UNBOUND;
#endif
    for (t = us->ppa; t->next != 0; t = t->next)
	;
    t->next = us;
    us->next = 0;
    if (mp->b_datap->db_type == M_IOCTL) {
	mp->b_datap->db_type = M_IOCACK;
	qreply(q, mp);
    } else {
#ifndef NO_DLPI
	dlpi_ok(q, DL_ATTACH_REQ);
#endif
	freemsg(mp);
    }
}

#ifndef NO_DLPI
static void
detach_ppa(q, mp)
    queue_t *q;
    mblk_t *mp;
{
    upperstr_t *us, *t;

    us = (upperstr_t *) q->q_ptr;
    if (us == 0) {
	DPRINT("detach_ppa: q_ptr = 0!\n");
	return;
    }

    for (t = us->ppa; t->next != 0; t = t->next)
	if (t->next == us) {
	    t->next = us->next;
	    break;
	}
    us->next = 0;
    us->ppa = 0;
    us->state = DL_UNATTACHED;
    dlpi_ok(q, DL_DETACH_REQ);
    freemsg(mp);
}
#endif

/*
 * We call this with qwriter in order to give the upper queue procedures
 * the guarantee that the lower queue is not going to go away while
 * they are executing.
 */
static void
detach_lower(q, mp)
    queue_t *q;
    mblk_t *mp;
{
    upperstr_t *us;

    us = (upperstr_t *) q->q_ptr;
    if (us == 0) {
	DPRINT("detach_lower: q_ptr = 0!\n");
	return;
    }

    LOCK_LOWER_W;
    us->lowerq->q_ptr = 0;
    RD(us->lowerq)->q_ptr = 0;
    us->lowerq = 0;
    UNLOCK_LOWER;

    /* Unblock streams which now feed back up the control stream. */
    qenable(us->q);

    mp->b_datap->db_type = M_IOCACK;
    qreply(q, mp);
}

static int
pppuwsrv(q)
    queue_t *q;
{
    upperstr_t *us, *as;
    mblk_t *mp;

    us = (upperstr_t *) q->q_ptr;
    if (us == 0) {
	DPRINT("pppuwsrv: q_ptr = 0!\n");
	return 0;
    }

    /*
     * If this is a control stream, then this service procedure
     * probably got enabled because of flow control in the lower
     * stream being enabled (or because of the lower stream going
     * away).  Therefore we enable the service procedure of all
     * attached upper streams.
     */
    if (us->flags & US_CONTROL) {
	for (as = us->next; as != 0; as = as->next)
	    qenable(WR(as->q));
    }

    /* Try to send on any data queued here. */
    us->flags &= ~US_BLOCKED;
    while ((mp = getq(q)) != 0) {
	if (!send_data(mp, us)) {
	    putbq(q, mp);
	    break;
	}
    }

    return 0;
}

/* should never get called... */
static int
ppplwput(q, mp)
    queue_t *q;
    mblk_t *mp;
{
    putnext(q, mp);
    return 0;
}

static int
ppplwsrv(q)
    queue_t *q;
{
    queue_t *uq;

    /*
     * Flow control has back-enabled this stream:
     * enable the upper write service procedure for
     * the upper control stream for this lower stream.
     */
    LOCK_LOWER_R;
    uq = (queue_t *) q->q_ptr;
    if (uq != 0)
	qenable(uq);
    UNLOCK_LOWER;
    return 0;
}

/*
 * This should only get called for control streams.
 */
static int
pppurput(q, mp)
    queue_t *q;
    mblk_t *mp;
{
    upperstr_t *ppa, *us;
    int proto, len;
    struct iocblk *iop;

    ppa = (upperstr_t *) q->q_ptr;
    if (ppa == 0) {
	DPRINT("pppurput: q_ptr = 0!\n");
	return 0;
    }

    switch (mp->b_datap->db_type) {
    case M_CTL:
	MT_ENTER(&ppa->stats_lock);
	switch (*mp->b_rptr) {
	case PPPCTL_IERROR:
#ifdef INCR_IERRORS
	    INCR_IERRORS(ppa);
#endif
	    ppa->stats.ppp_ierrors++;
	    break;
	case PPPCTL_OERROR:
#ifdef INCR_OERRORS
	    INCR_OERRORS(ppa);
#endif
	    ppa->stats.ppp_oerrors++;
	    break;
	}
	MT_EXIT(&ppa->stats_lock);
	freemsg(mp);
	break;

    case M_IOCACK:
    case M_IOCNAK:
	/*
	 * Attempt to match up the response with the stream
	 * that the request came from.
	 */
	iop = (struct iocblk *) mp->b_rptr;
	for (us = ppa; us != 0; us = us->next)
	    if (us->ioc_id == iop->ioc_id)
		break;
	if (us == 0)
	    freemsg(mp);
	else
	    putnext(us->q, mp);
	break;

    case M_HANGUP:
	/*
	 * The serial device has hung up.  We don't want to send
	 * the M_HANGUP message up to pppd because that will stop
	 * us from using the control stream any more.  Instead we
	 * send a zero-length message as an end-of-file indication.
	 */
	freemsg(mp);
	mp = allocb(1, BPRI_HI);
	if (mp == 0) {
	    DPRINT1("ppp/%d: couldn't allocate eof message!\n", ppa->mn);
	    break;
	}
	putnext(ppa->q, mp);
	break;

    case M_DATA:
	len = msgdsize(mp);
	if (mp->b_wptr - mp->b_rptr < PPP_HDRLEN) {
	    PULLUP(mp, PPP_HDRLEN);
	    if (mp == 0) {
		DPRINT1("ppp_urput: msgpullup failed (len=%d)\n", len);
		break;
	    }
	}
	MT_ENTER(&ppa->stats_lock);
	ppa->stats.ppp_ipackets++;
	ppa->stats.ppp_ibytes += len;
#ifdef INCR_IPACKETS
	INCR_IPACKETS(ppa);
#endif
	MT_EXIT(&ppa->stats_lock);

	proto = PPP_PROTOCOL(mp->b_rptr);

#if defined(SOL2)
	/*
	 * Should there be any promiscuous stream(s), send the data
	 * up for each promiscuous stream that we recognize.
	 */
	promisc_sendup(ppa, mp, proto, 1);
#endif /* defined(SOL2) */

	if (proto < 0x8000 && (us = find_dest(ppa, proto)) != 0) {
	    /*
	     * A data packet for some network protocol.
	     * Queue it on the upper stream for that protocol.
	     * XXX could we just putnext it?  (would require thought)
	     * The rblocked flag is there to ensure that we keep
	     * messages in order for each network protocol.
	     */
	    /* pass_packet frees the packet on returning 0 */
	    if (!pass_packet(us, mp, 0))
		break;
	    if (!us->rblocked && !canput(us->q))
		us->rblocked = 1;
	    if (!putq(us->rblocked ? q : us->q, mp))
		freemsg(mp);
	    break;
	}

	/* FALLTHROUGH */

   default:
	/*
	 * A control frame, a frame for an unknown protocol,
	 * or some other message type.
	 * Send it up to pppd via the control stream.
	 */
	if (queclass(mp) == QPCTL || canputnext(ppa->q))
	    putnext(ppa->q, mp);
	else if (!putq(q, mp))
	    freemsg(mp);
	break;
    }

    return 0;
}

static int
pppursrv(q)
    queue_t *q;
{
    upperstr_t *us, *as;
    mblk_t *mp, *hdr;
#ifndef NO_DLPI
    dl_unitdata_ind_t *ud;
#endif
    int proto;

    us = (upperstr_t *) q->q_ptr;
    if (us == 0) {
	DPRINT("pppursrv: q_ptr = 0!\n");
	return 0;
    }

    if (us->flags & US_CONTROL) {
	/*
	 * A control stream.
	 * If there is no lower queue attached, run the write service
	 * routines of other upper streams attached to this PPA.
	 */
	if (us->lowerq == 0) {
	    as = us;
	    do {
		if (as->flags & US_BLOCKED)
		    qenable(WR(as->q));
		as = as->next;
	    } while (as != 0);
	}

	/*
	 * Messages get queued on this stream's read queue if they
	 * can't be queued on the read queue of the attached stream
	 * that they are destined for.  This is for flow control -
	 * when this queue fills up, the lower read put procedure will
	 * queue messages there and the flow control will propagate
	 * down from there.
	 */
	while ((mp = getq(q)) != 0) {
	    proto = PPP_PROTOCOL(mp->b_rptr);
	    if (proto < 0x8000 && (as = find_dest(us, proto)) != 0) {
		if (!canput(as->q))
		    break;
		if (!putq(as->q, mp))
		    freemsg(mp);
	    } else {
		if (!canputnext(q))
		    break;
		putnext(q, mp);
	    }
	}
	if (mp) {
	    putbq(q, mp);
	} else {
	    /* can now put stuff directly on network protocol streams again */
	    for (as = us->next; as != 0; as = as->next)
		as->rblocked = 0;
	}

	/*
	 * If this stream has a lower stream attached,
	 * enable the read queue's service routine.
	 * XXX we should really only do this if the queue length
	 * has dropped below the low-water mark.
	 */
	if (us->lowerq != 0)
	    qenable(RD(us->lowerq));
		
    } else {
	/*
	 * A network protocol stream.  Put a DLPI header on each
	 * packet and send it on.
	 * (Actually, it seems that the IP module will happily
	 * accept M_DATA messages without the DL_UNITDATA_IND header.)
	 */
	while ((mp = getq(q)) != 0) {
	    if (!canputnext(q)) {
		putbq(q, mp);
		break;
	    }
#ifndef NO_DLPI
	    proto = PPP_PROTOCOL(mp->b_rptr);
	    mp->b_rptr += PPP_HDRLEN;
	    hdr = allocb(sizeof(dl_unitdata_ind_t) + 2 * sizeof(uint),
			 BPRI_MED);
	    if (hdr == 0) {
		/* XXX should put it back and use bufcall */
		freemsg(mp);
		continue;
	    }
	    hdr->b_datap->db_type = M_PROTO;
	    ud = (dl_unitdata_ind_t *) hdr->b_wptr;
	    hdr->b_wptr += sizeof(dl_unitdata_ind_t) + 2 * sizeof(uint);
	    hdr->b_cont = mp;
	    ud->dl_primitive = DL_UNITDATA_IND;
	    ud->dl_dest_addr_length = sizeof(uint);
	    ud->dl_dest_addr_offset = sizeof(dl_unitdata_ind_t);
	    ud->dl_src_addr_length = sizeof(uint);
	    ud->dl_src_addr_offset = ud->dl_dest_addr_offset + sizeof(uint);
#if DL_CURRENT_VERSION >= 2
	    ud->dl_group_address = 0;
#endif
	    /* Send the DLPI client the data with the SAP they requested,
	       (e.g. ETHERTYPE_IP) rather than the PPP protocol number
	       (e.g. PPP_IP) */
	    ((uint *)(ud + 1))[0] = us->req_sap;	/* dest SAP */
	    ((uint *)(ud + 1))[1] = us->req_sap;	/* src SAP */
	    putnext(q, hdr);
#else /* NO_DLPI */
	    putnext(q, mp);
#endif /* NO_DLPI */
	}
	/*
	 * Now that we have consumed some packets from this queue,
	 * enable the control stream's read service routine so that we
	 * can process any packets for us that might have got queued
	 * there for flow control reasons.
	 */
	if (us->ppa)
	    qenable(us->ppa->q);
    }

    return 0;
}

static upperstr_t *
find_dest(ppa, proto)
    upperstr_t *ppa;
    int proto;
{
    upperstr_t *us;

    for (us = ppa->next; us != 0; us = us->next)
	if (proto == us->sap)
	    break;
    return us;
}

#if defined (SOL2)
/*
 * Test upstream promiscuous conditions. As of now, only pass IPv4 and
 * Ipv6 packets upstream (let PPP packets be decoded elsewhere).
 */
static upperstr_t *
find_promisc(us, proto)
    upperstr_t *us;
    int proto;
{

    if ((proto != PPP_IP) && (proto != PPP_IPV6))
	return (upperstr_t *)0;

    for ( ; us; us = us->next) {
	if ((us->flags & US_PROMISC) && (us->state == DL_IDLE))
	    return us;
    }

    return (upperstr_t *)0;
}

/*
 * Prepend an empty Ethernet header to msg for snoop, et al.
 */
static mblk_t *
prepend_ether(us, mp, proto)
    upperstr_t *us;
    mblk_t *mp;
    int proto;
{
    mblk_t *eh;
    int type;

    if ((eh = allocb(sizeof(struct ether_header), BPRI_HI)) == 0) {
	freemsg(mp);
	return (mblk_t *)0;
    }

    if (proto == PPP_IP)
	type = ETHERTYPE_IP;
    else if (proto == PPP_IPV6)
	type = ETHERTYPE_IPV6;
    else 
	type = proto;	    /* What else? Let decoder decide */

    eh->b_wptr += sizeof(struct ether_header);
    bzero((caddr_t)eh->b_rptr, sizeof(struct ether_header));
    ((struct ether_header *)eh->b_rptr)->ether_type = htons((short)type);
    eh->b_cont = mp;
    return (eh);
}

/*
 * Prepend DL_UNITDATA_IND mblk to msg
 */
static mblk_t *
prepend_udind(us, mp, proto)
    upperstr_t *us;
    mblk_t *mp;
    int proto;
{
    dl_unitdata_ind_t *dlu;
    mblk_t *dh;
    size_t size;

    size = sizeof(dl_unitdata_ind_t);
    if ((dh = allocb(size, BPRI_MED)) == 0) {
	freemsg(mp);
	return (mblk_t *)0;
    }

    dh->b_datap->db_type = M_PROTO;
    dh->b_wptr = dh->b_datap->db_lim;
    dh->b_rptr = dh->b_wptr - size;

    dlu = (dl_unitdata_ind_t *)dh->b_rptr;
    dlu->dl_primitive = DL_UNITDATA_IND;
    dlu->dl_dest_addr_length = 0;
    dlu->dl_dest_addr_offset = sizeof(dl_unitdata_ind_t);
    dlu->dl_src_addr_length = 0;
    dlu->dl_src_addr_offset = sizeof(dl_unitdata_ind_t);
    dlu->dl_group_address = 0;

    dh->b_cont = mp;
    return (dh);
}

/*
 * For any recognized promiscuous streams, send data upstream
 */
static void
promisc_sendup(ppa, mp, proto, skip)
    upperstr_t *ppa;
    mblk_t *mp;
    int proto, skip;
{
    mblk_t *dup_mp, *dup_dup_mp;
    upperstr_t *prus, *nprus;

    if ((prus = find_promisc(ppa, proto)) != 0) {
	if (dup_mp = dupmsg(mp)) {

	    if (skip)
		dup_mp->b_rptr += PPP_HDRLEN;

	    for ( ; nprus = find_promisc(prus->next, proto); 
		    prus = nprus) {

		if (dup_dup_mp = dupmsg(dup_mp)) {
		    if (canputnext(prus->q)) {
			if (prus->flags & US_RAWDATA) {
			    dup_dup_mp = prepend_ether(prus, dup_dup_mp, proto);
			} else {
			    dup_dup_mp = prepend_udind(prus, dup_dup_mp, proto);
			}
			if (dup_dup_mp == 0)
			    continue;
			putnext(prus->q, dup_dup_mp);
		    } else {
			DPRINT("ppp_urput: data to promisc q dropped\n");
			freemsg(dup_dup_mp);
		    }
		}
	    }

	    if (canputnext(prus->q)) {
		if (prus->flags & US_RAWDATA) {
		    dup_mp = prepend_ether(prus, dup_mp, proto);
		} else {
		    dup_mp = prepend_udind(prus, dup_mp, proto);
		}
		if (dup_mp != 0)
		    putnext(prus->q, dup_mp);
	    } else {
		DPRINT("ppp_urput: data to promisc q dropped\n");
		freemsg(dup_mp);
	    }
	}
    }
}
#endif /* defined(SOL2) */

/*
 * We simply put the message on to the associated upper control stream
 * (either here or in ppplrsrv).  That way we enter the perimeters
 * before looking through the list of attached streams to decide which
 * stream it should go up.
 */
static int
ppplrput(q, mp)
    queue_t *q;
    mblk_t *mp;
{
    queue_t *uq;
    struct iocblk *iop;

    switch (mp->b_datap->db_type) {
    case M_IOCTL:
	iop = (struct iocblk *) mp->b_rptr;
	iop->ioc_error = EINVAL;
	mp->b_datap->db_type = M_IOCNAK;
	qreply(q, mp);
	return 0;
    case M_FLUSH:
	if (*mp->b_rptr & FLUSHR)
	    flushq(q, FLUSHDATA);
	if (*mp->b_rptr & FLUSHW) {
	    *mp->b_rptr &= ~FLUSHR;
	    qreply(q, mp);
	} else
	    freemsg(mp);
	return 0;
    }

    /*
     * If we can't get the lower lock straight away, queue this one
     * rather than blocking, to avoid the possibility of deadlock.
     */
    if (!TRYLOCK_LOWER_R) {
	if (!putq(q, mp))
	    freemsg(mp);
	return 0;
    }

    /*
     * Check that we're still connected to the driver.
     */
    uq = (queue_t *) q->q_ptr;
    if (uq == 0) {
	UNLOCK_LOWER;
	DPRINT1("ppplrput: q = %x, uq = 0??\n", q);
	freemsg(mp);
	return 0;
    }

    /*
     * Try to forward the message to the put routine for the upper
     * control stream for this lower stream.
     * If there are already messages queued here, queue this one so
     * they don't get out of order.
     */
    if (queclass(mp) == QPCTL || (qsize(q) == 0 && canput(uq)))
	put(uq, mp);
    else if (!putq(q, mp))
	freemsg(mp);

    UNLOCK_LOWER;
    return 0;
}

static int
ppplrsrv(q)
    queue_t *q;
{
    mblk_t *mp;
    queue_t *uq;

    /*
     * Packets get queued here for flow control reasons
     * or if the lrput routine couldn't get the lower lock
     * without blocking.
     */
    LOCK_LOWER_R;
    uq = (queue_t *) q->q_ptr;
    if (uq == 0) {
	UNLOCK_LOWER;
	flushq(q, FLUSHALL);
	DPRINT1("ppplrsrv: q = %x, uq = 0??\n", q);
	return 0;
    }
    while ((mp = getq(q)) != 0) {
	if (queclass(mp) == QPCTL || canput(uq))
	    put(uq, mp);
	else {
	    putbq(q, mp);
	    break;
	}
    }
    UNLOCK_LOWER;
    return 0;
}

static int
putctl2(q, type, code, val)
    queue_t *q;
    int type, code, val;
{
    mblk_t *mp;

    mp = allocb(2, BPRI_HI);
    if (mp == 0)
	return 0;
    mp->b_datap->db_type = type;
    mp->b_wptr[0] = code;
    mp->b_wptr[1] = val;
    mp->b_wptr += 2;
    putnext(q, mp);
    return 1;
}

static int
putctl4(q, type, code, val)
    queue_t *q;
    int type, code, val;
{
    mblk_t *mp;

    mp = allocb(4, BPRI_HI);
    if (mp == 0)
	return 0;
    mp->b_datap->db_type = type;
    mp->b_wptr[0] = code;
    ((short *)mp->b_wptr)[1] = val;
    mp->b_wptr += 4;
    putnext(q, mp);
    return 1;
}

static void
debug_dump(q, mp)
    queue_t *q;
    mblk_t *mp;
{
    upperstr_t *us;
    queue_t *uq, *lq;

    DPRINT("ppp upper streams:\n");
    for (us = minor_devs; us != 0; us = us->nextmn) {
	uq = us->q;
	DPRINT3(" %d: q=%x rlev=%d",
		us->mn, uq, (uq? qsize(uq): 0));
	DPRINT3(" wlev=%d flags=0x%b", (uq? qsize(WR(uq)): 0),
		us->flags, "\020\1priv\2control\3blocked\4last");
	DPRINT3(" state=%x sap=%x req_sap=%x", us->state, us->sap,
		us->req_sap);
	if (us->ppa == 0)
	    DPRINT(" ppa=?\n");
	else
	    DPRINT1(" ppa=%d\n", us->ppa->ppa_id);
	if (us->flags & US_CONTROL) {
	    lq = us->lowerq;
	    DPRINT3("    control for %d lq=%x rlev=%d",
		    us->ppa_id, lq, (lq? qsize(RD(lq)): 0));
	    DPRINT3(" wlev=%d mru=%d mtu=%d\n",
		    (lq? qsize(lq): 0), us->mru, us->mtu);
	}
    }
    mp->b_datap->db_type = M_IOCACK;
    qreply(q, mp);
}

#ifdef FILTER_PACKETS
#include <netinet/in_systm.h>
#include <netinet/ip.h>
#include <netinet/udp.h>
#include <netinet/tcp.h>

#define MAX_IPHDR    128     /* max TCP/IP header size */


/* The following table contains a hard-coded list of protocol/port pairs.
 * Any matching packets are either discarded unconditionally, or, 
 * if ok_if_link_up is non-zero when a connection does not currently exist
 * (i.e., they go through if the connection is present, but never initiate
 * a dial-out).
 * This idea came from a post by dm@garage.uun.org (David Mazieres)
 */
static struct pktfilt_tab { 
	int proto; 
	u_short port; 
	u_short ok_if_link_up; 
} pktfilt_tab[] = {
	{ IPPROTO_UDP,	520,	1 },	/* RIP, ok to pass if link is up */
	{ IPPROTO_UDP,	123,	1 },	/* NTP, don't keep up the link for it */
	{ -1, 		0,	0 }	/* terminator entry has port == -1 */
};


/*
 * Packet has already been freed if return value is 0.
 */
static int
ip_hard_filter(us, mp, outbound)
    upperstr_t *us;
    mblk_t *mp;
    int outbound;
{
    struct ip *ip;
    struct pktfilt_tab *pft;
    mblk_t *temp_mp;
    int proto;
    int len, hlen;


    /* Note, the PPP header has already been pulled up in all cases */
    proto = PPP_PROTOCOL(mp->b_rptr);
    if (us->flags & US_DBGLOG)
        DPRINT3("ppp/%d: filter, proto=0x%x, out=%d\n", us->mn, proto, outbound);

    switch (proto)
    {
    case PPP_IP:
	if ((mp->b_wptr - mp->b_rptr) == PPP_HDRLEN && mp->b_cont != 0) {
	    temp_mp = mp->b_cont;
    	    len = msgdsize(temp_mp);
	    hlen = (len < MAX_IPHDR) ? len : MAX_IPHDR;
	    PULLUP(temp_mp, hlen);
	    if (temp_mp == 0) {
		DPRINT2("ppp/%d: filter, pullup next failed, len=%d\n", 
			us->mn, hlen);
		mp->b_cont = 0;		/* PULLUP() freed the rest */
	        freemsg(mp);
	        return 0;
	    }
	    ip = (struct ip *)mp->b_cont->b_rptr;
	}
	else {
	    len = msgdsize(mp);
	    hlen = (len < (PPP_HDRLEN+MAX_IPHDR)) ? len : (PPP_HDRLEN+MAX_IPHDR);
	    PULLUP(mp, hlen);
	    if (mp == 0) {
		DPRINT2("ppp/%d: filter, pullup failed, len=%d\n", 
			us->mn, hlen);
	        return 0;
	    }
	    ip = (struct ip *)(mp->b_rptr + PPP_HDRLEN);
	}

	/* For IP traffic, certain packets (e.g., RIP) may be either
	 *   1.  ignored - dropped completely
	 *   2.  will not initiate a connection, but
	 *       will be passed if a connection is currently up.
	 */
	for (pft=pktfilt_tab; pft->proto != -1; pft++) {
	    if (ip->ip_p == pft->proto) {
		switch(pft->proto) {
		case IPPROTO_UDP:
		    if (((struct udphdr *) &((int *)ip)[ip->ip_hl])->uh_dport
				== htons(pft->port)) goto endfor;
		    break;
		case IPPROTO_TCP:
		    if (((struct tcphdr *) &((int *)ip)[ip->ip_hl])->th_dport
				== htons(pft->port)) goto endfor;
		    break;
		}	
	    }
	}
	endfor:
	if (pft->proto != -1) {
	    if (us->flags & US_DBGLOG)
		DPRINT3("ppp/%d: found IP pkt, proto=0x%x (%d)\n", 
				us->mn, pft->proto, pft->port);
	    /* Discard if not connected, or if not pass_with_link_up */
	    /* else, if link is up let go by, but don't update time */
	    if (pft->ok_if_link_up)
		return -1;
	    freemsg(mp);
	    return 0;
	}
        break;
    } /* end switch (proto) */

    return 1;
}
#endif /* FILTER_PACKETS */

