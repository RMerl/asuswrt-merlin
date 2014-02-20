/*
 * fino.c -- find inode functions for lsof library
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
 * fino.c -- find block (optional) and character device file inode numbers
 *
 * The caller must define:
 *
 *	HASBLKDEV	to activate the block device inode lookup
 */


#include "../machine.h"

#if	defined(HASBLKDEV) || defined(USE_LIB_FIND_CH_INO)

# if	!defined(lint)
static char copyright[] =
"@(#) Copyright 1997 Purdue Research Foundation.\nAll rights reserved.\n";
static char *rcsid = "$Id: fino.c,v 1.5 2008/10/21 16:12:36 abe Exp $";
# endif	/* !defined(lint) */

#include "../lsof.h"

#else	/* !defined(HASBLKDEV) && !defined(USE_LIB_FIND_CH_INO) */
char fino_d1[] = "d"; char *fino_d2 = fino_d1;
#endif	/* defined(HASBLKDEV) || defined(USE_LIB_FIND_CH_INO) */


#if	defined(HASBLKDEV)
/*
 * find_bl_ino() - find the inode number for a block device file
 */

void
find_bl_ino()
{
	dev_t ldev, tdev;
	int low, hi, mid;

	readdev(0);

# if	defined(HASDCACHE)
find_bl_ino_again:
# endif	/* defined(HASDCACHE) */

	low = mid = 0;
	hi = BNdev - 1;
	if (!Lf->dev_def || (Lf->dev != DevDev) || !Lf->rdev_def)
	    return;
	ldev = Lf->rdev;
	while (low <= hi) {
	    mid = (low + hi) / 2;
	    tdev = BSdev[mid]->rdev;
	    if (ldev < tdev)
		hi = mid - 1;
	    else if (ldev > tdev)
		low = mid + 1;
	    else {

# if	defined(HASDCACHE)
		if (DCunsafe && !BSdev[mid]->v && !vfy_dev(BSdev[mid]))
		    goto find_bl_ino_again;
# endif	/* defined(HASDCACHE) */

		Lf->inode = BSdev[mid]->inode;
		if (Lf->inp_ty == 0)
		    Lf->inp_ty = 1;
		return;
	    }
	}
}
#endif	/* defined(HASBLKDEV) */


#if	defined(USE_LIB_FIND_CH_INO)
/*
 * find_ch_ino() - find the inode number for a character device file
 */

void
find_ch_ino()
{
	dev_t ldev, tdev;
	int low, hi, mid;

	readdev(0);

# if	defined(HASDCACHE)
find_ch_ino_again:
# endif	/* defined(HASDCACHE) */

	low = mid = 0;
	hi = Ndev - 1;
	if (!Lf->dev_def || (Lf->dev != DevDev) || !Lf->rdev_def)
	    return;
	ldev = Lf->rdev;
	while (low <= hi) {
	    mid = (low + hi) / 2;
	    tdev = Sdev[mid]->rdev;
	    if (ldev < tdev)
		hi = mid - 1;
	    else if (ldev > tdev)
		low = mid + 1;
	    else {

# if	defined(HASDCACHE)
		if (DCunsafe && !Sdev[mid]->v && !vfy_dev(Sdev[mid]))
		    goto find_ch_ino_again;
# endif	/* defined(HASDCACHE) */

		Lf->inode = Sdev[mid]->inode;
		if (Lf->inp_ty == 0)
		    Lf->inp_ty = 1;
		return;
	    }
	}
}
#endif	/* defined(USE_LIB_FIND_CH_INO) */
