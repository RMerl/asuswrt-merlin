/*
 * rnmh.c -- functions to read BSD format name cache information from a
 *	     kernel hash table
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

#if	defined(HASNCACHE) && defined(USE_LIB_RNMH)

# if	!defined(lint)
static char copyright[] =
"@(#) Copyright 1997 Purdue Research Foundation.\nAll rights reserved.\n";
static char *rcsid = "$Id: rnmh.c,v 1.13 2008/10/21 16:13:23 abe Exp $";
# endif	/* !defined(lint) */

#include "../lsof.h"


/*
 * rnmh.c - read BSD format hashed kernel name cache
 */

/*
 * The caller must:
 *
 *	#include the relevant header file -- e.g., <sys/namei.h>.
 *
 *	Define X_NCACHE as the nickname for the kernel cache hash tables
 *	address.
 *
 *	Define X_NCSIZE as the nickname for the size of the kernel cache has
 *	table length.
 *
 *	Define NCACHE_NO_ROOT if the calling dialect doesn't support
 *	the locating of the root node of a file system.
 *
 *	Define the name of the name cache structure -- e.g.,
 *
 *		#define NCACHE	<structure name>
 *
 *
 *	Define the following casts, if they differ from the defaults:
 *
 *		NCACHE_SZ_CAST	case for X_NCSIZE (default unsigned long)
 *
 *	Define the names of these elements of struct NCACHE:
 *
 *		#define NCACHE_NM	<name>
 *		#define NCACHE_NXT	<link to next entry>
 *		#define NCACHE_NODEADDR	<node address>
 *		#define NCACHE_PARADDR	<parent node address>
 *
 *	Optionally define:
 *
 *		#define NCACHE_NMLEN	<name length>
 *
 *	Optionally define *both*:
 *
 *		#define	NCACHE_NODEID	<node capability ID>
 *		#define	NCACHE_PARID	<parent node capability ID>
 *
 * The caller may need to:
 *
 *	Define this prototype for ncache_load():
 *
 *		_PROTOTYPE(static void ncache_load,(void));
 *
 *	Define NCACHE_VROOT to be the value of the flag that signifies that
 *	the vnode is the root of its file system.
 *
 *		E.g., for BSDI >= 5:
 *
 *			#define NCACHE_VROOT	VV_ROOT
 *
 *	If not defined, NCACHE_VROOT is defined as "VROOT".
 *
 *	Define VNODE_VFLAG if the vnode's flag member's name isn't v_flag.
 *
 * Note: if NCHNAMLEN is defined, the name is assumed to be in
 * NCACHE_NM[NCHNAMLEN]; if it isn't defined, the name is assumed to be in an
 * extension that begins at NCACHE_NM[0].
 *
 * Note: if NCACHE_NMLEN is not defined, then NCACHE_NM must be a pointer to
 * a kernel allocated, NUL-terminated, string buffer.
 */


/*
 * Casts
 */

# if	!defined(NCACHE_NC_CAST)
#define	NCACHE_SZ_CAST  unsigned long
# endif	/* !defined(NCACHE_NC_CAST) */


/*
 * Flags
 */

# if	!defined(NCACHE_NMLEN)
#undef	NCHNAMLEN
# endif	/* !defined(NCACHE_NMLEN) */

# if	!defined(NCACHE_VROOT)
#define	NCACHE_VROOT	VROOT		/* vnode is root of its file system */
# endif	/* !defined(NCACHE_VROOT) */

# if	!defined(VNODE_VFLAG)
#define	VNODE_VFLAG	v_flag
# endif	/* !defined(VNODE_VFLAG) */


/*
 * Local static values
 */

static int Mch;				/* name cache hash mask */

struct l_nch {
	KA_T na;			/* node address */
	KA_T pa;			/* parent node address */
	struct l_nch *pla;		/* parent local node address */
	int nl;				/* name length */
	struct l_nch *next;		/* next entry */

# if	defined(NCACHE_NODEID)
	unsigned long id;		/* capability ID */
	unsigned long did;		/* parent capability ID */
# endif	/* defined(NCACHE_NODEID) */

# if	defined(NCHNAMLEN)
	char nm[NCHNAMLEN + 1];		/* name */
# else	/* !defined(NCHNAMLEN) */
	char nm[1];			/* variable length name */
# endif	/* defined(NCHNAMLEN) */

};

static struct l_nch *Ncache = (struct l_nch *)NULL;
					/* the head of the local name cache */
static struct l_nch **Nchash = (struct l_nch **)NULL;
					/* Ncache hash pointers */

# if	defined(NCACHE_NODEID)
#define ncachehash(i,n)		Nchash+(((((int)(n)>>2)+((int)(i)))*31415)&Mch)
_PROTOTYPE(static struct l_nch *ncache_addr,(unsigned long i, KA_T na));
# else	/* !defined(NCACHE_NODEID) */
#define ncachehash(n)		Nchash+((((int)(n)>>2)*31415)&Mch)
_PROTOTYPE(static struct l_nch *ncache_addr,(KA_T na));
# endif	/* defined(NCACHE_NODEID) */

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
	KA_T na;			/* kernel node address */
	char *cp;			/* partial path */
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
 * Read the vnode and see if it's a VDIR node with the NCACHE_VROOT flag set.
 * If it is, then the path is complete.
 *
 * If it isn't, and if the file has an inode number, search the mount table
 * and see if the file system's inode number is known.  If it is, form the
 * possible full path, safely stat() it, and see if it's inode number matches
 * the one we have for this file.  If it does, then the path is complete.
 */
	if (kread((KA_T)na, (char *)&v, sizeof(v))
	||  v.v_type != VDIR || !(v.VNODE_VFLAG & NCACHE_VROOT)) {

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
	struct NCACHE c;
	struct l_nch **hp, *ln;
	KA_T ka, knx;
	static struct NCACHE **khp = (struct namecache **)NULL;
	static int khpl = 0;
	NCACHE_SZ_CAST khsz;
	unsigned long kx;
	static struct l_nch *lc = (struct l_nch *)NULL;
	static int lcl = 0;
	int len, lim, n, nch, nchl, nlcl;
	char tbuf[32];
	KA_T v;

# if	!defined(NCHNAMLEN)
	int cin = sizeof(c.NCACHE_NM);
	KA_T nmo = (KA_T)offsetof(struct NCACHE, NCACHE_NM);
# endif	/* !defined(NCHNAMLEN) */

# if	!defined(NCACHE_NMLEN)
	char nbf[MAXPATHLEN + 1];
	int nbfl = (int)(sizeof(nbf) - 1);
	KA_T nk;
	char *np;
	int rl;

	nbf[nbfl] = '\0';
# endif	/* !defined(NCACHE_NMLEN) */

	if (!Fncache)
	    return;
/*
 * Free previously allocated space.
 */
	for (lc = Ncache; lc; lc = ln) {
	    ln = lc->next;
	    (void) free((FREE_P *)lc);
	}
	Ncache = (struct l_nch *)NULL;
	if (Nchash)
	    (void) free((FREE_P *)Nchash);
	Nchash = (struct l_nch **)NULL;
/*
 * Get kernel cache hash table size
 */
	v = (KA_T)0;
	if (get_Nl_value(X_NCSIZE, (struct drive_Nl *)NULL, &v) < 0
	||  !v
	||  kread((KA_T)v, (char *)&khsz, sizeof(khsz)))
	{
	    if (!Fwarn)
		(void) fprintf(stderr,
		    "%s: WARNING: can't read name cache hash size: %s\n",
		    Pn, print_kptr(v, (char *)NULL, 0));
	    return;
	}
	if (khsz < 1) {
	    if (!Fwarn)
		(void) fprintf(stderr,
		    "%s: WARNING: name cache hash size length error: %#lx\n",
		    Pn, khsz);
	    return;
	}
/*
 * Get kernel cache hash table address.
 */
	ka = (KA_T)0;
	v = (KA_T)0;
	if (get_Nl_value(X_NCACHE, (struct drive_Nl *)NULL, &v) < 0
	||  !v
	||  kread((KA_T)v, (char *)&ka, sizeof(ka))
	||  !ka)
	{
	    if (!Fwarn)
		(void) fprintf(stderr,
		    "%s: WARNING: unusable name cache hash pointer: (%s)=%s\n",
	            Pn, print_kptr(v, tbuf, sizeof(tbuf)),
		    print_kptr(ka, (char *)NULL, 0));
	    return;
	}
/*
 * Allocate space for the hash table pointers and read them.
 */
	len = (MALLOC_S)(khsz * sizeof(struct NCACHE *));
	if (len > khpl) {
	    if (khp)
		khp = (struct NCACHE **)realloc((MALLOC_P *)khp, len);
	    else
		khp = (struct NCACHE **)malloc(len);
	    if (!khp) {
		(void) fprintf(stderr,
		    "%s: can't allocate %d bytes for name cache hash table\n",
		    Pn, len);
		Exit(1);
	    }
	    khpl = len;
	}
	if (kread((KA_T)ka, (char *)khp, len)) {
	    (void) fprintf(stderr,
		"%s: can't read name cache hash pointers from: %s\n",
		Pn, print_kptr(ka, (char *)NULL, 0));
	    return;
	}
/*
 * Process the kernel's name cache hash table buckets.
 */
	lim = khsz * 10;
	for (kx = nch = 0; kx < khsz; kx++) {

	/*
	 * Loop through the entries for a hash bucket.
	 */
	    for (ka = (KA_T)khp[kx], n = 0; ka; ka = knx, n++) {
		if (n > lim) {
		    if (!Fwarn)
			(void) fprintf(stderr,
			    "%s: WARNING: name cache hash chain too long\n",
			    Pn);
		    break;
		}
		if (kread(ka, (char *)&c, sizeof(c)))
		    break;
		knx = (KA_T)c.NCACHE_NXT;
		if (!c.NCACHE_NODEADDR)
		    continue;

# if	defined(NCACHE_NMLEN)
		if ((len = c.NCACHE_NMLEN) < 1)
		    continue;
# else	/* !defined(NCACHE_NMLEN) */
	    /*
	     * If it's possible to read the first four characters of the name,
	     * do so and check for "." and "..".
	     */
		if (!c.NCACHE_NM
		||  kread((KA_T)c.NCACHE_NM, nbf, 4))
		    continue;
		if (nbf[0] == '.') {
		    if (!nbf[1]
		    ||  ((nbf[1] == '.') && !nbf[2]))
			continue;
		}
	   /*
	    * Read the rest of the name, 32 characters at a time, until a NUL
	    * character has been read or nbfl characters have been read.
	    */
		nbf[4] = '\0';
		if ((len = (int)strlen(nbf)) < 4) {
		    if (!len)
			continue;
		} else {
		    for (np = &nbf[4]; len < nbfl; np += rl) {
			if ((rl = nbfl - len) > 32) {
			    rl = 32;
			    nbf[len + rl] = '\0';
			}
			nk = (KA_T)((char *)c.NCACHE_NM + len);
			if (kread(nk, np, rl)) {
			    rl = -1;
			    break;
			}
			rl = (int)strlen(np);
			len += rl;
			if (rl < 32)
			    break;
		    }
		    if (rl < 0)
			continue;
		}
# endif	/* defined(NCACHE_NMLEN) */

	    /*
	     * Allocate a cache entry long enough to contain the name and
	     * move the name to it.
	     */

# if	defined(NCHNAMLEN)
		if (len > NCHNAMLEN)
		    continue;
		if (len < 3 && c.NCACHE_NM[0] == '.') {
		    if (len == 1 || (len == 2 && c.NCACHE_NM[1] == '.'))
			continue;
		}
		if ((nlcl = sizeof(struct l_nch)) > lcl)
# else	/* !defined(NCHNAMLEN) */
		if ((nlcl = sizeof(struct l_nch) + len) > lcl)
# endif	/* defined(NCHNAMLEN) */

		{
		    if (lc)
			lc = (struct l_nch *)realloc(lc, nlcl);
		    else
			lc = (struct l_nch *)malloc(nlcl);
		    if (!lc) {
			(void) fprintf(stderr,
			    "%s: can't allocate %d local name cache bytes\n",
			    Pn, nlcl);
			Exit(1);
		    }
		    lcl = nlcl;
		}

# if	defined(NCHNAMLEN)
		(void) strncpy(lc->nm, c.NCACHE_NM, len);
# else	/* !defined(NCHNAMLEN) */
#  if	defined(NCACHE_NMLEN)
		if ((len < 3) && (cin > 1)) {

		/*
		 * If this is a one or two character name, and if NCACHE_NM[]
		 * in c has room for at least two characters, check for "."
		 * and ".." first, ignoring this entry if the name is either.
		 */
		    if (len < 3 && c.NCACHE_NM[0] == '.') {
			if (len == 1 || (len == 2 && c.NCACHE_NM[1] == '.'))
			    continue;
		    }
		}
		if (len > cin) {

		/*
		 * If not all (possibly not any, depending on the value in
		 * cin) of the name has yet been read to lc->nm[], read it
		 * or the rest of it.  If it wasn't possible before to check
		 * for "." or "..", do that. too.
		 */
		    if (cin > 0)
			(void) strncpy(lc->nm, c.NCACHE_NM, cin);
		    if (kread(ka + (KA_T)(nmo + cin), &lc->nm[cin], len - cin))
			continue;
		    if ((cin < 2) && (len < 3) && (lc->nm[0] == '.')) {
			if (len == 1 || (len == 2 && lc->nm[1] == '.'))
			    continue;
		    }
		} else
		    (void) strncpy(lc->nm, c.NCACHE_NM, len);
#  else	/* !defined(NCACHE_NMLEN) */
		(void) strncpy(lc->nm, nbf, len);
#  endif	/* defined(NCACHE_NMLEN) */

# endif	/* defined(NCHNAMLEN) */
		lc->nm[len] = '\0';
	    /*
	     * Complete the new local cache entry and link it to the previous
	     * local cache chain.
	     */
		lc->next = Ncache;
		Ncache = lc;
		lc->na = (KA_T)c.NCACHE_NODEADDR;
		lc->nl = len;
		lc->pa = (KA_T)c.NCACHE_PARADDR;
		lc->pla = (struct l_nch *)NULL;

# if	defined(NCACHE_NODEID)
		lc->id = c.NCACHE_NODEID;
		lc->did = c.NCACHE_PARID;
# endif	/* defined(NCACHE_NODEID) */

		lcl = 0;
		lc = (struct l_nch *)NULL;
		nch++;
	    }
	}
/*
 * Reduce memory usage, as required.
 */
	if (!RptTm) {
	    (void) free((FREE_P *)khp);
	    khp = (struct NCACHE **)NULL;
	    khpl = 0;
	}
	if (nch < 1) {
	    if (!Fwarn)
		(void) fprintf(stderr,
		    "%s: WARNING: unusable name cache size: %d\n", Pn, nch);
	    return;
	}
/*
 * Build a hash table to locate Ncache entries.
 */
	for (nchl = 1; nchl < nch; nchl <<= 1)
	    ;
	nchl <<= 1;
	Mch = nchl - 1;
	len = nchl + nch;
	if (!(Nchash = (struct l_nch **)calloc(len, sizeof(struct l_nch *)))) {
	    if (!Fwarn)
		(void) fprintf(stderr,
		    "%s: no space for %d local name cache hash pointers\n",
		    Pn, len);
	    Exit(1);
	}
	for (lc = Ncache; lc; lc = lc->next) {

# if	defined(NCACHE_NODEID)
	    for (hp = ncachehash(lc->id, lc->na),
# else	/* !defined(NCACHE_NODEID) */
	    for (hp = ncachehash(lc->na),
# endif	/* defined(NCACHE_NODEID) */

		    n = 1; *hp; hp++)
		{
		if ((*hp)->na == lc->na && strcmp((*hp)->nm, lc->nm) == 0) {
		    n = 0;
		    break;
		}
	    }
	    if (n)
		*hp = lc;
	    else
		lc->pa = (KA_T)0;
	}
/*
 * Make a final pass through the local cache and convert parent node
 * addresses to local name cache pointers.
 */
	for (lc = Ncache; lc; lc = lc->next) {
	    if (!lc->pa)
		continue;

# if	defined(NCACHE_NODEID)
	    lc->pla = ncache_addr(lc->did, lc->pa);
# else	/* !defined(NCACHE_NODEID) */
	    lc->pla = ncache_addr(lc->pa);
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

# if	defined(NCACHE_NODEID)
	if (!Nchash || !(lc = ncache_addr(Lf->id, Lf->na)))
# else	/* !defined(NCACHE_NODEID) */
	if (!Nchash || !(lc = ncache_addr(Lf->na)))
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
		&&  (strcmp(mtp->dir, Lf->fsdir) == 0))
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
	return(cp);
}
#else	/* !defined(HASNCACHE) || !defined(USE_LIB_RNMH) */
char rnmh_d1[] = "d"; char *rnmh_d2 = rnmh_d1;
#endif	/* defined(HASNCACHE) && defined(USE_LIB_RNMH) */
