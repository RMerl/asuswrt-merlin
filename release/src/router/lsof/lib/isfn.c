/*
 * isfn.c -- is_file_named() function for lsof library
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
 * To use this source file:
 *
 * 1. Define USE_LIB_IS_FILE_NAMED.
 *
 * 2. If clone support is required:
 *
 *    a.  Define HAVECLONEMAJ to be the name of the variable that
 *	  contains the status of the clone major device -- e.g.,
 *
 *		#define HAVECLONEMAJ HaveCloneMaj
 *
 *    b.  Define CLONEMAJ to be the name of the constant or
 *	  variable that defines the clone major device -- e.g.,
 *
 *		#define CLONEMAJ CloneMaj
 *
 *    c.  Make sure that clone devices are identified by an lfile
 *	  element is_stream value of 1.
 *
 *    d.  Accept clone searching by device number only.
 */


#include "../machine.h"

#if	defined(USE_LIB_IS_FILE_NAMED)

# if	!defined(lint)
static char copyright[] =
"@(#) Copyright 1997 Purdue Research Foundation.\nAll rights reserved.\n";
static char *rcsid = "$Id: isfn.c,v 1.10 2008/10/21 16:12:36 abe Exp $";
# endif	/* !defined(lint) */

#include "../lsof.h"


/*
 * Local structures
 */

struct hsfile {
	struct sfile *s;		/* the Sfile table address */
	struct hsfile *next;		/* the next hash bucket entry */
};

/*
 * Local static variables
 */

# if	defined(HAVECLONEMAJ)
static struct hsfile *HbyCd =		/* hash by clone buckets */
	(struct hsfile *)NULL;
static int HbyCdCt = 0;			/* HbyCd entry count */
# endif	/* defined(HAVECLONEMAJ) */

static struct hsfile *HbyFdi =		/* hash by file (dev,ino) buckets */
	(struct hsfile *)NULL;
static int HbyFdiCt = 0;		/* HbyFdi entry count */
static struct hsfile *HbyFrd =		/* hash by file raw device buckets */
	(struct hsfile *)NULL;
static int HbyFrdCt = 0;		/* HbyFrd entry count */
static struct hsfile *HbyFsd =		/* hash by file system buckets */
	(struct hsfile *)NULL;
static int HbyFsdCt = 0;		/* HbyFsd entry count */
static struct hsfile *HbyNm =		/* hash by name buckets */
	(struct hsfile *)NULL;
static int HbyNmCt = 0;			/* HbyNm entry count */


/*
 * Local definitions
 */

# if	defined(HAVECLONEMAJ)
#define	SFCDHASH	1024		/* Sfile hash by clone device (power
					 * of 2!) */
# endif	/* defined(HAVECLONEMAJ) */

#define	SFDIHASH	4094		/* Sfile hash by (device,inode) number
					 * pair bucket count (power of 2!) */
#define	SFFSHASH	1024		/* Sfile hash by file system device
					 * number bucket count (power of 2!) */
#define SFHASHDEVINO(maj, min, ino, mod) ((int)(((int)((((int)(maj+1))*((int)((min+1))))+ino)*31415)&(mod-1)))
					/* hash for Sfile by major device,
					 * minor device, and inode, modulo mod
					 * (mod must be a power of 2) */
#define	SFRDHASH	1024		/* Sfile hash by raw device number
					 * bucket count (power of 2!) */
#define SFHASHRDEVI(maj, min, rmaj, rmin, ino, mod) ((int)(((int)((((int)(maj+1))*((int)((min+1))))+((int)(rmaj+1)*(int)(rmin+1))+ino)*31415)&(mod-1)))
					/* hash for Sfile by major device,
					 * minor device, major raw device,
					 * minor raw device, and inode, modulo
					 * mod (mod must be a power of 2) */
#define	SFNMHASH	4096		/* Sfile hash by name bucket count
					 * (must be a power of 2!) */



/*
 * hashSfile() - hash Sfile entries for use in is_file_named() searches
 */

void
hashSfile()
{
	static int hs = 0;
	int i;
	int sfplm = 3;
	struct sfile *s;
	struct hsfile *sh, *sn;
/*
 * Do nothing if there are no file search arguments cached or if the
 * hashes have already been constructed.
 */
	if (!Sfile || hs)
	    return;
/*
 * Allocate hash buckets by (device,inode), file system device, and file name.
 */

# if	defined(HAVECLONEMAJ)
	if (HAVECLONEMAJ) {
	    if (!(HbyCd = (struct hsfile *)calloc((MALLOC_S)SFCDHASH,
						  sizeof(struct hsfile))))
	    {
		(void) fprintf(stderr,
		    "%s: can't allocate space for %d clone hash buckets\n",
		    Pn, SFCDHASH);
		Exit(1);
	    }
	    sfplm++;
	}
# endif	/* defined(HAVECLONEMAJ) */

	if (!(HbyFdi = (struct hsfile *)calloc((MALLOC_S)SFDIHASH,
					       sizeof(struct hsfile))))
	{
	    (void) fprintf(stderr,
		"%s: can't allocate space for %d (dev,ino) hash buckets\n",
		Pn, SFDIHASH);
	    Exit(1);
	}
	if (!(HbyFrd = (struct hsfile *)calloc((MALLOC_S)SFRDHASH,
					       sizeof(struct hsfile))))
	{
	    (void) fprintf(stderr,
		"%s: can't allocate space for %d rdev hash buckets\n",
		Pn, SFRDHASH);
	    Exit(1);
	}
	if (!(HbyFsd = (struct hsfile *)calloc((MALLOC_S)SFFSHASH,
					       sizeof(struct hsfile))))
	{
	    (void) fprintf(stderr,
		"%s: can't allocate space for %d file sys hash buckets\n",
		Pn, SFFSHASH);
	    Exit(1);
	}
	if (!(HbyNm = (struct hsfile *)calloc((MALLOC_S)SFNMHASH,
					      sizeof(struct hsfile))))
	{
	    (void) fprintf(stderr,
		"%s: can't allocate space for %d name hash buckets\n",
		Pn, SFNMHASH);
	    Exit(1);
	}
	hs++;
/*
 * Scan the Sfile chain, building file, file system, raw device, and file
 * name hash bucket chains.
 */
	for (s = Sfile; s; s = s->next) {
	    for (i = 0; i < sfplm; i++) {
		if (i == 0) {
		    if (!s->aname)
			continue;
		    sh = &HbyNm[hashbyname(s->aname, SFNMHASH)];
		    HbyNmCt++;
		} else if (i == 1) {
		    if (s->type) {
			sh = &HbyFdi[SFHASHDEVINO(GET_MAJ_DEV(s->dev),
						  GET_MIN_DEV(s->dev), s->i,
						  SFDIHASH)];
			HbyFdiCt++;
		    } else {
			sh = &HbyFsd[SFHASHDEVINO(GET_MAJ_DEV(s->dev),
						  GET_MIN_DEV(s->dev),
						  0,
						  SFFSHASH)];
			HbyFsdCt++;
		    }
		} else if (i == 2) {
		    if ((s->mode == S_IFCHR) || (s->mode == S_IFBLK)) {
			sh = &HbyFrd[SFHASHRDEVI(GET_MAJ_DEV(s->dev),
						 GET_MIN_DEV(s->dev),
						 GET_MAJ_DEV(s->rdev),
						 GET_MIN_DEV(s->rdev),
						 s->i,
						 SFRDHASH)];
			HbyFrdCt++;
		    } else
			continue;
		}

# if	defined(HAVECLONEMAJ)
		else {
		    if (!HAVECLONEMAJ || (GET_MAJ_DEV(s->rdev) != CLONEMAJ))
			continue;
		    sh = &HbyCd[SFHASHDEVINO(0, GET_MIN_DEV(s->rdev), 0,
					     SFCDHASH)];
		    HbyCdCt++;
		}
# else	/* ! defined(HAVECLONEMAJ) */
		else
		    continue;
# endif	/* defined(HAVECLONEMAJ) */

		if (!sh->s) {
		    sh->s = s;
		    sh->next = (struct hsfile *)NULL;
		    continue;
		} else {
		    if (!(sn = (struct hsfile *)malloc(
				(MALLOC_S)sizeof(struct hsfile))))
		    {
			(void) fprintf(stderr,
			    "%s: can't allocate hsfile bucket for: %s\n",
			    Pn, s->aname);
			Exit(1);
		    }
		    sn->s = s;
		    sn->next = sh->next;
		    sh->next = sn;
		}
	    }
	}
}


/*
 * is_file_named() - is this file named?
 */

int
is_file_named(p, cd)
	char *p;			/* path name; NULL = search by device
					 * and inode (from *Lf) */
	int cd;				/* character or block type file --
					 * VCHR or VBLK vnode, or S_IFCHR
					 * or S_IFBLK inode */
{
	char *ep;
	int f = 0;
	struct sfile *s = (struct sfile *)NULL;
	struct hsfile *sh;
	size_t sz;
/*
 * Check for a path name match, as requested.
 */
	if (p && HbyNmCt) {
	    for (sh = &HbyNm[hashbyname(p, SFNMHASH)]; sh; sh = sh->next) {
		if ((s = sh->s) && strcmp(p, s->aname) == 0) {
		    f = 2;
		    break;
		}
	    }
	}

# if	defined(HAVECLONEMAJ)
/*
 * If this is a stream, check for a clone device match.
 */
	if (!f && HbyCdCt && Lf->is_stream && Lf->dev_def && Lf->rdev_def
	&&  (Lf->dev == DevDev))
	{
	    for (sh = &HbyCd[SFHASHDEVINO(0, GET_MAJ_DEV(Lf->rdev), 0,
					  SFCDHASH)];
		 sh;
		 sh = sh->next)
	    {
		if ((s = sh->s) && (GET_MAJ_DEV(Lf->rdev)
		==		    GET_MIN_DEV(s->rdev))) {
		    f = 3;
		    break;
		}
	    }
	}
# endif	/* defined(HAVECLONEMAJ) */

/*
 * Check for a regular file.
 */
	if (!f && HbyFdiCt && Lf->dev_def
	&& (Lf->inp_ty == 1 || Lf->inp_ty == 3))
	{
	    for (sh = &HbyFdi[SFHASHDEVINO(GET_MAJ_DEV(Lf->dev),
					   GET_MIN_DEV(Lf->dev),
					   Lf->inode,
					   SFDIHASH)];
		 sh;
		 sh = sh->next)
	    {
		if ((s = sh->s) && (Lf->dev == s->dev)
		&&  (Lf->inode == s->i)) {
		    f = 1;
		    break;
		}
	    }
	}
/*
 * Check for a file system match.
 */
	if (!f && HbyFsdCt && Lf->dev_def) {
	    for (sh = &HbyFsd[SFHASHDEVINO(GET_MAJ_DEV(Lf->dev),
	    				   GET_MIN_DEV(Lf->dev), 0,
					   SFFSHASH)];
		 sh;
		 sh = sh->next)
	    {
		if ((s = sh->s) && (s->dev == Lf->dev)) {
		    f = 1;
		    break;
		}
	    }
	}
/*
 * Check for a character or block device match.
 */
	if (!f && HbyFrdCt && cd
	&&  Lf->dev_def && (Lf->dev == DevDev)
	&&  Lf->rdev_def
	&& (Lf->inp_ty == 1 || Lf->inp_ty == 3))
	{
	    for (sh = &HbyFrd[SFHASHRDEVI(GET_MAJ_DEV(Lf->dev),
					  GET_MIN_DEV(Lf->dev),
					  GET_MAJ_DEV(Lf->rdev),
					  GET_MIN_DEV(Lf->rdev),
					  Lf->inode, SFRDHASH)];
		 sh;
		 sh = sh->next)
	    {
		if ((s = sh->s) && (s->dev == Lf->dev)
		&&  (s->rdev == Lf->rdev) && (s->i == Lf->inode))
		{
		    f = 1;
		    break;
		}
	    }
	}
/*
 * Convert the name if a match occurred.
 */
	switch (f) {
	case 0:
	    return(0);
	case 1:
	    if (s->type) {

	    /*
	     * If the search argument isn't a file system, propagate it
	     * to Namech[]; otherwise, let printname() compose the name.
	     */
		(void) snpf(Namech, Namechl, "%s", s->name);
		if (s->devnm) {
		    ep = endnm(&sz);
		    (void) snpf(ep, sz, " (%s)", s->devnm);
		}
	    }
	    break;
	case 2:
	    (void) strcpy(Namech, p);
	    break;

# if	defined(HAVECLONEMAJ)
	/* case 3:		do nothing for stream clone matches */
# endif	/* defined(HAVECLONEMAJ) */

	}
	if (s)
	    s->f = 1;
	return(1);
}
#else	/* !defined(USE_LIB_IS_FILE_NAMED) */
char isfn_d1[] = "d"; char *isfn_d2 = isfn_d1;
#endif	/* defined(USE_LIB_IS_FILE_NAMED) */
