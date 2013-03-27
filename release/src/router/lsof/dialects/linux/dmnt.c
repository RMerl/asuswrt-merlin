/*
 * dmnt.c -- Linux mount support functions for /proc-based lsof
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

#ifndef	lint
static char copyright[] =
"@(#) Copyright 1997 Purdue Research Foundation.\nAll rights reserved.\n";
static char *rcsid = "$Id: dmnt.c,v 1.17 2008/04/15 13:32:26 abe Exp $";
#endif


#include "lsof.h"


/*
 * Local definitions
 */

#if	defined(HASMNTSUP)
#define	HASHMNT	128			/* mount supplement hash bucket count
					 * !!!MUST BE A POWER OF 2!!! */
#endif	/* defined(HASMNTSUP) */


/*
 * Local function prototypes
 */

_PROTOTYPE(static char *cvtoe,(char *os));

#if	defined(HASMNTSUP)
_PROTOTYPE(static int getmntdev,(char *dn, struct stat *s, int *ss));
_PROTOTYPE(static int hash_mnt,(char *dn));
#endif	/* defined(HASMNTSUP) */


/*
 * Local structure definitions.
 */

#if	defined(HASMNTSUP)
typedef struct mntsup {
	char *dn;			/* directory name */
	dev_t dev;			/* device number */
	int ln;				/* line on which defined */
	struct mntsup *next;		/* next entry */
} mntsup_t;
#endif	/* defined(HASMNTSUP) */


/*
 * Local static definitions
 */

static struct mounts *Lmi = (struct mounts *)NULL;	/* local mount info */
static int Lmist = 0;					/* Lmi status */
static mntsup_t **MSHash = (mntsup_t **)NULL;		/* mount supplement
							 * hash buckets */


/*
 * cvtoe() -- convert octal-escaped characters in string
 */

static char *
cvtoe(os)
	char *os;			/* original string */
{
	int c, cl, cx, ol, ox, tx;
	char *cs;
	int tc;
/*
 * Allocate space for a copy of the string in which octal-escaped characters
 * can be replaced by the octal value -- e.g., \040 with ' '.  Leave room for
 * a '\0' terminator.
 */
	if (!(ol = (int)strlen(os)))
	   return((char *)NULL);
	if (!(cs = (char *)malloc(ol + 1))) {
	    (void) fprintf(stderr,
		"%s: can't allocate %d bytes for octal-escaping.\n",
		Pn, ol + 1);
	    Exit(1);
	}
/*
 * Copy the string, replacing octal-escaped characters as they are found.
 */
	for (cx = ox = 0, cl = ol; ox < ol; ox++) {
	    if (((c = (int)os[ox]) == (int)'\\') && ((ox + 3) < ol)) {

	    /*
	     * The beginning of an octal-escaped character has been found.
	     *
	     * Convert the octal value to a character value.
	     */
		for (tc = 0, tx = 1; os[ox + tx] && (tx < 4); tx++) {
		    if (((int)os[ox + tx] < (int)'0')
		    ||  ((int)os[ox + tx] > (int)'7'))
		    {

		    /*
		     * The escape isn't followed by octets, so ignore the
		     * escape and just copy it.
		     */
			break;
		    }
		    tc <<= 3;
		    tc += (int)(os[ox + tx] - '0');
		}
		if (tx == 4) {

		/*
		 * If three octets (plus the escape) were assembled, use their
		 * character-forming result.
		 *
		 * Otherwise copy the escape and what follows it until another
		 * escape is found.
		 */
		    ox += 3;
		    c = (tc & 0xff);
		}
	    }
	    if (cx >= cl) {

	    /*
	     * Expand the copy string, as required.  Leave room for a '\0'
	     * terminator.
	     */
		cl += 64;		/* (Make an arbitrary increase.) */
		if (!(cs = (char *)realloc(cs, cl + 1))) {
		    (void) fprintf(stderr,
			"%s: can't realloc %d bytes for octal-escaping.\n",
			Pn, cl + 1);
		    Exit(1);
		}
	    }
	/*
	 * Copy the character.
	 */
	    cs[cx++] = (char)c;
	}
/*
 * Terminate the copy and return its pointer.
 */
	cs[cx] = '\0';
	return(cs);
}


#if	defined(HASMNTSUP)
/*
 * getmntdev() - get mount device from mount supplement
 */

static int
getmntdev(dn, s, ss)
	char *dn;			/* mount point directory name */
	struct stat *s;			/* stat(2) buffer receptor */
	int *ss;			/* stat(2) status result -- i.e., SB_*
					 * values */
{
	static int err = 0;
	int h;
	mntsup_t *mp, *mpn;
	static char *vbuf = (char *)NULL;
	static size_t vsz = (size_t)0;

	if (err)
	    return(0);
	if (!MSHash) {

	/*
	 * No mount supplement hash buckets have been allocated, so read the
	 * mount supplement file and create hash buckets for its entries.
	 */
	    char buf[(MAXPATHLEN*2) + 1], *dp, path[(MAXPATHLEN*2) + 1];
	    dev_t dev;
	    FILE *fs;
	    int ln = 0;
	    size_t sz;

	    if ((MntSup != 2) || !MntSupP)
		return(0);
	    if (!is_readable(MntSupP, 1)) {

	    /*
	     * The mount supplement file isn't readable.
	     */
		err = 1;
		return(0);
	    }
	    if (!(fs = open_proc_stream(MntSupP, "r", &vbuf, &vsz, 0))) {

	    /*
	     * The mount supplement file can't be opened for reading.
	     */
		if (!Fwarn)
		    (void) fprintf(stderr, "%s: can't open(%s): %s\n",
			Pn, MntSupP, strerror(errno));
		err = 1;
		return(0);
	    }
	    buf[sizeof(buf) - 1] = '\0';
	/*
	 * Read the mount supplement file.
	 */
	    while (fgets(buf, sizeof(buf) - 1, fs)) {
		ln++;
		if ((dp = strchr(buf, '\n')))
		    *dp = '\0';
		if (buf[0] != '/') {

		/*
		 * The mount supplement line doesn't begin with the absolute
		 * path character '/'.
		 */
		    if (!Fwarn)
			(void) fprintf(stderr,
			    "%s: %s line %d: no path: \"%s\"\n",
			    Pn, MntSupP, ln, buf);
		    err = 1;
		    continue;
		}
		if (!(dp = strchr(buf, ' ')) || strncmp(dp + 1, "0x", 2)) {

		/*
		 * The path on the mount supplement line isn't followed by
		 * " 0x".
		 */
		    if (!Fwarn)
			(void) fprintf(stderr,
			    "%s: %s line %d: no device: \"%s\"\n",
			    Pn, MntSupP, ln, buf);
		    err = 1;
		    continue;
		}
		sz = (size_t)(dp - buf);
		(void) strncpy(path, buf, sz);
		path[sz] = '\0';
	    /*
	     * Assemble the hexadecimal device number of the mount supplement
	     * line.
	     */
		for (dev = 0, dp += 3; *dp; dp++) {
		    if (!isxdigit((int)*dp))
			break;
		    if (isdigit((int)*dp))
			dev = (dev << 4) + (int)*dp - (int)'0';
		    else
			dev = (dev << 4) + (int)tolower(*dp) - (int)'a' + 10;
		}
		if (*dp) {

		/*
		 * The device number couldn't be assembled.
		 */
		    if (!Fwarn)
			(void) fprintf(stderr,
			    "%s: %s line %d: illegal device: \"%s\"\n",
			    Pn, MntSupP, ln, buf);
		    err = 1;
		    continue;
		}
	    /*
	     * Search the mount supplement hash buckets.  (Allocate them as
	     * required.)
	     */
		if (!MSHash) {
		    if (!(MSHash = (mntsup_t **)calloc(HASHMNT,
						       sizeof(mntsup_t *)))
		    ) {
			(void) fprintf(stderr,
			    "%s: no space for mount supplement hash buckets\n",
			    Pn);
			Exit(1);
		    }
		}
		h = hash_mnt(path);
		for (mp = MSHash[h]; mp; mp = mp->next) {
		    if (!strcmp(mp->dn, path))
			break;
		}
		if (mp) {

		/*
		 * A path match was located.  If the device number is the
		 * same, skip this mount supplement line.  Otherwise, issue
		 * a warning.
		 */
		    if (mp->dev != dev) {
			(void) fprintf(stderr,
			    "%s: %s line %d path duplicate of %d: \"%s\"\n",
			    Pn, MntSupP, ln, mp->ln, buf);
			err = 1;
		    }
		    continue;
		}
	    /*
	     * Allocate and fill a new mount supplement hash entry.
	     */
		if (!(mpn = (mntsup_t *)malloc(sizeof(mntsup_t)))) {
		    (void) fprintf(stderr,
			"%s: no space for mount supplement entry: %d \"%s\"\n",
			Pn, ln, buf);
		    Exit(1);
		}
		if (!(mpn->dn = (char *)malloc(sz + 1))) {
		    (void) fprintf(stderr,
			"%s: no space for mount supplement path: %d \"%s\"\n",
			Pn, ln, buf);
		    Exit(1);
		}
		(void) strcpy(mpn->dn, path);
		mpn->dev = dev;
		mpn->ln = ln;
		mpn->next = MSHash[h];
		MSHash[h] = mpn;
	    }
	    if (ferror(fs)) {
		if (!Fwarn)
		    (void) fprintf(stderr, "%s: error reading %s\n",
			Pn, MntSupP);
		err = 1;
	    }
	    (void) fclose(fs);
	    if (err) {
		if (MSHash) {
		    for (h = 0; h < HASHMNT; h++) {
			for (mp = MSHash[h]; mp; mp = mpn) {
			    mpn = mp->next;
			    if (mp->dn)
				(void) free((MALLOC_P *)mp->dn);
			    (void) free((MALLOC_P *)mp);
			}
		    }
		    (void) free((MALLOC_P *)MSHash);
		    MSHash = (mntsup_t **)NULL;
		}
		return(0);
	    }
	}
/*
 * If no errors have been detected reading the mount supplement file, search
 * its hash biuckets for the supplied directory path.
 */
	if (err)
	    return(0);
	h = hash_mnt(dn);
	for (mp = MSHash[h]; mp; mp = mp->next) {
	    if (!strcmp(dn, mp->dn)) {
		memset((void *)s, 0, sizeof(struct stat));
		s->st_dev = mp->dev;
		*ss |= SB_DEV;
		return(1);
	    }
	}
	return(0);
}


/*
 * hash_mnt() - hash mount point
 */

static int
hash_mnt(dn)
	char *dn;			/* mount point directory name */
{
	register int i, h;
	size_t l;

	if (!(l = strlen(dn)))
	    return(0);
	if (l == 1)
	    return((int)*dn & (HASHMNT - 1));
	for (i = h = 0; i < (int)(l - 1); i++) {
	    h ^= ((int)dn[i] * (int)dn[i+1]) << ((i*3)%13);
	}
	return(h & (HASHMNT - 1));
}
#endif	/* defined(HASMNTSUP) */


/*
 * readmnt() - read mount table
 */

struct mounts *
readmnt()
{
	char buf[MAXPATHLEN], *cp, **fp;
	char *dn = (char *)NULL;
	int ds;
	char *fp0 = (char *)NULL;
	char *fp1 = (char *)NULL;
	char *ln;
	struct mounts *mp;
	FILE *ms;
	int nfs;
	struct stat sb;
	static char *vbuf = (char *)NULL;
	static size_t vsz = (size_t)0;

	if (Lmi || Lmist)
	    return(Lmi);
/*
 * Open access to /proc/mounts, assigning a page size buffer to its stream.
 */
	(void) snpf(buf, sizeof(buf), "%s/mounts", PROCFS);
	ms = open_proc_stream(buf, "r", &vbuf, &vsz, 1);
/*
 * Read mount table entries.
 */
	while (fgets(buf, sizeof(buf), ms)) {
	    if (get_fields(buf, (char *)NULL, &fp, (int *)NULL, 0) < 3
	    ||  !fp[0] || !fp[1] || !fp[2])
		continue;
	/*
	 * Convert octal-escaped characters in the device name and mounted-on
	 * path name.
	 */
	    if (fp0) {
		(void) free((FREE_P *)fp0);
		fp0 = (char *)NULL;
	    }
	    if (fp1) {
		(void) free((FREE_P *)fp1);
		fp1 = (char *)NULL;
	    }
	    if (!(fp0 = cvtoe(fp[0])) || !(fp1 = cvtoe(fp[1])))
		continue;
	/*
	 * Ignore an entry with a colon in the device name, followed by
	 * "(pid*" -- it's probably an automounter entry.
	 *
	 * Ignore autofs, pipefs, and sockfs entries.
	 */
	    if ((cp = strchr(fp0, ':')) && !strncasecmp(++cp, "(pid", 4))
		continue;
	    if (!strcasecmp(fp[2], "autofs") || !strcasecmp(fp[2], "pipefs")
	    ||  !strcasecmp(fp[2], "sockfs"))
		continue;
	/*
	 * Interpolate a possible symbolic directory link.
	 */
	    if (dn)
		(void) free((FREE_P *)dn);
	    dn = fp1;
	    fp1 = (char *)NULL;
	    if (!(ln = Readlink(dn))) {
		if (!Fwarn){
		    (void) fprintf(stderr,
			"      Output information may be incomplete.\n");
		}
		continue;
	    }
	    if (ln != dn) {
		(void) free((FREE_P *)dn);
		dn = ln;
	    }
	    if (*dn != '/')
		continue;
	/*
	 * Detect an NFS mount point.
	 */
	    if (!(nfs = strcasecmp(fp[2], "nfs")) && !HasNFS)
		HasNFS = 1;
	/*
	 * Stat() the directory.
	 */
	    if (statsafely(dn, &sb)) {
		if (!Fwarn) {
		    (void) fprintf(stderr, "%s: WARNING: can't stat() ", Pn);
		    safestrprt(fp[2], stderr, 0);
		    (void) fprintf(stderr, " file system ");
		    safestrprt(dn, stderr, 1);
		    (void) fprintf(stderr,
			"      Output information may be incomplete.\n");
		}

#if	defined(HASMNTSUP)
		if ((MntSup == 2) && MntSupP) {
		    ds = 0;
		    if (!getmntdev(dn, &sb, &ds) || !(ds & SB_DEV))
			continue;
		    (void) fprintf(stderr,
			"      assuming dev=%#lx from %s\n",
			(long)sb.st_dev, MntSupP);
		} else
		    continue;
#else	/* !defined(HASMNTSUP) */
		continue;
#endif	/* defined(HASMNTSUP) */

	    } else
		ds = SB_ALL;
	/*
	 * Allocate and fill a local mount structure.
	 */
	    if (!(mp = (struct mounts *)malloc(sizeof(struct mounts)))) {
		(void) fprintf(stderr,
		    "%s: can't allocate mounts struct for: ", Pn);
		safestrprt(dn, stderr, 1);
		Exit(1);
	    }
	    mp->dir = dn;
	    dn = (char *)NULL;
	    mp->next = Lmi;
	    mp->dev = ((mp->ds = ds) & SB_DEV) ? sb.st_dev : 0;
	    mp->rdev = (ds & SB_RDEV) ? sb.st_rdev : 0;
	    mp->inode = (INODETYPE)((ds & SB_INO) ? sb.st_ino : 0);
	    mp->mode = (ds & SB_MODE) ? sb.st_mode : 0;
	    if (!nfs) {
		mp->ty = N_NFS;
		if (HasNFS < 2)
		    HasNFS = 2;
	    } else
		mp->ty = N_REGLR;

#if	defined(HASMNTSUP)
	/*
	 * If support for the mount supplement file is defined and if the
	 * +m option was supplied, print mount supplement information.
	 */
	    if (MntSup == 1)
		(void) printf("%s %#lx\n", mp->dir, (long)mp->dev);
#endif	/* defined(HASMNTSUP) */

	/*
	 * Interpolate a possible file system (mounted-on) device name link.
	 */
	    dn = fp0;
	    fp0 = (char *)NULL;
	    mp->fsname = dn;
	    ln = Readlink(dn);
	    dn = (char *)NULL;
	/*
	 * Stat() the file system (mounted-on) name and add file system
	 * information to the local mount table entry.
	 */
	    if (!ln || statsafely(ln, &sb))
		sb.st_mode = 0;
	    mp->fsnmres = ln;
	    mp->fs_mode = sb.st_mode;
	    Lmi = mp;
	}
/*
 * Clean up and return the local mount info table address.
 */
	(void) fclose(ms);
	if (dn)
	    (void) free((FREE_P *)dn);
	if (fp0)
	    (void) free((FREE_P *)fp0);
	if (fp1)
	    (void) free((FREE_P *)fp1);
	Lmist = 1;
	return(Lmi);
}
