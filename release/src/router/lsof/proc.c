/*
 * proc.c - common process and file structure functions for lsof
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
static char *rcsid = "$Id: proc.c,v 1.45 2009/03/25 19:20:44 abe Exp $";
#endif


#include "lsof.h"


/*
 * Local function prototypes
 */

_PROTOTYPE(static int is_file_sel,(struct lproc *lp, struct lfile *lf));


/*
 * add_nma() - add to NAME column addition
 */

void
add_nma(cp, len)
	char *cp;			/* string to add */
	int len;			/* string length */
{
	int nl;

	if (!cp || !len)
	    return;
	if (Lf->nma) {
	    nl = (int)strlen(Lf->nma);
	    Lf->nma = (char *)realloc((MALLOC_P *)Lf->nma,
				      (MALLOC_S)(len + nl + 2));
	} else {
	    nl = 0;
	    Lf->nma = (char *)malloc((MALLOC_S)(len + 1));
	}
	if (!Lf->nma) {
	    (void) fprintf(stderr, "%s: no name addition space: PID %ld, FD %s",
		Pn, (long)Lp->pid, Lf->fd);
	    Exit(1);
	}
	if (nl) {
	    Lf->nma[nl] = ' ';
	    (void) strncpy(&Lf->nma[nl + 1], cp, len);
	    Lf->nma[nl + 1 + len] = '\0';
	} else {
	    (void) strncpy(Lf->nma, cp, len);
	    Lf->nma[len] = '\0';
	}
}


#if	defined(HASFSTRUCT)
_PROTOTYPE(static char *alloc_fflbuf,(char **bp, int *al, int lr));


/*
 * alloc_fflbuf() - allocate file flags print buffer
 */

static char *
alloc_fflbuf(bp, al, lr)
	char **bp;			/* current buffer pointer */
	int *al;			/* current allocated length */
	int lr;				/* length required */
{
	int sz;

	sz = (int)(lr + 1);		/* allocate '\0' space */
	if (*bp && (sz <= *al))
	    return(*bp);
	if (*bp)
	    *bp = (char *)realloc((MALLOC_P *)*bp, (MALLOC_S)sz);
	else
	    *bp = (char *)malloc((MALLOC_S)sz);
	if (!*bp) {
	    (void) fprintf(stderr, "%s: no space (%d) for print flags\n",
		Pn, sz);
	    Exit(1);
	}
	*al = sz;
	return(*bp);
}
#endif	/* defined(HASFSTRUCT) */


/*
 * alloc_lfile() - allocate local file structure space
 */

void
alloc_lfile(nm, num)
	char *nm;			/* file descriptor name (may be NULL) */
	int num;			/* file descriptor number -- -1 if
					 * none */
{
	int fds;

	if (Lf) {
/*
 * If reusing a previously allocated structure, release any allocated
 * space it was using.
 */
	    if (Lf->dev_ch)
		(void) free((FREE_P *)Lf->dev_ch);
	    if (Lf->nm)
		(void) free((FREE_P *)Lf->nm);
	    if (Lf->nma)
		(void) free((FREE_P *)Lf->nma);

#if	defined(HASLFILEADD) && defined(CLRLFILEADD)
	    CLRLFILEADD(Lf)
#endif	/* defined(HASLFILEADD) && defined(CLRLFILEADD) */

/*
 * Othwerise, allocate a new structure.
 */
	} else if (!(Lf = (struct lfile *)malloc(sizeof(struct lfile)))) {
	    (void) fprintf(stderr, "%s: no local file space at PID %d\n",
		Pn, Lp->pid);
	    Exit(1);
	}
/*
 * Initialize the structure.
 */
	Lf->access = Lf->lock = ' ';
	Lf->dev_def = Lf->inp_ty = Lf->is_com = Lf->is_nfs = Lf->is_stream
		    = Lf->lmi_srch = Lf->nlink_def = Lf->off_def = Lf->sz_def
		    = Lf->rdev_def
		    = (unsigned char)0;
	Lf->li[0].af = Lf->li[1].af = 0;
	Lf->lts.type = -1;
	Lf->nlink = 0l;

#if	defined(HASMNTSTAT)
	Lf->mnt_stat = (unsigned char)0;
#endif	/* defined(HASMNTSTAT) */

#if	defined(HASSOOPT)
	Lf->lts.kai = Lf->lts.ltm = 0;
	Lf->lts.opt = Lf->lts.qlen = Lf->lts.qlim = Lf->lts.pqlen
		    = (unsigned int)0;
	Lf->lts.rbsz = Lf->lts.sbsz = (unsigned long)0;
	Lf->lts.qlens = Lf->lts.qlims = Lf->lts.pqlens = Lf->lts.rbszs
		      = Lf->lts.sbszs = (unsigned char)0;
#endif	/* defined(HASSOOPT) */

#if	defined(HASSOSTATE)
	Lf->lts.ss = 0;
#endif	/* defined(HASSOSTATE) */

#if	defined(HASTCPOPT)
	Lf->lts.mss = (unsigned long)0;
	Lf->lts.msss = (unsigned char)0;
	Lf->lts.topt = (unsigned int)0;
#endif	/* defined(HASTCPOPT) */

#if	defined(HASTCPTPIQ)
	Lf->lts.rqs = Lf->lts.sqs = (unsigned char)0;
#endif	/* defined(HASTCPTPIQ) */

#if	defined(HASTCPTPIW)
	Lf->lts.rws = Lf->lts.wws = (unsigned char)0;
#endif	/* defined(HASTCPTPIW) */

#if	defined(HASFSINO)
	Lf->fs_ino = 0;
#endif	/* defined(HASFSINO) */

#if	defined(HASVXFS) && defined(HASVXFSDNLC)
	Lf->is_vxfs = 0;
#endif	/* defined(HASVXFS) && defined(HASVXFSDNLC) */

	Lf->inode = (INODETYPE)0;
	Lf->off = (SZOFFTYPE)0;
	if (Lp->pss & PS_PRI)
	    Lf->sf = Lp->sf;
	else
	    Lf->sf = 0;
	Lf->iproto[0] = Lf->type[0] = '\0';
	if (nm) {
	    (void) strncpy(Lf->fd, nm, FDLEN - 1);
	    Lf->fd[FDLEN - 1] = '\0';
	} else if (num >= 0) {
	    if (num < 10000)
		(void) snpf(Lf->fd, sizeof(Lf->fd), "%4d", num);
	    else
		(void) snpf(Lf->fd, sizeof(Lf->fd), "*%03d", num % 1000);
	} else
	    Lf->fd[0] = '\0';
	Lf->dev_ch = Lf->fsdir = Lf->fsdev = Lf->nm = Lf->nma = (char *)NULL;
	Lf->ch = -1;

#if	defined(HASNCACHE) && HASNCACHE<2
	Lf->na = (KA_T)NULL;
#endif	/* defined(HASNCACHE) && HASNCACHE<2 */

	Lf->next = (struct lfile *)NULL;
	Lf->ntype = Ntype = N_REGLR;
	Namech[0] = '\0';

#if	defined(HASFSTRUCT)
	Lf->fct = Lf->ffg = Lf->pof = (long)0;
	Lf->fna = (KA_T)NULL;
	Lf->fsv = (unsigned char)0;
#endif	/* defined(HASFSTRUCT) */

#if	defined(HASLFILEADD) && defined(SETLFILEADD)
/*
 * Do local initializations.
 */
	SETLFILEADD
#endif	/* defined(HASLFILEADD) && defined(SETLFILEADD) */

/*
 * See if the file descriptor has been selected.
 */
	if (!Fdl || (!nm && num < 0))
	    return;
	fds = ck_fd_status(nm, num);
	switch (FdlTy) {
	case 0:			/* inclusion list */
	    if (fds == 2)
		Lf->sf |= SELFD;
	    break;
	case 1:			/* exclusion list */
	    if (fds != 1)
		Lf->sf |= SELFD;
	}
}


/*
 * alloc_lproc() - allocate local proc structure space
 */

void
alloc_lproc(pid, pgid, ppid, uid, cmd, pss, sf)
	int pid;			/* Process ID */
	int pgid;			/* process group ID */
	int ppid;			/* parent process ID */
	UID_ARG uid;			/* User ID */
	char *cmd;			/* command */
	int pss;			/* process select state */
	int sf;				/* process select flags */
{
	static int sz = 0;

	if (!Lproc) {
	    if (!(Lproc = (struct lproc *)malloc(
			  (MALLOC_S)(LPROCINCR * sizeof(struct lproc)))))
	    {
		(void) fprintf(stderr,
		    "%s: no malloc space for %d local proc structures\n",
		    Pn, LPROCINCR);
		Exit(1);
	    }
	    sz = LPROCINCR;
	} else if ((Nlproc + 1) > sz) {
	    sz += LPROCINCR;
	    if (!(Lproc = (struct lproc *)realloc((MALLOC_P *)Lproc,
			  (MALLOC_S)(sz * sizeof(struct lproc)))))
	    {
		(void) fprintf(stderr,
		    "%s: no realloc space for %d local proc structures\n",
		    Pn, sz);
		Exit(1);
	    }
	}
	Lp = &Lproc[Nlproc++];
	Lp->pid = pid;
	Lp->pgid = pgid;
	Lp->ppid = ppid;
	Lp->file = (struct lfile *)NULL;
	Lp->sf = (short)sf;
	Lp->pss = (short)pss;
	Lp->uid = (uid_t)uid;
/*
 * Allocate space for the full command name and copy it there.
 */
	if (!(Lp->cmd = mkstrcpy(cmd, (MALLOC_S *)NULL))) {
	    (void) fprintf(stderr, "%s: PID %d, no space for command name: ",
		Pn, pid);
	    safestrprt(cmd, stderr, 1);
	    Exit(1);
	}

#if	defined(HASZONES)
/*
 * Clear the zone name pointer.  The dialect's own code will set it.
 */
	Lp->zn = (char *)NULL;
#endif	/* defined(HASZONES) */
 
#if	defined(HASSELINUX)
/*
 * Clear the security context pointer.  The dialect's own code will
 * set it.
 */
	Lp->cntx = (char *)NULL;
#endif	/* defined(HASSELINUX) */

}


/*
 * ck_fd_status() - check FD status
 *
 * return: 0 == FD is neither included nor excluded
 *	   1 == FD is excluded
 *	   2 == FD is included
 */

extern int
ck_fd_status(nm, num)
	char *nm;			/* file descriptor name (may be NULL) */
	int num;			/* file descriptor number -- -1 if
					 * none */
{
	char *cp;
	struct fd_lst *fp;

	if (!(fp = Fdl) || (!nm && num < 0))
	    return(0);
	if ((cp = nm)) {
	    while (*cp && *cp == ' ')
		cp++;
	}
/*
 * Check for an exclusion match.
 */
	if (FdlTy == 1) {
	    for (; fp; fp = fp->next) {
		if (cp) {
		    if (fp->nm && strcmp(fp->nm, cp) == 0)
			return(1);
		    continue;
		}
		if (num >= fp->lo && num <= fp->hi)
		    return(1);
	    }
	    return(0);
	}
/*
 * If Fdl isn't an exclusion list, check for an inclusion match.
 */
	for (; fp; fp = fp->next) {
	    if (cp) {
		if (fp->nm && strcmp(fp->nm, cp) == 0)
		    return(2);
		continue;
	    }
	    if (num >= fp->lo && num <= fp->hi)
		return(2);
	}
	return(0);
}


/*
 * comppid() - compare PIDs
 */

int
comppid(a1, a2)
	COMP_P *a1, *a2;
{
	struct lproc **p1 = (struct lproc **)a1;
	struct lproc **p2 = (struct lproc **)a2;

	if ((*p1)->pid < (*p2)->pid)
	    return(-1);
	if ((*p1)->pid > (*p2)->pid)
	    return(1);
	return(0);
}


/*
 * ent_inaddr() - enter Internet addresses
 */

void
ent_inaddr(la, lp, fa, fp, af)
	unsigned char *la;		/* local Internet address */
	int lp;				/* local port */
	unsigned char *fa;		/* foreign Internet address -- may
					 * be NULL to indicate no foreign
					 * address is known */
	int fp;				/* foreign port */
	int af;				/* address family -- e.g, AF_INET,
					 * AF_INET */
{
	int m;

	if (la) {
	    Lf->li[0].af = af;

#if	defined(HASIPv6)
	    if (af == AF_INET6)
		Lf->li[0].ia.a6 = *(struct in6_addr *)la;
	    else
#endif	/* defined(HASIPv6) */

		Lf->li[0].ia.a4 = *(struct in_addr *)la;
	    Lf->li[0].p = lp;
	} else
	    Lf->li[0].af = 0;
	if (fa) {
	    Lf->li[1].af = af;

#if	defined(HASIPv6)
	    if (af == AF_INET6)
		Lf->li[1].ia.a6 = *(struct in6_addr *)fa;
	    else
#endif	/* defined(HASIPv6) */

		Lf->li[1].ia.a4 = *(struct in_addr *)fa;
	    Lf->li[1].p = fp;
	} else
	    Lf->li[1].af = 0;
/*
 * If network address matching has been selected, check both addresses.
 */
	if ((Selflags & SELNA) && Nwad) {
	    m = (fa && is_nw_addr(fa, fp, af)) ? 1 : 0;
	    m |= (la && is_nw_addr(la, lp, af)) ? 1 : 0;
	    if (m)
		Lf->sf |= SELNA;
	}
}


/*
 * examine_lproc() - examine local process
 *
 * return: 1 = last process
 */

int
examine_lproc()
{
	int sbp = 0;

	if (RptTm)
	    return(0);
/*
 * List the process if the process is selected and:
 *
 *	o  listing is limited to a single PID selection -- this one;
 *
 *	o  listing is selected by an ANDed option set (not all options)
 *	   that includes a single PID selection -- this one.
 */
	if ((Lp->sf & SELPID) && !Selall) {
	    if ((Selflags == SELPID)
	    ||  (Fand && (Selflags & SELPID))) {
		sbp = 1;
		Npuns--;
	    }
	}
	if (Lp->pss && Npid == 1 && sbp) {
	    print_init();
	    (void) print_proc();
	    PrPass++;
	    if (PrPass < 2)
		(void) print_proc();
	    Lp->pss = 0;
	}
/*
 * Deprecate an unselected (or listed) process.
 */
	if ( ! Lp->pss) {
	    (void) free_lproc(Lp);
	    Nlproc--;
	}
/*
 * Indicate last-process if listing is limited to PID selections,
 * and all selected processes have been listed.
 */
	return((sbp && Npuns == 0) ? 1 : 0);
}


/*
 * free_lproc() - free lproc entry and its associated malloc'd space
 */

void
free_lproc(lp)
	struct lproc *lp;
{
	struct lfile *lf, *nf;

	for (lf = lp->file; lf; lf = nf) {
	    if (lf->dev_ch) {
		(void) free((FREE_P *)lf->dev_ch);
		lf->dev_ch = (char *)NULL;
	    }
	    if (lf->nm) {
		(void) free((FREE_P *)lf->nm);
		lf->nm = (char *)NULL;
	    }
	    if (lf->nma) {
		(void) free((FREE_P *)lf->nma);
		lf->nma = (char *)NULL;
	    }

#if	defined(HASLFILEADD) && defined(CLRLFILEADD)
	    CLRLFILEADD(lf)
#endif	/* defined(HASLFILEADD) && defined(CLRLFILEADD) */

	    nf = lf->next;
	    (void) free((FREE_P *)lf);
	}
	lp->file = (struct lfile *)NULL;
	if (lp->cmd) {
	    (void) free((FREE_P *)lp->cmd);
	    lp->cmd = (char *)NULL;
	}
}


/*
 * is_cmd_excl() - is command excluded?
 */

int
is_cmd_excl(cmd, pss, sf)
	char *cmd;			/* command name */
	short *pss;			/* process state */
	short *sf;			/* process select flags */
{
	int i;
	struct str_lst *sp;
/*
 * See if the command is excluded by a "-c^<command>" option.
 */
	if (Cmdl && Cmdnx) {
	    for (sp = Cmdl; sp; sp = sp->next) {
		if (sp->x && !strncmp(sp->str, cmd, sp->len))
		    return(1);
	    }
	}
/*
 * The command is not excluded if no command selection was requested,
 * or if its name matches any -c <command> specification.
 * 
 */
	if ((Selflags & SELCMD) == 0)
	    return(0);
	for (sp = Cmdl; sp; sp = sp->next) {
	    if (!sp->x && !strncmp(sp->str, cmd, sp->len)) {
		sp->f = 1;
		*pss |= PS_PRI;
		*sf |= SELCMD;
		return(0);
	    }
	}
/*
 * The command name doesn't match any -c <command> specification.  See if it
 * matches a -c /RE/[bix] specification.
 */
	for (i = 0; i < NCmdRxU; i++) {
	    if (!regexec(&CmdRx[i].cx, cmd, 0, NULL, 0)) {
		CmdRx[i].mc = 1;
		*pss |= PS_PRI;
		*sf |= SELCMD;
		return(0);
	    }
	}
/*
 * The command name matches no -c specification.
 *
 * It's excluded if the only selection condition is command name,
 * or if command name selection is part of an ANDed set.
 */
	if (Selflags == SELCMD)
	    return(1);
	return (Fand ? 1 : 0);
}


/*
 * is_file_sel() - is file selected?
 */

static int
is_file_sel(lp, lf)
	struct lproc *lp;		/* lproc structure pointer */
	struct lfile *lf;		/* lfile structure pointer */
{
	if (!lf || !lf->sf)
	    return(0);
	if (Lf->sf & SELEXCLF)
	    return(0);

#if	defined(HASSECURITY) && defined(HASNOSOCKSECURITY)
	if (Myuid && (Myuid != lp->uid)) {
	    if (!(lf->sf & (SELNA | SELNET)))
		return(0);
	}
#endif	/* defined(HASSECURITY) && defined(HASNOSOCKSECURITY) */

	if (Selall)
	    return(1);
	if (Fand && ((lf->sf & Selflags) != Selflags))
	    return(0);
	return(1);
}


/*
 * is_proc_excl() - is process excluded?
 */

int
is_proc_excl(pid, pgid, uid, pss, sf)
	int pid;			/* Process ID */
	int pgid;			/* process group ID */
	UID_ARG uid;			/* User ID */
	short *pss;			/* process select state for lproc */
	short *sf;			/* select flags for lproc */
{
	int i, j;

	*pss = *sf = 0;

#if	defined(HASSECURITY)
/*
 * The process is excluded by virtue of the security option if it
 * isn't owned by the owner of this lsof process, unless the
 * HASNOSOCKSECURITY option is also specified.  In that case the
 * selected socket files of any process may be listed.
 */
# if	!defined(HASNOSOCKSECURITY)
	if (Myuid && Myuid != (uid_t)uid)
	    return(1);
# endif	/* !defined(HASNOSOCKSECURITY) */
#endif	/* defined(HASSECURITY) */

/*
 * If the excluding of process listing by UID has been specified, see if the
 * owner of this process is excluded.
 */
	if (Nuidexcl) {
	    for (i = j = 0; (i < Nuid) && (j < Nuidexcl); i++) {
		if (!Suid[i].excl)
		    continue;
		if (Suid[i].uid == (uid_t)uid)
		    return(1);
		j++;
	    }
	}
/*
 * If the excluding of process listing by PGID has been specified, see if this
 * PGID is excluded.
 */
	if (Npgidx) {
	    for (i = j = 0; (i < Npgid) && (j < Npgidx); i++) {
		if (!Spgid[i].x)
		    continue;
		if (Spgid[i].i == pgid)
		    return(1);
		j++;
	    }
	}
/*
 * If the excluding of process listing by PID has been specified, see if this
 * PID is excluded.
 */
	if (Npidx) {
	    for (i = j = 0; (i < Npid) && (j < Npidx); i++) {
		if (!Spid[i].x)
		    continue;
		if (Spid[i].i == pid)
		    return(1);
		j++;
	    }
	}
/*
 * If the listing of all processes is selected, then this one is not excluded.
 *
 * However, if HASSECURITY and HASNOSOCKSECURITY are both specified, exclude
 * network selections from the file flags, so that the tests in is_file_sel()
 * work as expected.
 */
	if (Selall) {
	    *pss = PS_PRI;

#if	defined(HASSECURITY) && defined(HASNOSOCKSECURITY)
	    *sf = SELALL & ~(SELNA | SELNET);
#else	/* !defined(HASSECURITY) || !defined(HASNOSOCKSECURITY) */
	    *sf = SELALL;
#endif	/* defined(HASSECURITY) && defined(HASNOSOCKSECURITY) */

	    return(0);
	}
/*
 * If the listing of processes has been specified by process group ID, see
 * if this one is included or excluded.
 */
	if (Npgidi && (Selflags & SELPGID)) {
	    for (i = j = 0; (i < Npgid) && (j < Npgidi); i++) {
		if (Spgid[i].x)
		    continue;
		if (Spgid[i].i == pgid) {
		    Spgid[i].f = 1;
		    *pss = PS_PRI;
		    *sf = SELPGID;
		    if (Selflags == SELPGID)
			return(0);
		    break;
		}
		j++;
	    }
	    if ((Selflags == SELPGID) && !*sf)
		return(1);
	}
/*
 * If the listing of processes has been specified by PID, see if this one is
 * included or excluded.
 */
	if (Npidi && (Selflags & SELPID)) {
	    for (i = j = 0; (i < Npid) && (j < Npidi); i++) {
		if (Spid[i].x)
		    continue;
		if (Spid[i].i == pid) {
		    Spid[i].f = 1;
		    *pss = PS_PRI;
		    *sf |= SELPID;
		    if (Selflags == SELPID)
			return(0);
		    break;
		}
		j++;
	    }
	    if ((Selflags == SELPID) && !*sf)
		return(1);
	}
/*
 * If the listing of processes has been specified by UID, see if the owner of
* this process has been included.
 */
	if (Nuidincl && (Selflags & SELUID)) {
	    for (i = j = 0; (i < Nuid) && (j < Nuidincl); i++) {
		if (Suid[i].excl)
		    continue;
		if (Suid[i].uid == (uid_t)uid) {
		    Suid[i].f = 1;
		    *pss = PS_PRI;
		    *sf |= SELUID;
		    if (Selflags == SELUID)
			return(0);
		    break;
		}
		j++;
	    }
	    if (Selflags == SELUID && (*sf & SELUID) == 0)
		return(1);
	}
/*
 * When neither the process group ID, nor the PID, nor the UID is selected:
 *
 *	If list option ANDing of process group IDs, PIDs or UIDs is specified,
 *	the process is excluded;
 *
 *	Otherwise, it's not excluded by the tests of this function.
 */
	if ( ! *sf)
	    return((Fand && (Selflags & (SELPGID|SELPID|SELUID))) ? 1 : 0);
/*
 * When the process group ID, PID, or UID is selected and the process group
 * ID, PID, or UID list option has been specified:
 *
 *	If list option ANDing has been specified, and the correct
 *	combination of process group ID, PID, and UID is selected, reply that
 *	the process is not excluded;
 * or
 *	If list option ANDing has not been specified, reply that the
 *	process is not excluded by the tests of this function.
 */
	if (Selflags & (SELPGID|SELPID|SELUID)) {
	    if (Fand)
		return(((Selflags & (SELPGID|SELPID|SELUID)) != *sf) ? 1 : 0);
	    return(0);
	}
/*
 * Finally, when neither the process group ID, nor the PID, nor the UID is
 * selected, and no process group ID, PID or UID list option has been
 * specified:
 *
 *	If list option ANDing has been specified, this process is
 *	excluded;
 *
 *	Otherwise, it isn't excluded by the tests of this function.
 */
	return(Fand ? 1 : 0);
}


/*
 * link_lfile() - link local file structures
 */

void
link_lfile()
{
	if (Lf->sf & SELEXCLF)
	    return;
	Lp->pss |= PS_SEC;
	if (Plf)
	    Plf->next = Lf;
	else
	    Lp->file = Lf;
	Plf = Lf;
	if (Fnet && (Lf->sf & SELNET))
	    Fnet = 2;
	if (Fnfs && (Lf->sf & SELNFS))
	    Fnfs = 2;
	Lf = (struct lfile *)NULL;
}


#if	defined(HASFSTRUCT)
/*
 * print_fflags() - print interpreted f_flag[s]
 */

char *
print_fflags(ffg, pof)
	long ffg;		/* file structure's flags value */
	long pof;		/* process open files flags value */
{
	int al, ct, fx;
	static int bl = 0;
	static char *bp = (char *)NULL;
	char *sep;
	int sepl;
	struct pff_tab *tp;
	long wf;
	char xbuf[64];
/*
 * Reduce the supplied flags according to the definitions in Pff_tab[] and
 * Pof_tab[].
 */
	for (ct = fx = 0; fx < 2; fx++) {
	    if (fx == 0) {
		sep = "";
		sepl = 0;
		tp = Pff_tab;
		wf = ffg;
	    } else {
		sep = ";";
		sepl = 1;
		tp = Pof_tab;
		wf = pof;
	    }
	    for (; wf && !FsvFlagX; ct += al ) {
		while (tp->nm) {
		    if (wf & tp->val)
			break;
		    tp++;
		}
		if (!tp->nm)
		    break;
		al = (int)strlen(tp->nm) + sepl;
		bp = alloc_fflbuf(&bp, &bl, al + ct);
		(void) snpf(bp + ct, al + 1, "%s%s", sep, tp->nm);
		sep = ",";
		sepl = 1;
		wf &= ~(tp->val);
	    }
	/*
	 * If flag bits remain, print them in hex.  If hex output was
	 * specified with +fG, print all flag values, including zero,
	 * in hex.
	 */
	    if (wf || FsvFlagX) {
		(void) snpf(xbuf, sizeof(xbuf), "0x%lx", wf);
		al = (int)strlen(xbuf) + sepl;
		bp = alloc_fflbuf(&bp, &bl, al + ct);
		(void) snpf(bp + ct, al + 1, "%s%s", sep, xbuf);
		ct += al;
	    }
	}
/*
 * Make sure there is at least a NUL terminated reply.
 */
	if (!bp) {
	    bp = alloc_fflbuf(&bp, &bl, 0);
	    *bp = '\0';
	}
	return(bp);
}
#endif	/* defined(HASFSTRUCT) */


/*
 * print_proc() - print process
 */

int
print_proc()
{
	char buf[128], *cp;
	int lc, len, st, ty;
	int rv = 0;
	unsigned long ul;
/*
 * If nothing in the process has been selected, skip it.
 */
	if (!Lp->pss)
	    return(0);
	if (Fterse) {

	/*
	 * The mode is terse and something in the process appears to have
	 * been selected.  Make sure of that by looking for a selected file,
	 * so that the HASSECURITY and HASNOSOCKSECURITY option combination
	 * won't produce a false positive result.
	 */
	    for (Lf = Lp->file; Lf; Lf = Lf->next) {
		if (is_file_sel(Lp, Lf)) {
		    (void) printf("%d\n", Lp->pid);
		    return(1);
		}
	    }
	    return(0);
	}
/*
 * If fields have been selected, output the process-only ones, provided
 * that some file has also been selected.
 */
	if (Ffield) {
	    for (Lf = Lp->file; Lf; Lf = Lf->next) {
		if (is_file_sel(Lp, Lf))
		    break;
	    }
	    if (!Lf)
		return(rv);
	    rv = 1;
	    (void) printf("%c%d%c", LSOF_FID_PID, Lp->pid, Terminator);

#if	defined(HASZONES)
	    if (FieldSel[LSOF_FIX_ZONE].st && Fzone && Lp->zn)
		(void) printf("%c%s%c", LSOF_FID_ZONE, Lp->zn, Terminator);
#endif	/* defined(HASZONES) */
 
#if	defined(HASSELINUX)
	    if (FieldSel[LSOF_FIX_CNTX].st && Fcntx && Lp->cntx && CntxStatus)
		(void) printf("%c%s%c", LSOF_FID_CNTX, Lp->cntx, Terminator);
#endif	/* defined(HASSELINUX) */

	    if (FieldSel[LSOF_FIX_PGID].st && Fpgid)
		(void) printf("%c%d%c", LSOF_FID_PGID, Lp->pgid, Terminator);

#if	defined(HASPPID)
	    if (FieldSel[LSOF_FIX_PPID].st && Fppid)
		(void) printf("%c%d%c", LSOF_FID_PPID, Lp->ppid, Terminator);
#endif	/* defined(HASPPID) */

	    if (FieldSel[LSOF_FIX_CMD].st) {
		putchar(LSOF_FID_CMD);
		safestrprt(Lp->cmd ? Lp->cmd : "(unknown)", stdout, 0);
		putchar(Terminator);
	    }
	    if (FieldSel[LSOF_FIX_UID].st)
		(void) printf("%c%d%c", LSOF_FID_UID, (int)Lp->uid, Terminator);
	    if (FieldSel[LSOF_FIX_LOGIN].st) {
		cp = printuid((UID_ARG)Lp->uid, &ty);
		if (ty == 0)
		    (void) printf("%c%s%c", LSOF_FID_LOGIN, cp, Terminator);
	    }
	    if (Terminator == '\0')
	    putchar('\n');
	}
/*
 * Print files.
 */
	for (Lf = Lp->file; Lf; Lf = Lf->next) {
	    if (!is_file_sel(Lp, Lf))
		continue;
	    rv = 1;
	/*
	 * If no field output selected, print dialects-specific formatted
	 * output.
	 */
	    if (!Ffield) {
		print_file();
		continue;
	    }
	/*
	 * Print selected fields.
	 */
	    lc = st = 0;
	    if (FieldSel[LSOF_FIX_FD].st) {
		for (cp = Lf->fd; *cp == ' '; cp++)
		    ;
		if (*cp) {
		    (void) printf("%c%s%c", LSOF_FID_FD, cp, Terminator);
		    lc++;
		}
	    }
	    if (FieldSel[LSOF_FIX_ACCESS].st) {
		(void) printf("%c%c%c",
		    LSOF_FID_ACCESS, Lf->access, Terminator);
		lc++;
	    }
	    if (FieldSel[LSOF_FIX_LOCK].st) {
		(void) printf("%c%c%c", LSOF_FID_LOCK, Lf->lock, Terminator);
		lc++;
	    }
	    if (FieldSel[LSOF_FIX_TYPE].st) {
		for (cp = Lf->type; *cp == ' '; cp++)
		    ;
		if (*cp) {
		    (void) printf("%c%s%c", LSOF_FID_TYPE, cp, Terminator);
		    lc++;
		}
	    }

#if	defined(HASFSTRUCT)
	    if (FieldSel[LSOF_FIX_FA].st && (Fsv & FSV_FA)
	    &&  (Lf->fsv & FSV_FA)) {
		(void) printf("%c%s%c", LSOF_FID_FA,
		    print_kptr(Lf->fsa, (char *)NULL, 0), Terminator);
		lc++;
	    }
	    if (FieldSel[LSOF_FIX_CT].st && (Fsv & FSV_CT)
	    &&  (Lf->fsv & FSV_CT)) {
		(void) printf("%c%ld%c", LSOF_FID_CT, Lf->fct, Terminator);
		lc++;
	    }
	    if (FieldSel[LSOF_FIX_FG].st && (Fsv & FSV_FG)
	    &&  (Lf->fsv & FSV_FG) && (FsvFlagX || Lf->ffg || Lf->pof)) {
		(void) printf("%c%s%c", LSOF_FID_FG,
		    print_fflags(Lf->ffg, Lf->pof), Terminator);
		lc++;
	    }
	    if (FieldSel[LSOF_FIX_NI].st && (Fsv & FSV_NI)
	    &&  (Lf->fsv & FSV_NI)) {
		(void) printf("%c%s%c", LSOF_FID_NI,
		    print_kptr(Lf->fna, (char *)NULL, 0), Terminator);
		lc++;
	    }
#endif	/* defined(HASFSTRUCT) */

	    if (FieldSel[LSOF_FIX_DEVCH].st && Lf->dev_ch && Lf->dev_ch[0]) {
		for (cp = Lf->dev_ch; *cp == ' '; cp++)
		    ;
		if (*cp) {
		    (void) printf("%c%s%c", LSOF_FID_DEVCH, cp, Terminator);
		    lc++;
		}
	    }
	    if (FieldSel[LSOF_FIX_DEVN].st && Lf->dev_def) {
		if (sizeof(unsigned long) > sizeof(dev_t))
		    ul = (unsigned long)((unsigned int)Lf->dev);
		else
		    ul = (unsigned long)Lf->dev;
		(void) printf("%c0x%lx%c", LSOF_FID_DEVN, ul, Terminator);
		lc++;
	    }
	    if (FieldSel[LSOF_FIX_RDEV].st && Lf->rdev_def) {
		if (sizeof(unsigned long) > sizeof(dev_t))
		    ul = (unsigned long)((unsigned int)Lf->rdev);
		else
		    ul = (unsigned long)Lf->rdev;
		(void) printf("%c0x%lx%c", LSOF_FID_RDEV, ul, Terminator);
		lc++;
	    }
	    if (FieldSel[LSOF_FIX_SIZE].st && Lf->sz_def) {
		putchar(LSOF_FID_SIZE);

#if	defined(HASPRINTSZ)
		cp = HASPRINTSZ(Lf);
#else	/* !defined(HASPRINTSZ) */
		(void) snpf(buf, sizeof(buf), SzOffFmt_d, Lf->sz);
		cp = buf;
#endif	/* defined(HASPRINTSZ) */

		(void) printf("%s", cp);
		putchar(Terminator);
		lc++;
	    }
	    if (FieldSel[LSOF_FIX_OFFSET].st && Lf->off_def) {
		putchar(LSOF_FID_OFFSET);

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

		}
		(void) printf("%s", cp);
		putchar(Terminator);
		lc++;
	    }
	    if (FieldSel[LSOF_FIX_INODE].st && Lf->inp_ty == 1) {
		putchar(LSOF_FID_INODE);
		(void) printf(InodeFmt_d, Lf->inode);
		putchar(Terminator);
		lc++;
	    }
	    if (FieldSel[LSOF_FIX_NLINK].st && Lf->nlink_def) {
		(void) printf("%c%ld%c", LSOF_FID_NLINK, Lf->nlink, Terminator);
		lc++;
	    }
	    if (FieldSel[LSOF_FIX_PROTO].st && Lf->inp_ty == 2) {
		for (cp = Lf->iproto; *cp == ' '; cp++)
		    ;
		if (*cp) {
		    (void) printf("%c%s%c", LSOF_FID_PROTO, cp, Terminator);
		    lc++;
		}
	    }
	    if (FieldSel[LSOF_FIX_STREAM].st && Lf->nm && Lf->is_stream) {
		if (strncmp(Lf->nm, "STR:", 4) == 0
		||  strcmp(Lf->iproto, "STR") == 0) {
		    putchar(LSOF_FID_STREAM);
		    printname(0);
		    putchar(Terminator);
		    lc++;
		    st++;
		}
	    }
	    if (st == 0 && FieldSel[LSOF_FIX_NAME].st) {
		putchar(LSOF_FID_NAME);
		printname(0);
		putchar(Terminator);
		lc++;
	    }
	    if (Lf->lts.type >= 0 && FieldSel[LSOF_FIX_TCPTPI].st) {
		print_tcptpi(0);
		lc++;
	    }
	    if (Terminator == '\0' && lc)
		putchar('\n');
	}
	return(rv);
}
