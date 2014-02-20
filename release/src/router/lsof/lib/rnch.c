/*
 * rnch.c -- Sun format name cache functions for lsof library
 */


/*
 * Copyright 1997 Purdue Research Foundation, West Lafayette, Indiana
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


#include "../machine.h"

#if	defined(HASNCACHE) && defined(USE_LIB_RNCH)

# if	!defined(lint)
static char copyright[] =
"@(#) Copyright 1997 Purdue Research Foundation.\nAll rights reserved.\n";
static char *rcsid = "$Id: rnch.c,v 1.11 2008/10/21 16:13:23 abe Exp $";
# endif	/* !defined(lint) */

#include "../lsof.h"


/*
 * rnch.c - read Sun format (struct ncache) name cache
 *
 * This code is effective only when HASNCACHE is defined.
 */

/*
 * The caller must:
 *
 *	#include the relevant header file -- e.g., <sys/dnlc.h>.
 *
 *	Define X_NCSIZE as the nickname for the kernel cache size variable,
 *	or, if X_NCSIZE is undefined, define FIXED_NCSIZE as the size of the
 *	kernel cache.
 *
 *	Define X_NCACHE as the nickname for the kernel cache address and
 *	define ADDR_NCACHE if the address is the address of the cache,
 *	rather than the address of a pointer to it.
 *
 *	Define NCACHE_NXT if the kernel's name cache is a linked list, starting
 *	at the X_NCACHE address, rather than a table, starting at that address.
 *
 *	Define any of the following casts that differ from their defaults:
 *
 *		NCACHE_SZ_CAST	cast for X_NCACHE (default int)
 *
 * The caller may:
 *
 *	Define NCACHE_DP	as the name of the element in the
 *				ncache structure that contains the
 *				parent vnode pointer.
 *
 *				Default: dp
 *
 *	Define NCACHE_NAME	as the name of the element in the
 *				ncache structure that contains the
 *				name.
 *
 *				Default: name
 *
 *	Define NCACHE_NAMLEN	as the name of the element in the
 *				ncache structure that contains the
 *				name length.
 *
 *				Deafult: namlen
 *
 *	Define NCACHE_NEGVN	as the name of the name list element
 *				whose value is a vnode address to
 *				ignore when loading the kernel name
 *				cache.
 *
 *	Define NCACHE_NODEID	as the name of the element in the
 *				ncache structure that contains the
 *				vnode's capability ID.
 *
 *	Define NCACHE_PARID	as the name of the element in the
 *				ncache structure that contains the
 *				parent vnode's capability ID.
 *
 *	Define NCACHE_VP	as the name of the element in the
 *				ncache structure that contains the
 *				vnode pointer.
 *
 *				Default: vp
 *
 * Note: if NCACHE_NODEID is defined, then NCACHE_PARID must be defined.
 *
 *
 * The caller must:
 *
 *	Define this prototype for ncache_load():
 *
 *		_PROTOTYPE(void ncache_load,(void));
 */


/*
 * Local static values
 */

static int Mch;				/* name cache hash mask */

# if	!defined(NCACHE_NC_CAST)
#define	NCACHE_SZ_CAST	int
# endif	/* !defined(NCACHE_NC_CAST) */

static NCACHE_SZ_CAST Nc = 0;		/* size of name cache */
static int Nch = 0;			/* size of name cache hash pointer
					 * table */
struct l_nch {
	KA_T vp;			/* vnode address */
	KA_T dp;			/* parent vnode address */
	struct l_nch *pa;		/* parent Ncache address */

# if	defined(NCACHE_NODEID)
	unsigned long id;		/* node's capability ID */
	unsigned long did;		/* parent node's capability ID */
# endif	/* defined(NCACHE_NODEID) */

	char *nm;			/* name */
	int nl;				/* name length */
};

static struct l_nch *Ncache = (struct l_nch *)NULL;
					/* the local name cache */
static struct l_nch **Nchash = (struct l_nch **)NULL;
					/* Ncache hash pointers */
static int Ncfirst = 1;			/* first-call status */

# if 	defined(NCACHE_NEGVN)
static KA_T NegVN = (KA_T)NULL;		/* negative vnode address */
static int NegVNSt = 0;			/* NegVN status: 0 = not loaded */
# endif	/* defined(NCACHE_NEGVN) */

# if	defined(NCACHE_NODEID)
_PROTOTYPE(static struct l_nch *ncache_addr,(unsigned long i, KA_T v));
#define ncachehash(i,v)		Nchash+(((((int)(v)>>2)+((int)(i)))*31415)&Mch)
# else	/* !defined(NCACHE_NODEID) */
_PROTOTYPE(static struct l_nch *ncache_addr,(KA_T v));
#define ncachehash(v)		Nchash+((((int)(v)>>2)*31415)&Mch)
# endif	/* defined(NCACHE_NODEID) */

_PROTOTYPE(static int ncache_isroot,(KA_T va, char *cp));

#define DEFNCACHESZ	1024	/* local size if X_NCSIZE kernel value < 1 */
#define	LNCHINCRSZ	64	/* local size increment */

# if	!defined(NCACHE_DP)
#define	NCACHE_DP	dp
# endif	/* !defined(NCACHE_DP) */

# if	!defined(NCACHE_NAME)
#define	NCACHE_NAME	name
# endif	/* !defined(NCACHE_NAME) */

# if	!defined(NCACHE_NAMLEN)
#define	NCACHE_NAMLEN	namlen
# endif	/* !defined(NCACHE_NAMLEN) */

# if	!defined(NCACHE_VP)
#define	NCACHE_VP	vp
# endif	/* !defined(NCACHE_VP) */


/*
 * ncache_addr() - look up a node's local ncache address
 */

static struct l_nch *

# if	defined(NCACHE_NODEID)
ncache_addr(i, v)
# else	/* !defined(NCACHE_NODEID) */
ncache_addr(v)
# endif	/* defined(NCACHE_NODEID) */

# if	defined(NCACHE_NODEID)
	unsigned long i;			/* capability ID */
# endif	/* defined(NCACHE_NODEID) */

	KA_T v;					/* vnode's address */
{
	struct l_nch **hp;

# if	defined(NCACHE_NODEID)
	for (hp = ncachehash(i, v); *hp; hp++)
# else	/* !defined(NCACHE_NODEID) */
	for (hp = ncachehash(v); *hp; hp++)
# endif	/* defined(NCACHE_NODEID) */

	{

# if	defined(NCACHE_NODEID)
	    if ((*hp)->vp == v && (*hp)->id == i)
# else	/* !defined(NCACHE_NODEID) */
	    if ((*hp)->vp == v)
# endif	/* defined(NCACHE_NODEID) */

		return(*hp);
	}
	return((struct l_nch *)NULL);
}


/*
 * ncache_isroot() - is head of name cache path a file system root?
 */

static int
ncache_isroot(va, cp)
	KA_T va;			/* kernel vnode address */
	char *cp;			/* partial path */
{
	char buf[MAXPATHLEN];
	int i;
	MALLOC_S len;
	struct mounts *mtp;
	struct stat sb;
	struct vnode v;
	static int vca = 0;
	static int vcn = 0;
	static KA_T *vc = (KA_T *)NULL;

	if (!va)
	    return(0);
/*
 * Search the root vnode cache.
 */
	for (i = 0; i < vcn; i++) {
	    if (va == vc[i])
		return(1);
	}
/*
 * Read the vnode and see if it's a VDIR node with the VROOT flag set.  If
 * it is, then the path is complete.
 *
 * If it isn't, and if the file has an inode number, search the mount table
 * and see if the file system's inode number is known.  If it is, form the
 * possible full path, safely stat() it, and see if it's inode number matches
 * the one we have for this file.  If it does, then the path is complete.
 */
	if (kread((KA_T)va, (char *)&v, sizeof(v))
	||  v.v_type != VDIR || !(v.v_flag & VROOT)) {

	/*
	 * The vnode tests failed.  Try the inode tests.
	 */
	    if (Lf->inp_ty != 1 || !Lf->inode
	    ||  !Lf->fsdir || (len = strlen(Lf->fsdir)) < 1)
		return(0);
	    if ((len + 1 + strlen(cp) + 1) > sizeof(buf))
		return(0);
	    for (mtp = readmnt(); mtp; mtp = mtp->next) {
		if (!mtp->dir || !mtp->inode)
		    continue;
		if (strcmp(Lf->fsdir, mtp->dir) == 0)
		    break;
	    }
	    if (!mtp)
		return(0);
	    (void) strcpy(buf, Lf->fsdir);
	    if (buf[len - 1] != '/')
		buf[len++] = '/';
	    (void) strcpy(&buf[len], cp);
	    if (statsafely(buf, &sb) != 0
	    ||  (unsigned long)sb.st_ino != Lf->inode)
		return(0);
	}
/*
 * Add the vnode address to the root vnode cache.
 */
	if (vcn >= vca) {
	    vca += 10;
	    len = (MALLOC_S)(vca * sizeof(KA_T));
	    if (!vc)
		vc = (KA_T *)malloc(len);
	    else
		vc = (KA_T *)realloc(vc, len);
	    if (!vc) {
		(void) fprintf(stderr, "%s: no space for root vnode table\n",
		    Pn);
		Exit(1);
	    }
	}
	vc[vcn++] = va;
	return(1);
}


/*
 * ncache_load() - load the kernel's name cache
 */

void
ncache_load()
{
	char *cp, *np;
	struct l_nch **hp, *lc;
	int i, len, n;
	static int iNc = 0;
	struct ncache *kc;
	static KA_T kp = (KA_T)NULL;
	KA_T v;

# if	defined(HASDNLCPTR)
	static int na = 0;
	static char *nb = (char *)NULL;
# endif	/* defined(HASDNLCPTR) */

# if	defined(NCACHE_NXT)
	static KA_T kf;
	struct ncache nc;
# else	/* !defined(NCACHE_NXT) */
	static struct ncache *kca = (struct ncache *)NULL;
# endif	/* defined(NCACHE_NXT) */

	if (!Fncache)
	    return;
	if (Ncfirst) {

	/*
	 * Do startup (first-time) functions.
	 */
	    Ncfirst = 0;
	/*
	 * Establish kernel cache size.
	 */

# if	defined(X_NCSIZE)
	    v = (KA_T)0;
	    if (get_Nl_value(X_NCSIZE, (struct drive_Nl *)NULL, &v) < 0
	    ||  !v
	    ||  kread((KA_T)v, (char *)&Nc, sizeof(Nc)))
	    {
		if (!Fwarn)
		(void) fprintf(stderr,
		    "%s: WARNING: can't read name cache size: %s\n",
			Pn, print_kptr(v, (char *)NULL, 0));
		iNc = Nc = 0;
		return;
	    }
	    iNc = Nc;
# else	/* !defined(X_NCSIZE) */
	    iNc = Nc = FIXED_NCSIZE;
# endif	/* defined(X_NCSIZE) */

	    if (Nc < 1) {
		if (!Fwarn) {
		    (void) fprintf(stderr,
			"%s: WARNING: kernel name cache size: %d\n", Pn, Nc);
		    (void) fprintf(stderr,
			"      Cache size assumed to be: %d\n", DEFNCACHESZ);
		}
		iNc = Nc = DEFNCACHESZ;
	    }

# if	defined(NCACHE_NEGVN)
	/*
	 * Get negative vnode address.
	 */
	    if (!NegVNSt) {
		if (get_Nl_value(NCACHE_NEGVN, (struct drive_Nl *)NULL, &NegVN)
		< 0)
		    NegVN = (KA_T)NULL;
		NegVNSt = 1;
	    }
# endif	/* defined(NCACHE_NEGVN) */

	/*
	 * Establish kernel cache address.
	 */

# if	defined(ADDR_NCACHE)
	    kp = (KA_T)0;
	    if (get_Nl_value(X_NCACHE,(struct drive_Nl *)NULL,(KA_T *)&kp) < 0
	    || !kp) {
		if (!Fwarn)
		    (void) fprintf(stderr,
			"%s: WARNING: no name cache address\n", Pn);
		iNc = Nc = 0;
		return;
	    }
# else	/* !defined(ADDR_NCACHE) */
	    v = (KA_T)0;
	    if (get_Nl_value(X_NCACHE, (struct drive_Nl *)NULL, &v) < 0
	    || !v
	    ||  kread((KA_T)v, (char *)&kp, sizeof(kp))) {
		if (!Fwarn)
		    (void) fprintf(stderr,
			"%s: WARNING: can't read name cache ptr: %s\n",
			Pn, print_kptr(v, (char *)NULL, 0));
		iNc = Nc = 0;
		return;
	    }
# endif	/* defined(ADDR_NCACHE) */

	/*
	 * Allocate space for a local copy of the kernel's cache.
	 */

# if	!defined(NCACHE_NXT)
	    len = Nc * sizeof(struct ncache);
	    if (!(kca = (struct ncache *)malloc((MALLOC_S)len))) {
		if (!Fwarn)
		    (void) fprintf(stderr,
			"%s: can't allocate name cache space: %d\n", Pn, len);
		Exit(1);
	    }
# endif	/* !defined(NCACHE_NXT) */

	/*
	 * Allocate space for the local cache.
	 */
	    len = Nc * sizeof(struct l_nch);
	    if (!(Ncache = (struct l_nch *)calloc(Nc, sizeof(struct l_nch)))) {

no_local_space:

		if (!Fwarn)
		    (void) fprintf(stderr,
		      "%s: no space for %d byte local name cache\n", Pn, len);
		Exit(1);
	    }
	} else {

	/*
	 * Do setup for repeat calls.
	 */
	    if (!iNc)
		return;
	    if (Nchash) {
		(void) free((FREE_P *)Nchash);
		Nchash = (struct l_nch **)NULL;
	    }
	    if (Ncache) {

	    /*
	     * Free space malloc'd to names in local name cache.
	     */
	        for (i = 0, lc = Ncache; i < Nc; i++, lc++) {
		    if (lc->nm) {
			(void) free((FREE_P *)lc->nm);
			lc->nm = (char *)NULL;
		    }
	        }
	    }
	    Nc = iNc;

# if	defined(NCACHE_NXT)
	    kp = kf;
# endif	/* defined(NCACHE_NXT) */

	}

# if	!defined(NCACHE_NXT)

/*
 * Read the kernel's name cache.
 */
	if (kread(kp, (char *)kca, (Nc * sizeof(struct ncache)))) {
	    if (!Fwarn)
		(void) fprintf(stderr,
		    "%s: WARNING: can't read kernel's name cache: %s\n",
		    Pn, print_kptr(kp, (char *)NULL, 0));
	    Nc = 0;
	    return;
	}
# endif	/* !defined(NCACHE_NXT) */

/*
 * Build a local copy of the kernel name cache.
 */

# if	defined(NCACHE_NXT)
	for (i = iNc * 16, kc = &nc, kf = kp, lc = Ncache, n = 0; kp; )
# else	/* !defined(NCACHE_NXT) */
	for (i = n = 0, kc = kca, lc = Ncache; i < Nc; i++, kc++)
# endif	/* defined(NCACHE_NXT) */

	{

# if	defined(NCACHE_NXT)
	    if (kread(kp, (char *)kc, sizeof(nc)))
		break;
	    if ((kp = (KA_T)kc->NCACHE_NXT) == kf)
		kp = (KA_T)NULL;
# endif	/* defined(NCACHE_NXT) */

	    if (!kc->NCACHE_VP || (len = kc->NCACHE_NAMLEN) < 1)
		continue;

# if	defined(NCACHE_NEGVN)
	    if (NegVN && ((KA_T)kc->NCACHE_VP == NegVN))
		continue;
# endif	/* defined(NCACHE_NEGVN) */

# if	defined(HASDNLCPTR)
	/*
	 * Read name from kernel to a temporary buffer.
	 */
	    if (len > na) {
		na = len;
		if (!nb)
		    nb = (char *)malloc(na);
		else
		    nb = (char *)realloc((MALLOC_P *)nb, na);
		if (!nb) {
		    (void) fprintf(stderr,
			"%s: can't allocate %d byte temporary name buffer\n",
			Pn, na);
		    Exit(1);
		}
	    }
	    if (!kc->NCACHE_NAME || kread((KA_T)kc->NCACHE_NAME, nb, len))
		continue;
	    np = nb;
# else	/* !defined(HASDNLCPTR) */
	/*
	 * Use name that is in the kernel cache entry.
	 */
	    if (len > NC_NAMLEN)
		continue;
	    np = kc->NCACHE_NAME;
# endif	/* defined(HASDNLCPTR) */

	    if (len < 3 && *np == '.') {
		if (len == 1 || (len == 2 && np[1] == '.'))
		    continue;
	    }
	/*
	 * Allocate space for name in local cache entry.
	 */
	    if (!(cp = (char *)malloc(len + 1))) {
		(void) fprintf(stderr,
		    "%s: can't allocate %d bytes for name cache name: %s\n",
		    Pn, len + 1, np);
		Exit(1);
	    }
	    (void) strncpy(cp, np, len);
	    cp[len] = '\0';

# if	defined(NCACHE_NXT)
	    if (n >= Nc) {

	    /*
	     * Allocate more local space to receive the kernel's linked
	     * entries.
	     */
		Nc += LNCHINCRSZ;
		if (!(Ncache = (struct l_nch *)realloc(Ncache,
		     (MALLOC_S)(Nc * sizeof(struct l_nch)))))
		{
		    (void) fprintf(stderr,
			"%s: no more space for %d entry local name cache\n",
			Pn, Nc);
		    Exit(1);
		}
		lc = &Ncache[n];
		iNc = Nc;
	    }
# endif	/* defined(NCACHE_NXT) */

	/*
	 * Complete the local cache entry.
	 */
	    lc->vp = (KA_T)kc->NCACHE_VP;
	    lc->dp = (KA_T)kc->NCACHE_DP;
	    lc->pa = (struct l_nch *)NULL;
	    lc->nm = cp;
	    lc->nl = len;

# if	defined(NCACHE_NODEID)
	    lc->id = (unsigned long)kc->NCACHE_NODEID;
	    lc->did = (unsigned long)kc->NCACHE_PARID;
# endif	/* defined(NCACHE_NODEID) */

	    n++;
	    lc++;

# if	defined(NCACHE_NXT)
	    if (n >= i) {
		if (!Fwarn)
		    (void) fprintf(stderr,
			"%s: WARNING: name cache truncated at %d entries\n",
			Pn, n);
		break;
	    }
# endif	/* defined(NCACHE_NXT) */

	}
/*
 * Reduce memory usage, as required.
 */

# if	!defined(NCACHE_NXT)
	if (!RptTm)
	    (void) free((FREE_P *)kca);
# endif	/* !defined(NCACHE_NXT) */

	if (n < 1) {
	    if (!RptTm && Ncache) {

	    /*
	     * If not in repeat mode, free the space that has been malloc'd
	     * to the local name cache.
	     */
		for (i = 0, lc = Ncache; i < Nc; i++, lc++) {
		    if (lc->nm) {
			(void) free((FREE_P *)lc->nm);
			lc->nm = (char *)NULL;
		    }
		}
		(void) free((FREE_P *)Ncache);
	 	Ncache = (struct l_nch *)NULL;
		Nc = 0;
	    }
	    if (!Fwarn)
		(void) fprintf(stderr,
		    "%s: WARNING: unusable name cache size: %d\n", Pn, n);
	    return;
	}
	if (n < Nc) {
	    Nc = n;
	    if (!RptTm) {
		len = Nc * sizeof(struct l_nch);
		if (!(Ncache = (struct l_nch *)realloc(Ncache, len)))
		    goto no_local_space;
	    }
	}
/*
 * Build a hash table to locate Ncache entries.
 */
	for (Nch = 1; Nch < Nc; Nch <<= 1)
	    ;
	Nch <<= 1;
	Mch = Nch - 1;
	if (!(Nchash = (struct l_nch **)calloc(Nch+Nc, sizeof(struct l_nch *))))
	{
	    if (!Fwarn)
		(void) fprintf(stderr,
		    "%s: no space for %d name cache hash pointers\n",
		    Pn, Nch + Nc);
	    Exit(1);
	}
	for (i = 0, lc = Ncache; i < Nc; i++, lc++) {

# if	defined(NCACHE_NODEID)
	    for (hp = ncachehash(lc->id, lc->vp), n = 1; *hp; hp++)
# else	/* !defined(NCACHE_NODEID) */
	    for (hp = ncachehash(lc->vp), n = 1; *hp; hp++)
# endif	/* defined(NCACHE_NODEID) */

	    {
		if ((*hp)->vp == lc->vp && strcmp((*hp)->nm, lc->nm) == 0
		&&  (*hp)->dp == lc->dp

# if	defined(NCACHE_NODEID)
		&&  (*hp)->id == lc->id && (*hp)->did == lc->did
# endif	/* defined(NCACHE_NODEID) */

		) {
		    n = 0;
		    break;
		}
	    }
	    if (n)
		*hp = lc;
	}
/*
 * Make a final pass through the local cache and convert parent vnode
 * addresses to local name cache pointers.
 */
	for (i = 0, lc = Ncache; i < Nc; i++, lc++) {
	    if (!lc->dp)
		continue;

# if	defined(NCACHE_NEGVN)
	     if (NegVN && (lc->dp == NegVN)) {
		lc->pa = (struct l_nch *)NULL;
		continue;
	     }
# endif	/* defined(NCACHE_NEGVN) */

# if	defined(NCACHE_NODEID)
	    lc->pa = ncache_addr(lc->did, lc->dp);
# else	/* !defined(NCACHE_NODEID) */
	    lc->pa = ncache_addr(lc->dp);
# endif	/* defined(NCACHE_NODEID) */
	
	}
}


/*
 * ncache_lookup() - look up a node's name in the kernel's name cache
 */

char *
ncache_lookup(buf, blen, fp)
	char *buf;			/* receiving name buffer */
	int blen;			/* receiving buffer length */
	int *fp;			/* full path reply */
{
	char *cp = buf;
	struct l_nch *lc;
	struct mounts *mtp;
	int nl, rlen;

	*cp = '\0';
	*fp = 0;

# if	defined(HASFSINO)
/*
 * If the entry has an inode number that matches the inode number of the
 * file system mount point, return an empty path reply.  That tells the
 * caller to print the file system mount point name only.
 */
	if ((Lf->inp_ty == 1) && Lf->fs_ino && (Lf->inode == Lf->fs_ino))
	    return(cp);
# endif	/* defined(HASFSINO) */

/*
 * Look up the name cache entry for the node address.
 */
	if (!Nc

# if	defined(NCACHE_NODEID)
	||  !(lc = ncache_addr(Lf->id, Lf->na))
# else	/* !defined(NCACHE_NODEID) */
	||  !(lc = ncache_addr(Lf->na))
# endif	/* defined(NCACHE_NODEID) */

	) {

	/*
	 * If the node has no cache entry, see if it's the mount
	 * point of a known file system.
	 */
	    if (!Lf->fsdir || !Lf->dev_def || Lf->inp_ty != 1)
		return((char *)NULL);
	    for (mtp = readmnt(); mtp; mtp = mtp->next) {
		if (!mtp->dir || !mtp->inode)
		    continue;
		if (Lf->dev == mtp->dev
		&&  mtp->inode == Lf->inode
		&&  strcmp(mtp->dir, Lf->fsdir) == 0)
		    return(cp);
	    }
	    return((char *)NULL);
	}
/*
 * Begin the path assembly.
 */
	if ((nl = lc->nl) > (blen - 1))
	    return((char *)NULL);
	cp = buf + blen - nl - 1;
	rlen = blen - nl - 1;
	(void) strcpy(cp, lc->nm);
/*
 * Look up the name cache entries that are parents of the node address.
 * Quit when:
 *
 *	there's no parent;
 *	the name is too large to fit in the receiving buffer.
 */
	for (;;) {
	    if (!lc->pa) {
		if (ncache_isroot(lc->dp, cp))
		    *fp = 1;
		break;
	    }
	    lc = lc->pa;
	    if (((nl = lc->nl) + 1) > rlen)
		break;
	    *(cp - 1) = '/';
	    cp--;
	    rlen--;
	    (void) strncpy((cp - nl), lc->nm, nl);
	    cp -= nl;
	    rlen -= nl;
	}
	return(cp);
}
#else	/* !defined(HASNCACHE) || !defined(USE_LIB_RNCH) */
char rnch_d1[] = "d"; char *rnch_d2 = rnch_d1;
#endif	/* defined(HASNCACHE) && defined(USE_LIB_RNCH) */
