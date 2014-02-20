/*
 * arg.c - common argument processing support functions for lsof
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
static char *rcsid = "$Id: arg.c,v 1.49 2009/03/25 19:20:30 abe Exp $";
#endif


#include "lsof.h"


/*
 * Local definitions
 */

#define	CMDRXINCR	32		/* CmdRx[] allocation increment */


/*
 * Local static variables
 */

static int NCmdRxA = 0;			/* space allocated to CmdRx[] */


/*
 * Local function prototypes
 */

_PROTOTYPE(static int ckfd_range,(char *first, char *dash, char *last, int *lo, int *hi));
_PROTOTYPE(static int enter_fd_lst,(char *nm, int lo, int hi, int excl));
_PROTOTYPE(static int enter_nwad,(struct nwad *n, int sp, int ep, char *s, struct hostent *he));
_PROTOTYPE(static struct hostent *lkup_hostnm,(char *hn, struct nwad *n));
_PROTOTYPE(static char *isIPv4addr,(char *hn, unsigned char *a, int al));


/*
 * ckfd_range() - check fd range
 */

static int
ckfd_range(first, dash, last, lo, hi)
	char *first;			/* starting character */
	char *dash;			/* '-' location */
	char *last;			/* '\0' location */
	int *lo;			/* returned low value */
	int *hi;			/* returned high value */
{
	char *cp;
/*
 * See if the range character pointers make sense.
 */
	if (first >= dash || dash >= last) {
	    (void) fprintf(stderr, "%s: illegal FD range for -d: ", Pn);
	    safestrprt(first, stderr, 1);
	    return(1);
	}
/*
 * Assemble and check the high and low values.
 */
	for (cp = first, *lo = 0; *cp && cp < dash; cp++) {
	    if (!isdigit((unsigned char)*cp)) {

FD_range_nondigit:

		(void) fprintf(stderr, "%s: non-digit in -d FD range: ", Pn);
		safestrprt(first, stderr, 1);
		return(1);
	    }
	    *lo = (*lo * 10) + (int)(*cp - '0');
	}
	for (cp = dash+1, *hi = 0; *cp && cp < last; cp++) {
	    if (!isdigit((unsigned char)*cp))
		goto FD_range_nondigit;
	    *hi = (*hi * 10) + (int)(*cp - '0');
	}
	if (*lo >= *hi) {
	    (void) fprintf(stderr, "%s: -d FD range's low >= its high: ", Pn);
	    safestrprt(first, stderr, 1);
	    return(1);
	}
	return(0);
}


/*
 * ck_file_arg() - check file arguments
 */

int
ck_file_arg(i, ac, av, fv, rs, sbp)
	int i;			/* first file argument index */
	int ac;			/* argument count */
	char *av[];		/* argument vector */
	int fv;			/* Ffilesys value (real or temporary) */
	int rs;			/* Readlink() status if argument count == 1:
				 *	0 = undone; 1 = done */
	struct stat *sbp;	/* if non-NULL, pointer to stat(2) buffer
				 * when argument count == 1 */
{
	char *ap, *fnm, *fsnm, *path;
	short err = 0;
	int fsm, ftype, j, k;
	MALLOC_S l;
	struct mounts *mp;
	static struct mounts **mmp = (struct mounts **)NULL;
	int mx, nm;
	static int nma = 0;
	struct stat sb;
	struct sfile *sfp;
	short ss = 0;

#if	defined(CKFA_EXPDEV)
	dev_t dev, rdev;
#endif	/* defined(CKFA_EXPDEV) */

#if	defined(HASPROCFS)
	unsigned char ad, an;
	int pfsnl = -1;
	pid_t pid;
	struct procfsid *pfi;
#endif	/* defined(HASPROCFS) */

/*
 * Loop through arguments.
 */
	for (; i < ac; i++) {
	    if (rs && (ac == 1) && (i == 0))
		path = av[i];
	    else {
		if (!(path = Readlink(av[i]))) {
		    ErrStat = 1;
		    continue;
		}
	    }
	/*
	 * Remove terminating `/' characters from paths longer than one.
	 */
	    j = k = strlen(path);
	    while ((k > 1) && (path[k-1] == '/')) {
		k--;
	    }
	    if (k < j) {
		if (path != av[i])
		    path[k] = '\0';
		else {
		    if (!(ap = (char *)malloc((MALLOC_S)(k + 1)))) {
			(void) fprintf(stderr, "%s: no space for copy of %s\n",
			    Pn, path);
			Exit(1);
		    }
		    (void) strncpy(ap, path, k);
		    ap[k] = '\0';
		    path = ap;
		}
	    }
	/*
	 * Check for file system argument.
	 */
	    for (ftype = 1, mp = readmnt(), nm = 0;
		 (fv != 1) && mp;
		 mp = mp->next)
	    {
		fsm = 0;
		if (strcmp(mp->dir, path) == 0)
		    fsm++;
		else if (fv == 2 || (mp->fs_mode & S_IFMT) == S_IFBLK) {
		    if (mp->fsnmres && strcmp(mp->fsnmres, path) == 0)
			fsm++;
		}
		if (!fsm)
		    continue;
		ftype = 0;
	    /*
	     * Skip duplicates.
	     */
		for (mx = 0; mx < nm; mx++) {
		    if (strcmp(mp->dir, mmp[mx]->dir) == 0
		    &&  mp->dev == mmp[mx]->dev
		    &&  mp->rdev == mmp[mx]->rdev
		    &&  mp->inode == mmp[mx]->inode)
			break;
		}
		if (mx < nm)
		    continue;
	    /*
	     * Allocate space for and save another mount point match and
	     * the type of match -- directory name (mounted) or file system
	     * name (mounted-on).
	     */
		if (nm >= nma) {
		    nma += 5;
		    l = (MALLOC_S)(nma * sizeof(struct mounts *));
		    if (mmp)
			mmp = (struct mounts **)realloc((MALLOC_P *)mmp, l);
		    else
			mmp = (struct mounts **)malloc(l);
		    if (!mmp) {
			(void) fprintf(stderr,
			    "%s: no space for mount pointers\n", Pn);
			Exit(1);
		    }
		}
		mmp[nm++] = mp;
	    }
	    if (fv == 2 && nm == 0) {
		(void) fprintf(stderr, "%s: not a file system: ", Pn);
		safestrprt(av[i], stderr, 1);
		ErrStat = 1;
		continue;
	    }
	/*
	 * Loop through the file system matches.  If there were none, make one
	 * pass through the loop, using simply the path name.
	 */
	    mx = 0;
	    do {

	    /*
	     * Allocate an sfile structure and fill in the type and link.
	     */
		if (!(sfp = (struct sfile *)malloc(sizeof(struct sfile)))) {
		    (void) fprintf(stderr, "%s: no space for files\n", Pn);
		    Exit(1);
		}
		sfp->next = Sfile;
		Sfile = sfp;
		sfp->f = 0;
		if ((sfp->type = ftype)) {

		/*
		 * For a non-file system path, use the path as the file name
		 * and set a NULL file system name.
		 */
		    fnm = path;
		    fsnm = (char *)NULL;
		/*
		 * Stat the path to obtain its characteristics.
		 */
		    if (sbp && (ac == 1))
			sb = *sbp;
		    else {
			if (statsafely(fnm, &sb) != 0) {
			    int en = errno;

			    (void) fprintf(stderr, "%s: status error on ", Pn);
			    safestrprt(fnm, stderr, 0);
			    (void) fprintf(stderr, ": %s\n", strerror(en));
			    Sfile = sfp->next;
			    (void) free((FREE_P *)sfp);
			    ErrStat = 1;
			    continue;
			}

#if	defined(HASSPECDEVD)
			(void) HASSPECDEVD(fnm, &sb);
#endif	/* defined(HASSPECDEVD) */

		    }
		    sfp->i = (INODETYPE)sb.st_ino;
		    sfp->mode = sb.st_mode & S_IFMT;
		
#if	defined(CKFA_EXPDEV)
		/*
		 * Expand device numbers before saving, so that they match the
		 * already-expanded local mount info table device numbers.
		 * (This is an EP/IX 2.1.1 and above artifact.)
		 */
		    sfp->dev = expdev(sb.st_dev);
		    sfp->rdev = expdev(sb.st_rdev);
#else	/* !defined(CKFA_EXPDEV) */
		    sfp->dev = sb.st_dev;
		    sfp->rdev = sb.st_rdev;
#endif	/* defined(CKFA_EXPDEV) */

#if	defined(CKFA_MPXCHAN)
		/*
		 * Save a (possible) multiplexed channel number.  (This is an
		 * AIX artifact.)
		 */
		    sfp->ch = getchan(path);
#endif	/* defined(CKFA_MPXCHAN) */

		} else {
		    mp = mmp[mx++];
		    ss++;

#if	defined(HASPROCFS)
		/*
		 * If this is a /proc file system, set the search flag and
		 * abandon the sfile entry.
		 */
		    if (mp == Mtprocfs) {
			Sfile = sfp->next;
			(void) free((FREE_P *)sfp);
			Procsrch = 1;
			continue;
		    }
#endif	/* defined(HASPROCFS) */

		/*
		 * Derive file name and file system name for a mount point.
		 *
		 * Save the device numbers, inode number, and modes.
		 */
		    fnm = mp->dir;
		    fsnm = mp->fsname;
		    sfp->dev = mp->dev;
		    sfp->rdev = mp->rdev;
		    sfp->i = mp->inode;
		    sfp->mode = mp->mode & S_IFMT;
		}
		ss = 1;		/* indicate a "safe" stat() */
	    /*
	     * Store the file name and file system name pointers in the sfile
	     * structure, allocating space as necessary.
	     */
		if (!fnm || fnm == path) {
		    sfp->name = fnm;

#if	defined(HASPROCFS)
		    an = 0;
#endif	/* defined(HASPROCFS) */

		} else {
		    if (!(sfp->name = mkstrcpy(fnm, (MALLOC_S *)NULL))) {
			(void) fprintf(stderr,
			    "%s: no space for file name: ", Pn);
			safestrprt(fnm, stderr, 1);
			Exit(1);
		    }

#if	defined(HASPROCFS)
		    an = 1;
#endif	/* defined(HASPROCFS) */

		}
		if (!fsnm || fsnm == path) {
		    sfp->devnm = fsnm;

#if	defined(HASPROCFS)
		    ad = 0;
#endif	/* defined(HASPROCFS) */

		} else {
		    if (!(sfp->devnm = mkstrcpy(fsnm, (MALLOC_S *)NULL))) {
			(void) fprintf(stderr,
			    "%s: no space for file system name: ", Pn);
			safestrprt(fsnm, stderr, 1);
			Exit(1);
		    }

#if	defined(HASPROCFS)
		    ad = 1;
#endif	/* defined(HASPROCFS) */

		}
		if (!(sfp->aname = mkstrcpy(av[i], (MALLOC_S *)NULL))) {
		    (void) fprintf(stderr,
			"%s: no space for argument file name: ", Pn);
			safestrprt(av[i], stderr, 1);
		    Exit(1);
		}

#if	defined(HASPROCFS)
	    /*
	     * See if this is an individual member of a proc file system.
	     */
		if (!Mtprocfs || Procsrch)
		    continue;

# if	defined(HASFSTYPE) && HASFSTYPE==1
		if (strcmp(sb.st_fstype, HASPROCFS) != 0)
		    continue;
# endif	/* defined(HASFSTYPE) && HASFSTYPE==1 */

		if (pfsnl == -1)
		    pfsnl = strlen(Mtprocfs->dir);
		if (!pfsnl)
		    continue;
		if (strncmp(Mtprocfs->dir, path, pfsnl) != 0)
		    continue;
		if (path[pfsnl] != '/')

# if	defined(HASPINODEN)
		    pid = 0;
# else	/* !defined(HASPINODEN) */
		    continue;
# endif	/* defined(HASPINODEN) */

		else {
		    for (j = pfsnl+1; path[j]; j++) {
			if (!isdigit((unsigned char)path[j]))
			    break;
		    }
		    if (path[j] || (j - pfsnl - 1) < 1
		    ||  (sfp->mode & S_IFMT) != S_IFREG)

# if	defined(HASPINODEN)
			pid = 0;
# else	/* !defined(HASPINODEN) */
			continue;
# endif	/* defined(HASPINODEN) */

		    else
			pid = atoi(&path[pfsnl+1]);
		}
		if (!(pfi = (struct procfsid *)malloc((MALLOC_S)
			     sizeof(struct procfsid))))
		{
		    (void) fprintf(stderr, "%s: no space for %s ID: ",
			Pn, Mtprocfs->dir);
		    safestrprt(path, stderr, 1);
		    Exit(1);
		}
		pfi->pid = pid;
		pfi->f = 0;
		pfi->nm = sfp->aname;
		pfi->next = Procfsid;
		Procfsid = pfi;

# if	defined(HASPINODEN)
		pfi->inode = (INODETYPE)sfp->i;
# endif	/* defined(HASPINODEN) */

	    /*
	     * Abandon the Sfile entry, lest it be used in is_file_named().
	     */
		Sfile = sfp->next;
		if (ad)
		    (void) free((FREE_P *)sfp->devnm);
		if (an)
		    (void) free((FREE_P *)sfp->name);
		(void) free((FREE_P *)sfp);
#endif	/* defined(HASPROCFS) */

	    } while (mx < nm);
	}
	if (!ss)
	    err = 1;
	return((int)err);
}


#if	defined(HASDCACHE)
/*
 * ctrl_dcache() - enter device cache control
 */

int
ctrl_dcache(c)
	char *c;			/* control string */
{
	int rc = 0;
	
	if (!c) {
	    (void) fprintf(stderr,
		"%s: no device cache option control string\n", Pn);
	    return(1);
	}
/*
 * Decode argument function character.
 */
	switch (*c) {
	case '?':
	    if (*(c+1) != '\0') {
		(void) fprintf(stderr, "%s: nothing should follow -D?\n", Pn);
		return(1);
	    }
	    DChelp = 1;
	    return(0);
	case 'b':
	case 'B':
	    if (Setuidroot

#if	!defined(WILLDROPGID)
	    ||  Myuid
#endif	/* !defined(WILLDROPGID) */

	    )
		rc = 1;
	    else
		DCstate = 1;
	    break;
	case 'r':
	case 'R':
	    if (Setuidroot && *(c+1))
		rc = 1;
	    else
		DCstate = 2;
	    break;
	case 'u':
	case 'U':
	    if (Setuidroot

#if	!defined(WILLDROPGID)
	    ||  Myuid
#endif	/* !defined(WILLDROPGID) */

	    )
		rc = 1;
	    else
		DCstate = 3;
	    break;
	case 'i':
	case 'I':
	    if (*(c+1) == '\0') {
		DCstate = 0;
		return(0);
	    }
	    /* fall through */
	default:
	    (void) fprintf(stderr, "%s: unknown -D option: ", Pn);
	    safestrprt(c, stderr, 1);
	    return(1);
	}
	if (rc) {
	    (void) fprintf(stderr, "%s: -D option restricted to root: ", Pn);
	    safestrprt(c, stderr, 1);
	    return(1);
	}
/*
 * Skip to optional path name and save it.
 */
	for (c++; *c && (*c == ' ' || *c == '\t'); c++)
	    ;
	if (strlen(c)) {
	    if (!(DCpathArg = mkstrcpy(c, (MALLOC_S *)NULL))) {
		(void) fprintf(stderr, "%s: no space for -D path: ", Pn);
		safestrprt(c, stderr, 1);
		Exit(1);
	    }
	}
	return(0);
}
#endif	/* defined(HASDCACHE) */


/*
 * enter_cmd_rx() - enter command regular expression
 */

int
enter_cmd_rx(x)
	char *x;			/* regular expression */
{
	int bmod = 0;
	int bxmod = 0;
	int i, re;
	int imod = 0;
	int xmod = 0;
	int co = REG_NOSUB|REG_EXTENDED;
	char reb[256], *xb, *xe, *xm;
	MALLOC_S xl;
	char *xp = (char *)NULL;
/*
 * Make sure the supplied string starts a regular expression.
 */
	if (!*x || (*x != '/')) {
	    (void) fprintf(stderr, "%s: regexp doesn't begin with '/': ", Pn);
	    if (x)
		safestrprt(x, stderr, 1);
	    return(1);
	}
/*
 * Skip to the end ('/') of the regular expression.
 */
	xb = x + 1;
	for (xe = xb; *xe; xe++) {
	    if (*xe == '/')
		break;
	}
	if (*xe != '/') {
	    (void) fprintf(stderr, "%s: regexp doesn't end with '/': ", Pn);
	    safestrprt(x, stderr, 1);
	    return(1);
	}
/*
 * Decode any regular expression modifiers.
 */
	for (i = 0, xm = xe + 1; *xm; xm++) {
	    switch(*xm) {
	    case 'b':			/* This is a basic expression. */
		if (++bmod > 1) {
		    if (bmod == 2) {
			(void) fprintf(stderr,
			    "%s: b regexp modifier already used: ", Pn);
			safestrprt(x, stderr, 1);
		    }
		    i = 1;
		} else if (xmod) {
		    if (++bxmod == 1) {
			(void) fprintf(stderr,
			    "%s: b and x regexp modifiers conflict: ", Pn);
			safestrprt(x, stderr, 1);
		    }
		    i = 1;
		} else
		    co &= ~REG_EXTENDED;
		break;
	    case 'i':			/* Ignore case. */
		if (++imod > 1) {
		    if (imod == 2) {
			(void) fprintf(stderr,
			    "%s: i regexp modifier already used: ", Pn);
			safestrprt(x, stderr, 1);
		    }
		    i = 1;
		} else
		    co |= REG_ICASE;
		break;
	    case 'x':			/* This is an extended expression. */
		if (++xmod > 1) {
		    if (xmod == 2) {
			(void) fprintf(stderr,
			    "%s: x regexp modifier already used: ", Pn);
			safestrprt(x, stderr, 1);
		    }
		    i = 1;
		} else if (bmod) {
		    if (++bxmod == 1) {
			(void) fprintf(stderr,
			    "%s: b and x regexp modifiers conflict: ", Pn);
			safestrprt(x, stderr, 1);
		    }
		    i = 1;
		} else
		    co |= REG_EXTENDED;
		break;
	    default:
		(void) fprintf(stderr, "%s: invalid regexp modifier: %c\n",
		Pn, (int)*xm);
		i = 1;
	    }
	}
	if (i)
	    return(1);
/*
 * Allocate space to hold expression and copy it there.
 */
	xl = (MALLOC_S)(xe - xb);
	if (!(xp = (char *)malloc(xl + 1))) {
	    (void) fprintf(stderr, "%s: no regexp space for: ", Pn);
	    safestrprt(x, stderr, 1);
	    Exit(1);
	}
	(void) strncpy(xp, xb, xl);
	xp[(int)xl] = '\0';
/*
 * Assign a new CmdRx[] slot for this expression.
 */
	if (NCmdRxA >= NCmdRxU) {

	/*
	 * More CmdRx[] space must be assigned.
	 */
	    NCmdRxA += CMDRXINCR;
	    xl = (MALLOC_S)(NCmdRxA * sizeof(lsof_rx_t));
	    if (CmdRx)
		CmdRx = (lsof_rx_t *)realloc((MALLOC_P *)CmdRx, xl);
	    else
		CmdRx = (lsof_rx_t *)malloc(xl);
	    if (!CmdRx) {
		(void) fprintf(stderr, "%s: no space for regexp: ", Pn);
		safestrprt(x, stderr, 1);
		Exit(1);
	    }
	}
	i = NCmdRxU;
	CmdRx[i].exp = xp;
/*
 * Compile the expression.
 */
	if ((re = regcomp(&CmdRx[i].cx, xp, co))) {
	    (void) fprintf(stderr, "%s: regexp error: ", Pn);
	    safestrprt(x, stderr, 0);
	    (void) regerror(re, &CmdRx[i].cx, &reb[0], sizeof(reb));
	    (void) fprintf(stderr, ": %s\n", reb);
	    if (xp) {
		(void) free((FREE_P *)xp);
		xp = (char *)NULL;
	    }
	    return(1);
	}
/*
 * Complete the CmdRx[] table entry.
 */
	CmdRx[i].mc = 0;
	CmdRx[i].exp = xp;
	NCmdRxU++;
	return(0);
}


/*
 * enter_fd() - enter file descriptor list for searching
 */

int
enter_fd(f)
	char *f;			/* file descriptor list pointer */
{
	char c, *cp1, *cp2, *dash;
	int err, excl, hi, lo;
	char *fc;
/*
 *  Check for non-empty list and make a copy.
 */
	if (!f || (strlen(f) + 1) < 2) {
	    (void) fprintf(stderr, "%s: no file descriptor specified\n", Pn);
	    return(1);
	}
	if (!(fc = mkstrcpy(f, (MALLOC_S *)NULL))) {
	    (void) fprintf(stderr, "%s: no space for fd string: ", Pn);
	    safestrprt(f, stderr, 1);
	    Exit(1);
	}
/*
 * Isolate each file descriptor in the comma-separated list, then enter it
 * in the file descriptor string list.  If a descriptor has the form:
 *
 *	[0-9]+-[0-9]+
 *
 * treat it as an ascending range of file descriptor numbers.
 *
 * Accept a leading '^' as an excusion on match.
 */
	for (cp1 = fc, err = 0; *cp1;) {
	    if (*cp1 == '^') {
		excl = 1;
		cp1++;
	    } else
		excl = 0;
	    for (cp2 = cp1, dash = (char *)NULL; *cp2 && *cp2 != ','; cp2++) {
		if (*cp2 == '-')
		    dash = cp2;
	    }
	    if ((c = *cp2) != '\0')
		*cp2 = '\0';
	    if (cp2 > cp1) {
		if (dash) {
		    if (ckfd_range(cp1, dash, cp2, &lo, &hi))
			err = 1;
		    else {
			if (enter_fd_lst((char *)NULL, lo, hi, excl))
			    err = 1;
		    }
		} else {
		    if (enter_fd_lst(cp1, 0, 0, excl))
			err = 1;
		}
	    }
	    if (c == '\0')
		break;
	    cp1 = cp2 + 1;
	}
	(void) free((FREE_P *)fc);
	return(err);
}


/*
 * enter_fd_lst() - make an entry in the FD list, Fdl
 */

static int
enter_fd_lst(nm, lo, hi, excl)
	char *nm;			/* FD name (none if NULL) */
	int lo;				/* FD low boundary (if nm NULL) */
	int hi;				/* FD high boundary (if nm NULL) */
	int excl;			/* exclusion on match */
{
	char buf[256], *cp;
	int n;
	struct fd_lst *f, *ft;
/*
 * Don't allow a mixture of exclusions and inclusions.
 */
	if (FdlTy >= 0) {
	    if (FdlTy != excl) {
		if (!Fwarn) {

		/*
		 * If warnings are enabled, report a mixture.
		 */
		    if (nm) {
			(void) snpf(buf, sizeof(buf) - 1, "%s%s",
			    excl ? "^" : "", nm);
		    } else {
			if (lo != hi) {
			    (void) snpf(buf, sizeof(buf) - 1, "%s%d-%d",
				excl ? "^" : "", lo, hi);
			} else {
			    (void) snpf(buf, sizeof(buf) - 1, "%s%d",
				excl ? "^" : "", lo);
			}
		    }
		    buf[sizeof(buf) - 1] = '\0';
		    (void) fprintf(stderr,
		        "%s: %s in an %s -d list: %s\n", Pn,
			excl ? "exclude" : "include",
			FdlTy ? "exclude" : "include",
			buf);
		}
		return(1);
	    }
	}
/*
 * Allocate an fd_lst entry.
 */
	if (!(f = (struct fd_lst *)malloc((MALLOC_S)sizeof(struct fd_lst)))) {
	   (void) fprintf(stderr, "%s: no space for FD list entry\n", Pn);
	   Exit(1);
	}
	if (nm) {

	/*
	 * Process an FD name.  First see if it contains only digits; if it
	 * does, convert them to an integer and set the low and high
	 * boundaries to the result.
	 *
	 * If the name has a non-digit, store it as a string, and set the
	 * boundaries to impossible values (i.e., low > high).
	 */
	    for (cp = nm, n = 0; *cp; cp++) {
		if (!isdigit((unsigned char)*cp))
		    break;
		n = (n * 10) + (int)(*cp - '0');
	    }
	    if (*cp) {
		if (!(f->nm = mkstrcpy(nm, (MALLOC_S *)NULL))) {
		    (void) fprintf(stderr,
			"%s: no space for copy of: %s\n", Pn, nm);
		    Exit(1);
		}
		lo = 1;
		hi = 0;
	    } else {
		f->nm = (char *)NULL;
		lo = hi = n;
	    }
	} else
	    f->nm = (char *)NULL;
/*
 * Skip duplicates.
 */
	for (ft = Fdl; ft; ft = ft->next) {
	    if (f->nm) {
		if (!ft->nm || strcmp(f->nm, ft->nm))
		    continue;
	    } else if ((lo != ft->lo) || (hi != ft->hi))
		continue;
	    (void) free((FREE_P *)f);
	    return(0);
	}
/*
 * Complete the fd_lst entry and link it to the head of the chain.
 */
	f->hi = hi;
	f->lo = lo;
	f->next = Fdl;
	Fdl = f;
	FdlTy = excl;
	return(0);
}


/*
 * enter_dir() - enter the files of a directory for searching
 */

#define	EDDEFFNL	128		/* default file name length */

int
enter_dir(d, descend)
	char *d;			/* directory path name pointer */
	int descend;			/* subdirectory descend flag:
					 *	0 = don't descend
					 *	1 = descend */
{
	char *av[2];
	dev_t ddev;
	DIR *dfp;
	char *dn = (char *)NULL;
	MALLOC_S dnl, dnamlen;
	struct DIRTYPE *dp;
	int en, sl;
	int fct = 0;
	char *fp = (char *)NULL;
	MALLOC_S fpl = (MALLOC_S)0;
	MALLOC_S fpli = (MALLOC_S)0;
	struct stat sb;
/*
 * Check the directory path; reduce symbolic links; stat(2) it; make sure it's
 * really a directory.
 */
	if (!d || !*d || *d == '+' || *d == '-') {
	    if (!Fwarn)
		(void) fprintf(stderr,
		    "%s: +d not followed by a directory path\n", Pn);
	    return(1);
	}
	if (!(dn = Readlink(d)))
	    return(1);
	if (statsafely(dn, &sb)) {
	    if (!Fwarn) {
		en = errno;
		(void) fprintf(stderr, "%s: WARNING: can't stat(", Pn);
		safestrprt(dn, stderr, 0);
		(void) fprintf(stderr, "): %s\n", strerror(en));
	    }
	    if (dn && dn != d) {
		(void) free((FREE_P *)dn);
		dn = (char *)NULL;
	    }
	    return(1);
	}
	if ((sb.st_mode & S_IFMT) != S_IFDIR) {
	    if (!Fwarn) {
		(void) fprintf(stderr, "%s: WARNING: not a directory: ", Pn);
		safestrprt(dn, stderr, 1);
	    }
	    if (dn && dn != d) {
		(void) free((FREE_P *)dn);
		dn = (char *)NULL;
	    }
	    return(1);
	}

#if	defined(HASSPECDEVD)
	(void) HASSPECDEVD(dn, &sb);
#endif	/* defined(HASSPECDEVD) */

	ddev = sb.st_dev;
/*
 * Stack the directory and record it in Sfile for searching.
 */
	Dstkn = Dstkx = 0;
	Dstk = (char **)NULL;
	(void) stkdir(dn);
	av[0] = (dn == d) ? mkstrcpy(dn, (MALLOC_S *)NULL) : dn;
	av[1] = (char *)NULL;
	dn = (char *)NULL;
	if (!ck_file_arg(0, 1, av, 1, 1, &sb)) {
	    av[0] = (char *)NULL;
	    fct++;
	}
/*
 * Unstack the next directory and examine it.
 */
	while (--Dstkx >= 0) {
	    if (!(dn = Dstk[Dstkx]))
		continue;
	    Dstk[Dstkx] = (char *)NULL;
	/*
	 * Open the directory path and prepare its name for use with the
	 * files in the directory.
	 */
	    if (!(dfp = OpenDir(dn))) {
		if (!Fwarn) {
		    if ((en = errno) != ENOENT) {
			(void) fprintf(stderr,
			    "%s: WARNING: can't opendir(", Pn);
			safestrprt(dn, stderr, 0);
			(void) fprintf(stderr, "): %s\n", strerror(en));
		    }
	        }
		(void) free((FREE_P *)dn);
		dn = (char *)NULL;
		continue;
	    }
	    dnl = strlen(dn);
	    sl = ((dnl > 0) && (*(dn + dnl - 1) == '/')) ? 0 : 1;
	/*
	 * Define space for possible addition to the directory path.
	 */
	    fpli = (MALLOC_S)(dnl + sl + EDDEFFNL + 1);
	    if ((int)fpli > (int)fpl) {
		fpl = fpli;
		if (!fp)
		    fp = (char *)malloc(fpl);
		else
		    fp = (char *)realloc(fp, fpl);
		if (!fp) {
		    (void) fprintf(stderr,
			"%s: no space for path to entries in directory: %s\n",
			Pn, dn);
		    Exit(1);
		}
	    }
	    (void) snpf(fp, (size_t)fpl, "%s%s", dn, sl ? "/" : "");
	    (void) free((FREE_P *)dn);
	    dn = (char *)NULL;
	/*
	 * Read the contents of the directory.
	 */
	    for (dp = ReadDir(dfp); dp; dp = ReadDir(dfp)) {

	    /*
	     * Skip: entries with no inode number;
	     *	     entries with a zero length name;
	     *	     ".";
	     *	     and "..".
	     */
		if (!dp->d_ino)
		    continue;

#if     defined(HASDNAMLEN)
		dnamlen = (MALLOC_S)dp->d_namlen;
#else   /* !defined(HASDNAMLEN) */
		dnamlen = (MALLOC_S)strlen(dp->d_name);
#endif  /* defined(HASDNAMLEN) */

		if (!dnamlen)
		    continue;
		if (dnamlen <= 2 && dp->d_name[0] == '.') {
		    if (dnamlen == 1)
			continue;
		    if (dp->d_name[1] == '.')
			continue;
		}
	    /*
	     * Form the entry's path name.
	     */
		fpli = (MALLOC_S)(dnamlen - (fpl - dnl - sl - 1));
		if ((int)fpli > 0) {
		    fpl += fpli;
		    if (!(fp = (char *)realloc(fp, fpl))) {
			(void) fprintf(stderr, "%s: no space for: ", Pn);
			safestrprt(dn, stderr, 0);
			putc('/', stderr);
			safestrprtn(dp->d_name, dnamlen, stderr, 1);
			Exit(1);
		    }
		}
		(void) strncpy(fp + dnl + sl, dp->d_name, dnamlen);
		fp[dnl + sl + dnamlen] = '\0';
	    /*
	     * Lstatsafely() the entry; complain if that fails.
	     *
	     * Stack entries that represent subdirectories.
	     */
		if (lstatsafely(fp, &sb)) {
		    if ((en = errno) != ENOENT) {
			if (!Fwarn) {
			    (void) fprintf(stderr,
				"%s: WARNING: can't lstat(", Pn);
			    safestrprt(fp, stderr, 0);
			    (void) fprintf(stderr, "): %s\n", strerror(en));
			}
		    }
		    continue;
		}

#if	defined(HASSPECDEVD)
		(void) HASSPECDEVD(fp, &sb);
#endif	/* defined(HASSPECDEVD) */

		if (!(Fxover & XO_FILESYS)) {

		/*
		 * Unless "-x" or "-x f" was specified, don't cross over file
		 * system mount points.
		 */
		    if (sb.st_dev != ddev)
			continue;
		}
		if ((sb.st_mode & S_IFMT) == S_IFLNK) {

		/*
		 * If this is a symbolic link and "-x_ or "-x l" was specified,
		 * Statsafely() the entry and process it.
		 *
		 * Otherwise skip symbolic links.
		 */
		    if (Fxover & XO_SYMLINK) {
			if (statsafely(fp, &sb)) {
			    if ((en = errno) != ENOENT) {
				if (!Fwarn) {
				    (void) fprintf(stderr,
					"%s: WARNING: can't stat(", Pn);
				    safestrprt(fp, stderr, 0);
				    (void) fprintf(stderr,
					") symbolc link: %s\n", strerror(en));
				}
			    }
			    continue;
		        }
		    } else
			continue;
		}
		if (av[0]) {
		    (void) free((FREE_P *)av[0]);
		    av[0] = (char *)NULL;
		}
		av[0] = mkstrcpy(fp, (MALLOC_S *)NULL);
		if ((sb.st_mode & S_IFMT) == S_IFDIR && descend)

		/*
		 * Stack a subdirectory according to the descend argument.
		 */
		    stkdir(av[0]);
	    /*
	     * Use ck_file_arg() to record the entry for searching.  Force it
	     * to consider the entry a file, not a file system.
	     */
		if (!ck_file_arg(0, 1, av, 1, 1, &sb)) {
		    av[0] = (char *)NULL;
		    fct++;
		}
	    }
	    (void) CloseDir(dfp);
	    if (dn && dn != d) {
		(void) free((FREE_P *)dn);
		dn = (char *)NULL;
	    }
	}
/*
 * Free malloc()'d space.
 */
	if (dn && dn != d) {
	    (void) free((FREE_P *)dn);
	    dn = (char *)NULL;
	}
	if (av[0] && av[0] != fp) {
	    (void) free((FREE_P *)av[0]);
	    av[0] = (char *)NULL;
	}
	if (fp) {
	    (void) free((FREE_P *)fp);
	    fp = (char *)NULL;
	}
	if (Dstk) {
	    (void) free((FREE_P *)Dstk);
	    Dstk = (char **)NULL;
	}
	if (!fct) {

	/*
	 * Warn if no files were recorded for searching.
	 */
	    if (!Fwarn) {
		(void) fprintf(stderr,
		    "%s: WARNING: no files found in directory: ", Pn);
		safestrprt(d, stderr, 1);
	    }
	    return(1);
	}
	return(0);
}


/*
 * enter_id() - enter PGID or PID for searching
 */

int
enter_id(ty, p)
	enum IDType ty;			/* type: PGID or PID */
	char *p;			/* process group ID string pointer */
{
	char *cp;
	int err, i, id, j, mx, n, ni, nx, x;
	struct int_lst *s;

	if (!p) {
	    (void) fprintf(stderr, "%s: no process%s ID specified\n",
		Pn, (ty == PGID) ? " group" : "");
	    return(1);
	}
/*
 * Set up variables for the type of ID.
 */
	switch (ty) {
	case PGID:
	    mx = Mxpgid;
	    n = Npgid;
	    ni = Npgidi;
	    nx = Npgidx;
	    s = Spgid;
	    break;
	case PID:
	    mx = Mxpid;
	    n = Npid;
	    ni = Npidi;
	    nx = Npidx;
	    s = Spid;
	    break;
	default:
	    (void) fprintf(stderr, "%s: enter_id \"", Pn);
	    safestrprt(p, stderr, 0);
	    (void) fprintf(stderr, "\", invalid type: %d\n", ty);
	    Exit(1);
	}
/*
 * Convert and store the ID.
 */
	for (cp = p, err = 0; *cp;) {

	/*
	 * Assemble ID.
	 */
	    for (i = id = x = 0; *cp && *cp != ','; cp++) {
		if (!i) {
		    i = 1;
		    if (*cp == '^') {
			x = 1;
			continue;
		    }
		}

#if	defined(__STDC__)
		if (!isdigit((unsigned char)*cp))
#else	/* !defined(__STDC__) */
		if (!isascii(*cp) || ! isdigit((unsigned char)*cp))
#endif	/* __STDC__ */

		{
		    (void) fprintf(stderr, "%s: illegal process%s ID: ",
			Pn, (ty == PGID) ? " group" : "");
		    safestrprt(p, stderr, 1);
		    return(1);
		}
		id = (id * 10) + *cp - '0';
	    }
	    if (*cp)
		cp++;
	/*
	 * Avoid entering duplicates and conflicts.
	 */
	    for (i = j = 0; i < n; i++) {
		if (id == s[i].i) {
		    if (x == s[i].x) {
			j = 1;
			continue;
		    }
		    (void) fprintf(stderr,
			"%s: P%sID %d has been included and excluded.\n",
			Pn,
			(ty == PGID) ? "G" : "",
			id);
		    err = j = 1;
		    break;
		}
	    }
	    if (j)
		continue;
	/*
	 * Allocate table table space.
	 */
	    if (n >= mx) {
		mx += IDINCR;
		if (!s)
		    s = (struct int_lst *)malloc(
			(MALLOC_S)(sizeof(struct int_lst) * mx));
		else
		    s = (struct int_lst *)realloc((MALLOC_P *)s,
			(MALLOC_S)(sizeof(struct int_lst) * mx));
		if (!s) {
		    (void) fprintf(stderr, "%s: no space for %d process%s IDs",
			Pn, mx, (ty == PGID) ? " group" : "");
		    Exit(1);
		}
	    }
	    s[n].f = 0;
	    s[n].i = id;
	    s[n++].x = x;
	    if (x)
		nx++;
	    else
		ni++;
	}
/*
 * Save variables for the type of ID.
 */
	if (ty == PGID) {
	    Mxpgid = mx;
	    Npgid = n;
	    Npgidi = ni;
	    Npgidx = nx;
	    Spgid = s;
	} else {
	    Mxpid = mx;
	    Npid = Npuns = n;
	    Npidi = ni;
	    Npidx = nx;
	    Spid = s;
	}
	return(err);
}


/*
 * enter_network_address() - enter Internet address for searching
 */

int
enter_network_address(na)
	char *na;			/* Internet address string pointer */
{
	int ae, i, pr;
	int ep = -1;
	int ft = 0;
	struct hostent *he = (struct hostent *)NULL;
	char *hn = (char *)NULL;
	MALLOC_S l;
	struct nwad n;
	char *p, *wa;
	int pt = 0;
	int pu = 0;
	struct servent *se, *se1;
	char *sn = (char *)NULL;
	int sp = -1;
	MALLOC_S snl = 0;

#if	defined(HASIPv6)
	char *cp;
#endif	/* defined(HASIPv6) */

	if (!na) {
	    (void) fprintf(stderr, "%s: no network address specified\n", Pn);
	    return(1);
	}
	zeromem((char *)&n, sizeof(n));
	wa = na;
/*
 * Process an IP version type specification, IPv4 or IPv6, optionally followed
 * by a '@' and a host name or Internet address, or a ':' and a service name or
 * port number.
 */
	if ((*wa == '4') || (*wa == '6')) {
	    if (*wa == '4')
		ft = 4;
	    else if (*wa == '6') {

#if	defined(HASIPv6)
		ft = 6;
#else	/* !defined(HASIPv6) */
		(void) fprintf(stderr, "%s: IPv6 not supported: -i ", Pn);
		safestrprt(na, stderr, 1);
		goto nwad_exit;
#endif	/* defined(HASIPv6) */

	    }
	    wa++;
	    if (!*wa) {

	    /*
	     * If nothing follows 4 or 6, then all network files of the
	     * specified IP version are selected.  Sequential -i, -i4, and
	     * -i6 specifications interact logically -- e.g., -i[46] followed
	     * by -i[64] is the same as -i.
	     */
		if (!Fnet) {
		    Fnet = 1;
		    FnetTy = ft;
		} else {
		    if (FnetTy) {
			if (FnetTy != ft)
			    FnetTy = 0;
		    } else
			FnetTy = ft;
		}
		return(0);
	    }
	} else if (Fnet)
	    ft = FnetTy;
/*
 * If an IP version has been specified, use it to set the address family.
 */
	switch (ft) {
	case 4:
	    n.af = AF_INET;
	    break;

#if	defined(HASIPv6)
	case 6:
	    n.af = AF_INET6;
	    break;
#endif	/* defined(HASIPv6) */

	}
/*
 * Process protocol name, optionally followed by a '@' and a host name or
 * Internet address, or a ':' and a service name or port number.
 */
	if (*wa && *wa != '@' && *wa != ':') {
	    for (p = wa; *wa && *wa != '@' && *wa != ':'; wa++)
		;
	    if ((l = wa - p)) {
		if (!(n.proto = mkstrcat(p, l, (char *)NULL, -1, (char *)NULL,
			        -1, (MALLOC_S *)NULL)))
		{
		    (void) fprintf(stderr,
			"%s: no space for protocol name from: -i ", Pn);
		    safestrprt(na, stderr, 1);
nwad_exit:
		    if (n.proto)
			(void) free((FREE_P *)n.proto);
		    if (hn)
			(void) free((FREE_P *)hn);
		    if (sn)
			(void) free((FREE_P *)sn);
		    return(1);
		}
	    /*
	     * The protocol name should be "tcp", "udp" or "udplite".
	     */
		if ((strcasecmp(n.proto, "tcp") != 0)
		&&  (strcasecmp(n.proto, "udp") != 0)
		&&  (strcasecmp(n.proto, "udplite") != 0))
		{
		    (void) fprintf(stderr,
			"%s: unknown protocol name (%s) in: -i ", Pn, n.proto);
		    safestrprt(na, stderr, 1);
		    goto nwad_exit;
		}
	    /*
	     * Convert protocol name to lower case.
	     */
		for (p = n.proto; *p; p++) {
		    if (*p >= 'A' && *p <= 'Z')
			*p = *p - 'A' + 'a';
		}
	    }
	}
/*
 * Process an IPv4 address (1.2.3.4), IPv6 address ([1:2:3:4:5:6:7:8]),
 * or host name, preceded by a '@' and optionally followed by a colon
 * and a service name or port number.
 */
	if (*wa == '@') {
	    wa++;
	    if (!*wa || *wa == ':') {

#if	defined(HASIPv6)
unacc_address:
#endif	/* defined(HASIPv6) */

		(void) fprintf(stderr,
		    "%s: unacceptable Internet address in: -i ", Pn);
		safestrprt(na, stderr, 1);
		goto nwad_exit;
	    }

	    if ((p = isIPv4addr(wa, n.a, sizeof(n.a)))) {

	    /*
	     * Process IPv4 address.
	     */
		if (ft == 6) {
		    (void) fprintf(stderr,
			"%s: IPv4 addresses are prohibited: -i ", Pn);
		    safestrprt(na, stderr, 1);
		    goto nwad_exit;
		}
		wa = p;
		n.af = AF_INET;
	    } else if (*wa == '[') {

#if	defined(HASIPv6)
	    /*
	     * Make sure IPv6 addresses are permitted.  If they are, assemble
	     * one.
	     */
		if (ft == 4) {
		    (void) fprintf(stderr,
			"%s: IPv6 addresses are prohibited: -i ", Pn);
		    safestrprt(na, stderr, 1);
		    goto nwad_exit;
		}
		if (!(cp = strrchr(++wa, ']')))
		    goto unacc_address;
		*cp = '\0';
		i = inet_pton(AF_INET6, wa, (void *)&n.a);
		*cp = ']';
		if (i != 1)
		    goto unacc_address;
		for (ae = i = 0; i < MAX_AF_ADDR; i++) {
		    if ((ae |= n.a[i]))
			break;
		}
		if (!ae)
		    goto unacc_address;
		if (IN6_IS_ADDR_V4MAPPED((struct in6_addr *)&n.a[0])) {
		    if (ft == 6) {
			(void) fprintf(stderr,
			    "%s: IPv4 addresses are prohibited: -i ", Pn);
			safestrprt(na, stderr, 1);
			goto nwad_exit;
		    }
		    for (i = 0; i < 4; i++) {
			n.a[i] = n.a[i+12];
		    }
		    n.af = AF_INET;
		} else
		    n.af = AF_INET6;
		wa = cp + 1;
#else	/* !defined(HASIPv6) */
		(void) fprintf(stderr,
		    "%s: unsupported IPv6 address in: -i ", Pn);
		safestrprt(na, stderr, 1);
		goto nwad_exit;
#endif	/* defined(HASIPv6) */

	    } else {

	    /*
	     * Assemble host name.
	     */
		for (p = wa; *p && *p != ':'; p++)
		    ;
		if ((l = p - wa)) {
		    if (!(hn = mkstrcat(wa, l, (char *)NULL, -1, (char *)NULL,
			       -1, (MALLOC_S *)NULL)))
		    {
			(void) fprintf(stderr,
			    "%s: no space for host name: -i ", Pn);
			safestrprt(na, stderr, 1);
			goto nwad_exit;
		    }

#if	defined(HASIPv6)

		/*
		 * If no IP version has been specified, look up an IPv6 host
		 * name first.  If that fails, look up an IPv4 host name.
		 *
		 * If the IPv6 version has been specified, look up the host
		 * name only under its IP version specification.
		 */
		    if (!ft)
			n.af = AF_INET6;
		    if (!(he = lkup_hostnm(hn, &n)) && !ft) {
			n.af = AF_INET;
			he = lkup_hostnm(hn, &n);
		    }
#else	/* !defined(HASIPv6) */
		    if (!ft)
			n.af = AF_INET;
		    he = lkup_hostnm(hn, &n);
#endif	/* defined(HASIPv6) */
		
		    if (!he) {
			fprintf(stderr, "%s: unknown host name (%s) in: -i ",
			    Pn, hn);
			safestrprt(na, stderr, 1);
			goto nwad_exit;
		    }
		}
		wa = p;
	    }
	}
/*
 * If there is no port number, enter the address.
 */
	if (!*wa)
	    goto nwad_enter;
/*
 * Process a service name or port number list, preceded by a colon.
 *
 * Entries of the list are separated with commas; elements of a numeric range
 * are specified with a separating minus sign (`-'); all service names must
 * belong to the same protocol; embedded spaces are not allowed.  An embedded
 * minus sign in a name is taken to be part of the name, the starting entry
 * of a range can't be a service name.
 */
	if (*wa != ':' || *(wa + 1) == '\0') {

unacc_port:
	    (void) fprintf(stderr,
		"%s: unacceptable port specification in: -i ", Pn);
	    safestrprt(na, stderr, 1);
	    goto nwad_exit;
	}
	for (++wa; wa && *wa; wa++) {
	    for (ep = pr = sp = 0; *wa; wa++) {
		if (*wa < '0' || *wa > '9') {

		/*
		 * Convert service name to port number, using already-specified
		 * protocol name.  A '-' is taken to be part of the name; hence
		 * the starting entry of a range can't be a service name.
		 */
		    for (p = wa; *wa && *wa != ','; wa++)
			;
		    if (!(l = wa - p)) {
			(void) fprintf(stderr,
			    "%s: invalid service name: -i ", Pn);
			safestrprt(na, stderr, 1);
			goto nwad_exit;
		    }
		    if (sn) {
			if (l > snl) {
			    sn = (char *)realloc((MALLOC_P *)sn, l + 1);
			    snl = l;
			}
		    } else {
			sn = (char *)malloc(l + 1);
			snl = l;
		    }
		    if (!sn) {
			(void) fprintf(stderr,
			    "%s: no space for service name: -i ", Pn);
			safestrprt(na, stderr, 1);
			goto nwad_exit;
		    }
		    (void) strncpy(sn, p, l);
		    *(sn + l) = '\0';
		    if (n.proto) {

		    /*
		     * If the protocol has been specified, look up the port
		     * number for the service name for the specified protocol.
		     */
			if (!(se = getservbyname(sn, n.proto))) {
			    (void) fprintf(stderr,
				"%s: unknown service %s for %s in: -i ",
				Pn, sn, n.proto);
			    safestrprt(na, stderr, 1);
			    goto nwad_exit;
			}
			pt = (int)ntohs(se->s_port);
		    } else {

		    /*
		     * If no protocol has been specified, look up the port
		     * numbers for the service name for both TCP and UDP.
		     */
			if((se = getservbyname(sn, "tcp")))
			    pt = (int)ntohs(se->s_port);
			if ((se1 = getservbyname(sn, "udp")))
			    pu = (int)ntohs(se1->s_port);
			if (!se && !se1) {
			    (void) fprintf(stderr,
				"%s: unknown service %s in: -i ", Pn, sn);
			    safestrprt(na, stderr, 1);
			    goto nwad_exit;
			}
			if (se && se1 && pt != pu) {
			    (void) fprintf(stderr,
				"%s: TCP=%d and UDP=%d %s ports conflict;\n",
				Pn, pt, pu, sn);
			    (void) fprintf(stderr,
				"      specify \"tcp:%s\" or \"udp:%s\": -i ",
				sn, sn);
			    safestrprt(na, stderr, 1);
			    goto nwad_exit;
			}
			if (!se && se1)
			    pt = pu;
		    }
		    if (pr)
			ep = pt;
		    else {
			sp = pt;
			if (*wa == '-')
			    pr++;
		    }
		} else {

		/*
		 * Assemble port number.
		 */
		    for (; *wa && *wa != ','; wa++) {
			if (*wa == '-') {
			    if (pr)
				goto unacc_port;
			    pr++;
			    break;
			}
			if (*wa < '0' || *wa > '9')
			    goto unacc_port;
			if (pr)
			    ep = (ep * 10) + *wa - '0';
			else
			    sp = (sp * 10) + *wa - '0';
		    }
		}
		if (!*wa || *wa == ',')
		    break;
		if (pr)
		    continue;
		goto unacc_port;
	    }
	    if (!pr)
		ep = sp;
	    if (ep < sp)
		goto unacc_port;
	/*
	 * Enter completed port or port range specification.
	 */

nwad_enter:

	    for (i = 1; i;) {
		if (enter_nwad(&n, sp, ep, na, he))
		    goto nwad_exit;

#if	defined(HASIPv6)
	    /*
	     * If IPv6 is enabled, a host name was specified, and the
	     * associated * address is for the AF_INET6 address family,
	     * try to get and address for the AF_INET family, too, unless
	     * IPv4 is prohibited.
	     */
		if (hn && (n.af == AF_INET6) && (ft != 6)) {
		    n.af = AF_INET;
		    if ((he = lkup_hostnm(hn, &n)))
			continue;
		}
#endif	/* defined(HASIPv6) */

		i = 0;
	    }
	    if (!*wa)
		break;
	}
	if (sn)
	    (void) free((FREE_P *)sn);
	return(0);
}

/*
 * enter_nwad() - enter nwad structure
 */

static int
enter_nwad(n, sp, ep, s, he)
	struct nwad *n;			/* pointer to partially completed
					 * nwad (less port) */
	int sp;				/* starting port number */
	int ep;				/* ending port number */
	char *s;			/* string that states the address */
	struct hostent *he;		/* pointer to hostent struct from which
					 * network address came */
{
	int ac;
	unsigned char *ap;
	static int na = 0;
	struct nwad nc;
	struct nwad *np;
/*
 * Allocate space for the argument specification.
 */
	if (strlen(s)) {
	    if (!(n->arg = mkstrcpy(s, (MALLOC_S *)NULL))) {
		(void) fprintf(stderr,
		    "%s: no space for Internet argument: -i ", Pn);
		safestrprt(s, stderr, 1);
		Exit(1);
	    }
	} else
	    n->arg = (char *)NULL;
/*
 * Loop through all hostent addresses.
 */
	for (ac = 1, nc = *n;;) {

	/*
	 * Test address specification -- it must contain at least one of:
	 * protocol, Internet address or port.  If correct, link into search
	 * list.
	 */
	    if (!nc.proto
	    &&  !nc.a[0] && !nc.a[1] && !nc.a[2] && !nc.a[3]

#if	defined(HASIPv6)
	    &&  (nc.af != AF_INET6
	    ||   (!nc.a[4]  && !nc.a[5]  && !nc.a[6]  && !nc.a[7]
	    &&    !nc.a[8]  && !nc.a[9]  && !nc.a[10] && !nc.a[11]
	    &&    !nc.a[12] && !nc.a[13] && !nc.a[14] && !nc.a[15]))
#endif	/* defined(HASIPv6) */

	    &&  sp == -1) {
		(void) fprintf(stderr,
		    "%s: incomplete Internet address specification: -i ", Pn);
		safestrprt(s, stderr, 1);
		return(1);
	    }
	/*
	 * Limit the network address chain length to MAXNWAD for reasons of
	 * search efficiency.
	 */
	    if (na >= MAXNWAD) {
		(void) fprintf(stderr,
		    "%s: network address limit (%d) exceeded: -i ",
		    Pn, MAXNWAD);
		safestrprt(s, stderr, 1);
		return(1);
	    }
	/*
	 * Allocate space for the address specification.
	 */
	    if ((np = (struct nwad *)malloc(sizeof(struct nwad))) == NULL) {
		(void) fprintf(stderr,
		    "%s: no space for network address from: -i ", Pn);
		safestrprt(s, stderr, 1);
		return(1);
	    }
	/*
	 * Construct and link the address specification.
	 */
	    *np = nc;
	    np->sport = sp;
	    np->eport = ep;
	    np->f = 0;
	    np->next = Nwad;
	    Nwad = np;
	    na++;
	/*
	 * If the network address came from gethostbyname(), advance to
	 * the next address; otherwise quit.
	 */
	    if (!he)
		break;
	    if (!(ap = (unsigned char *)he->h_addr_list[ac++]))
		break;

#if	defined(HASIPv6)
	    {
		int i;

		for (i = 0;
		     (i < (he->h_length - 1)) && (i < (MAX_AF_ADDR - 1));
		     i++)
		{
		    nc.a[i] = *ap++;
		}
		nc.a[i] = *ap;
	    }
#else	/* !defined(HASIPv6) */
	    nc.a[0] = *ap++;
	    nc.a[1] = *ap++;
	    nc.a[2] = *ap++;
	    nc.a[3] = *ap;
#endif	/* defined(HASIPv6) */

	}
	return(0);
}


#if	defined(HASTCPUDPSTATE)
/*
 * enter_state_spec() -- enter TCP and UDP state specifications
 */

int
enter_state_spec(ss)
	char *ss;			/* state specification string */
{
	char *cp, *ne, *ns, *pr;
	int err, d, f, i, tx, x;
	size_t len;
	static char *ssc = (char *)NULL;
	char *ty;
/*
 * Check the protocol specification.
 */
	if (!strncasecmp(ss, "tcp:", 4)) {
	    pr = "TCP";
	    tx = 0;
	}

#if	!defined(USE_LIB_PRINT_TCPTPI)
	else if (!strncasecmp(ss, "UDP:", 4)) {
	    pr = "UDP";
	    tx = 1;
	}

#endif	/* !defined(USE_LIB_PRINT_TCPTPI) */

	else {
	    (void) fprintf(stderr, "%s: unknown -s protocol: \"%s\"\n",
		Pn, ss);
	    return(1);
	}
	cp = ss + 4;
	if (!*cp) {
	    (void) fprintf(stderr, "%s: no %s state names in: %s\n",
		Pn, pr, ss);
	    return(1);
	}
	(void) build_IPstates();
	if (!(tx ? UdpSt : TcpSt)) {
	    (void) fprintf(stderr, "%s: no %s state names available: %s\n",
		Pn, pr, ss);
	    return(1);
	}
/*
 * Allocate the inclusion and exclusion tables for the protocol.
 */
	if (tx) {
	    if (UdpNstates) {
		if (!UdpStI) {
		    if (!(UdpStI = (unsigned char *)calloc((MALLOC_S)UdpNstates,
				   sizeof(unsigned char))))
		    {
			ty = "UDP state inclusion";

no_IorX_space:

			(void) fprintf(stderr, "%s: no %s table space\n",
			    Pn, ty);
			Exit(1);
		    }
		}
		if (!UdpStX) {
		    if (!(UdpStX = (unsigned char *)calloc((MALLOC_S)UdpNstates,
				   sizeof(unsigned char))))
		    {
			ty = "UDP state exclusion";
			goto no_IorX_space;
		    }
		}
	    }
	} else {
	    if (TcpNstates) {
		if (!TcpStI) {
		    if (!(TcpStI = (unsigned char *)calloc((MALLOC_S)TcpNstates,
				   sizeof(unsigned char))))
		    {
			ty = "TCP state inclusion";
			goto no_IorX_space;
		    }
		}
		if (!TcpStX) {
		    if (!(TcpStX = (unsigned char *)calloc((MALLOC_S)TcpNstates,
				   sizeof(unsigned char))))
		    {
			ty = "TCP state exclusion";
			goto no_IorX_space;
		    }
		}
	    }
	}
/*
 * Convert the state names in the rest of the string to state indexes and
 * record them in the appropriate inclusion or exclusion table.
 */
	if (ssc)
	    (void) free((MALLOC_P *)ssc);
	if (!(ssc = mkstrcpy(cp, (MALLOC_S *)NULL))) {
	    (void) fprintf(stderr,
		"%s: no temporary state argument space for: %s\n", Pn, ss);
	    Exit(1);
	}
	cp = ssc;
	err = 0;
	while (*cp) {
	
	/*
	 * Determine inclusion or exclusion for this state name.
	 */
	    if (*cp == '^') {
		x = 1;
		cp++;
	    } else
		x = 0;
	/*
	 * Find the end of the state name.  Make sure it is non-null in length
	 * and terminated with '\0'.
	 */
	    ns = cp;
	    while (*cp && (*cp != ',')) {
		cp++;
	    }
	    ne = cp;
	    if (*cp) {
		*cp = '\0';
		cp++;
	    }
	    if (!(len = (size_t)(ne - ns))) {
		(void) fprintf(stderr, "%s: NULL %s state name in: %s\n",
		    Pn, pr, ss);
		err = 1;
		continue;
	    }
	/*
	 * Find the state name in the appropriate table.
	 */
	    f = 0;
	    if (tx) {
		if (UdpSt) {
		    for (i = 0; i < UdpNstates; i++) {
			if (!strcasecmp(ns, UdpSt[i])) {
			    f = 1;
			    break;
			}
		    }
		}
	    } else {
		if (TcpSt) {
		    for (i = 0; i < TcpNstates; i++) {
			if (!strcasecmp(ns, TcpSt[i])) {
			    f = 1;
			    break;
			}
		    }
		}
	    }
	    if (!f) {
		(void) fprintf(stderr, "%s: unknown %s state name: %s\n",
		    Pn, pr, ns);
		err = 1;
		continue;
	    }
	/*
	 * Set the inclusion or exclusion status in the appropriate table.
	 */
	    d = 0;
	    if (x) {
		if (tx) {
		    if (!UdpStX[i]) {
			UdpStX[i] = 1;
			UdpStXn++;
		    } else
			d = 1;
		} else {
		    if (!TcpStX[i]) {
			TcpStX[i] = 1;
			TcpStXn++;
		    } else
			d = 1;
		}
	    } else {
		if (tx) {
		    if (!UdpStI[i]) {
			UdpStI[i] = 1;
			UdpStIn++;
		    } else
			d = 1;
		} else {
		    if (!TcpStI[i]) {
			TcpStI[i] = 1;
			TcpStIn++;
		    } else
			d = 1;
		}
	    }
	    if (d) {

	    /*
	     * Report a duplicate.
	     */
		(void) fprintf(stderr, "%s: duplicate %s %sclusion: %s\n",
		    Pn, pr,
		    x ? "ex" : "in",
		    ns);
		err = 1;
	    }
	}
/*
 * Release any temporary space and return.
 */
	if (ssc) {
	    (void) free((MALLOC_P *)ssc);
	    ssc = (char *)NULL;
	}
	return(err);
}
#endif	/* defined(HASTCPUDPSTATE) */


/*
 * enter_str_lst() - enter a string on a list
 */

int
enter_str_lst(opt, s, lp, incl, excl)
	char *opt;			/* option name */
	char *s;			/* string to enter */
	struct str_lst **lp;		/* string's list */
	int *incl;			/* included count */
	int *excl;			/* excluded count */
{
	char *cp;
	short i, x;
	MALLOC_S len;
	struct str_lst *lpt;

	if (!s || *s == '-' || *s == '+') {
	    (void) fprintf(stderr, "%s: missing %s option value\n",
		Pn, opt);
	    return(1);
	}
	if (*s == '^') {
	    i = 0;
	    x = 1;
	    s++;
	} else {
	    i = 1;
	    x = 0;
	}
	if (!(cp = mkstrcpy(s, &len))) {
	    (void) fprintf(stderr, "%s: no string copy space: ", Pn);
	    safestrprt(s, stderr, 1);
	    return(1);
	}
	if ((lpt = (struct str_lst *)malloc(sizeof(struct str_lst))) == NULL) {
	    (void) fprintf(stderr, "%s: no list space: ", Pn);
	    safestrprt(s, stderr, 1);
	    (void) free((FREE_P *)cp);
	    return(1);
	}
	lpt->f = 0;
	lpt->str = cp;
	lpt->len = (int)len;
	lpt->x = x;
	if (i)
	    *incl += 1;
	if (x)
	    *excl += 1;
	lpt->next = *lp;
	*lp = lpt;
	return(0);
}


/*
 * enter_uid() - enter User Identifier for searching
 */

int
enter_uid(us)
	char *us;			/* User IDentifier string pointer */
{
	int err, i, j, lnml, nn;
	unsigned char excl;
	MALLOC_S len;
	char lnm[LOGINML+1], *lp;
	struct passwd *pw;
	char *s, *st;
	uid_t uid;

	if (!us) {
	    (void) fprintf(stderr, "%s: no UIDs specified\n", Pn);
	    return(1);
	}
	for (err = 0, s = us; *s;) {

	/*
	 * Assemble next User IDentifier.
	 */
	    for (excl = i = j = lnml = nn = uid = 0, st = s;
		 *s && *s != ',';
		 i++, s++)
	    {
		if (lnml >= LOGINML) {
		    while (*s && *s != ',') {
			s++;
			lnml++;
		    }
		    (void) fprintf(stderr,
			"%s: -u login name > %d characters: ", Pn,
			    (int)LOGINML);
		    safestrprtn(st, lnml, stderr, 1);
		    err = j = 1;
		    break;
		}
		if (i == 0 && *s == '^') {
		    excl = 1;
		    continue;
		}
		lnm[lnml++] = *s;
		if (nn)
		    continue;

#if	defined(__STDC__)
		if (isdigit((unsigned char)*s))
#else	/* !defined(__STDC__) */
		if (isascii(*s) && isdigit((unsigned char)*s))
#endif	/* defined(__STDC__) */

		    uid = (uid * 10) + *s - '0';
		else
		    nn++;
	    }
	    if (*s)
		s++;
	    if (j)
		continue;
	    if (nn) {
	       lnm[lnml++] = '\0';
		if ((pw = getpwnam(lnm)) == NULL) {
		    (void) fprintf(stderr, "%s: can't get UID for ", Pn);
		    safestrprt(lnm, stderr, 1);
		    err = 1;
		    continue;
		} else
		    uid = pw->pw_uid;
	    }

#if	defined(HASSECURITY) && !defined(HASNOSOCKSECURITY)
	/*
	 * If the security mode is enabled, only the root user may list files
	 * belonging to user IDs other than the real user ID of this lsof
	 * process.  If HASNOSOCKSECURITY is also defined, then anyone may
	 * list anyone else's socket files.
	 */
	    if (Myuid && uid != Myuid) {
		(void) fprintf(stderr,
		    "%s: ID %d request rejected because of security mode.\n",
		    Pn, uid);
		err = 1;
		continue;
	    }
#endif	/* defined(HASSECURITY)  && !defined(HASNOSOCKSECURITY) */

	/*
	 * Avoid entering duplicates.
	 */
	    for (i = j = 0; i < Nuid; i++) {
		if (uid != Suid[i].uid)
		    continue;
		if (Suid[i].excl == excl) {
		    j = 1;
		    continue;
		}
		(void) fprintf(stderr,
		    "%s: UID %d has been included and excluded.\n",
			Pn, (int)uid);
		err = j = 1;
		break;
	    }
	    if (j)
		continue;
	/*
	 * Allocate space for User IDentifier.
	 */
	    if (Nuid >= Mxuid) {
		Mxuid += UIDINCR;
		len = (MALLOC_S)(Mxuid * sizeof(struct seluid));
		if (!Suid)
		    Suid = (struct seluid *)malloc(len);
		else
		    Suid = (struct seluid *)realloc((MALLOC_P *)Suid, len);
		if (!Suid) {
		    (void) fprintf(stderr, "%s: no space for UIDs", Pn);
		    Exit(1);
		}
	    }
	    if (nn) {
		if (!(lp = mkstrcpy(lnm, (MALLOC_S *)NULL))) {
		    (void) fprintf(stderr, "%s: no space for login: ", Pn);
		    safestrprt(lnm, stderr, 1);
		    Exit(1);
		}
		Suid[Nuid].lnm = lp;
	    } else
		Suid[Nuid].lnm = (char *)NULL;
	    Suid[Nuid].uid = uid;
	    Suid[Nuid++].excl = excl;
	    if (excl)
		Nuidexcl++;
	    else
		Nuidincl++;
	}
	return(err);
}


/*
 * isIPv4addr() - is host name an IPv4 address
 */

static char *
isIPv4addr(hn, a, al)
	char *hn;			/* host name */
	unsigned char *a;		/* address receptor */
	int al;				/* address receptor length */
{
	int dc = 0;			/* dot count */
	int i;				/* temorary index */
	int ov[MIN_AF_ADDR];		/* octet values */
	int ovx = 0;			/* ov[] index */
/*
 * The host name must begin with a number and the return octet value
 * arguments must be acceptable.
 */
	if ((*hn < '0') || (*hn > '9'))
	    return((char *)NULL);
	if (!a || (al < MIN_AF_ADDR))
	    return((char *)NULL);
/*
 * Start the first octet assembly, then parse tge remainder of the host
 * name for four octets, separated by dots.
 */
	ov[0] = (int)(*hn++ - '0');
	while (*hn && (*hn != ':')) {
	    if (*hn == '.') {

	    /*
	     * Count a dot.  Make sure a preceding octet value has been
	     * assembled.  Don't assemble more than MIN_AF_ADDR octets.
	     */
		dc++;
		if ((ov[ovx] < 0) || (ov[ovx] > 255))
		    return((char *)NULL);
		if (++ovx > (MIN_AF_ADDR - 1))
		    return((char *)NULL);
		ov[ovx] = -1;
	    } else if ((*hn >= '0') && (*hn <= '9')) {

	    /*
	     * Assemble an octet.
	     */
		if (ov[ovx] < 0)
		    ov[ovx] = (int)(*hn - '0');
		else
		    ov[ovx] = (ov[ovx] * 10) + (int)(*hn - '0');
	    } else {

	    /*
	     * A non-address character has been detected.
	     */
		return((char *)NULL);
	    }
	    hn++;
	}
/*
 * Make sure there were three dots and four non-null octets.
 */
	if ((dc != 3)
	||  (ovx != (MIN_AF_ADDR - 1))
	||  (ov[ovx] < 0) || (ov[ovx] > 255))
	    return((char *)NULL);
/*
 * Copy the octets as unsigned characters and return the ending host name
 * character position.
 */
	for (i = 0; i < MIN_AF_ADDR; i++) {
	     a[i] = (unsigned char)ov[i];
	}
	return(hn);
}


/*
 * lkup_hostnm() - look up host name
 */

static struct hostent *
lkup_hostnm(hn, n)
	char *hn;			/* host name */
	struct nwad *n;			/* network address destination */
{
	unsigned char *ap;
	struct hostent *he;
	int ln;
/*
 * Get hostname structure pointer.  Return NULL if there is none.
 */

#if	defined(HASIPv6)
	he = gethostbyname2(hn, n->af);
#else	/* !defined(HASIPv6) */
	he = gethostbyname(hn);
#endif	/* defined(HASIPv6) */

	if (!he)
	    return(he);
/*
 * Copy first hostname structure address to destination structure.
 */

#if	defined(HASIPv6)
	if (n->af != he->h_addrtype)
	    return((struct hostent *)NULL);
	if (n->af == AF_INET6) {

	/*
	 * Copy an AF_INET6 address.
	 */
	    if (he->h_length > MAX_AF_ADDR)
		return((struct hostent *)NULL);
	    (void) memcpy((void *)&n->a[0], (void *)he->h_addr, he->h_length);
	    if ((ln = MAX_AF_ADDR - he->h_length) > 0)
		zeromem((char *)&n->a[he->h_length], ln);
	    return(he);
	}
#endif	/* defined(HASIPv6) */

/*
 * Copy an AF_INET address.
 */
	if (he->h_length != 4)
	    return((struct hostent *)NULL);
	ap = (unsigned char *)he->h_addr;
	n->a[0] = *ap++;
	n->a[1] = *ap++;
	n->a[2] = *ap++;
	n->a[3] = *ap;
	if ((ln = MAX_AF_ADDR - 4) > 0)
	    zeromem((char *)&n->a[4], ln);
	return(he);
}
