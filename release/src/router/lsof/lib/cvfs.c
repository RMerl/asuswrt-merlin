/*
 * cvfs.c -- completevfs() function for lsof library
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
 * The caller must define CVFS_DEVSAVE to have the device number moved
 * from the mounts entry to the local vfs structure.
 *
 * The caller must define CVFS_NLKSAVE to have the link count moved from
 * the mounts entry to the local vfs structure.
 *
 * The caller must define CVFS_SZSAVE to have the size moved from the
 * mounts entry to the local vfs structure.
 */


#include "../machine.h"

#if	defined(USE_LIB_COMPLETEVFS)

# if	!defined(lint)
static char copyright[] =
"@(#) Copyright 1997 Purdue Research Foundation.\nAll rights reserved.\n";
static char *rcsid = "$Id: cvfs.c,v 1.6 2008/10/21 16:12:36 abe Exp $";
# endif	/* !defined(lint) */

#include	"../lsof.h"


/*
 * completevfs() - complete local vfs structure
 */

void
completevfs(vfs, dev)
	struct l_vfs *vfs;		/* local vfs structure pointer */
	dev_t *dev;			/* device */
{
	struct mounts *mp;
/*
 * If only Internet socket files are selected, don't bother completing the
 * local vfs structure.
 */
	if (Selinet)
	    return;
/*
 * Search for a match on device number.
 */
	for (mp = readmnt(); mp; mp = mp->next) {
	    if (mp->dev == *dev) {

# if	defined(CVFS_DEVSAVE)
		vfs->dev = mp->dev;
# endif	/* defined(CVFS_DEVSAVE) */

# if	defined(CVFS_NLKSAVE)
		vfs->nlink = mp->nlink;
# endif	/* defined(CVFS_NLKSAVE) */

# if	defined(CVFS_SZSAVE)
		vfs->size = mp->size;
# endif	/* defined(CVFS_SZSAVE) */

		vfs->dir = mp->dir;
		vfs->fsname = mp->fsname;

# if	defined(HASFSINO)
		vfs->fs_ino = mp->inode;
# endif	/* defined(HASFSINO) */

# if	defined(HASMNTSTAT)
		vfs->mnt_stat = mp->stat;
# endif	/* defined(HASMNTSTAT) */


		return;
	    }
	}
}
#else	/* !defined(USE_LIB_COMPLETEVFS) */
char cvfs_d1[] = "d"; char *cvfs_d2 = cvfs_d1;
#endif	/* defined(USE_LIB_COMPLETEVFS) */
