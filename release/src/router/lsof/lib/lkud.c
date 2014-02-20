/*
 * lkud.c -- device lookup functions for lsof library
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


/*
 * lkud.c -- lookup device
 *
 * The caller may define:
 *
 *	HASBLKDEV	to activate block device lookup
 */


#include "../machine.h"

#if	defined(HASBLKDEV) || defined(USE_LIB_LKUPDEV)

# if	!defined(lint)
static char copyright[] =
"@(#) Copyright 1997 Purdue Research Foundation.\nAll rights reserved.\n";
static char *rcsid = "$Id: lkud.c,v 1.7 2008/10/21 16:12:36 abe Exp $";
# endif	/* !defined(lint) */

#include "../lsof.h"

#else	/* !defined(HASBLKDEV) && !defined(USE_LIB_LKUPDEV) */
char lkud_d1[] = "d"; char *lkud_d2 = lkud_d1;
#endif	/* defined(HASBLKDEV) || defined(USE_LIB_LKUPDEV) */



#if	defined(HASBLKDEV)
/*
 * lkupbdev() - look up a block device
 */

struct l_dev *
lkupbdev(dev, rdev, i, r)
	dev_t *dev;			/* pointer to device number */
	dev_t *rdev;			/* pointer to raw device number */
	int i;				/* inode match status */
	int r;				/* if 1, rebuild the device cache with
					 * rereaddev() when no match is found
					 * and HASDCACHE is defined and
					 * DCunsafe is one */
{
	INODETYPE inode = (INODETYPE)0;
	int low, hi, mid;
	struct l_dev *dp;
	int ty = 0;

	if (*dev != DevDev)
	    return((struct l_dev *)NULL);
	readdev(0);
	if (i) {
	    inode = Lf->inode;
	    ty = Lf->inp_ty;
	}
/*
 * Search block device table for match.
 */

# if	defined(HASDCACHE)

lkupbdev_again:

# endif	/* defined(HASDCACHE) */

	low = mid = 0;
	hi = BNdev - 1;
	while (low <= hi) {
	    mid = (low + hi) / 2;
	    dp = BSdev[mid];
	    if (*rdev < dp->rdev)
		hi = mid - 1;
	    else if (*rdev > dp->rdev)
		low = mid + 1;
	    else {
		if ((i == 0) || (ty != 1) || (inode == dp->inode)) {

# if	defined(HASDCACHE)
		    if (DCunsafe && !dp->v && !vfy_dev(dp))
			goto lkupbdev_again;
# endif	/* defined(HASDCACHE) */

		    return(dp);
		}
		if (inode < dp->inode)
		    hi = mid - 1;
		else
		    low = mid + 1;
	    }
	}

# if	defined(HASDCACHE)
	if (DCunsafe && r) {
	    (void) rereaddev();
	    goto lkupbdev_again;
	}
# endif	/* defined(HASDCACHE) */

	return((struct l_dev *)NULL);
}
#endif	/* defined(HASBLKDEV) */


#if	defined(USE_LIB_LKUPDEV)
/*
 * lkupdev() - look up a character device
 */

struct l_dev *
lkupdev(dev, rdev, i, r)
	dev_t *dev;			/* pointer to device number */
	dev_t *rdev;			/* pointer to raw device number */
	int i;				/* inode match status */
	int r;				/* if 1, rebuild the device cache with
					 * rereaddev() when no match is found
					 * and HASDCACHE is defined and
					 * DCunsafe is one */
{
	INODETYPE inode = (INODETYPE)0;
	int low, hi, mid;
	struct l_dev *dp;
	int ty = 0;

	if (*dev != DevDev)
	    return((struct l_dev *)NULL);
	readdev(0);
	if (i) {
	    inode = Lf->inode;
	    ty = Lf->inp_ty;
	}
/*
 * Search device table for match.
 */

# if	defined(HASDCACHE)

lkupdev_again:

# endif	/* defined(HASDCACHE) */

	low = mid = 0;
	hi = Ndev - 1;
	while (low <= hi) {
	    mid = (low + hi) / 2;
	    dp = Sdev[mid];
	    if (*rdev < dp->rdev)
		hi = mid - 1;
	    else if (*rdev > dp->rdev)
		low = mid + 1;
	    else {
		if ((i == 0) || (ty != 1) || (inode == dp->inode)) {

# if	defined(HASDCACHE)
		    if (DCunsafe && !dp->v && !vfy_dev(dp))
			goto lkupdev_again;
# endif	/* defined(HASDCACHE) */

		    return(dp);
		}
		if (inode < dp->inode)
		    hi = mid - 1;
		else
		    low = mid + 1;
	    }
	}

# if	defined(HASDCACHE)
	if (DCunsafe && r) {
	    (void) rereaddev();
	    goto lkupdev_again;
	}
# endif	/* defined(HASDCACHE) */

	return((struct l_dev *)NULL);
}
#endif	/* defined(USE_LIB_LKUPDEV) */
