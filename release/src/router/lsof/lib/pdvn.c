/*
 * pdvn.c -- print device name functions for lsof library
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

#if	defined(USE_LIB_PRINTDEVNAME)

# if	!defined(lint)
static char copyright[] =
"@(#) Copyright 1997 Purdue Research Foundation.\nAll rights reserved.\n";
static char *rcsid = "$Id: pdvn.c,v 1.8 2008/10/21 16:12:36 abe Exp $";
# endif	/* !defined(lint) */

#include "../lsof.h"

#else	/* !defined(USE_LIB_PRINTDEVNAME) */
char pdvn_d1[] = "d"; char *pdvn_d2 = pdvn_d1;
#endif	/* defined(USE_LIB_PRINTDEVNAME) */


/*
 * To use this source file:
 *
 * 1. Define USE_LIB_PRINTDEVNAME, or both.
 *
 * 2. Define HAS_STD_CLONE to enable standard clone searches in
 *    printdevname().
 *
 * 3. Define HASBLDKDEV to enable block device processing.
 */


/*
 * Local definitions
 */

#define	LIKE_BLK_SPEC	"like block special"
#define	LIKE_CHR_SPEC	"like character special"


# if	defined(USE_LIB_PRINTDEVNAME)
/*
 * printdevname() - print block or character device name
 */

int
printdevname(dev, rdev, f, nty)
	dev_t *dev;			/* device */
	dev_t *rdev;			/* raw device */
	int f;				/* 1 = print trailing '\n' */
	int nty;			/* node type: N_BLK or N_CHR */
{

#  if	defined(HAS_STD_CLONE)
	struct clone *c;
#  endif	/* defined(HAS_STD_CLONE) */

	struct l_dev *dp;
	int r = 1;

#  if	defined(HASDCACHE)

printdevname_again:

#  endif	/* defined(HASDCACHE) */

# if	defined(HAS_STD_CLONE)
/*
 * Search for clone if this is a character device on the same device as
 * /dev (or /devices).
 */
	if ((nty == N_CHR) && Lf->is_stream && Clone && (*dev == DevDev)) {
	    r = 0;	/* Don't let lkupdev() rebuild the device cache,
			 * because when it has been rebuilt we want to
			 * search again for clones. */
	    readdev(0);
	    for (c = Clone; c; c = c->next) {
		if (GET_MAJ_DEV(*rdev) == GET_MIN_DEV(Devtp[c->dx].rdev)) {

#  if	defined(HASDCACHE)
		    if (DCunsafe && !Devtp[c->dx].v && !vfy_dev(&Devtp[c->dx]))
			goto printdevname_again;
#  endif	/* defined(HASDCACHE) */

		    safestrprt(Devtp[c->dx].name, stdout, f);
		    return(1);
		}
	    }
	}
# endif	/* defined(HAS_STD_CLONE) */

/*
 * Search appropriate device table for a full match.
 */

# if	defined(HASBLKDEV)
	if (nty == N_BLK)
	    dp = lkupbdev(dev, rdev, 1, r);
	else
# endif	/* defined(HASBLKDEV) */

	dp = lkupdev(dev, rdev, 1, r);
	if (dp) {
	    safestrprt(dp->name, stdout, f);
	    return(1);
	}
/*
 * Search device table for a match without inode number and dev.
 */

# if	defined(HASBLKDEV)
	if (nty == N_BLK)
	    dp = lkupbdev(&DevDev, rdev, 0, r);
	else
# endif	/* defined(HASBLKDEV) */

	dp = lkupdev(&DevDev, rdev, 0, r);
	if (dp) {
	/*
	 * A match was found.  Record it as a name column addition.
	 */
	    char *cp, *ttl;
	    int len;

	    ttl = (nty == N_BLK) ? LIKE_BLK_SPEC : LIKE_CHR_SPEC;
	    len = (int)(1 + strlen(ttl) + 1 + strlen(dp->name) + 1);
	    if (!(cp = (char *)malloc((MALLOC_S)(len + 1)))) {
		(void) fprintf(stderr, "%s: no nma space for: (%s %s)\n",
		    Pn, ttl, dp->name);
		Exit(1);
	    }
	    (void) snpf(cp, len + 1, "(%s %s)", ttl, dp->name);
	    (void) add_nma(cp, len);
	    (void) free((MALLOC_P *)cp);
	    return(0);
	}

# if	defined(HASDCACHE)
/*
 * We haven't found a match.
 *
 * If rebuilding the device cache was suppressed and the device cache is
 * "unsafe," rebuild it.
 */
	if (!r && DCunsafe) {
	    (void) rereaddev();
	    goto printdevname_again;
	}
# endif	/* defined(HASDCACHE) */

	return(0);
}
#endif	/* defined(USE_LIB_PRINTDEVNAME) */
