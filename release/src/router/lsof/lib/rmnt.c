/*
 * rmnt.c -- readmnt() function for lsof library
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

#if	defined(USE_LIB_READMNT)

# if	!defined(lint)
static char copyright[] =
"@(#) Copyright 1997 Purdue Research Foundation.\nAll rights reserved.\n";
static char *rcsid = "$Id: rmnt.c,v 1.12 2008/10/21 16:13:23 abe Exp $";
# endif	/* !defined(lint) */

#include "../lsof.h"



/*
 * The caller may define:
 *
 * 1.  An RMNT_EXPDEV macro to expand (ala EP/IX) device numbers;
 *
 *     EP/IX, for example, uses:
 *
 *	#define RMNT_EXPDEV(n) expdev(n)
 *
 * 2.  A custom macro, MNTSKIP, for making decisions to skip entries
 *     -- e.g., ones whose mnt_type is MNTTYPE_IGNORE.
 *
 * 3.  RMNT_FSTYPE to specify the member name of the character string of the
 *     mntent structure containing the file system type, and MOUNTS_FSTYPE to
 *     specify the member name of the character string pointer of the local
 *     mounts structure where RMNT_FSTYPE is to be copied.
 *
 * 4.  RMNT_STAT_FSTYPE to specify the member name of the stat structure
 *     containing an integer file system type, and MOUNTS_STAT_FSTYPE to
 *     specify the member name of the integer in the local mounts structure
 *     where RMNT_STAT_FSTYPE is to be copied.
 *     
 */

#if	!defined(RMNT_EXPDEV)
#define	RMNT_EXPDEV(n)	n
#endif	/* !defined(RMNT_EXPDEV) */


/*
 * Local static definitions
 */

static struct mounts *Lmi = (struct mounts *)NULL;	/* local mount info */
static int Lmist = 0;					/* Lmi status */


/*
 * readmnt() - read mount table
 */

struct mounts *
readmnt()
{
	char *dn = (char *)NULL;
	char *ln;
	FILE *mfp;
	struct mntent *mp;
	struct mounts *mtp;
	char *opt, *opte;
	struct stat sb;

	if (Lmi || Lmist)
	    return(Lmi);
/*
 * Open access to the mount table.
 */
	if (!(mfp = setmntent(MOUNTED, "r"))) {
	    (void) fprintf(stderr, "%s: can't access %s\n", Pn, MOUNTED);
	    Exit(1);
	}
/*
 * Read mount table entries.
 */
	while ((mp = getmntent(mfp))) {

#if	defined(MNTSKIP)
	/*
	 * Specfy in the MNTSKIP macro the decisions needed to determine
	 * that this entry should be skipped.
	 *
	 * Typically entries whose mnt_type is MNTTYPE_IGNORE are skipped.
	 *
	 * The MNTSKIP macro allows the caller to use other tests.
	 */
	    MNTSKIP
#endif	/* MNTSKIP */

	/*
	 * Interpolate a possible symbolic directory link.
	 */
	    if (dn)
		(void) free((FREE_P *)dn);
	    if (!(dn = mkstrcpy(mp->mnt_dir, (MALLOC_S *)NULL)))
		goto no_space_for_mount;
	    if (!(ln = Readlink(dn))) {
		if (!Fwarn)
		    (void) fprintf(stderr,
			"      Output information may be incomplete.\n");
		continue;
	    }
	    if (ln != dn) {
		(void) free((FREE_P *)dn);
		dn = ln;
	    }
	    if (*dn != '/')
		continue;
	/*
	 * Stat() the directory.
	 */
	    if (statsafely(dn, &sb)) {
		if (!Fwarn) {
		    (void) fprintf(stderr, "%s: WARNING: can't stat() ", Pn);
		    safestrprt(mp->mnt_type, stderr, 0);
		    (void) fprintf(stderr, " file system ");
		    safestrprt(mp->mnt_dir, stderr, 1);
		    (void) fprintf(stderr,
			"      Output information may be incomplete.\n");
		}
		if ((opt = strstr(mp->mnt_opts, "dev="))) {
		    (void) zeromem(&sb, sizeof(sb));
		    if ((opte = x2dev(opt + 4, (dev_t *)&sb.st_dev))) {
			sb.st_mode = S_IFDIR | 0777;
			if (!Fwarn)
			    (void) fprintf(stderr,
				"      assuming \"%.*s\" from %s\n",
				(int)(opte - opt), opt, MOUNTED);
		    } else
			opt = (char *)NULL;
		}
		if (!opt)
		    continue;
	    }
	/*
	 * Allocate and fill a local mounts structure with the directory
	 * (mounted) information.
	 */
	    if (!(mtp = (struct mounts *)malloc(sizeof(struct mounts)))) {

no_space_for_mount:

		(void) fprintf(stderr, "%s: no space for mount at ", Pn);
		safestrprt(mp->mnt_fsname, stderr, 0);
		(void) fprintf(stderr, " (");
		safestrprt(mp->mnt_dir, stderr, 0);
		(void) fprintf(stderr, ")\n");
		Exit(1);
	    }
	    mtp->dir = dn;
	    dn = (char *)NULL;
	    mtp->next = Lmi;
	    mtp->dev = RMNT_EXPDEV(sb.st_dev);
	    mtp->rdev = RMNT_EXPDEV(sb.st_rdev);
	    mtp->inode = (INODETYPE)sb.st_ino;
	    mtp->mode = sb.st_mode;

# if	defined(RMNT_FSTYPE) && defined(MOUNTS_FSTYPE)
	/*
	 * Make a copy of RMNT_FSTYPE in MOUNTS_FSTYPE.
	 */
	    if (!(mtp->MOUNTS_FSTYPE = mkstrcpy(mp->RMNT_FSTYPE,
						(MALLOC_S *)NULL)))
	    {
		(void) fprintf(stderr, "%s: no space for fstype (%s): %s\n",
		    Pn, mtp->dir, mp->RMNT_FSTYPE);
		Exit(1);
	    }
	    (void) strcpy(mtp->MOUNTS_FSTYPE, mp->RMNT_FSTYPE);
# endif	/* defined(RMNT_FSTYP) && defined(MOUNTS_FSTYP) */

# if	defined(RMNT_STAT_FSTYPE) && defined(MOUNTS_STAT_FSTYPE)
	/*
	 * Make a copy of RMNT_STAT_FSTYPE in MOUNTS_STAT_FSTYPE.
	 */
	    mtp->MOUNTS_STAT_FSTYPE = (int)sb.RMNT_STAT_FSTYPE;
# endif	/* defined(RMNT_STAT_FSTYP) && defined(MOUNTS_STAT_FSTYP) */

	/*
	 * Interpolate a possible file system (mounted-on device) name link.
	 */
	    if (!(dn = mkstrcpy(mp->mnt_fsname, (MALLOC_S *)NULL)))
		goto no_space_for_mount;
	    mtp->fsname = dn;
	    ln = Readlink(dn);
	    dn = (char *)NULL;
	/*
	 * Stat() the file system (mounted-on) name and add file system
	 * information to the local mounts structure.
	 */
	    if (!ln || statsafely(ln, &sb))
		sb.st_mode = 0;
	    mtp->fsnmres = ln;
	    mtp->fs_mode = sb.st_mode;
	    Lmi = mtp;
	}
	(void) endmntent(mfp);
/*
 * Clean up and return the local nount info table address.
 */
	if (dn)
	    (void) free((FREE_P *)dn);
	Lmist = 1;
	return(Lmi);
}
#else	/* !defined(USE_LIB_READMNT) */
char rmnt_d1[] = "d"; char *rmnt_d2 = rmnt_d1;
#endif	/* defined(USE_LIB_READMNT) */
