/*
 * rdev.c -- readdev() function for lsof library
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

#if	defined(USE_LIB_READDEV)

# if	!defined(lint)
static char copyright[] =
"@(#) Copyright 1997 Purdue Research Foundation.\nAll rights reserved.\n";
static char *rcsid = "$Id: rdev.c,v 1.12 2008/10/21 16:13:23 abe Exp $";
# endif	/* !defined(lint) */

#include "../lsof.h"


_PROTOTYPE(static int rmdupdev,(struct l_dev ***dp, int n, char *nm));


/*
 * To use this source file:
 *
 * 1. Define DIRTYPE as:
 *
 *	  #define DIRTYPE direct
 *    or  #define DIRTYPE dirent
 *
 * 2. Define HASDNAMLEN if struct DIRTYPE has a d_namlen element, giving
 *    the length of d_name.
 *
 * 3. Define the RDEV_EXPDEV macro to apply special handling to device
 *    numbers, as required.  For example, for EP/IX 2.1.1:
 *
 *	#define RDEV_EXPDEV(n)	expdev(n)
 *
 *    to use the expdev() function to expand device numbers.  If
 *    no RDEV_EXPDEV macro is defined, it defaults to:
 *
 *	#define RDEV_EXPDEV(n)	(n)
 *
 * 4. Define HASBLKDEV to request that information on S_IFBLK devices be
 *    recorded in BDevtp[].
 *
 *    Define NOWARNBLKDEV to suppress the issuance of a warning when no
 *    block devices are found.
 *
 * 5. Define RDEV_STATFN to be a stat function other than stat() or lstat()
 *    -- e.g.,
 *
 *	#define	RDEV_STATFN	private_stat
 *
 * 6. Define HAS_STD_CLONE to request that clone device information be stored
 *    in standard clone structures (defined in lsof.h and addressed via
 *    Clone).  If HAS_STD_CLONE is defined, these must also be defined:
 *
 *	a.  Define CLONEMAJ to be the name of the constant or
 *	    variable that defines the clone major device -- e.g.,
 *
 *		#define CLONEMAJ CloneMaj
 *
 *	b.  Define HAVECLONEMAJ to be the name of the variable that
 *	    contains the status of the clone major device -- e.g.,
 *
 *		#define HAVECLONEMAJ HaveCloneMaj
 *
 *    Define HAS_STD_CLONE to be 1 if readdev() is expected to build the
 *    clone table, the clone table is cached (if HASDCACHE is defined), and
 *    there is a function to clear the cache table when the device table must
 *    be reloaded.  (See dvch.c for naming the clone cache build and clear
 *    functions.)
 */


# if	!defined(RDEV_EXPDEV)
#define	RDEV_EXPDEV(n)		(n)
# endif	/* !defined(RDEV_EXPDEV) */

# if	!defined(RDEV_STATFN)
#  if	defined(USE_STAT)
#define	RDEV_STATFN	stat
#  else	/* !defined(USE_STAT) */
#define	RDEV_STATFN	lstat
#  endif	/* defined(USE_STAT) */
# endif	/* !defined(RDEV_STATFN) */


/*
 * readdev() - read device names, modes and types
 */

void
readdev(skip)
	int skip;			/* skip device cache read if 1 */
{

# if	defined(HAS_STD_CLONE) && HAS_STD_CLONE==1
	struct clone *c;
# endif	/* defined(HAS_STD_CLONE) && HAS_STD_CLONE==1 */

# if	defined(HASDCACHE)
	int dcrd;
# endif	/* defined(HASDCACHE) */

	DIR *dfp;
	int dnamlen;
	struct DIRTYPE *dp;
	char *fp = (char *)NULL;
	int i = 0;

# if	defined(HASBLKDEV)
	int j = 0;
# endif	/* defined(HASBLKDEV) */

	char *path = (char *)NULL;
	MALLOC_S pl;
	struct stat sb;

	if (Sdev)
	    return;

# if	defined(HASDCACHE)
/*
 * Read device cache, as directed.
 */
	if (!skip) {
	    if (DCstate == 2 || DCstate == 3) {
		if ((dcrd = read_dcache()) == 0)
		    return;
	    }
	} else
	    dcrd = 1;
# endif	/* defined(HASDCACHE) */

	Dstkn = Dstkx = 0;
	Dstk = (char **)NULL;
	(void) stkdir("/dev");
/*
 * Unstack the next /dev or /dev/<subdirectory> directory.
 */
	while (--Dstkx >= 0) {
	    if (!(dfp = OpenDir(Dstk[Dstkx]))) {

# if	defined(WARNDEVACCESS)
		if (!Fwarn) {
		    (void) fprintf(stderr, "%s: WARNING: can't open: ", Pn);
		    safestrprt(Dstk[Dstkx], stderr, 1);
		}
# endif	/* defined(WARNDEVACCESS) */

		(void) free((FREE_P *)Dstk[Dstkx]);
		Dstk[Dstkx] = (char *)NULL;
		continue;
	    }
	    if (path) {
		(void) free((FREE_P *)path);
		path = (char *)NULL;
	    }
	    if (!(path = mkstrcat(Dstk[Dstkx], -1, "/", 1, (char *)NULL, -1,
				  &pl)))
	    {
		(void) fprintf(stderr, "%s: no space for: ", Pn);
		safestrprt(Dstk[Dstkx], stderr, 1);
		Exit(1);
	    }
	    (void) free((FREE_P *)Dstk[Dstkx]);
	    Dstk[Dstkx] = (char *)NULL;
	/*
	 * Scan the directory.
	 */
	    for (dp = ReadDir(dfp); dp; dp = ReadDir(dfp)) {
		if (dp->d_ino == 0 || dp->d_name[0] == '.')
		    continue;
	    /*
	     * Form the full path name and get its status.
	     */

# if	defined(HASDNAMLEN)
		dnamlen = (int)dp->d_namlen;
# else	/* !defined(HASDNAMLEN) */
		dnamlen = (int)strlen(dp->d_name);
# endif	/* defined(HASDNAMLEN) */

		if (fp) {
		    (void) free((FREE_P *)fp);
		    fp = (char *)NULL;
		}
		if (!(fp = mkstrcat(path, pl, dp->d_name, dnamlen,
				    (char *)NULL, -1, (MALLOC_S *)NULL)))
		{
		    (void) fprintf(stderr, "%s: no space for: ", Pn);
		    safestrprt(path, stderr, 0);
		    safestrprtn(dp->d_name, dnamlen, stderr, 1);
		    Exit(1);
		}
		if (RDEV_STATFN(fp, &sb) != 0) {
		    if (errno == ENOENT)	/* a sym link to nowhere? */
			continue;

# if	defined(WARNDEVACCESS)
		    if (!Fwarn) {
			int errno_save = errno;

			(void) fprintf(stderr, "%s: can't stat ", Pn);
			safestrprt(fp, stderr, 0);
			(void) fprintf(stderr, ": %s\n", strerror(errno_save));
		    }
# endif	/* defined(WARNDEVACCESS) */

		    continue;
		}
	    /*
	     * If it's a subdirectory, stack its name for later
	     * processing.
	     */
		if ((sb.st_mode & S_IFMT) == S_IFDIR) {
		    (void) stkdir(fp);
		    continue;
		}
		if ((sb.st_mode & S_IFMT) == S_IFCHR) {

		/*
		 * Save character device information in Devtp[].
		 */
		    if (i >= Ndev) {
			Ndev += DEVINCR;
			if (!Devtp)
			    Devtp = (struct l_dev *)malloc(
				    (MALLOC_S)(sizeof(struct l_dev)*Ndev));
			else
			    Devtp = (struct l_dev *)realloc((MALLOC_P *)Devtp,
				    (MALLOC_S)(sizeof(struct l_dev)*Ndev));
			if (!Devtp) {
			    (void) fprintf(stderr,
				"%s: no space for character device\n", Pn);
			    Exit(1);
			}
		    }
		    Devtp[i].rdev = RDEV_EXPDEV(sb.st_rdev);
		    Devtp[i].inode = (INODETYPE)sb.st_ino;
		    if (!(Devtp[i].name = mkstrcpy(fp, (MALLOC_S *)NULL))) {
			(void) fprintf(stderr,
			    "%s: no space for device name: ", Pn);
			safestrprt(fp, stderr, 1);
			Exit(1);
		    }
		    Devtp[i].v = 0;

# if	defined(HAS_STD_CLONE) && HAS_STD_CLONE==1
		    if (HAVECLONEMAJ && GET_MAJ_DEV(Devtp[i].rdev) == CLONEMAJ)
		    {

		    /*
		     * Record clone device information.
		     */
			if (!(c = (struct clone *)malloc(sizeof(struct clone))))
			{
			    (void) fprintf(stderr,
				"%s: no space for clone device: ", Pn);
			    safestrprt(fp, stderr, 1);
			    Exit(1);
			}
			c->dx = i;
			c->next = Clone;
			Clone = c;
		    }
# endif	/* defined(HAS_STD_CLONE) && HAS_STD_CLONE==1 */

		    i++;
		}

# if	defined(HASBLKDEV)
		if ((sb.st_mode & S_IFMT) == S_IFBLK) {

		/*
		 * Save block device information in BDevtp[].
		 */
		    if (j >= BNdev) {
			BNdev += DEVINCR;
			if (!BDevtp)
			    BDevtp = (struct l_dev *)malloc(
				     (MALLOC_S)(sizeof(struct l_dev)*BNdev));
			else
			    BDevtp = (struct l_dev *)realloc((MALLOC_P *)BDevtp,
				     (MALLOC_S)(sizeof(struct l_dev)*BNdev));
			if (!BDevtp) {
			    (void) fprintf(stderr,
				"%s: no space for block device\n", Pn);
			    Exit(1);
			}
		    }
		    BDevtp[j].name = fp;
		    fp = (char *)NULL;
		    BDevtp[j].inode = (INODETYPE)sb.st_ino;
		    BDevtp[j].rdev = RDEV_EXPDEV(sb.st_rdev);
		    BDevtp[j].v = 0;
		    j++;
		}
# endif	/* defined(HASBLKDEV) */

	    }
	    (void) CloseDir(dfp);
	}
/*
 * Free any allocated space.
 */
	if (!Dstk) {
	    (void) free((FREE_P *)Dstk);
	    Dstk = (char **)NULL;
	}
	if (fp)
	    (void) free((FREE_P *)fp);
	if (path)
	    (void) free((FREE_P *)path);

# if	defined(HASBLKDEV)
/*
 * Reduce the BDevtp[] (optional) and Devtp[] tables to their minimum
 * sizes; allocate and build sort pointer lists; and sort the tables by
 * device number.
 */
	if (BNdev) {
	    if (BNdev > j) {
		BNdev = j;
		BDevtp = (struct l_dev *)realloc((MALLOC_P *)BDevtp,
			 (MALLOC_S)(sizeof(struct l_dev) * BNdev));
	    }
	    if (!(BSdev = (struct l_dev **)malloc(
			  (MALLOC_S)(sizeof(struct l_dev *) * BNdev))))
	    {
		(void) fprintf(stderr,
		    "%s: no space for block device sort pointers\n", Pn);
		Exit(1);
	    }
	    for (j = 0; j < BNdev; j++) {
		BSdev[j] = &BDevtp[j];
	    }
	    (void) qsort((QSORT_P *)BSdev, (size_t)BNdev,
		(size_t)sizeof(struct l_dev *), compdev);
	    BNdev = rmdupdev(&BSdev, BNdev, "block");
	}
	
#  if	!defined(NOWARNBLKDEV)
	else {
	    if (!Fwarn)
		(void) fprintf(stderr,
		    "%s: WARNING: no block devices found\n", Pn);
	}
#  endif	/* !defined(NOWARNBLKDEV) */
# endif	/* defined(HASBLKDEV) */

	if (Ndev) {
	    if (Ndev > i) {
		Ndev = i;
		Devtp = (struct l_dev *)realloc((MALLOC_P *)Devtp,
			(MALLOC_S)(sizeof(struct l_dev) * Ndev));
	    }
	    if (!(Sdev = (struct l_dev **)malloc(
			 (MALLOC_S)(sizeof(struct l_dev *) * Ndev))))
	    {
		(void) fprintf(stderr,
		    "%s: no space for character device sort pointers\n", Pn);
		Exit(1);
	    }
	    for (i = 0; i < Ndev; i++) {
		Sdev[i] = &Devtp[i];
	    }
	    (void) qsort((QSORT_P *)Sdev, (size_t)Ndev,
		(size_t)sizeof(struct l_dev *), compdev);
	    Ndev = rmdupdev(&Sdev, Ndev, "char");
	} else {
	    (void) fprintf(stderr, "%s: no character devices found\n", Pn);
	    Exit(1);
	}

# if	defined(HASDCACHE)
/*
 * Write device cache file, as required.
 */
	if (DCstate == 1 || (DCstate == 3 && dcrd))
	    write_dcache();
# endif	/* defined(HASDCACHE) */

}


# if	defined(HASDCACHE)
/*
 * rereaddev() - reread device names, modes and types
 */

void
rereaddev()
{
	(void) clr_devtab();

# if	defined(DCACHE_CLR)
	(void) DCACHE_CLR();
# endif	/* defined(DCACHE_CLR) */

	readdev(1);
	DCunsafe = 0;
}
#endif	/* defined(HASDCACHE) */


/*
 * rmdupdev() - remove duplicate (major/minor/inode) devices
 */

static int
rmdupdev(dp, n, nm)
	struct l_dev ***dp;	/* device table pointers address */
	int n;			/* number of pointers */
	char *nm;		/* device table name for error message */
{

# if	defined(HAS_STD_CLONE) && HAS_STD_CLONE==1
	struct clone *c, *cp;
# endif	/* defined(HAS_STD_CLONE) && HAS_STD_CLONE==1 */

	int i, j, k;
	struct l_dev **p;

	for (i = j = 0, p = *dp; i < n ;) {
	    for (k = i + 1; k < n; k++) {
		if (p[i]->rdev != p[k]->rdev || p[i]->inode != p[k]->inode)
		    break;

# if	defined(HAS_STD_CLONE) && HAS_STD_CLONE==1
	    /*
	     * See if we're deleting a duplicate clone device.  If so,
	     * delete its clone table entry.
	     */
		for (c = Clone, cp = (struct clone *)NULL;
		     c;
		     cp = c, c = c->next)
		{
		    if (&Devtp[c->dx] != p[k])
			continue;
		    if (!cp)
			Clone = c->next;
		    else
			cp->next = c->next;
		    (void) free((FREE_P *)c);
		    break;
		}
# endif	/* defined(HAS_STD_CLONE) && HAS_STD_CLONE==1 */

	    }
	    if (i != j)
		p[j] = p[i];
	    j++;
	    i = k;
	}
	if (n == j)
	    return(n);
	if (!(*dp = (struct l_dev **)realloc((MALLOC_P *)*dp,
		    (MALLOC_S)(j * sizeof(struct l_dev *)))))
	{
	    (void) fprintf(stderr, "%s: can't realloc %s device pointers\n",
		Pn, nm);
	    Exit(1);
	}
	return(j);
}


# if	defined(HASDCACHE)
/*
 * vfy_dev() - verify a device table entry (usually when DCunsafe == 1)
 *
 * Note: rereads entire device table when an entry can't be verified.
 */

int
vfy_dev(dp)
	struct l_dev *dp;		/* device table pointer */
{
	struct stat sb;

	if (!DCunsafe || dp->v)
	    return(1);
	if (RDEV_STATFN(dp->name, &sb) != 0
	||  dp->rdev != RDEV_EXPDEV(sb.st_rdev)
	||  dp->inode != sb.st_ino) {
	   (void) rereaddev();
	   return(0);
	}
	dp->v = 1;
	return(1);
}
# endif	/* defined(HASDCACHE) */
#else	/* !defined(USE_LIB_READDEV) */
char rdev_d1[] = "d"; char *rdev_d2 = rdev_d1;
#endif	/* defined(USE_LIB_READDEV) */
