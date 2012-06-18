/*
 * if_ppp.c - a network interface connected to a STREAMS module.
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
 * $Id: if_ppp.c,v 1.18 2002/12/06 09:49:15 paulus Exp $
 */

/*
 * This file is used under SunOS 4 and Digital UNIX.
 *
 * This file provides the glue between PPP and IP.
 */

#define INET	1

#include <sys/types.h>
#include <sys/param.h>
#include <sys/errno.h>
#include <sys/mbuf.h>
#include <sys/socket.h>
#include <net/if.h>
#include <net/netisr.h>
#include <net/ppp_defs.h>
#include <net/pppio.h>
#include <netinet/in.h>
#include <netinet/in_var.h>
#ifdef __osf__
#include <sys/ioctl.h>
#include <net/if_types.h>
#else
#include <sys/sockio.h>
#endif
#include "ppp_mod.h"

#include <sys/stream.h>

#ifdef SNIT_SUPPORT
#include <sys/time.h>
#include <net/nit_if.h>
#include <netinet/if_ether.h>
#endif

#ifdef __osf__
#define SIOCSIFMTU SIOCSIPMTU
#define SIOCGIFMTU SIOCRIPMTU
#define IFA_ADDR(ifa)   (*(ifa)->ifa_addr)
#else
#define IFA_ADDR(ifa)   ((ifa)->ifa_addr)
#endif

#define ifr_mtu		ifr_metric

static int if_ppp_open __P((queue_t *, int, int, int));
static int if_ppp_close __P((queue_t *, int));
static int if_ppp_wput __P((queue_t *, mblk_t *));
static int if_ppp_rput __P((queue_t *, mblk_t *));

#define PPP_IF_ID 0x8021
static struct module_info minfo = {
    PPP_IF_ID, "if_ppp", 0, INFPSZ, 4096, 128
};

static struct qinit rinit = {
    if_ppp_rput, NULL, if_ppp_open, if_ppp_close, NULL, &minfo, NULL
};

static struct qinit winit = {
    if_ppp_wput, NULL, NULL, NULL, NULL, &minfo, NULL
};

struct streamtab if_pppinfo = {
    &rinit, &winit, NULL, NULL
};

typedef struct if_ppp_state {
    int unit;
    queue_t *q;
    int flags;
} if_ppp_t;

/* Values for flags */
#define DBGLOG		1

static int if_ppp_count;	/* Number of currently-active streams */

static int ppp_nalloc;		/* Number of elements of ifs and states */
static struct ifnet **ifs;	/* Array of pointers to interface structs */
static if_ppp_t **states;	/* Array of pointers to state structs */

static int if_ppp_output __P((struct ifnet *, struct mbuf *,
			      struct sockaddr *));
static int if_ppp_ioctl __P((struct ifnet *, u_int, caddr_t));
static struct mbuf *make_mbufs __P((mblk_t *, int));
static mblk_t *make_message __P((struct mbuf *, int));

#ifdef SNIT_SUPPORT
/* Fake ether header for SNIT */
static struct ether_header snit_ehdr = {{0}, {0}, ETHERTYPE_IP};
#endif

#ifndef __osf__
static void ppp_if_detach __P((struct ifnet *));

/*
 * Detach all the interfaces before unloading.
 * Not sure this works.
 */
int
if_ppp_unload()
{
    int i;

    if (if_ppp_count > 0)
	return EBUSY;
    for (i = 0; i < ppp_nalloc; ++i)
	if (ifs[i] != 0)
	    ppp_if_detach(ifs[i]);
    if (ifs) {
	FREE(ifs, ppp_nalloc * sizeof (struct ifnet *));
	FREE(states, ppp_nalloc * sizeof (struct if_ppp_t *));
    }
    ppp_nalloc = 0;
    return 0;
}
#endif /* __osf__ */

/*
 * STREAMS module entry points.
 */
static int
if_ppp_open(q, dev, flag, sflag)
    queue_t *q;
    int dev;
    int flag, sflag;
{
    if_ppp_t *sp;

    if (q->q_ptr == 0) {
	sp = (if_ppp_t *) ALLOC_SLEEP(sizeof (if_ppp_t));
	if (sp == 0)
	    return OPENFAIL;
	bzero(sp, sizeof (if_ppp_t));
	q->q_ptr = (caddr_t) sp;
	WR(q)->q_ptr = (caddr_t) sp;
	sp->unit = -1;		/* no interface unit attached at present */
	sp->q = WR(q);
	sp->flags = 0;
	++if_ppp_count;
    }
    return 0;
}

static int
if_ppp_close(q, flag)
    queue_t *q;
    int flag;
{
    if_ppp_t *sp;
    struct ifnet *ifp;

    sp = (if_ppp_t *) q->q_ptr;
    if (sp != 0) {
	if (sp->flags & DBGLOG)
	    printf("if_ppp closed, q=%x sp=%x\n", q, sp);
	if (sp->unit >= 0) {
	    if (sp->unit < ppp_nalloc) {
		states[sp->unit] = 0;
		ifp = ifs[sp->unit];
		if (ifp != 0)
		    ifp->if_flags &= ~(IFF_UP | IFF_RUNNING);
#ifdef DEBUG
	    } else {
		printf("if_ppp: unit %d nonexistent!\n", sp->unit);
#endif
	    }
	}
	FREE(sp, sizeof (if_ppp_t));
	--if_ppp_count;
    }
    return 0;
}

static int
if_ppp_wput(q, mp)
    queue_t *q;
    mblk_t *mp;
{
    if_ppp_t *sp;
    struct iocblk *iop;
    int error, unit;
    struct ifnet *ifp;

    sp = (if_ppp_t *) q->q_ptr;
    switch (mp->b_datap->db_type) {
    case M_DATA:
	/*
	 * Now why would we be getting data coming in here??
	 */
	if (sp->flags & DBGLOG)
	    printf("if_ppp: got M_DATA len=%d\n", msgdsize(mp));
	freemsg(mp);
	break;

    case M_IOCTL:
	iop = (struct iocblk *) mp->b_rptr;
	error = EINVAL;

	if (sp->flags & DBGLOG)
	    printf("if_ppp: got ioctl cmd=%x count=%d\n",
		   iop->ioc_cmd, iop->ioc_count);

	switch (iop->ioc_cmd) {
	case PPPIO_NEWPPA:		/* well almost */
	    if (iop->ioc_count != sizeof(int) || sp->unit >= 0)
		break;
	    if ((error = NOTSUSER()) != 0)
		break;
	    unit = *(int *)mp->b_cont->b_rptr;

	    /* Check that this unit isn't already in use */
	    if (unit < ppp_nalloc && states[unit] != 0) {
		error = EADDRINUSE;
		break;
	    }

	    /* Extend ifs and states arrays if necessary. */
	    error = ENOSR;
	    if (unit >= ppp_nalloc) {
		int newn;
		struct ifnet **newifs;
		if_ppp_t **newstates;

		newn = unit + 4;
		if (sp->flags & DBGLOG)
		    printf("if_ppp: extending ifs to %d\n", newn);
		newifs = (struct ifnet **)
		    ALLOC_NOSLEEP(newn * sizeof (struct ifnet *));
		if (newifs == 0)
		    break;
		bzero(newifs, newn * sizeof (struct ifnet *));
		newstates = (if_ppp_t **)
		    ALLOC_NOSLEEP(newn * sizeof (struct if_ppp_t *));
		if (newstates == 0) {
		    FREE(newifs, newn * sizeof (struct ifnet *));
		    break;
		}
		bzero(newstates, newn * sizeof (struct if_ppp_t *));
		bcopy(ifs, newifs, ppp_nalloc * sizeof(struct ifnet *));
		bcopy(states, newstates, ppp_nalloc * sizeof(if_ppp_t *));
		if (ifs) {
		    FREE(ifs, ppp_nalloc * sizeof(struct ifnet *));
		    FREE(states, ppp_nalloc * sizeof(if_ppp_t *));
		}
		ifs = newifs;
		states = newstates;
		ppp_nalloc = newn;
	    }

	    /* Allocate a new ifnet struct if necessary. */
	    ifp = ifs[unit];
	    if (ifp == 0) {
		ifp = (struct ifnet *) ALLOC_NOSLEEP(sizeof (struct ifnet));
		if (ifp == 0)
		    break;
		bzero(ifp, sizeof (struct ifnet));
		ifs[unit] = ifp;
		ifp->if_name = "ppp";
		ifp->if_unit = unit;
		ifp->if_mtu = PPP_MTU;
		ifp->if_flags = IFF_POINTOPOINT | IFF_RUNNING;
#ifndef __osf__
#ifdef IFF_MULTICAST
		ifp->if_flags |= IFF_MULTICAST;
#endif
#endif /* __osf__ */
		ifp->if_output = if_ppp_output;
#ifdef __osf__
		ifp->if_version = "Point-to-Point Protocol, version 2.3.11";
		ifp->if_mediamtu = PPP_MTU;
		ifp->if_type = IFT_PPP;
		ifp->if_hdrlen = PPP_HDRLEN;
		ifp->if_addrlen = 0;
		ifp->if_flags |= IFF_NOARP | IFF_SIMPLEX | IFF_NOTRAILERS;
#ifdef IFF_VAR_MTU
		ifp->if_flags |= IFF_VAR_MTU;
#endif
#ifdef NETMASTERCPU
		ifp->if_affinity = NETMASTERCPU;
#endif
#endif
		ifp->if_ioctl = if_ppp_ioctl;
		ifp->if_snd.ifq_maxlen = IFQ_MAXLEN;
		if_attach(ifp);
		if (sp->flags & DBGLOG)
		    printf("if_ppp: created unit %d\n", unit);
	    } else {
		ifp->if_mtu = PPP_MTU;
		ifp->if_flags |= IFF_RUNNING;
	    }

	    states[unit] = sp;
	    sp->unit = unit;

	    error = 0;
	    iop->ioc_count = 0;
	    if (sp->flags & DBGLOG)
		printf("if_ppp: attached unit %d, sp=%x q=%x\n", unit,
		       sp, sp->q);
	    break;

	case PPPIO_DEBUG:
	    error = -1;
	    if (iop->ioc_count == sizeof(int)) {
		if (*(int *)mp->b_cont->b_rptr == PPPDBG_LOG + PPPDBG_IF) {
		    printf("if_ppp: debug log enabled, q=%x sp=%x\n", q, sp);
		    sp->flags |= DBGLOG;
		    error = 0;
		    iop->ioc_count = 0;
		}
	    }
	    break;

	default:
	    error = -1;
	    break;
	}

	if (sp->flags & DBGLOG)
	    printf("if_ppp: ioctl result %d\n", error);
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

    default:
	putnext(q, mp);
    }
    return 0;
}

static int
if_ppp_rput(q, mp)
    queue_t *q;
    mblk_t *mp;
{
    if_ppp_t *sp;
    int proto, s;
    struct mbuf *mb;
    struct ifqueue *inq;
    struct ifnet *ifp;
    int len;

    sp = (if_ppp_t *) q->q_ptr;
    switch (mp->b_datap->db_type) {
    case M_DATA:
	/*
	 * Convert the message into an mbuf chain
	 * and inject it into the network code.
	 */
	if (sp->flags & DBGLOG)
	    printf("if_ppp: rput pkt len %d data %x %x %x %x %x %x %x %x\n",
		   msgdsize(mp), mp->b_rptr[0], mp->b_rptr[1], mp->b_rptr[2],
		   mp->b_rptr[3], mp->b_rptr[4], mp->b_rptr[5], mp->b_rptr[6],
		   mp->b_rptr[7]);

	if (sp->unit < 0) {
	    freemsg(mp);
	    break;
	}
	if (sp->unit >= ppp_nalloc || (ifp = ifs[sp->unit]) == 0) {
#ifdef DEBUG
	    printf("if_ppp: no unit %d!\n", sp->unit);
#endif
	    freemsg(mp);
	    break;
	}

	if ((ifp->if_flags & IFF_UP) == 0) {
	    freemsg(mp);
	    break;
	}
	++ifp->if_ipackets;

	proto = PPP_PROTOCOL(mp->b_rptr);
	adjmsg(mp, PPP_HDRLEN);
	len = msgdsize(mp);
	mb = make_mbufs(mp, sizeof(struct ifnet *));
	freemsg(mp);
	if (mb == NULL) {
	    if (sp->flags & DBGLOG)
		printf("if_ppp%d: make_mbufs failed\n", ifp->if_unit);
	    ++ifp->if_ierrors;
	    break;
	}

#ifdef SNIT_SUPPORT
	if (proto == PPP_IP && (ifp->if_flags & IFF_PROMISC)) {
	    struct nit_if nif;

	    nif.nif_header = (caddr_t) &snit_ehdr;
	    nif.nif_hdrlen = sizeof(snit_ehdr);
	    nif.nif_bodylen = len;
	    nif.nif_promisc = 0;
	    snit_intr(ifp, mb, &nif);
	}
#endif

/*
 * For Digital UNIX, there's space set aside in the header mbuf
 * for the interface info.
 *
 * For Sun it's smuggled around via a pointer at the front of the mbuf.
 */
#ifdef __osf__
        mb->m_pkthdr.rcvif = ifp;
        mb->m_pkthdr.len = len;
#else
	mb->m_off -= sizeof(struct ifnet *);
	mb->m_len += sizeof(struct ifnet *);
	*mtod(mb, struct ifnet **) = ifp;
#endif

	inq = 0;
	switch (proto) {
	case PPP_IP:
	    inq = &ipintrq;
	    schednetisr(NETISR_IP);
	}

	if (inq != 0) {
	    s = splhigh();
	    if (IF_QFULL(inq)) {
		IF_DROP(inq);
		++ifp->if_ierrors;
		if (sp->flags & DBGLOG)
		    printf("if_ppp: inq full, proto=%x\n", proto);
		m_freem(mb);
	    } else {
		IF_ENQUEUE(inq, mb);
	    }
	    splx(s);
	} else {
	    if (sp->flags & DBGLOG)
		printf("if_ppp%d: proto=%x?\n", ifp->if_unit, proto);
	    ++ifp->if_ierrors;
	    m_freem(mb);
	}
	break;

    default:
	putnext(q, mp);
    }
    return 0;
}

/*
 * Network code wants to output a packet.
 * Turn it into a STREAMS message and send it down.
 */
static int
if_ppp_output(ifp, m0, dst)
    struct ifnet *ifp;
    struct mbuf *m0;
    struct sockaddr *dst;
{
    mblk_t *mp;
    int proto, s;
    if_ppp_t *sp;
    u_char *p;

    if ((ifp->if_flags & IFF_UP) == 0) {
	m_freem(m0);
	return ENETDOWN;
    }

    if ((unsigned)ifp->if_unit >= ppp_nalloc) {
#ifdef DEBUG
	printf("if_ppp_output: unit %d?\n", ifp->if_unit);
#endif
	m_freem(m0);
	return EINVAL;
    }
    sp = states[ifp->if_unit];
    if (sp == 0) {
#ifdef DEBUG
	printf("if_ppp_output: no queue?\n");
#endif
	m_freem(m0);
	return ENETDOWN;
    }

    if (sp->flags & DBGLOG) {
	p = mtod(m0, u_char *);
	printf("if_ppp_output%d: af=%d data=%x %x %x %x %x %x %x %x q=%x\n",
	       ifp->if_unit, dst->sa_family, p[0], p[1], p[2], p[3], p[4],
	       p[5], p[6], p[7], sp->q);
    }

    switch (dst->sa_family) {
    case AF_INET:
	proto = PPP_IP;
#ifdef SNIT_SUPPORT
	if (ifp->if_flags & IFF_PROMISC) {
	    struct nit_if nif;
	    struct mbuf *m;
	    int len;

	    for (len = 0, m = m0; m != NULL; m = m->m_next)
		len += m->m_len;
	    nif.nif_header = (caddr_t) &snit_ehdr;
	    nif.nif_hdrlen = sizeof(snit_ehdr);
	    nif.nif_bodylen = len;
	    nif.nif_promisc = 0;
	    snit_intr(ifp, m0, &nif);
	}
#endif
	break;

    default:
	m_freem(m0);
	return EAFNOSUPPORT;
    }

    ++ifp->if_opackets;
    mp = make_message(m0, PPP_HDRLEN);
    m_freem(m0);
    if (mp == 0) {
	++ifp->if_oerrors;
	return ENOBUFS;
    }
    mp->b_rptr -= PPP_HDRLEN;
    mp->b_rptr[0] = PPP_ALLSTATIONS;
    mp->b_rptr[1] = PPP_UI;
    mp->b_rptr[2] = proto >> 8;
    mp->b_rptr[3] = proto;

    s = splstr();
    if (sp->flags & DBGLOG)
	printf("if_ppp: putnext(%x, %x), r=%x w=%x p=%x\n",
	       sp->q, mp, mp->b_rptr, mp->b_wptr, proto);
    putnext(sp->q, mp);
    splx(s);

    return 0;
}

/*
 * Socket ioctl routine for ppp interfaces.
 */
static int
if_ppp_ioctl(ifp, cmd, data)
    struct ifnet *ifp;
    u_int cmd;
    caddr_t data;
{
    int s, error;
    struct ifreq *ifr = (struct ifreq *) data;
    struct ifaddr *ifa = (struct ifaddr *) data;
    u_short mtu;

    error = 0;
    s = splimp();
    switch (cmd) {
    case SIOCSIFFLAGS:
	if ((ifp->if_flags & IFF_RUNNING) == 0)
	    ifp->if_flags &= ~IFF_UP;
	break;

    case SIOCSIFADDR:
	if (IFA_ADDR(ifa).sa_family != AF_INET)
	    error = EAFNOSUPPORT;
	break;

    case SIOCSIFDSTADDR:
	if (IFA_ADDR(ifa).sa_family != AF_INET)
	    error = EAFNOSUPPORT;
	break;

    case SIOCSIFMTU:
	if ((error = NOTSUSER()) != 0)
	    break;
#ifdef __osf__
	/* this hack is necessary because ifioctl checks ifr_data
	 * in 4.0 and 5.0, but ifr_data and ifr_metric overlay each 
	 * other in the definition of struct ifreq so pppd can't set both.
	 */
        bcopy(ifr->ifr_data, &mtu, sizeof (u_short));
        ifr->ifr_mtu = mtu;
#endif

	if (ifr->ifr_mtu < PPP_MINMTU || ifr->ifr_mtu > PPP_MAXMTU) {
	    error = EINVAL;
	    break;
	}
	ifp->if_mtu = ifr->ifr_mtu;
	break;

    case SIOCGIFMTU:
	ifr->ifr_mtu = ifp->if_mtu;
	break;

    case SIOCADDMULTI:
    case SIOCDELMULTI:
	switch(ifr->ifr_addr.sa_family) {
	case AF_INET:
	    break;
	default:
	    error = EAFNOSUPPORT;
	    break;
	}
	break;

    default:
	error = EINVAL;
    }
    splx(s);
    return (error);
}

/*
 * Turn a STREAMS message into an mbuf chain.
 */
static struct mbuf *
make_mbufs(mp, off)
    mblk_t *mp;
    int off;
{
    struct mbuf *head, **prevp, *m;
    int len, space, n;
    unsigned char *cp, *dp;

    len = msgdsize(mp);
    if (len == 0)
	return 0;
    prevp = &head;
    space = 0;
    cp = mp->b_rptr;
#ifdef __osf__
    MGETHDR(m, M_DONTWAIT, MT_DATA);
    m->m_len = 0;
    space = MHLEN;
    *prevp = m;
    prevp = &m->m_next;
    dp = mtod(m, unsigned char *);
    len -= space;
    off = 0;
#endif
    for (;;) {
	while (cp >= mp->b_wptr) {
	    mp = mp->b_cont;
	    if (mp == 0) {
		*prevp = 0;
		return head;
	    }
	    cp = mp->b_rptr;
	}
	n = mp->b_wptr - cp;
	if (space == 0) {
	    MGET(m, M_DONTWAIT, MT_DATA);
	    *prevp = m;
	    if (m == 0) {
		if (head != 0)
		    m_freem(head);
		return 0;
	    }
	    if (len + off > 2 * MLEN) {
#ifdef __osf__
		MCLGET(m, M_DONTWAIT);
#else
		MCLGET(m);
#endif
	    }
#ifdef __osf__
	    space = ((m->m_flags & M_EXT) ? MCLBYTES : MLEN);
#else
	    space = (m->m_off > MMAXOFF? MCLBYTES: MLEN) - off;
	    m->m_off += off;
#endif
	    m->m_len = 0;
	    len -= space;
	    dp = mtod(m, unsigned char *);
	    off = 0;
	    prevp = &m->m_next;
	}
	if (n > space)
	    n = space;
	bcopy(cp, dp, n);
	cp += n;
	dp += n;
	space -= n;
	m->m_len += n;
    }
}

/*
 * Turn an mbuf chain into a STREAMS message.
 */
#define ALLOCB_MAX	4096

static mblk_t *
make_message(m, off)
    struct mbuf *m;
    int off;
{
    mblk_t *head, **prevp, *mp;
    int len, space, n, nb;
    unsigned char *cp, *dp;
    struct mbuf *nm;

    len = 0;
    for (nm = m; nm != 0; nm = nm->m_next)
	len += nm->m_len;
    prevp = &head;
    space = 0;
    cp = mtod(m, unsigned char *);
    nb = m->m_len;
    for (;;) {
	while (nb <= 0) {
	    m = m->m_next;
	    if (m == 0) {
		*prevp = 0;
		return head;
	    }
	    cp = mtod(m, unsigned char *);
	    nb = m->m_len;
	}
	if (space == 0) {
	    space = len + off;
	    if (space > ALLOCB_MAX)
		space = ALLOCB_MAX;
	    mp = allocb(space, BPRI_LO);
	    *prevp = mp;
	    if (mp == 0) {
		if (head != 0)
		    freemsg(head);
		return 0;
	    }
	    dp = mp->b_rptr += off;
	    space -= off;
	    len -= space;
	    off = 0;
	    prevp = &mp->b_cont;
	}
	n = nb < space? nb: space;
	bcopy(cp, dp, n);
	cp += n;
	dp += n;
	nb -= n;
	space -= n;
	mp->b_wptr = dp;
    }
}

/*
 * Digital UNIX doesn't allow for removing ifnet structures
 * from the list.  But then we're not using this as a loadable
 * module anyway, so that's OK.
 *
 * Under SunOS, this should allow the module to be unloaded.
 * Unfortunately, it doesn't seem to detach all the references,
 * so your system may well crash after you unload this module :-(
 */
#ifndef __osf__

/*
 * Remove an interface from the system.
 * This routine contains magic.
 */
#include <net/route.h>
#include <netinet/in_pcb.h>
#include <netinet/ip_var.h>
#include <netinet/tcp.h>
#include <netinet/tcp_timer.h>
#include <netinet/tcp_var.h>
#include <netinet/udp.h>
#include <netinet/udp_var.h>

static void
ppp_if_detach(ifp)
    struct ifnet *ifp;
{
    int s;
    struct inpcb *pcb;
    struct ifaddr *ifa;
    struct in_ifaddr **inap;
    struct ifnet **ifpp;

    s = splhigh();

    /*
     * Clear the interface from any routes currently cached in
     * TCP or UDP protocol control blocks.
     */
    for (pcb = tcb.inp_next; pcb != &tcb; pcb = pcb->inp_next)
	if (pcb->inp_route.ro_rt && pcb->inp_route.ro_rt->rt_ifp == ifp)
	    in_losing(pcb);
    for (pcb = udb.inp_next; pcb != &udb; pcb = pcb->inp_next)
	if (pcb->inp_route.ro_rt && pcb->inp_route.ro_rt->rt_ifp == ifp)
	    in_losing(pcb);

    /*
     * Delete routes through all addresses of the interface.
     */
    for (ifa = ifp->if_addrlist; ifa != 0; ifa = ifa->ifa_next) {
	rtinit(ifa, ifa, SIOCDELRT, RTF_HOST);
	rtinit(ifa, ifa, SIOCDELRT, 0);
    }

    /*
     * Unlink the interface's address(es) from the in_ifaddr list.
     */
    for (inap = &in_ifaddr; *inap != 0; ) {
	if ((*inap)->ia_ifa.ifa_ifp == ifp)
	    *inap = (*inap)->ia_next;
	else
	    inap = &(*inap)->ia_next;
    }

    /*
     * Delete the interface from the ifnet list.
     */
    for (ifpp = &ifnet; (*ifpp) != 0; ) {
	if (*ifpp == ifp)
	    break;
	ifpp = &(*ifpp)->if_next;
    }
    if (*ifpp == 0)
	printf("couldn't find interface ppp%d in ifnet list\n", ifp->if_unit);
    else
	*ifpp = ifp->if_next;

    splx(s);
}

#endif /* __osf__ */
