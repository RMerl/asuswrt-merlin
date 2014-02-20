/*
 * rnam.c -- BSD format name cache functions for lsof library
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

#if	defined(HASNCACHE) && defined(USE_LIB_RNAM)

# if	!defined(lint)
static char copyright[] =
"@(#) Copyright 1997 Purdue Research Foundation.\nAll rights reserved.\n";
static char *rcsid = "$Id: rnam.c,v 1.11 2008/10/21 16:13:23 abe Exp $";
# endif	/* !defined(lint) */

#include "../lsof.h"


/*
 * rnam.c - read BSD format (struct namecache or nch) name cache
 *
 * This code is effective only when HASNCACHE is defined.
 */

/*
 * The caller must:
 *
 *	#include the relevant header file -- e.g., <sys/namei.h>.
 *
 *	Define X_NCACHE as the nickname for the kernel cache address.
 *
 *	Define X_NCSIZE as the nickname for the size of the kernel cache.
 *
 *	Define NCACHE_NXT if the kernel's name cache is a linked list, starting
 *	at the X_NCACHE address, rather than a table, starting at that address.
 *
 *	Define NCACHE_NO_ROOT if the calling dialect doesn't support
 *	the locating of the root node of a file system.
 *
 *	Define the name of the name cache structure -- e.g.,
 *
 *		#define NCACHE	<structure name>
 *
 *	Define the following casts, if they differ from the defaults:
 *
 *		NCACHE_SZ_CAST	cast for X_NCSIZE (default int)
 *
 *	e.g.,
 *		#define NCACHE_SZ_CAST unsigned long
 *
 *	Define the names of these elements of struct NCACHE:
 *
 *	must		#define NCACHE_NM	<name>
 *	must		#define NCACHE_NMLEN	<name length
 *	optional	#define NCACHE_NXT	<link to next entry>
 *	must		#define NCACHE_NODEADDR	<node address>
 *	must		#define NCACHE_NODEID	<node capability ID)
 *	optional	#define NCACHE_PARADDR	<parent node address>
 *	optional	#define NCACHE_PARID	<parent node capability ID)
 *
 * The caller may need to:
 *
 *	Define NCHNAMLEN as the length of the name element of NCACHE, if it's
 *	not defined in the header file that defines the NCACHE structure.
 *
 *	Define this prototype for ncache_load():
 *
 *		_PROTOTYPE(static void ncache_load,(void));
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
	KA_T na;			/* node address */

# if	defined(NCACHE_NODEID)
	unsigned long id;		/* capability ID */
# endif	/* defined(NCACHE_NODEID) */

# if	defined(NCACHE_PARADDR) && defined(NCACHE_PARID)
	KA_T pa;			/* parent node address */
	struct l_nch *pla;		/* parent local node address */
	unsigned long did;		/* parent capability ID */
# endif	/* defined(NCACHE_PARADDR) && defined(NCACHE_PARID) */

	char nm[NCHNAMLEN+1];		/* name */
	int nl;				/* name length */
};

static struct l_nch *Ncache = (struct l_nch*)NULL;
					/* the local name cache */
static struct l_nch **Nchash = (struct l_nch **)NULL;
					/* Ncache hash pointers */
static int Ncfirst = 1;			/* first-call status */

# if	defined(NCACHE_NODEID)
#define ncachehash(i,n)		Nchash+(((((int)(n)>>2)+((int)(i)))*31415)&Mch)
_PROTOTYPE(static struct l_nch *ncache_addr,(unsigned long i, KA_T na));
# else	/* !defined(NCACHE_NODEID) */
#define ncachehash(n)		Nchash+((((int)(n)>>2)*31415)&Mch)
_PROTOTYPE(static struct l_nch *ncache_addr,(KA_T na));
# endif	/* defined(NCACHE_NODEID) */

#define DEFNCACHESZ	1024	/* local size if X_NCSIZE kernel value < 1 */
#define	LNCHINCRSZ	64	/* local size increment */

# if	!defined(NCACHE_NO_ROOT)
_PROTOTYPE(static int ncache_isroot,(KA_T na, char *cp));
# endif	/* !defined(NCACHE_NO_ROOT) */


/*
 * ncache_addr() - look up a node's local ncache address
 */

static struct l_nch *

# if	defined(NCACHE_NODEID)
ncache_addr(i, na)
	unsigned long i;		/* node's capability ID */
# else	/* !defined(NCACHE_NODEID) */
ncache_addr(na)
# endif	/* defined(NCACHE_NODEID) */

	KA_T na;			/* node's address */
{
	struct l_nch **hp;

# if	defined(NCACHE_NODEID)
	for (hp = ncachehash(i, na); *hp; hp++)
# else	/* !defined(NCACHE_NODEID) */
	for (hp = ncachehash(na); *hp; hp++)
# endif	/* defined(NCACHE_NODEID) */

	{

# if	defined(NCACHE_NODEID)
	    if ((*hp)->id == i && (*hp)->na == na)
# else	/* !defined(NCACHE_NODEID) */
	    if ((*hp)->na == na)
# endif	/* defined(NCACHE_NODEID) */

		return(*hp);
	}
	return((struct l_nch *)NULL);
}


# if	!defined(NCACHE_NO_ROOT)
/*
 * ncache_isroot() - is head of name cache path a file system root?
 */

static int
ncache_isroot(na, cp)
	KA_T na;				/* kernel node address */
	char *cp;				/* partial path */
{
	char buf[MAXPATHLEN];
	int i;
	MALLOC_S len;
	struct mounts *mtp;
	static int nca = 0;
	static int ncn = 0;
	static KA_T *nc = (KA_T *)NULL;
	struct stat sb;
	struct vnode v;

	if (!na)
	    return(0);
/*
 * Search the root vnode cache.
 */
	for (i = 0; i < ncn; i++) {
	    if (na == nc[i])
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
	if (kread((KA_T)na, (char *)&v, sizeof(v))
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
 * Add the node address to the root node cache.
 */
	if (ncn >= nca) {
	    if (!nca) {
		len = (MALLOC_S)(10 * sizeof(KA_T));
		nc = (KA_T *)malloc(len);
	    } else {
		len = (MALLOC_S)((nca + 10) * sizeof(KA_T));
		nc = (KA_T *)realloc(nc, len);
	    }
	    if (!nc) {
		(void) fprintf(stderr, "%s: no space for root node table\n",
		    Pn);
		Exit(1);
	    }
	    nca += 10;
	}
	nc[ncn++] = na;
	return(1);
}
# endif	/* !defined(NCACHE_NO_ROOT) */


/*
 * ncache_load() - load the kernel's name cache
 */

void
ncache_load()
{
	struct l_nch **hp, *lc;
	int i, len, n;
	static int iNc = 0;
	struct NCACHE *kc;
	static KA_T kp = (KA_T)NULL;
	KA_T v;

# if	defined(NCACHE_NXT)
	static KA_T kf;
	struct NCACHE nc;
# else	/* !defined NCACHE_NXT) */
	static struct NCACHE *kca = (struct NCACHE *)NULL;
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
	    if (Nc < 1) {
		if (!Fwarn) {
		    (void) fprintf(stderr,
			"%s: WARNING: kernel name cache size: %d\n", Pn, Nc);
		    (void) fprintf(stderr,
			"      Cache size assumed to be: %d\n", DEFNCACHESZ);
		}
		iNc = Nc = DEFNCACHESZ;
	    }
	/*
	 * Establish kernel cache address.
	 */
	    v = (KA_T)0;
	    if (get_Nl_value(X_NCACHE, (struct drive_Nl *)NULL, &v) < 0
	    ||  !v
	    ||  kread((KA_T)v, (char *)&kp, sizeof(kp))) {
		if (!Fwarn)
		    (void) fprintf(stderr,
			"%s: WARNING: can't read name cache address: %s\n",
			Pn, print_kptr(v, (char *)NULL, 0));
		iNc = Nc = 0;
		return;
	    }

# if	defined(NCACHE_NXT)
	    kf = kp;

# else	/* !defined(NCACHE_NXT) */
	/*
	 * Allocate space for a local copy of the kernel's cache.
	 */
	    len = Nc * sizeof(struct NCACHE);
	    if (!(kca = (struct NCACHE *)malloc((MALLOC_S)len))) {
		if (!Fwarn)
		    (void) fprintf(stderr,
			"%s: can't allocate name cache space: %d\n", Pn, len);
		Exit(1);
	    }
# endif	/* defined(NCACHE_NXT) */

	/*
	 * Allocate space for the local cache.
	 */
	    len = Nc * sizeof(struct l_nch);
	    if (!(Ncache = (struct l_nch *)malloc((MALLOC_S)len))) {

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
	    if ((Nc = iNc) == 0)
		return;
	    if (Nchash) {
		(void) free((FREE_P *)Nchash);
		Nchash = (struct l_nch **)NULL;
	    }

# if    defined(NCACHE_NXT)
	    kp = kf;
# endif /* defined(NCACHE_NXT) */

	}

# if    !defined(NCACHE_NXT)

/*
 * Read the kernel's name cache.
 */
	if (kread(kp, (char *)kca, (Nc * sizeof(struct NCACHE)))) {
	    if (!Fwarn)
		(void) fprintf(stderr,
		    "%s: WARNING: can't read kernel's name cache: %s\n",
		    Pn, print_kptr(kp, (char *)NULL, 0));
	    Nc = 0;
	    return;
        }
# endif /* !defined(NCACHE_NXT) */

/*
 * Build a local copy of the kernel name cache.
 */

# if	defined(NCACHE_NXT)
	for (i = iNc * 16, kc = &nc, lc = Ncache, n = 0; kp; )
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

	    if (!kc->NCACHE_NODEADDR)
		continue;
	    if ((len = kc->NCACHE_NMLEN) < 1 || len > NCHNAMLEN)
		continue;
	    if (len < 3 && kc->NCACHE_NM[0] == '.') {
		if (len == 1 || (len == 2 && kc->NCACHE_NM[1] == '.'))
		    continue;
	    }

# if	defined(NCACHE_NXT)
	    if (n >= Nc) {
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
	    }
# endif	/* defined(NCACHE_NXT) */

#  if	defined(NCACHE_NODEID)
	    lc->na = (KA_T)kc->NCACHE_NODEADDR;
	    lc->id = kc->NCACHE_NODEID;
#  endif	/* defined(NCACHE_NODEID) */

#  if	defined(NCACHE_PARADDR)
	    lc->pa = (KA_T)kc->NCACHE_PARADDR;
	    lc->pla = (struct l_nch *)NULL;
#  endif	/* defined(NCACHE_PARADDR) */

#  if	defined(NCACHE_PARID)
	    lc->did = kc->NCACHE_PARID;
#  endif	/* defined(NCACHE_PARID) */

	    (void) strncpy(lc->nm, kc->NCACHE_NM, len);
	    lc->nm[len] = '\0';
	    lc->nl = strlen(lc->nm);
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
	    Nc = 0;
	    if (!RptTm) {
		(void) free((FREE_P *)Ncache);
		Ncache = (struct l_nch *)NULL;
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
	    for (hp = ncachehash(lc->id, lc->na), n = 1; *hp; hp++)
# else	/* defined(NCACHE_NODEID) */
	    for (hp = ncachehash(lc->na), n = 1; *hp; hp++)
# endif	/* defined(NCACHE_NODEID) */

	    {

# if	defined(NCACHE_NODEID)
		if ((*hp)->na == lc->na && (*hp)->id == lc->id
# else	/* defined(NCACHE_NODEID) */
		if ((*hp)->na == lc->na
# endif	/* defined(NCACHE_NODEID) */

		&&  strcmp((*hp)->nm, lc->nm) == 0

# if	defined(NCACHE_PARADDR) && defined(NCACHE_PARID)
		&&  (*hp)->pa == lc->pa && (*hp)->did == lc->did
# endif	/* defined(NCACHE_PARADDR) && defined(NCACHE_PARID) */

		) {
		    n = 0;
		    break;
		}
	    }
	    if (n)
		*hp = lc;
	}

# if	defined(NCACHE_PARADDR) && defined(NCACHE_PARID)
/*
 * Make a final pass through the local cache and convert parent node
 * addresses to local name cache pointers.
 */
	for (i = 0, lc = Ncache; i < Nc; i++, lc++) {
	    if (!lc->pa)
		continue;
	    lc->pla = ncache_addr(lc->did, lc->pa);
	}
# endif	/* defined(NCACHE_PARADDR) && defined(NCACHE_PARID) */
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

# if	defined(NCACHE_NODEID)
	if (Nc == 0 || !(lc = ncache_addr(Lf->id, Lf->na)))
# else	/* defined(NCACHE_NODEID) */
	if (Nc == 0 || !(lc = ncache_addr(Lf->na)))
# endif	/* defined(NCACHE_NODEID) */


	{

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
 * Start the path assembly.
 */
	if ((nl = lc->nl) > (blen - 1))
	    return((char *)NULL);
	cp = buf + blen - nl - 1;
	rlen = blen - nl - 1;
	(void) strcpy(cp, lc->nm);

# if	defined(NCACHE_PARADDR) && defined(NCACHE_PARID)
/*
 * Look up the name cache entries that are parents of the node address.
 * Quit when:
 *
 *	there's no parent;
 *	the name length is too large to fit in the receiving buffer.
 */
	for (;;) {
	    if (!lc->pla) {

#  if	!defined(NCACHE_NO_ROOT)
		if (ncache_isroot(lc->pa, cp))
		    *fp = 1;
#  endif	/* !defined(NCACHE_NO_ROOT) */

		break;
	    }
	    lc = lc->pla;
	    if (((nl = lc->nl) + 1) > rlen)
		break;
	    *(cp - 1) = '/';
	    cp--;
	    rlen--;
	    (void) strncpy((cp - nl), lc->nm, nl);
	    cp -= nl;
	    rlen -= nl;
	}
# endif	/* defined(NCACHE_PARADDR) && defined(NCACHE_PARID) */
	return(cp);
}
#else	/* !defined(HASNCACHE) || !defined(USE_LIB_RNAM) */
char rnam_d1[] = "d"; char *rnam_d2 = rnam_d1;
#endif	/* defined(HASNCACHE) && defined(USE_LIB_RNAM) */
