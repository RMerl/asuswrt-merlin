/*
 * print.c - common print support functions for lsof
 */


/*
 * Copyright 1994 Purdue Research Foundation, West Lafayette, Indiana
 * 47907.  All rights reserved.
 *
 * Written by Victor A. Abell
 *
 * This software is not subject to any license of the American Telephone
 * and Telegraph Company or the Regents of the University of California.
 *
 * Permission is granted to anyone to use this software for any purpose on
 * any computer system, and to alter it and redistribute it freely, subject
 * to the following restrictions:
 *
 * 1. Neither the authors nor Purdue University are responsible for any
 *    consequences of the use of this software.
 *
 * 2. The origin of this software must not be misrepresented, either by
 *    explicit claim or by omission.  Credit to the authors and Purdue
 *    University must appear in documentation and sources.
 *
 * 3. Altered versions must be plainly marked as such, and must not be
 *    misrepresented as being the original software.
 *
 * 4. This notice may not be removed or altered.
 */

#ifndef lint
static char copyright[] =
"@(#) Copyright 1994 Purdue Research Foundation.\nAll rights reserved.\n";
static char *rcsid = "$Id: print.c,v 1.50 2008/10/21 16:21:41 abe Exp $";
#endif


#include "lsof.h"


/*
 * Local definitions, structures and function prototypes
 */

#define HCINC		64		/* host cache size increase chunk */
#define PORTHASHBUCKETS	128		/* port hash bucket count
					 * !!MUST BE A POWER OF 2!! */
#define	PORTTABTHRESH	10		/* threshold at which we will switch
					 * from using getservbyport() to
					 * getservent() -- see lkup_port()
					 * and fill_porttab() */

struct hostcache {
	unsigned char a[MAX_AF_ADDR];	/* numeric address */
	int af;				/* address family -- e.g., AF_INET
					 * or AF_INET6 */
	char *name;			/* name */
};

struct porttab {
	int port;
	MALLOC_S nl;			/* name length (excluding '\0') */
	int ss;				/* service name status, 0 = lookup not
					 * yet performed */
	char *name;
	struct porttab *next;
};


static struct porttab **Pth[4] = { NULL, NULL, NULL, NULL };
						/* port hash buckets:
						 * Pth[0] for TCP service names
						 * Pth[1] for UDP service names
						 * Pth[2] for TCP portmap info
						 * Pth[3] for UDP portmap info
						 */
#define HASHPORT(p)	(((((int)(p)) * 31415) >> 3) & (PORTHASHBUCKETS - 1))


_PROTOTYPE(static void fill_portmap,(void));
_PROTOTYPE(static void fill_porttab,(void));
_PROTOTYPE(static char *lkup_port,(int p, int pr, int src));
_PROTOTYPE(static char *lkup_svcnam,(int h, int p, int pr, int ss));
_PROTOTYPE(static int printinaddr,(void));
_PROTOTYPE(static void update_portmap,(struct porttab *pt, char *pn));


/*
 * endnm() - locate end of Namech
 */

char *
endnm(sz)
	size_t *sz;			/* returned remaining size */
{
	register char *s;
	register size_t tsz;

	for (s = Namech, tsz = Namechl; *s; s++, tsz--)
		;
	*sz = tsz;
	return(s);
}


/*
 * fill_portmap() -- fill the RPC portmap program name table via a conversation
 *		     with the portmapper
 *
 * The following copyright notice acknowledges that this function was adapted
 * from getrpcportnam() of the source code of the OpenBSD netstat program.
 */

/*
* Copyright (c) 1983, 1988, 1993
*      The Regents of the University of California.  All rights reserved.
*
* Redistribution and use in source and binary forms, with or without
* modification, are permitted provided that the following conditions
* are met:
* 1. Redistributions of source code must retain the above copyright
*    notice, this list of conditions and the following disclaimer.
* 2. Redistributions in binary form must reproduce the above copyright
*    notice, this list of conditions and the following disclaimer in the
*    documentation and/or other materials provided with the distribution.
* 3. All advertising materials mentioning features or use of this software
*    must display the following acknowledgement:
*      This product includes software developed by the University of
*      California, Berkeley and its contributors.
* 4. Neither the name of the University nor the names of its contributors
*    may be used to endorse or promote products derived from this software
*    without specific prior written permission.
*
* THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
* ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
* IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
* ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
* FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
* DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
* OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
* HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
* LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
* OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
* SUCH DAMAGE.
*/

static void
fill_portmap()
{
	char buf[128], *cp, *nm;
	CLIENT *c;
	int h, port, pr;
	MALLOC_S nl;
	struct pmaplist *p = (struct pmaplist *)NULL;
	struct porttab *pt;
	struct rpcent *r;
	struct TIMEVAL_LSOF tm;

#if	!defined(CAN_USE_CLNT_CREATE)
	struct hostent *he;
	struct sockaddr_in ia;
	int s = RPC_ANYSOCK;
#endif	/* !defined(CAN_USE_CLNT_CREATE) */

/*
 * Construct structures for communicating with the portmapper.
 */

#if	!defined(CAN_USE_CLNT_CREATE)
	zeromem(&ia, sizeof(ia));
	ia.sin_family = AF_INET;
	if ((he = gethostbyname("localhost")))
	    MEMMOVE((caddr_t)&ia.sin_addr, he->h_addr, he->h_length);
	ia.sin_port = htons(PMAPPORT);
#endif	/* !defined(CAN_USE_CLNT_CREATE) */

	tm.tv_sec = 60;
	tm.tv_usec = 0;
/*
 * Get an RPC client handle.  Then ask for a dump of the port map.
 */

#if	defined(CAN_USE_CLNT_CREATE)
	if (!(c = clnt_create("localhost", PMAPPROG, PMAPVERS, "tcp")))
#else	/* !defined(CAN_USE_CLNT_CREATE) */
	if (!(c = clnttcp_create(&ia, PMAPPROG, PMAPVERS, &s, 0, 0)))
#endif	/* defined(CAN_USE_CLNT_CREATE) */

	    return;
	if (clnt_call(c, PMAPPROC_DUMP, XDR_VOID, NULL, XDR_PMAPLIST,
		      (caddr_t)&p, tm)
	!= RPC_SUCCESS) {
	    clnt_destroy(c);
	    return;
	}
/*
 * Loop through the port map dump, creating portmap table entries from TCP
 * and UDP members.
 */
	for (; p; p = p->pml_next) {
	
	/*
	 * Determine the port map entry's protocol; ignore all but TCP and UDP.
	 */
	    if (p->pml_map.pm_prot == IPPROTO_TCP)
		pr = 2;
	    else if (p->pml_map.pm_prot == IPPROTO_UDP)
		pr = 3;
	    else
		continue;
	/*
	 * See if there's already a portmap entry for this port.  If there is,
	 * ignore this entry.
	 */
	    h = HASHPORT((port = (int)p->pml_map.pm_port));
	    for (pt = Pth[pr][h]; pt; pt = pt->next) {
		if (pt->port == port)
		    break;
	    }
	    if (pt)
		continue;
	/*
	 * Save the registration name or number.
	 */
	    cp = (char *)NULL;
	    if ((r = (struct rpcent *)getrpcbynumber(p->pml_map.pm_prog))) {
		if (r->r_name && strlen(r->r_name))
		    cp = r->r_name;
	    }
	    if (!cp) {
		(void) snpf(buf, sizeof(buf), "%lu",
			    (unsigned long)p->pml_map.pm_prog);
		cp = buf;
	    }
	    if (!strlen(cp))
		continue;
	/*
	 * Allocate space for the portmap name entry and copy it there.
	 */
	    if (!(nm = mkstrcpy(cp, &nl))) {
		(void) fprintf(stderr,
		    "%s: can't allocate space for portmap entry: ", Pn);
		safestrprt(cp, stderr, 1);
		Exit(1);
	    }
	    if (!nl) {
		(void) free((FREE_P *)nm);
		continue;
	    }
	/*
	 * Allocate and fill a porttab struct entry for the portmap table.
	 * Link it to the head of its hash bucket, and make it the new head.
	 */
	    if (!(pt = (struct porttab *)malloc(sizeof(struct porttab)))) {
		(void) fprintf(stderr,
		    "%s: can't allocate porttab entry for portmap: ", Pn);
		safestrprt(nm, stderr, 1);
		Exit(1);
	    }
	    pt->name = nm;
	    pt->nl = nl;
	    pt->port = port;
	    pt->next = Pth[pr][h];
	    pt->ss = 0;
	    Pth[pr][h] = pt;
	}
	clnt_destroy(c);
}


/*
 * fill_porttab() -- fill the TCP and UDP service name port table with a
 *		     getservent() scan
 */

static void
fill_porttab()
{
	int h, p, pr;
	MALLOC_S nl;
	char *nm;
	struct porttab *pt;
	struct servent *se;

	(void) endservent();
/*
 * Scan the services data base for TCP and UDP entries that have a non-null
 * name associated with them.
 */
	(void) setservent(1);
	while ((se = getservent())) {
	    if (!se->s_name || !se->s_proto)
		continue;
	    if (strcasecmp(se->s_proto, "TCP") == 0)
		pr = 0;
	    else if (strcasecmp(se->s_proto, "UDP") == 0)
		pr = 1;
	    else
		continue;
	    if (!se->s_name || !strlen(se->s_name))
		continue;
	    p = ntohs(se->s_port);
	/*
	 * See if a port->service entry is already cached for this port and
	 * prototcol.  If it is, leave it alone.
	 */
	    h = HASHPORT(p);
	    for (pt = Pth[pr][h]; pt; pt = pt->next) {
		if (pt->port == p)
		    break;
	    }
	    if (pt)
		continue;
	/*
	 * Add a new entry to the cache for this port and protocol.
	 */
	    if (!(nm = mkstrcpy(se->s_name, &nl))) {
		(void) fprintf(stderr,
		    "%s: can't allocate %d bytes for port %d name: %s\n",
		    Pn, (int)(nl + 1), p, se->s_name);
		Exit(1);
	    }
	    if (!nl) {
		(void) free((FREE_P *)nm);
		continue;
	    }
	    if (!(pt = (struct porttab *)malloc(sizeof(struct porttab)))) {
		(void) fprintf(stderr,
		    "%s: can't allocate porttab entry for port %d: %s\n",
		    Pn, p, se->s_name);
		Exit(1);
	    }
	    pt->name = nm;
	    pt->nl = nl - 1;
	    pt->port = p;
	    pt->next = Pth[pr][h];
	    pt->ss = 0;
	    Pth[pr][h] = pt;
	}
	(void) endservent();
}


/*
 * gethostnm() - get host name
 */

char *
gethostnm(ia, af)
	unsigned char *ia;		/* Internet address */
	int af;				/* address family -- e.g., AF_INET
					 * or AF_INET6 */
{
	int al = MIN_AF_ADDR;
	char hbuf[256];
	static struct hostcache *hc = (struct hostcache *)NULL;
	static int hcx = 0;
	char *hn, *np;
	struct hostent *he = (struct hostent *)NULL;
	int i, j;
	MALLOC_S len;
	static int nhc = 0;
/*
 * Search cache.
 */

#if	defined(HASIPv6)
	if (af == AF_INET6)
	    al = MAX_AF_ADDR;
#endif	/* defined(HASIPv6) */

	for (i = 0; i < hcx; i++) {
	    if (af != hc[i].af)
		continue;
	    for (j = 0; j < al; j++) {
		if (ia[j] != hc[i].a[j])
		    break;
	    }
	    if (j >= al)
		return(hc[i].name);
	}
/*
 * If -n has been specified, construct a numeric address.  Otherwise, look up
 * host name by address.  If that fails, or if there is no name in the returned
 * hostent structure, construct a numeric version of the address.
 */
	if (Fhost)
	    he = gethostbyaddr((char *)ia, al, af);
	if (!he || !he->h_name) {

#if	defined(HASIPv6)
	    if (af == AF_INET6) {

	    /*
	     * Since IPv6 numeric addresses use `:' as a separator, enclose
	     * them in brackets.
	     */
		hbuf[0] = '[';
		if (!inet_ntop(af, ia, hbuf + 1, sizeof(hbuf) - 3)) {
		    (void) snpf(&hbuf[1], (sizeof(hbuf) - 1),
			"can't format IPv6 address]");
		} else {
		    len = strlen(hbuf);
		    (void) snpf(&hbuf[len], sizeof(hbuf) - len, "]");
		}
	    } else
#endif	/* defined(HASIPv6) */

	    if (af == AF_INET)
		(void) snpf(hbuf, sizeof(hbuf), "%u.%u.%u.%u", ia[0], ia[1],
			    ia[2], ia[3]);
	    else
		(void) snpf(hbuf, sizeof(hbuf), "(unknown AF value: %d)", af);
	    hn = hbuf;
	} else
	    hn = (char *)he->h_name;
/*
 * Allocate space for name and copy name to it.
 */
	if (!(np = mkstrcpy(hn, (MALLOC_S *)NULL))) {
	    (void) fprintf(stderr, "%s: no space for host name: ", Pn);
	    safestrprt(hn, stderr, 1);
	    Exit(1);
	}
/*
 * Add address/name entry to cache.  Allocate cache space in HCINC chunks.
 */
	if (hcx >= nhc) {
	    nhc += HCINC;
	    len = (MALLOC_S)(nhc * sizeof(struct hostcache));
	    if (!hc)
		hc = (struct hostcache *)malloc(len);
	    else
		hc = (struct hostcache *)realloc((MALLOC_P *)hc, len);
	    if (!hc) {
		(void) fprintf(stderr, "%s: no space for host cache\n", Pn);
		Exit(1);
	    }
	}
	hc[hcx].af = af;
	for (i = 0; i < al; i++) {
	    hc[hcx].a[i] = ia[i];
	}
	hc[hcx++].name = np;
	return(np);
}


/*
 * lkup_port() - look up port for protocol
 */

static char *
lkup_port(p, pr, src)
	int p;				/* port number */
	int pr;				/* protocol index: 0 = tcp, 1 = udp */
	int src;			/* port source: 0 = local
					 *		1 = foreign */
{
	int h, nh;
	MALLOC_S nl;
	char *nm, *pn;
	static char pb[128];
	static int pm = 0;
	struct porttab *pt;
/*
 * If the hash buckets haven't been allocated, do so.
 */
	if (!Pth[0]) {
	    nh = FportMap ? 4 : 2;
	    for (h = 0; h < nh; h++) {
		if (!(Pth[h] = (struct porttab **)calloc(PORTHASHBUCKETS,
				sizeof(struct porttab *))))
		{
		    (void) fprintf(stderr,
		      "%s: can't allocate %d bytes for %s %s hash buckets\n",
		      Pn,
		      (int)(2 * (PORTHASHBUCKETS * sizeof(struct porttab *))),
		      (h & 1) ? "UDP" : "TCP",
		      (h > 1) ? "portmap" : "port");
		    Exit(1);
		}
	    }
	}
/*
 * If we're looking up program names for portmapped ports, make sure the
 * portmap table has been loaded.
 */
	if (FportMap && !pm) {
	    (void) fill_portmap();
	    pm++;
	}
/*
 * Hash the port and see if its name has been cached.  Look for a local
 * port first in the portmap, if portmap searching is enabled.
 */
	h = HASHPORT(p);
	if (!src && FportMap) {
	    for (pt = Pth[pr+2][h]; pt; pt = pt->next) {
		if (pt->port != p)
		    continue;
		if (!pt->ss) {
		    pn = Fport ? lkup_svcnam(h, p, pr, 0) : (char *)NULL;
		    if (!pn) {
			(void) snpf(pb, sizeof(pb), "%d", p);
			pn = pb;
		    }
		    (void) update_portmap(pt, pn);
		}
		return(pt->name);
	    }
	}
	for (pt = Pth[pr][h]; pt; pt = pt->next) {
	    if (pt->port == p)
		return(pt->name);
	}
/*
 * Search for a possible service name, unless the -P option has been specified.
 *
 * If there is no service name, return a %d conversion.
 *
 * Don't cache %d conversions; a zero port number is a %d conversion that
 * is represented by "*".
 */
	pn = Fport ? lkup_svcnam(h, p, pr, 1) : (char *)NULL;
	if (!pn || !strlen(pn)) {
	    if (p) {
		(void) snpf(pb, sizeof(pb), "%d", p);
		return(pb);
	    } else
		return("*");
	}
/*
 * Allocate a new porttab entry for the TCP or UDP service name.
 */
	if (!(pt = (struct porttab *)malloc(sizeof(struct porttab)))) {
	    (void) fprintf(stderr,
		"%s: can't allocate porttab entry for port %d\n", Pn, p);
	    Exit(1);
	}
/*
 * Allocate space for the name; copy it to the porttab entry; and link the
 * porttab entry to its hash bucket.
 *
 * Return a pointer to the name.
 */
	if (!(nm = mkstrcpy(pn, &nl))) {
	    (void) fprintf(stderr,
		"%s: can't allocate space for port name: ", Pn);
	    safestrprt(pn, stderr, 1);
	    Exit(1);
	}
	pt->name = nm;
	pt->nl = nl;
	pt->port = p;
	pt->next = Pth[pr][h];
	pt->ss = 0;
	Pth[pr][h] = pt;
	return(nm);
}


/*
 * lkup_svcnam() - look up service name for port
 */

static char *
lkup_svcnam(h, p, pr, ss)
	int h;				/* porttab hash index */
	int p;				/* port number */
	int pr;				/* protocol: 0 = TCP, 1 = UDP */
	int ss;				/* search status: 1 = Pth[pr][h]
					 *		  already searched */
{
	static int fl[PORTTABTHRESH];
	static int fln = 0;
	static int gsbp = 0;
	int i;
	struct porttab *pt;
	static int ptf = 0;
	struct servent *se;
/*
 * Do nothing if -P has been specified.
 */
	if (!Fport)
	    return((char *)NULL);

	for (;;) {

	/*
	 * Search service name cache, if it hasn't already been done.
	 * Return the name of a match.
	 */
	    if (!ss) {
		for (pt = Pth[pr][h]; pt; pt = pt->next) {
		    if (pt->port == p)
			return(pt->name);
		}
	    }
/*
 * If fill_porttab() has been called, there is no service name.
 *
 * Do PORTTABTHRES getservbport() calls, remembering the failures, so they
 * won't be repeated.
 *
 * After PORTABTHRESH getservbyport() calls, call fill_porttab() once,
 */
	    if (ptf)
		break;
	    if (gsbp < PORTTABTHRESH) {
		for (i = 0; i < fln; i++) {
		    if (fl[i] == p)
			return((char *)NULL);
		}
		gsbp++;
		if ((se = getservbyport(htons(p), pr ? "udp" : "tcp")))
		    return(se->s_name);
		if (fln < PORTTABTHRESH)
		    fl[fln++] = p;
		return((char *)NULL);
	    }
	    (void) fill_porttab();
	    ptf++;
	    ss = 0;
	}
	return((char *)NULL);
}


/*
 * print_file() - print file
 */

void
print_file()
{
	char buf[128];
	char *cp = (char *)NULL;
	dev_t dev;
	int devs, len;

	if (PrPass && !Hdr) {

	/*
	 * Print the header line if this is the second pass and the
	 * header hasn't already been printed.
	 */
	    (void) printf("%-*.*s %*s", CmdColW, CmdColW, CMDTTL, PidColW,
		PIDTTL);

#if	defined(HASZONES)
	    if (Fzone)
		(void) printf(" %-*s", ZoneColW, ZONETTL);
#endif	/* defined(HASZONES) */

#if	defined(HASSELINUX)
	    if (Fcntx)
		(void) printf(" %-*s", CntxColW, CNTXTTL);
#endif /* defined(HASSELINUX) */

#if	defined(HASPPID)
	    if (Fppid)
	 	(void) printf(" %*s", PpidColW, PPIDTTL);
#endif	/* defined(HASPPID) */

	    if (Fpgid)
		(void) printf(" %*s", PgidColW, PGIDTTL);
	    (void) printf(" %*s %*s   %*s",
		UserColW, USERTTL,
		FdColW - 2, FDTTL,
		TypeColW, TYPETTL);

#if	defined(HASFSTRUCT)
	    if (Fsv) {

# if	!defined(HASNOFSADDR)
		if (Fsv & FSV_FA)
		    (void) printf(" %*s", FsColW, FSTTL);
# endif	/* !defined(HASNOFSADDR) */

# if	!defined(HASNOFSCOUNT)
		if (Fsv & FSV_CT)
		    (void) printf(" %*s", FcColW, FCTTL);
# endif	/* !defined(HASNOFSCOUNT) */

# if	!defined(HASNOFSFLAGS)
		if (Fsv & FSV_FG)
		    (void) printf(" %*s", FgColW, FGTTL);
# endif	/* !defined(HASNOFSFLAGS) */

# if	!defined(HASNOFSNADDR)
		if (Fsv & FSV_NI)
		    (void) printf(" %*s", NiColW, NiTtl);
# endif	/* !defined(HASNOFSNADDR) */

	    }
#endif	/* defined(HASFSTRUCT) */

	    (void) printf(" %*s", DevColW, DEVTTL);
	    if (Foffset)
		(void) printf(" %*s", SzOffColW, OFFTTL);
	    else if (Fsize)
		(void) printf(" %*s", SzOffColW, SZTTL);
	    else
		(void) printf(" %*s", SzOffColW, SZOFFTTL);
	    if (Fnlink)
		(void) printf(" %*s", NlColW, NLTTL);
	    (void) printf(" %*s %s\n", NodeColW, NODETTL, NMTTL);
	    Hdr++;
	}
/*
 * Size or print the command.
 */
	cp = (Lp->cmd && *Lp->cmd != '\0') ? Lp->cmd : "(unknown)";
	if (!PrPass) {
	    len = safestrlen(cp, 2);
	    if (CmdLim && (len > CmdLim))
		len = CmdLim;
	    if (len > CmdColW)
		CmdColW = len;
	} else
	    safestrprtn(cp, CmdColW, stdout, 2);
/*
 * Size or print the process ID.
 */
	if (!PrPass) {
	    (void) snpf(buf, sizeof(buf), "%d", Lp->pid);
	    if ((len = strlen(buf)) > PidColW)
		PidColW = len;
	} else
	    (void) printf(" %*d", PidColW, Lp->pid);

#if	defined(HASZONES)
/*
 * Size or print the zone.
 */
	if (Fzone) {
	    if (!PrPass) {
		if (Lp->zn) {
		    if ((len = strlen(Lp->zn)) > ZoneColW)
			ZoneColW = len;
		}
	    } else
		(void) printf(" %-*s", ZoneColW, Lp->zn ? Lp->zn : "");
	}
#endif	/* defined(HASZONES) */

#if	defined(HASSELINUX)
/*
 * Size or print the context.
 */
	if (Fcntx) {
	    if (!PrPass) {
		if (Lp->cntx) {
		    if ((len = strlen(Lp->cntx)) > CntxColW)
			CntxColW = len;
		}
	    } else
		(void) printf(" %-*s", CntxColW, Lp->cntx ? Lp->cntx : "");
	}
#endif	/* defined(HASSELINUX) */

#if	defined(HASPPID)
	if (Fppid) {

	/*
	 * Size or print the parent process ID.
	 */
	    if (!PrPass) {
		(void) snpf(buf, sizeof(buf), "%d", Lp->ppid);
		if ((len = strlen(buf)) > PpidColW)
		    PpidColW = len;
	    } else
		(void) printf(" %*d", PpidColW, Lp->ppid);
	}
#endif	/* defined(HASPPID) */

	if (Fpgid) {

	/*
	 * Size or print the process group ID.
	 */
	    if (!PrPass) {
		(void) snpf(buf, sizeof(buf), "%d", Lp->pgid);
		if ((len = strlen(buf)) > PgidColW)
		    PgidColW = len;
	    } else
		(void) printf(" %*d", PgidColW, Lp->pgid);
	}
/*
 * Size or print the user ID or login name.
 */
	if (!PrPass) {
	    if ((len = strlen(printuid((UID_ARG)Lp->uid, NULL))) > UserColW)
		UserColW = len;
	} else
	    (void) printf(" %*.*s", UserColW, UserColW,
		printuid((UID_ARG)Lp->uid, NULL));
/*
 * Size or print the file descriptor, access mode and lock status.
 */
	if (!PrPass) {
	    (void) snpf(buf, sizeof(buf), "%s%c%c",
		Lf->fd,
		(Lf->lock == ' ') ? Lf->access
				  : (Lf->access == ' ') ? '-'
							: Lf->access,
		Lf->lock);
	    if ((len = strlen(buf)) > FdColW)
		FdColW = len;
	} else
	    (void) printf(" %*.*s%c%c", FdColW - 2, FdColW - 2, Lf->fd,
		(Lf->lock == ' ') ? Lf->access
				  : (Lf->access == ' ') ? '-'
							: Lf->access,
		Lf->lock);
/*
 * Size or print the type.
 */
	if (!PrPass) {
	    if ((len = strlen(Lf->type)) > TypeColW)
		TypeColW = len;
	} else
	    (void) printf(" %*.*s", TypeColW, TypeColW, Lf->type);

#if	defined(HASFSTRUCT)
/*
 * Size or print the file structure address, file usage count, and node
 * ID (address).
 */

	if (Fsv) {

# if	!defined(HASNOFSADDR)
	    if (Fsv & FSV_FA) {
		cp =  (Lf->fsv & FSV_FA) ? print_kptr(Lf->fsa, buf, sizeof(buf))
					 : "";
		if (!PrPass) {
		    if ((len = strlen(cp)) > FsColW)
			FsColW = len;
		} else
		    (void) printf(" %*.*s", FsColW, FsColW, cp);
		    
	    }
# endif	/* !defined(HASNOFSADDR) */

# if	!defined(HASNOFSCOUNT)
	    if (Fsv & FSV_CT) {
		if (Lf->fsv & FSV_CT) {
		    (void) snpf(buf, sizeof(buf), "%ld", Lf->fct);
		    cp = buf;
		} else
		    cp = "";
		if (!PrPass) {
		    if ((len = strlen(cp)) > FcColW)
			FcColW = len;
		} else
		    (void) printf(" %*.*s", FcColW, FcColW, cp);
	    }
# endif	/* !defined(HASNOFSCOUNT) */

# if	!defined(HASNOFSFLAGS)
	    if (Fsv & FSV_FG) {
		if ((Lf->fsv & FSV_FG) && (FsvFlagX || Lf->ffg || Lf->pof))
		    cp = print_fflags(Lf->ffg, Lf->pof);
		else
		    cp = "";
		if (!PrPass) {
		    if ((len = strlen(cp)) > FgColW)
			FgColW = len;
		} else
		    (void) printf(" %*.*s", FgColW, FgColW, cp);
	    }
# endif	/* !defined(HASNOFSFLAGS) */

# if	!defined(HASNOFSNADDR)
	    if (Fsv & FSV_NI) {
		cp = (Lf->fsv & FSV_NI) ? print_kptr(Lf->fna, buf, sizeof(buf))
					: "";
		if (!PrPass) {
		    if ((len = strlen(cp)) > NiColW)
			NiColW = len;
		} else
		    (void) printf(" %*.*s", NiColW, NiColW, cp);
	    }
# endif	/* !defined(HASNOFSNADDR) */

	}
#endif	/* defined(HASFSTRUCT) */

/*
 * Size or print the device information.
 */

	if (Lf->rdev_def) {
	    dev = Lf->rdev;
	    devs = 1;
	} else if (Lf->dev_def) {
	    dev = Lf->dev;
	    devs = 1;
	} else
	    devs = 0;
	if (devs) {

#if	defined(HASPRINTDEV)
	    cp = HASPRINTDEV(Lf, &dev);
#else	/* !defined(HASPRINTDEV) */
	    (void) snpf(buf, sizeof(buf), "%u,%u", GET_MAJ_DEV(dev),
		GET_MIN_DEV(dev));
	    cp = buf;
#endif	/* defined(HASPRINTDEV) */

	}

	if (!PrPass) {
	    if (devs)
		len = strlen(cp);
	    else if (Lf->dev_ch)
		len = strlen(Lf->dev_ch);
	    else
		len = 0;
	    if (len > DevColW)
		DevColW = len;
	} else {
	    if (devs)
		(void) printf(" %*.*s", DevColW, DevColW, cp);
	    else {
		if (Lf->dev_ch)
		    (void) printf(" %*.*s", DevColW, DevColW, Lf->dev_ch);
		else
		    (void) printf(" %*.*s", DevColW, DevColW, "");
	    }
	}
/*
 * Size or print the size or offset.
 */
	if (!PrPass) {
	    if (Lf->sz_def) {

#if	defined(HASPRINTSZ)
		cp = HASPRINTSZ(Lf);
#else	/* !defined(HASPRINTSZ) */
		(void) snpf(buf, sizeof(buf), SzOffFmt_d, Lf->sz);
		cp = buf;
#endif	/* defined(HASPRINTSZ) */

		len = strlen(cp);
	    } else if (Lf->off_def) {

#if	defined(HASPRINTOFF)
		cp = HASPRINTOFF(Lf, 0);
#else	/* !defined(HASPRINTOFF) */
		(void) snpf(buf, sizeof(buf), SzOffFmt_0t, Lf->off);
		cp = buf;
#endif	/* defined(HASPRINTOFF) */

		len = strlen(cp);
		if (OffDecDig && len > (OffDecDig + 2)) {

#if	defined(HASPRINTOFF)
		    cp = HASPRINTOFF(Lf, 1);
#else	/* !defined(HASPRINTOFF) */
		    (void) snpf(buf, sizeof(buf), SzOffFmt_x, Lf->off);
		    cp = buf;
#endif	/* defined(HASPRINTOFF) */

		    len = strlen(cp);
		}
	    } else
		len = 0;
	    if (len > SzOffColW)
		SzOffColW = len;
	} else {
	    putchar(' ');
	    if (Lf->sz_def)

#if	defined(HASPRINTSZ)
		(void) printf("%*.*s", SzOffColW, SzOffColW, HASPRINTSZ(Lf));
#else	/* !defined(HASPRINTSZ) */
		(void) printf(SzOffFmt_dv, SzOffColW, Lf->sz);
#endif	/* defined(HASPRINTSZ) */

	    else if (Lf->off_def) {

#if	defined(HASPRINTOFF)
		cp = HASPRINTOFF(Lf, 0);
#else	/* !defined(HASPRINTOFF) */
		(void) snpf(buf, sizeof(buf), SzOffFmt_0t, Lf->off);
		cp = buf;
#endif	/* defined(HASPRINTOFF) */

		if (OffDecDig && (int)strlen(cp) > (OffDecDig + 2)) {

#if	defined(HASPRINTOFF)
		    cp = HASPRINTOFF(Lf, 1);
#else	/* !defined(HASPRINTOFF) */
		    (void) snpf(buf, sizeof(buf), SzOffFmt_x, Lf->off);
		    cp = buf;
#endif	/* defined(HASPRINTOFF) */

		}
		(void) printf("%*.*s", SzOffColW, SzOffColW, cp);
	    } else
		(void) printf("%*.*s", SzOffColW, SzOffColW, "");
	}
/*
 * Size or print the link count.
 */
	if (Fnlink) {
	    if (Lf->nlink_def) {
		(void) snpf(buf, sizeof(buf), " %ld", Lf->nlink);
		cp = buf;
	   } else
		cp = "";
	    if (!PrPass) {
		if ((len = strlen(cp)) > NlColW)
		    NlColW = len;
	    } else
		(void) printf(" %*s", NlColW, cp);
	}
/*
 * Size or print the inode information.
 */
	switch (Lf->inp_ty) {
	case 1:

#if	defined(HASPRINTINO)
	    cp = HASPRINTINO(Lf);
#else	/* !defined(HASPRINTINO) */
	    (void) snpf(buf, sizeof(buf), InodeFmt_d, Lf->inode);
	    cp = buf;
#endif	/* defined(HASPRINTINO) */

	    break;
	case 2:
	    if (Lf->iproto[0])
		cp = Lf->iproto;
	    else
		cp = "";
	    break;
	case 3:
	    (void) snpf(buf, sizeof(buf), InodeFmt_x, Lf->inode);
	    cp = buf;
	    break;
	default:
	    cp = "";
	}
	if (!PrPass) {
	    if ((len = strlen(cp)) > NodeColW)
		NodeColW = len;
	} else {
	    (void) printf(" %*.*s", NodeColW, NodeColW, cp);
	}
/*
 * If this is the second pass, print the name column.  (It doesn't need
 * to be sized.)
 */
	if (PrPass) {
	    putchar(' ');

#if	defined(HASPRINTNM)
	    HASPRINTNM(Lf);
#else	/* !defined(HASPRINTNM) */
	    printname(1);
#endif	/* defined(HASPRINTNM) */

	}
}


/*
 * printinaddr() - print Internet addresses
 */

static int
printinaddr()
{
	int i, len, src;
	char *host, *port;
	int nl = Namechl - 1;
	char *np = Namech;
	char pbuf[32];
/*
 * Process local network address first.  If there's a foreign address,
 * separate it from the local address with "->".
 */
	for (i = 0, *np = '\0'; i < 2; i++) {
	    if (!Lf->li[i].af)
		continue;
	    host = port = (char *)NULL;
	    if (i) {

	    /*
	     * If this is the foreign address, insert the separator.
	     */
		if (nl < 2)

addr_too_long:

		    {
			(void) snpf(Namech, Namechl,
			    "network addresses too long");
			return(1);
		    }
		(void) snpf(np, nl, "->");
		np += 2;
		nl -= 2;
	    }
	/*
	 * Convert the address to a host name.
	 */

#if	defined(HASIPv6)
	    if ((Lf->li[i].af == AF_INET6
	    &&   IN6_IS_ADDR_UNSPECIFIED(&Lf->li[i].ia.a6))
	    ||  (Lf->li[i].af == AF_INET
	    &&    Lf->li[i].ia.a4.s_addr == INADDR_ANY))
		host ="*";
	    else
		host = gethostnm((unsigned char *)&Lf->li[i].ia, Lf->li[i].af);
#else /* !defined(HASIPv6) */
	    if (Lf->li[i].ia.a4.s_addr == INADDR_ANY)
		host ="*";
	    else
		host = gethostnm((unsigned char *)&Lf->li[i].ia, Lf->li[i].af);
#endif	/* defined(HASIPv6) */

	/*
	 * Process the port number.
	 */
	    if (Lf->li[i].p > 0) {
		if (Fport || FportMap) {

		/*
		 * If converting port numbers to service names, or looking
		 * up portmap program names and numbers, do so by protocol.
		 *
		 * Identify the port source as local if: 1) it comes from the
		 * local entry (0) of the file's Internet address array; or
		 * 2) it comes from  the foreign entry (1), and the foreign
		 * Internet address matches the local one; or 3) it is the
		 * loopback address 127.0.0.1.  (Test 2 may not always work
		 * -- e.g., on hosts with multiple interfaces.)
		 */
		    if ((src = i) && FportMap) {

#if	defined(HASIPv6)
			if (Lf->li[0].af == AF_INET6) {
			    if (IN6_IS_ADDR_LOOPBACK(&Lf->li[i].ia.a6)
			    ||  IN6_ARE_ADDR_EQUAL(&Lf->li[0].ia.a6,
						   &Lf->li[1].ia.a6)
			    )
				src = 0;
			} else
#endif	/* defined(HASIPv6) */

			if (Lf->li[0].af == AF_INET) {
			    if (Lf->li[i].ia.a4.s_addr == htonl(INADDR_LOOPBACK)
			    ||  Lf->li[0].ia.a4.s_addr == Lf->li[1].ia.a4.s_addr
			    )
				src = 0;
			}
		    }
		    if (strcasecmp(Lf->iproto, "TCP") == 0)
			port = lkup_port(Lf->li[i].p, 0, src);
		    else if (strcasecmp(Lf->iproto, "UDP") == 0)
			port = lkup_port(Lf->li[i].p, 1, src);
		}
		if (!port) {
		    (void) snpf(pbuf, sizeof(pbuf), "%d", Lf->li[i].p);
		    port = pbuf;
		}
	    } else if (Lf->li[i].p == 0)
		port = "*";
	/*
	 * Enter the host name.
	 */
	    if (host) {
		if ((len = strlen(host)) > nl)
		    goto addr_too_long;
		if (len) {
		    (void) snpf(np, nl, "%s", host);
		    np += len;
		    nl -= len;
		}
	    }
	/*
	 * Enter the port number, preceded by a colon.
	 */
	    if (port) {
		if (((len = strlen(port)) + 1) >= nl)
		    goto addr_too_long;
		(void) snpf(np, nl, ":%s", port);
		np += len + 1;
		nl -= len - 1;
	    }
	}
	if (Namech[0]) {
	    safestrprt(Namech, stdout, 0);
	    return(1);
	}
	return(0);
}


/*
 * print_init() - initialize for printing
 */

void
print_init()
{
	PrPass = (Ffield || Fterse) ? 1 : 0;
	CmdColW = strlen(CMDTTL);
	DevColW = strlen(DEVTTL);
	FdColW = strlen(FDTTL);
	if (Fnlink)
	    NlColW = strlen(NLTTL);
	NmColW = strlen(NMTTL);
	NodeColW = strlen(NODETTL);
	PgidColW = strlen(PGIDTTL);
	PidColW = strlen(PIDTTL);
	PpidColW = strlen(PPIDTTL);
	if (Fsize)
	    SzOffColW = strlen(SZTTL);
	else if (Foffset)
	    SzOffColW = strlen(OFFTTL);
	else
	    SzOffColW = strlen(SZOFFTTL);
	TypeColW = strlen(TYPETTL);
	UserColW = strlen(USERTTL);

#if	defined(HASFSTRUCT)

# if	!defined(HASNOFSADDR)
	FsColW = strlen(FSTTL);
# endif	/* !defined(HASNOFSADDR) */

# if	!defined(HASNOFSCOUNT)
	FcColW = strlen(FCTTL);
# endif	/* !defined(HASNOFSCOUNT) */

# if	!defined(HASNOFSFLAGS)
	FgColW = strlen(FGTTL);
# endif	/* !defined(HASNOFSFLAGS) */

# if	!defined(HASNOFSNADDR)
	NiColW = strlen(NiTtl);
# endif	/* !defined(HASNOFSNADDR) */
#endif	/* defined(HASFSTRUCT) */

#if	defined(HASSELINUX)
	if (Fcntx)
	    CntxColW = strlen(CNTXTTL);
#endif	/* defined(HASSELINUX) */

#if	defined(HASZONES)
	if (Fzone)
	    ZoneColW = strlen(ZONETTL);
#endif	/* defined(HASZONES) */

}


#if	!defined(HASPRIVPRIPP)
/*
 * printiproto() - print Internet protocol name
 */

void
printiproto(p)
	int p;				/* protocol number */
{
	int i;
	static int m = -1;
	char *s;

	switch (p) {

#if	defined(IPPROTO_TCP)
	case IPPROTO_TCP:
	    s = "TCP";
	    break;
#endif	/* defined(IPPROTO_TCP) */

#if	defined(IPPROTO_UDP)
	case IPPROTO_UDP:
	    s = "UDP";
	    break;
#endif	/* defined(IPPROTO_UDP) */

#if	defined(IPPROTO_IP)
# if	!defined(IPPROTO_HOPOPTS) || IPPROTO_IP!=IPPROTO_HOPOPTS
	case IPPROTO_IP:
	    s = "IP";
	    break;
# endif	/* !defined(IPPROTO_HOPOPTS) || IPPROTO_IP!=IPPROTO_HOPOPTS */
#endif	/* defined(IPPROTO_IP) */

#if	defined(IPPROTO_ICMP)
	case IPPROTO_ICMP:
	    s = "ICMP";
	    break;
#endif	/* defined(IPPROTO_ICMP) */

#if	defined(IPPROTO_ICMPV6)
	case IPPROTO_ICMPV6:
	    s = "ICMPV6";
	    break;
#endif	/* defined(IPPROTO_ICMPV6) */

#if	defined(IPPROTO_IGMP)
	case IPPROTO_IGMP:
	    s = "IGMP";
	    break;
#endif	/* defined(IPPROTO_IGMP) */

#if	defined(IPPROTO_GGP)
	case IPPROTO_GGP:
	    s = "GGP";
	    break;
#endif	/* defined(IPPROTO_GGP) */

#if	defined(IPPROTO_EGP)
	case IPPROTO_EGP:
	    s = "EGP";
	    break;
#endif	/* defined(IPPROTO_EGP) */

#if	defined(IPPROTO_PUP)
	case IPPROTO_PUP:
	    s = "PUP";
	    break;
#endif	/* defined(IPPROTO_PUP) */

#if	defined(IPPROTO_IDP)
	case IPPROTO_IDP:
	    s = "IDP";
	    break;
#endif	/* defined(IPPROTO_IDP) */

#if	defined(IPPROTO_ND)
	case IPPROTO_ND:
	    s = "ND";
	    break;
#endif	/* defined(IPPROTO_ND) */

#if	defined(IPPROTO_RAW)
	case IPPROTO_RAW:
	    s = "RAW";
	    break;
#endif	/* defined(IPPROTO_RAW) */

#if	defined(IPPROTO_HELLO)
	case IPPROTO_HELLO:
	    s = "HELLO";
	    break;
#endif	/* defined(IPPROTO_HELLO) */

#if	defined(IPPROTO_PXP)
	case IPPROTO_PXP:
	    s = "PXP";
	    break;
#endif	/* defined(IPPROTO_PXP) */

#if	defined(IPPROTO_RAWIP)
	case IPPROTO_RAWIP:
	    s = "RAWIP";
	    break;
#endif	/* defined(IPPROTO_RAWIP) */

#if	defined(IPPROTO_RAWIF)
	case IPPROTO_RAWIF:
	    s = "RAWIF";
	    break;
#endif	/* defined(IPPROTO_RAWIF) */

#if	defined(IPPROTO_HOPOPTS)
	case IPPROTO_HOPOPTS:
	    s = "HOPOPTS";
	    break;
#endif	/* defined(IPPROTO_HOPOPTS) */

#if	defined(IPPROTO_IPIP)
	case IPPROTO_IPIP:
	    s = "IPIP";
	    break;
#endif	/* defined(IPPROTO_IPIP) */

#if	defined(IPPROTO_ST)
	case IPPROTO_ST:
	    s = "ST";
	    break;
#endif	/* defined(IPPROTO_ST) */

#if	defined(IPPROTO_PIGP)
	case IPPROTO_PIGP:
	    s = "PIGP";
	    break;
#endif	/* defined(IPPROTO_PIGP) */

#if	defined(IPPROTO_RCCMON)
	case IPPROTO_RCCMON:
	    s = "RCCMON";
	    break;
#endif	/* defined(IPPROTO_RCCMON) */

#if	defined(IPPROTO_NVPII)
	case IPPROTO_NVPII:
	    s = "NVPII";
	    break;
#endif	/* defined(IPPROTO_NVPII) */

#if	defined(IPPROTO_ARGUS)
	case IPPROTO_ARGUS:
	    s = "ARGUS";
	    break;
#endif	/* defined(IPPROTO_ARGUS) */

#if	defined(IPPROTO_EMCON)
	case IPPROTO_EMCON:
	    s = "EMCON";
	    break;
#endif	/* defined(IPPROTO_EMCON) */

#if	defined(IPPROTO_XNET)
	case IPPROTO_XNET:
	    s = "XNET";
	    break;
#endif	/* defined(IPPROTO_XNET) */

#if	defined(IPPROTO_CHAOS)
	case IPPROTO_CHAOS:
	    s = "CHAOS";
	    break;
#endif	/* defined(IPPROTO_CHAOS) */

#if	defined(IPPROTO_MUX)
	case IPPROTO_MUX:
	    s = "MUX";
	    break;
#endif	/* defined(IPPROTO_MUX) */

#if	defined(IPPROTO_MEAS)
	case IPPROTO_MEAS:
	    s = "MEAS";
	    break;
#endif	/* defined(IPPROTO_MEAS) */

#if	defined(IPPROTO_HMP)
	case IPPROTO_HMP:
	    s = "HMP";
	    break;
#endif	/* defined(IPPROTO_HMP) */

#if	defined(IPPROTO_PRM)
	case IPPROTO_PRM:
	    s = "PRM";
	    break;
#endif	/* defined(IPPROTO_PRM) */

#if	defined(IPPROTO_TRUNK1)
	case IPPROTO_TRUNK1:
	    s = "TRUNK1";
	    break;
#endif	/* defined(IPPROTO_TRUNK1) */

#if	defined(IPPROTO_TRUNK2)
	case IPPROTO_TRUNK2:
	    s = "TRUNK2";
	    break;
#endif	/* defined(IPPROTO_TRUNK2) */

#if	defined(IPPROTO_LEAF1)
	case IPPROTO_LEAF1:
	    s = "LEAF1";
	    break;
#endif	/* defined(IPPROTO_LEAF1) */

#if	defined(IPPROTO_LEAF2)
	case IPPROTO_LEAF2:
	    s = "LEAF2";
	    break;
#endif	/* defined(IPPROTO_LEAF2) */

#if	defined(IPPROTO_RDP)
	case IPPROTO_RDP:
	    s = "RDP";
	    break;
#endif	/* defined(IPPROTO_RDP) */

#if	defined(IPPROTO_IRTP)
	case IPPROTO_IRTP:
	    s = "IRTP";
	    break;
#endif	/* defined(IPPROTO_IRTP) */

#if	defined(IPPROTO_TP)
	case IPPROTO_TP:
	    s = "TP";
	    break;
#endif	/* defined(IPPROTO_TP) */

#if	defined(IPPROTO_BLT)
	case IPPROTO_BLT:
	    s = "BLT";
	    break;
#endif	/* defined(IPPROTO_BLT) */

#if	defined(IPPROTO_NSP)
	case IPPROTO_NSP:
	    s = "NSP";
	    break;
#endif	/* defined(IPPROTO_NSP) */

#if	defined(IPPROTO_INP)
	case IPPROTO_INP:
	    s = "INP";
	    break;
#endif	/* defined(IPPROTO_INP) */

#if	defined(IPPROTO_SEP)
	case IPPROTO_SEP:
	    s = "SEP";
	    break;
#endif	/* defined(IPPROTO_SEP) */

#if	defined(IPPROTO_3PC)
	case IPPROTO_3PC:
	    s = "3PC";
	    break;
#endif	/* defined(IPPROTO_3PC) */

#if	defined(IPPROTO_IDPR)
	case IPPROTO_IDPR:
	    s = "IDPR";
	    break;
#endif	/* defined(IPPROTO_IDPR) */

#if	defined(IPPROTO_XTP)
	case IPPROTO_XTP:
	    s = "XTP";
	    break;
#endif	/* defined(IPPROTO_XTP) */

#if	defined(IPPROTO_DDP)
	case IPPROTO_DDP:
	    s = "DDP";
	    break;
#endif	/* defined(IPPROTO_DDP) */

#if	defined(IPPROTO_CMTP)
	case IPPROTO_CMTP:
	    s = "CMTP";
	    break;
#endif	/* defined(IPPROTO_CMTP) */

#if	defined(IPPROTO_TPXX)
	case IPPROTO_TPXX:
	    s = "TPXX";
	    break;
#endif	/* defined(IPPROTO_TPXX) */

#if	defined(IPPROTO_IL)
	case IPPROTO_IL:
	    s = "IL";
	    break;
#endif	/* defined(IPPROTO_IL) */

#if	defined(IPPROTO_IPV6)
	case IPPROTO_IPV6:
	    s = "IPV6";
	    break;
#endif	/* defined(IPPROTO_IPV6) */

#if	defined(IPPROTO_SDRP)
	case IPPROTO_SDRP:
	    s = "SDRP";
	    break;
#endif	/* defined(IPPROTO_SDRP) */

#if	defined(IPPROTO_ROUTING)
	case IPPROTO_ROUTING:
	    s = "ROUTING";
	    break;
#endif	/* defined(IPPROTO_ROUTING) */

#if	defined(IPPROTO_FRAGMENT)
	case IPPROTO_FRAGMENT:
	    s = "FRAGMNT";
	    break;
#endif	/* defined(IPPROTO_FRAGMENT) */

#if	defined(IPPROTO_IDRP)
	case IPPROTO_IDRP:
	    s = "IDRP";
	    break;
#endif	/* defined(IPPROTO_IDRP) */

#if	defined(IPPROTO_RSVP)
	case IPPROTO_RSVP:
	    s = "RSVP";
	    break;
#endif	/* defined(IPPROTO_RSVP) */

#if	defined(IPPROTO_GRE)
	case IPPROTO_GRE:
	    s = "GRE";
	    break;
#endif	/* defined(IPPROTO_GRE) */

#if	defined(IPPROTO_MHRP)
	case IPPROTO_MHRP:
	    s = "MHRP";
	    break;
#endif	/* defined(IPPROTO_MHRP) */

#if	defined(IPPROTO_BHA)
	case IPPROTO_BHA:
	    s = "BHA";
	    break;
#endif	/* defined(IPPROTO_BHA) */

#if	defined(IPPROTO_ESP)
	case IPPROTO_ESP:
	    s = "ESP";
	    break;
#endif	/* defined(IPPROTO_ESP) */

#if	defined(IPPROTO_AH)
	case IPPROTO_AH:
	    s = "AH";
	    break;
#endif	/* defined(IPPROTO_AH) */

#if	defined(IPPROTO_INLSP)
	case IPPROTO_INLSP:
	    s = "INLSP";
	    break;
#endif	/* defined(IPPROTO_INLSP) */

#if	defined(IPPROTO_SWIPE)
	case IPPROTO_SWIPE:
	    s = "SWIPE";
	    break;
#endif	/* defined(IPPROTO_SWIPE) */

#if	defined(IPPROTO_NHRP)
	case IPPROTO_NHRP:
	    s = "NHRP";
	    break;
#endif	/* defined(IPPROTO_NHRP) */

#if	defined(IPPROTO_NONE)
	case IPPROTO_NONE:
	    s = "NONE";
	    break;
#endif	/* defined(IPPROTO_NONE) */

#if	defined(IPPROTO_DSTOPTS)
	case IPPROTO_DSTOPTS:
	    s = "DSTOPTS";
	    break;
#endif	/* defined(IPPROTO_DSTOPTS) */

#if	defined(IPPROTO_AHIP)
	case IPPROTO_AHIP:
	    s = "AHIP";
	    break;
#endif	/* defined(IPPROTO_AHIP) */

#if	defined(IPPROTO_CFTP)
	case IPPROTO_CFTP:
	    s = "CFTP";
	    break;
#endif	/* defined(IPPROTO_CFTP) */

#if	defined(IPPROTO_SATEXPAK)
	case IPPROTO_SATEXPAK:
	    s = "SATEXPK";
	    break;
#endif	/* defined(IPPROTO_SATEXPAK) */

#if	defined(IPPROTO_KRYPTOLAN)
	case IPPROTO_KRYPTOLAN:
	    s = "KRYPTOL";
	    break;
#endif	/* defined(IPPROTO_KRYPTOLAN) */

#if	defined(IPPROTO_RVD)
	case IPPROTO_RVD:
	    s = "RVD";
	    break;
#endif	/* defined(IPPROTO_RVD) */

#if	defined(IPPROTO_IPPC)
	case IPPROTO_IPPC:
	    s = "IPPC";
	    break;
#endif	/* defined(IPPROTO_IPPC) */

#if	defined(IPPROTO_ADFS)
	case IPPROTO_ADFS:
	    s = "ADFS";
	    break;
#endif	/* defined(IPPROTO_ADFS) */

#if	defined(IPPROTO_SATMON)
	case IPPROTO_SATMON:
	    s = "SATMON";
	    break;
#endif	/* defined(IPPROTO_SATMON) */

#if	defined(IPPROTO_VISA)
	case IPPROTO_VISA:
	    s = "VISA";
	    break;
#endif	/* defined(IPPROTO_VISA) */

#if	defined(IPPROTO_IPCV)
	case IPPROTO_IPCV:
	    s = "IPCV";
	    break;
#endif	/* defined(IPPROTO_IPCV) */

#if	defined(IPPROTO_CPNX)
	case IPPROTO_CPNX:
	    s = "CPNX";
	    break;
#endif	/* defined(IPPROTO_CPNX) */

#if	defined(IPPROTO_CPHB)
	case IPPROTO_CPHB:
	    s = "CPHB";
	    break;
#endif	/* defined(IPPROTO_CPHB) */

#if	defined(IPPROTO_WSN)
	case IPPROTO_WSN:
	    s = "WSN";
	    break;
#endif	/* defined(IPPROTO_WSN) */

#if	defined(IPPROTO_PVP)
	case IPPROTO_PVP:
	    s = "PVP";
	    break;
#endif	/* defined(IPPROTO_PVP) */

#if	defined(IPPROTO_BRSATMON)
	case IPPROTO_BRSATMON:
	    s = "BRSATMN";
	    break;
#endif	/* defined(IPPROTO_BRSATMON) */

#if	defined(IPPROTO_WBMON)
	case IPPROTO_WBMON:
	    s = "WBMON";
	    break;
#endif	/* defined(IPPROTO_WBMON) */

#if	defined(IPPROTO_WBEXPAK)
	case IPPROTO_WBEXPAK:
	    s = "WBEXPAK";
	    break;
#endif	/* defined(IPPROTO_WBEXPAK) */

#if	defined(IPPROTO_EON)
	case IPPROTO_EON:
	    s = "EON";
	    break;
#endif	/* defined(IPPROTO_EON) */

#if	defined(IPPROTO_VMTP)
	case IPPROTO_VMTP:
	    s = "VMTP";
	    break;
#endif	/* defined(IPPROTO_VMTP) */

#if	defined(IPPROTO_SVMTP)
	case IPPROTO_SVMTP:
	    s = "SVMTP";
	    break;
#endif	/* defined(IPPROTO_SVMTP) */

#if	defined(IPPROTO_VINES)
	case IPPROTO_VINES:
	    s = "VINES";
	    break;
#endif	/* defined(IPPROTO_VINES) */

#if	defined(IPPROTO_TTP)
	case IPPROTO_TTP:
	    s = "TTP";
	    break;
#endif	/* defined(IPPROTO_TTP) */

#if	defined(IPPROTO_IGP)
	case IPPROTO_IGP:
	    s = "IGP";
	    break;
#endif	/* defined(IPPROTO_IGP) */

#if	defined(IPPROTO_DGP)
	case IPPROTO_DGP:
	    s = "DGP";
	    break;
#endif	/* defined(IPPROTO_DGP) */

#if	defined(IPPROTO_TCF)
	case IPPROTO_TCF:
	    s = "TCF";
	    break;
#endif	/* defined(IPPROTO_TCF) */

#if	defined(IPPROTO_IGRP)
	case IPPROTO_IGRP:
	    s = "IGRP";
	    break;
#endif	/* defined(IPPROTO_IGRP) */

#if	defined(IPPROTO_OSPFIGP)
	case IPPROTO_OSPFIGP:
	    s = "OSPFIGP";
	    break;
#endif	/* defined(IPPROTO_OSPFIGP) */

#if	defined(IPPROTO_SRPC)
	case IPPROTO_SRPC:
	    s = "SRPC";
	    break;
#endif	/* defined(IPPROTO_SRPC) */

#if	defined(IPPROTO_LARP)
	case IPPROTO_LARP:
	    s = "LARP";
	    break;
#endif	/* defined(IPPROTO_LARP) */

#if	defined(IPPROTO_MTP)
	case IPPROTO_MTP:
	    s = "MTP";
	    break;
#endif	/* defined(IPPROTO_MTP) */

#if	defined(IPPROTO_AX25)
	case IPPROTO_AX25:
	    s = "AX25";
	    break;
#endif	/* defined(IPPROTO_AX25) */

#if	defined(IPPROTO_IPEIP)
	case IPPROTO_IPEIP:
	    s = "IPEIP";
	    break;
#endif	/* defined(IPPROTO_IPEIP) */

#if	defined(IPPROTO_MICP)
	case IPPROTO_MICP:
	    s = "MICP";
	    break;
#endif	/* defined(IPPROTO_MICP) */

#if	defined(IPPROTO_SCCSP)
	case IPPROTO_SCCSP:
	    s = "SCCSP";
	    break;
#endif	/* defined(IPPROTO_SCCSP) */

#if	defined(IPPROTO_ETHERIP)
	case IPPROTO_ETHERIP:
	    s = "ETHERIP";
	    break;
#endif	/* defined(IPPROTO_ETHERIP) */

#if	defined(IPPROTO_ENCAP)
# if	!defined(IPPROTO_IPIP) || IPPROTO_IPIP!=IPPROTO_ENCAP
	case IPPROTO_ENCAP:
	    s = "ENCAP";
	    break;
# endif	/* !defined(IPPROTO_IPIP) || IPPROTO_IPIP!=IPPROTO_ENCAP */
#endif	/* defined(IPPROTO_ENCAP) */

#if	defined(IPPROTO_APES)
	case IPPROTO_APES:
	    s = "APES";
	    break;
#endif	/* defined(IPPROTO_APES) */

#if	defined(IPPROTO_GMTP)
	case IPPROTO_GMTP:
	    s = "GMTP";
	    break;
#endif	/* defined(IPPROTO_GMTP) */

#if	defined(IPPROTO_DIVERT)
	case IPPROTO_DIVERT:
	    s = "DIVERT";
	    break;
#endif	/* defined(IPPROTO_DIVERT) */

	default:
	    s = (char *)NULL;
	}
	if (s)
	    (void) snpf(Lf->iproto, sizeof(Lf->iproto), "%.*s", IPROTOL-1, s);
	else {	
	    if (m < 0) {
		for (i = 0, m = 1; i < IPROTOL-2; i++)
		    m *= 10;
	    }
	    if (m > p)
		(void) snpf(Lf->iproto, sizeof(Lf->iproto), "%d?", p);
	    else
		(void) snpf(Lf->iproto, sizeof(Lf->iproto), "*%d?", p % (m/10));
	}
}
#endif	/* !defined(HASPRIVPRIPP) */


/*
 * printname() - print output name field
 */

void
printname(nl)
	int nl;				/* NL status */
{

#if	defined(HASNCACHE)
	char buf[MAXPATHLEN];
	char *cp;
	int fp;
#endif	/* defined(HASNCACHE) */

	int ps = 0;

	if (Lf->nm && Lf->nm[0]) {

	/*
	 * Print the name characters, if there are some.
	 */
	    safestrprt(Lf->nm, stdout, 0);
	    ps++;
	    if (!Lf->li[0].af && !Lf->li[1].af)
		goto print_nma;
	}
	if (Lf->li[0].af || Lf->li[1].af) {
	    if (ps)
		putchar(' ');
	/*
	 * If the file has Internet addresses, print them.
	 */
	    if (printinaddr())
		ps++;
	    goto print_nma;
	}
	if (((Lf->ntype == N_BLK) || (Lf->ntype == N_CHR))
	&&  Lf->dev_def && Lf->rdev_def
	&&  printdevname(&Lf->dev, &Lf->rdev, 0, Lf->ntype))
	{

	/*
	 * If this is a block or character device and it has a name, print it.
	 */
	    ps++;
	    goto print_nma;
	}
	if (Lf->is_com) {

	/*
	 * If this is a common node, print that fact.
	 */
	    (void) fputs("COMMON: ", stdout);
	    ps++;
	    goto print_nma;
	}

#if	defined(HASPRIVNMCACHE)
	if (HASPRIVNMCACHE(Lf)) {
	    ps++;
	    goto print_nma;
	}
#endif	/* defined(HASPRIVNMCACHE) */

	if (Lf->lmi_srch) {
	    struct mounts *mp;
	/*
	 * Do a deferred local mount info table search for the file system
	 * (mounted) directory name and inode number, and mounted device name.
	 */
	    for (mp = readmnt(); mp; mp = mp->next) {
		if (Lf->dev == mp->dev) {
		    Lf->fsdir = mp->dir;
		    Lf->fsdev = mp->fsname;

#if	defined(HASFSINO)
		    Lf->fs_ino = mp->inode;
#endif	/* defined(HASFSINO) */

		    break;
		}
	    }
	    Lf->lmi_srch = 0;
	}
	if (Lf->fsdir || Lf->fsdev) {

	/*
	 * Print the file system directory name, device name, and
	 * possible path name components.
	 */

#if	!defined(HASNCACHE) || HASNCACHE<2
	    if (Lf->fsdir) {
		safestrprt(Lf->fsdir, stdout, 0);
		ps++;
	    }
#endif	/* !defined(HASNCACHE) || HASNCACHE<2 */

#if	defined(HASNCACHE)

# if	HASNCACHE<2
	    if (Lf->na) {
		if (NcacheReload) {

#  if	defined(NCACHELDPFX)
		    NCACHELDPFX
#  endif	/* defined(NCACHELDPFX) */

		    (void) ncache_load();

#  if	defined(NCACHELDSFX)
		    NCACHELDSFX
#  endif	/* defined(NCACHELDSFX) */

		    NcacheReload = 0;
		}
		if ((cp = ncache_lookup(buf, sizeof(buf), &fp))) {
		    char *cp1; 

		    if (*cp == '\0')
			goto print_nma;
		    if (fp && Lf->fsdir) {
			if (*cp != '/') {
			    cp1 = strrchr(Lf->fsdir, '/');
			    if (cp1 == (char *)NULL ||  *(cp1 + 1) != '\0')
				putchar('/');
			    }
		    } else
			(void) fputs(" -- ", stdout);
		    safestrprt(cp, stdout, 0);
		    ps++;
		    goto print_nma;
		}
	    }
# else	/* HASNCACHE>1 */
	    if (NcacheReload) {

#  if	defined(NCACHELDPFX)
		    NCACHELDPFX
#  endif	/* defined(NCACHELDPFX) */

		(void) ncache_load();

#  if	defined(NCACHELDSFX)
		    NCACHELDSFX
#  endif	/* defined(NCACHELDSFX) */

		NcacheReload = 0;
	    }
	    if ((cp = ncache_lookup(buf, sizeof(buf), &fp))) {
		if (fp) {
		    safestrprt(cp, stdout, 0);
		    ps++;
		} else {
		    if (Lf->fsdir) {
			safestrprt(Lf->fsdir, stdout, 0);
			ps++;
		    }
		    if (*cp) {
			(void) fputs(" -- ", stdout);
			safestrprt(cp, stdout, 0);
			ps++;
		    }
		}
		goto print_nma;
	    }
	    if (Lf->fsdir) {
		safestrprt(Lf->fsdir, stdout, 0);
		ps++;
	    }
# endif	/* HASNCACHE<2 */
#endif	/* defined(HASNCACHE) */

	    if (Lf->fsdev) {
		if (Lf->fsdir)
		    (void) fputs(" (", stdout);
		else
		    (void) putchar('(');
		safestrprt(Lf->fsdev, stdout, 0);
		(void) putchar(')');
		ps++;
	    }
	}
/*
 * Print the NAME column addition, if there is one.  If there isn't
 * make sure a NL is printed, as requested.
 */

print_nma:

	if (Lf->nma) {
	    if (ps)
		putchar(' ');
	    safestrprt(Lf->nma, stdout, 0);
	    ps++;
	}
/*
 * If this file has TCP/IP state information, print it.
 */
	if (!Ffield && Ftcptpi
	&&  (Lf->lts.type >= 0

#if	defined(HASTCPTPIQ)
	||   ((Ftcptpi & TCPTPI_QUEUES) && (Lf->lts.rqs || Lf->lts.sqs))
#endif	/* defined(HASTCPTPIQ) */

#if	defined(HASTCPTPIW)
	||   ((Ftcptpi & TCPTPI_WINDOWS) && (Lf->lts.rws || Lf->lts.wws))
#endif	/* defined(HASTCPTPIW) */

	)) {
	    if (ps)
		putchar(' ');
	    (void) print_tcptpi(1);
	    return;
	}
	if (nl)
	    putchar('\n');
}


/*
 * printrawaddr() - print raw socket address
 */

void
printrawaddr(sa)
	struct sockaddr *sa;		/* socket address */
{
	char *ep;
	size_t sz;

	ep = endnm(&sz);
	(void) snpf(ep, sz, "%u/%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u",
	    sa->sa_family,
	    (unsigned char)sa->sa_data[0],
	    (unsigned char)sa->sa_data[1],
	    (unsigned char)sa->sa_data[2],
	    (unsigned char)sa->sa_data[3],
	    (unsigned char)sa->sa_data[4],
	    (unsigned char)sa->sa_data[5],
	    (unsigned char)sa->sa_data[6],
	    (unsigned char)sa->sa_data[7],
	    (unsigned char)sa->sa_data[8],
	    (unsigned char)sa->sa_data[9],
	    (unsigned char)sa->sa_data[10],
	    (unsigned char)sa->sa_data[11],
	    (unsigned char)sa->sa_data[12],
	    (unsigned char)sa->sa_data[13]);
}


/*
 * printsockty() - print socket type
 */

char *
printsockty(ty)
	int ty;				/* socket type -- e.g., from so_type */
{
	static char buf[64];
	char *cp;

	switch (ty) {

#if	defined(SOCK_STREAM)
	case SOCK_STREAM:
	    cp = "STREAM";
	    break;
#endif	/* defined(SOCK_STREAM) */

#if	defined(SOCK_STREAM)
	case SOCK_DGRAM:
	    cp = "DGRAM";
	    break;
#endif	/* defined(SOCK_DGRAM) */

#if	defined(SOCK_RAW)
	case SOCK_RAW:
	    cp = "RAW";
	    break;
#endif	/* defined(SOCK_RAW) */

#if	defined(SOCK_RDM)
	case SOCK_RDM:
	    cp = "RDM";
	    break;
#endif	/* defined(SOCK_RDM) */

#if	defined(SOCK_SEQPACKET)
	case SOCK_SEQPACKET:
	    cp = "SEQPACKET";
	    break;
#endif	/* defined(SOCK_SEQPACKET) */

	default:
	    (void) snpf(buf, sizeof(buf), "SOCK_%#x", ty);
	    return(buf);
	}
	(void) snpf(buf, sizeof(buf), "SOCK_%s", cp);
	return(buf);
}


/*
 * printuid() - print User ID or login name
 */

char *
printuid(uid, ty)
	UID_ARG uid;			/* User IDentification number */
	int *ty;			/* returned UID type pointer (NULL
					 * (if none wanted).  If non-NULL
					 * then: *ty = 0 = login name
					 *	     = 1 = UID number */
{
	int i;
	struct passwd *pw;
	struct stat sb;
	static struct stat sbs;
	static struct uidcache {
	    uid_t uid;
	    char nm[LOGINML+1];
	    struct uidcache *next;
	} **uc = (struct uidcache **)NULL;
	struct uidcache *up, *upn;
	static char user[USERPRTL+1];

	if (Futol) {
	    if (CkPasswd) {

	    /*
	     * Get the mtime and ctime of /etc/passwd, as required.
	     */
		if (stat("/etc/passwd", &sb) != 0) {
		    (void) fprintf(stderr, "%s: can't stat(/etc/passwd): %s\n",
			Pn, strerror(errno));
		    Exit(1);
		}
	    }
	/*
	 * Define the UID cache, if necessary.
	 */
	    if (!uc) {
		if (!(uc = (struct uidcache **)calloc(UIDCACHEL,
						sizeof(struct uidcache *))))
		{
		    (void) fprintf(stderr,
			"%s: no space for %d byte UID cache hash buckets\n",
			Pn, (int)(UIDCACHEL * (sizeof(struct uidcache *))));
		    Exit(1);
		}
		if (CkPasswd) {
		    sbs = sb;
		    CkPasswd = 0;
		}
	    }
	/*
	 * If it's time to check /etc/passwd and if its the mtime/ctime has
	 * changed, destroy the existing UID cache.
	 */
	    if (CkPasswd) {
		if (sbs.st_mtime != sb.st_mtime || sbs.st_ctime != sb.st_ctime)
		{
		    for (i = 0; i < UIDCACHEL; i++) {
			if ((up = uc[i])) {
			    do {
				upn = up->next;
				(void) free((FREE_P *)up);
			    } while ((up = upn) != (struct uidcache *)NULL);
			    uc[i] = (struct uidcache *)NULL;
			}
		    }
		    sbs = sb;
		}
		CkPasswd = 0;
	    }
	/*
	 * Search the UID cache.
	 */
	    i = (int)((((unsigned long)uid * 31415L) >> 7) & (UIDCACHEL - 1));
	    for (up = uc[i]; up; up = up->next) {
		if (up->uid == (uid_t)uid) {
		    if (ty)
			*ty = 0;
		    return(up->nm);
		}
	    }
	/*
	 * The UID is not in the cache.
	 *
	 * Look up the login name from the UID for a new cache entry.
	 */
	    if (!(pw = getpwuid((uid_t)uid))) {
		if (!Fwarn) {
		    (void) fprintf(stderr, "%s: no pwd entry for UID %lu\n",
			Pn, (unsigned long)uid);
		}
	    } else {

	    /*
	     * Allocate and fill a new cache entry.  Link it to its hash bucket.
	     */
		if (!(upn = (struct uidcache *)malloc(sizeof(struct uidcache))))
		{
		    (void) fprintf(stderr,
			"%s: no space for UID cache entry for: %lu, %s)\n",
			Pn, (unsigned long)uid, pw->pw_name);
		    Exit(1);
		}
		(void) strncpy(upn->nm, pw->pw_name, LOGINML);
		upn->nm[LOGINML] = '\0';
		upn->uid = (uid_t)uid;
		upn->next = uc[i];
		uc[i] = upn;
		if (ty)
		    *ty = 0;
		return(upn->nm);
	    }
	}
/*
 * Produce a numeric conversion of the UID.
 */
	(void) snpf(user, sizeof(user), "%*lu", USERPRTL, (unsigned long)uid);
	if (ty)
	    *ty = 1;
	return(user);
}


/*
 * printunkaf() - print unknown address family
 */

void
printunkaf(fam, ty)
	int fam;			/* unknown address family */
	int ty;				/* output type: 0 = terse; 1 = full */
{
	char *p, *s;

	p = "";
	switch (fam) {

#if	defined(AF_UNSPEC)
	case AF_UNSPEC:
	    s = "UNSPEC";
	    break;
#endif	/* defined(AF_UNSPEC) */

#if	defined(AF_UNIX)
	case AF_UNIX:
	    s = "UNIX";
	    break;
#endif	/* defined(AF_UNIX) */

#if	defined(AF_INET)
	case AF_INET:
	    s = "INET";
	    break;
#endif	/* defined(AF_INET) */

#if	defined(AF_INET6)
	case AF_INET6:
	    s = "INET6";
	    break;
#endif	/* defined(AF_INET6) */

#if	defined(AF_IMPLINK)
	case AF_IMPLINK:
	    s = "IMPLINK";
	    break;
#endif	/* defined(AF_IMPLINK) */

#if	defined(AF_PUP)
	case AF_PUP:
	    s = "PUP";
	    break;
#endif	/* defined(AF_PUP) */

#if	defined(AF_CHAOS)
	case AF_CHAOS:
	    s = "CHAOS";
	    break;
#endif	/* defined(AF_CHAOS) */

#if	defined(AF_NS)
	case AF_NS:
	    s = "NS";
	    break;
#endif	/* defined(AF_NS) */

#if	defined(AF_ISO)
	case AF_ISO:
	    s = "ISO";
	    break;
#endif	/* defined(AF_ISO) */

#if	defined(AF_NBS)
# if	!defined(AF_ISO) || AF_NBS!=AF_ISO
	case AF_NBS:
	    s = "NBS";
	    break;
# endif	/* !defined(AF_ISO) || AF_NBS!=AF_ISO */
#endif	/* defined(AF_NBS) */

#if	defined(AF_ECMA)
	case AF_ECMA:
	    s = "ECMA";
	    break;
#endif	/* defined(AF_ECMA) */

#if	defined(AF_DATAKIT)
	case AF_DATAKIT:
	    s = "DATAKIT";
	    break;
#endif	/* defined(AF_DATAKIT) */

#if	defined(AF_CCITT)
	case AF_CCITT:
	    s = "CCITT";
	    break;
#endif	/* defined(AF_CCITT) */

#if	defined(AF_SNA)
	case AF_SNA:
	    s = "SNA";
	    break;
#endif	/* defined(AF_SNA) */

#if	defined(AF_DECnet)
	case AF_DECnet:
	    s = "DECnet";
	    break;
#endif	/* defined(AF_DECnet) */

#if	defined(AF_DLI)
	case AF_DLI:
	    s = "DLI";
	    break;
#endif	/* defined(AF_DLI) */

#if	defined(AF_LAT)
	case AF_LAT:
	    s = "LAT";
	    break;
#endif	/* defined(AF_LAT) */

#if	defined(AF_HYLINK)
	case AF_HYLINK:
	    s = "HYLINK";
	    break;
#endif	/* defined(AF_HYLINK) */

#if	defined(AF_APPLETALK)
	case AF_APPLETALK:
	    s = "APPLETALK";
	    break;
#endif	/* defined(AF_APPLETALK) */

#if	defined(AF_BSC)
	case AF_BSC:
	    s = "BSC";
	    break;
#endif	/* defined(AF_BSC) */

#if	defined(AF_DSS)
	case AF_DSS:
	    s = "DSS";
	    break;
#endif	/* defined(AF_DSS) */

#if	defined(AF_ROUTE)
	case AF_ROUTE:
	    s = "ROUTE";
	    break;
#endif	/* defined(AF_ROUTE) */

#if	defined(AF_RAW)
	case AF_RAW:
	    s = "RAW";
	    break;
#endif	/* defined(AF_RAW) */

#if	defined(AF_LINK)
	case AF_LINK:
	    s = "LINK";
	    break;
#endif	/* defined(AF_LINK) */

#if	defined(pseudo_AF_XTP)
	case pseudo_AF_XTP:
	    p = "pseudo_";
	    s = "XTP";
	    break;
#endif	/* defined(pseudo_AF_XTP) */

#if	defined(AF_RMP)
	case AF_RMP:
	    s = "RMP";
	    break;
#endif	/* defined(AF_RMP) */

#if	defined(AF_COIP)
	case AF_COIP:
	    s = "COIP";
	    break;
#endif	/* defined(AF_COIP) */

#if	defined(AF_CNT)
	case AF_CNT:
	    s = "CNT";
	    break;
#endif	/* defined(AF_CNT) */

#if	defined(pseudo_AF_RTIP)
	case pseudo_AF_RTIP:
	    p = "pseudo_";
	    s = "RTIP";
	    break;
#endif	/* defined(pseudo_AF_RTIP) */

#if	defined(AF_NETMAN)
	case AF_NETMAN:
	    s = "NETMAN";
	    break;
#endif	/* defined(AF_NETMAN) */

#if	defined(AF_INTF)
	case AF_INTF:
	    s = "INTF";
	    break;
#endif	/* defined(AF_INTF) */

#if	defined(AF_NETWARE)
	case AF_NETWARE:
	    s = "NETWARE";
	    break;
#endif	/* defined(AF_NETWARE) */

#if	defined(AF_NDD)
	case AF_NDD:
	    s = "NDD";
	    break;
#endif	/* defined(AF_NDD) */

#if	defined(AF_NIT)
# if	!defined(AF_ROUTE) || AF_ROUTE!=AF_NIT
	case AF_NIT:
	    s = "NIT";
	    break;
# endif	/* !defined(AF_ROUTE) || AF_ROUTE!=AF_NIT */
#endif	/* defined(AF_NIT) */

#if	defined(AF_802)
# if	!defined(AF_RAW) || AF_RAW!=AF_802
	case AF_802:
	    s = "802";
	    break;
# endif	/* !defined(AF_RAW) || AF_RAW!=AF_802 */
#endif	/* defined(AF_802) */

#if	defined(AF_X25)
	case AF_X25:
	    s = "X25";
	    break;
#endif	/* defined(AF_X25) */

#if	defined(AF_CTF)
	case AF_CTF:
	    s = "CTF";
	    break;
#endif	/* defined(AF_CTF) */

#if	defined(AF_WAN)
	case AF_WAN:
	    s = "WAN";
	    break;
#endif	/* defined(AF_WAN) */

#if	defined(AF_OSINET)
# if	defined(AF_INET) && AF_INET!=AF_OSINET
	case AF_OSINET:
	    s = "OSINET";
	    break;
# endif	/* defined(AF_INET) && AF_INET!=AF_OSINET */
#endif	/* defined(AF_OSINET) */

#if	defined(AF_GOSIP)
	case AF_GOSIP:
	    s = "GOSIP";
	    break;
#endif	/* defined(AF_GOSIP) */

#if	defined(AF_SDL)
	case AF_SDL:
	    s = "SDL";
	    break;
#endif	/* defined(AF_SDL) */

#if	defined(AF_IPX)
	case AF_IPX:
	    s = "IPX";
	    break;
#endif	/* defined(AF_IPX) */

#if	defined(AF_SIP)
	case AF_SIP:
	    s = "SIP";
	    break;
#endif	/* defined(AF_SIP) */

#if	defined(psuedo_AF_PIP)
	case psuedo_AF_PIP:
	    p = "pseudo_";
	    s = "PIP";
	    break;
#endif	/* defined(psuedo_AF_PIP) */

#if	defined(AF_OTS)
	case AF_OTS:
	    s = "OTS";
	    break;
#endif	/* defined(AF_OTS) */

#if	defined(pseudo_AF_BLUE)
	case pseudo_AF_BLUE:	/* packets for Blue box */
	    p = "pseudo_";
	    s = "BLUE";
	    break;
#endif	/* defined(pseudo_AF_BLUE) */

#if	defined(AF_NDRV)	/* network driver raw access */
	case AF_NDRV:
	    s = "NDRV";
	    break;
#endif	/* defined(AF_NDRV) */

#if	defined(AF_SYSTEM)	/* kernel event messages */
	case AF_SYSTEM:
	    s = "SYSTEM";
	    break;
#endif	/* defined(AF_SYSTEM) */

#if	defined(AF_USER)
	case AF_USER:
	    s = "USER";
	    break;
#endif	/* defined(AF_USER) */

#if	defined(pseudo_AF_KEY)
	case pseudo_AF_KEY:
	    p = "pseudo_";
	    s = "KEY";
	    break;
#endif	/* defined(pseudo_AF_KEY) */

#if	defined(AF_KEY)		/* Security Association DB socket */
	case AF_KEY:			
	    s = "KEY";
	    break;
#endif	/* defined(AF_KEY) */

#if	defined(AF_NCA)		/* NCA socket */
	case AF_NCA:			
	    s = "NCA";
	    break;
#endif	/* defined(AF_NCA) */

#if	defined(AF_POLICY)		/* Security Policy DB socket */
	case AF_POLICY:
	    s = "POLICY";
	    break;
#endif	/* defined(AF_POLICY) */

#if	defined(AF_PPP)		/* PPP socket */
	case AF_PPP:			
	    s = "PPP";
	    break;
#endif	/* defined(AF_PPP) */

	default:
	    if (!ty)
		(void) snpf(Namech, Namechl, "%#x", fam);
	    else
		(void) snpf(Namech, Namechl,
		    "no further information on family %#x", fam);
	    return;
	}
	if (!ty)
	    (void) snpf(Namech, Namechl, "%sAF_%s", p, s);
	else
	    (void) snpf(Namech, Namechl, "no further information on %sAF_%s",
		p, s);
	return;
}


/*
 * update_portmap() - update a portmap entry with its port number or service
 *		      name
 */

static void
update_portmap(pt, pn)
	struct porttab *pt;		/* porttab entry */
	char *pn;			/* port name */
{
	MALLOC_S al, nl;
	char *cp;

	if (pt->ss)
	    return;
	if (!(al = strlen(pn))) {
	    pt->ss = 1;
	    return;
	}
	nl = al + pt->nl + 2;
	if (!(cp = (char *)malloc(nl + 1))) {
	    (void) fprintf(stderr,
		"%s: can't allocate %d bytes for portmap name: %s[%s]\n",
		Pn, (int)(nl + 1), pn, pt->name);
	    Exit(1);
	}
	(void) snpf(cp, nl + 1, "%s[%s]", pn, pt->name);
	(void) free((FREE_P *)pt->name);
	pt->name = cp;
	pt->nl = nl;
	pt->ss = 1;
}
