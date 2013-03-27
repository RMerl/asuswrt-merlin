/*
 * dvch.c -- device cache functions for lsof library
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

#if	defined(HASDCACHE)

# if	!defined(lint)
static char copyright[] =
"@(#) Copyright 1997 Purdue Research Foundation.\nAll rights reserved.\n";
static char *rcsid = "$Id: dvch.c,v 1.16 2008/10/21 16:12:36 abe Exp $";
# endif	/* !defined(lint) */

#include "../lsof.h"

/*
 * dvch.c - module that contains common device cache functions
 *
 * The caller may define the following:
 *
 *	DCACHE_CLONE	is the name of the function that reads and writes the
 *			clone section of the device cache file.  The clone
 *			section follows the device section.  If DCACHE_CLONE
 *			isn't defined, but HAS_STD_CLONE is defined to be 1,
 *			DCACHE_CLONE defaults to the local static function
 *			rw_clone_sect() that reads and writes a standard
 *			clone cache.
 *
 *	DCACHE_CLR	is the name of the function that clears the clone and
 *			pseudo caches when reading the device cache fails.   If
 *			DCACHE_CLR isn't defined, but HAS_STD_CLONE is defined
 *			to be 1, DCACHE_CLR defaults to the local static
 *			function clr_sect() that clears a standard clone cache.
 *
 *	DCACHE_PSEUDO	is the name of the function that reads and writes
 *			the pseudo section of the device cache file.  The
 *			pseudo section follows the device section and the
 *			clone section, if there is one.
 *
 *	DVCH_CHOWN	if the dialect has no fchown() function, so
 *			chown() must be used instead.
 *
 *	DVCH_DEVPATH	if the path to the device directory isn't "/dev".
 *
 *	DVCH_EXPDEV	if st_rdev must be expanded with the expdev()
 *			macro before use.  (This is an EP/IX artifact.)
 *
 *	HASBLKDEV	if block device information is stored in BDevtp[].
 */


/*
 * Local definitions
 */

# if	!defined(DVCH_DEVPATH)
#define	DVCH_DEVPATH	"/dev"
# endif	/* !defined(DVCH_DEVPATH) */

/*
 * Local storage
 */

static int crctbl[CRC_TBLL];		/* crc partial results table */


/*
 * Local function prototypes
 */

#undef	DCACHE_CLR_LOCAL
# if	!defined(DCACHE_CLR)
#  if	defined(HAS_STD_CLONE) && HAS_STD_CLONE==1
#define	DCACHE_CLR		clr_sect
#define	DCACHE_CLR_LOCAL	1
_PROTOTYPE(static void clr_sect,(void));
#  endif	/* defined(HAS_STD_CLONE) && HAS_STD_CLONE==1 */
# endif	/* !defined(DCACHE_CLR) */

#undef	DCACHE_CLONE_LOCAL
# if	!defined(DCACHE_CLONE)
#  if	defined(HAS_STD_CLONE) && HAS_STD_CLONE==1
#define	DCACHE_CLONE		rw_clone_sect
#define	DCACHE_CLONE_LOCAL	1
_PROTOTYPE(static int rw_clone_sect,(int m));
#  endif	/* defined(HAS_STD_CLONE) && HAS_STD_CLONE==1 */
# endif	/*!defined(DCACHE_CLONE) */


# if	defined(HASBLKDEV)
/*
 * alloc_bdcache() - allocate block device cache
 */

void
alloc_bdcache()
{
	if (!(BDevtp = (struct l_dev *)calloc((MALLOC_S)BNdev,
				       sizeof(struct l_dev))))
	{
	    (void) fprintf(stderr, "%s: no space for block devices\n", Pn);
	    Exit(1);
	}
	if (!(BSdev = (struct l_dev **)malloc((MALLOC_S)(sizeof(struct l_dev *)
				       * BNdev))))
	{
	    (void) fprintf(stderr, "%s: no space for block device pointers\n",
		Pn);
	    Exit(1);
	}
}
# endif	/* defined(HASBLKDEV) */


/*
 * alloc_dcache() - allocate device cache
 */

void
alloc_dcache()
{
	if (!(Devtp = (struct l_dev *)calloc((MALLOC_S)Ndev,
				      sizeof(struct l_dev))))
	{
	    (void) fprintf(stderr, "%s: no space for devices\n", Pn);
	    Exit(1);
	}
	if (!(Sdev = (struct l_dev **)malloc((MALLOC_S)(sizeof(struct l_dev *)
				      * Ndev))))
	{
	    (void) fprintf(stderr, "%s: no space for device pointers\n",
		Pn);
	    Exit(1);
	}
}


/*
 * clr_devtab() - clear the device tables and free their space
 */

void
clr_devtab()
{
	int i;

	if (Devtp) {
	    for (i = 0; i < Ndev; i++) {
		if (Devtp[i].name) {
		    (void) free((FREE_P *)Devtp[i].name);
		    Devtp[i].name = (char *)NULL;
		}
	    }
	    (void) free((FREE_P *)Devtp);
	    Devtp = (struct l_dev *)NULL;
	}
	if (Sdev) {
	    (void) free((FREE_P *)Sdev);
	    Sdev = (struct l_dev **)NULL;
	}
	Ndev = 0;

# if	defined(HASBLKDEV)
	if (BDevtp) {
	    for (i = 0; i < BNdev; i++) {
		if (BDevtp[i].name) {
		    (void) free((FREE_P *)BDevtp[i].name);
		    BDevtp[i].name = (char *)NULL;
		}
	    }
	    (void) free((FREE_P *)BDevtp);
	    BDevtp = (struct l_dev *)NULL;
	}
	if (BSdev) {
	    (void) free((FREE_P *)BSdev);
	    BSdev = (struct l_dev **)NULL;
	}
	BNdev = 0;
# endif	/* defined(HASBLKDEV) */

}


# if	defined(DCACHE_CLR_LOCAL)
/*
 * clr_sect() - clear cached standard clone sections
 */

static void
clr_sect()
{
	struct clone *c, *c1;

	if (Clone) {
	    for (c = Clone; c; c = c1) {
		c1 = c->next;
		(void) free((FREE_P *)c);
	    }
	    Clone = (struct clone *)NULL;
	}
}
# endif	/* defined(DCACHE_CLR_LOCAL) */


/*
 * crc(b, l, s) - compute a crc for a block of bytes
 */

void
crc(b, l, s)
	char *b;			/* block address */
	int  l;				/* length */
	unsigned *s;			/* sum */
{
	char *cp;			/* character pointer */
	char *lm;			/* character limit pointer */
	unsigned sum;			/* check sum */

	cp = b;
	lm = cp + l;
	sum = *s;
	do {
		sum ^= ((int) *cp++) & 0xff;
		sum = (sum >> 8) ^ crctbl[sum & 0xff];
	} while (cp < lm);
	*s = sum;
}


/*
 * crcbld - build the CRC-16 partial results table
 */

void
crcbld()
{
	int bit;			/* temporary bit value */
	unsigned entry;			/* entry under construction */
	int i;				/* polynomial table index */
	int j;				/* bit shift count */

	for(i = 0; i < CRC_TBLL; i++) {
		entry = i;
		for (j = 1; j <= CRC_BITS; j++) {
			bit = entry & 1;
			entry >>= 1;
			if (bit)
				entry ^= CRC_POLY;
		}
		crctbl[i] = entry;
	}
}


/*
 * dcpath() - define device cache file paths
 */

int
dcpath(rw, npw)
	int rw;				/* read (1) or write (2) mode */
	int npw;			/* inhibit (0) or enable (1) no
					 * path warning message */
{
	char buf[MAXPATHLEN+1], *cp1, *cp2, hn[MAXPATHLEN+1];
	int endf;
	int i, j;
	int l = 0;
	int ierr = 0;			/* intermediate error state */
	int merr = 0;			/* malloc error state */
	struct passwd *p = (struct passwd *)NULL;
	static short wenv = 1;		/* HASENVDC warning state */
	static short wpp = 1;		/* HASPERSDCPATH warning state */
/*
 * Release any space reserved by previous path calls to dcpath().
 */
	if (DCpath[1]) {
	    (void) free((FREE_P *)DCpath[1]);
	    DCpath[1] = (char *)NULL;
	}
	if (DCpath[3]) {
	    (void) free((FREE_P *)DCpath[3]);
	    DCpath[3] = (char *)NULL;
	}
/*
 * If a path was specified via -D, it's character address will have been
 * stored in DCpathArg by ctrl_dcache().  Use that address if the real UID
 * of this process is root, or the mode is read, or the process is neither
 * setuid-root nor setgid.
 */
	if (Myuid == 0 || rw == 1 || (!Setuidroot && !Setgid))
	    DCpath[0] = DCpathArg;
	else
	    DCpath[0] = (char *)NULL;

# if	defined(HASENVDC)
/*
 * If HASENVDC is defined, get its value from the environment, unless this
 * is a setuid-root process, or the real UID of the process is 0, or the
 * mode is write and the process is setgid.
 */
	if ((cp1 = getenv(HASENVDC)) && (l = strlen(cp1)) > 0
	&&  !Setuidroot && Myuid && (rw == 1 || !Setgid)) {
	    if (!(cp2 = mkstrcpy(cp1, (MALLOC_S *)NULL))) {
		(void) fprintf(stderr,
		    "%s: no space for device cache path: %s=", Pn, HASENVDC);
		safestrprt(cp1, stderr, 1);
		merr = 1;
	    } else
		DCpath[1] = cp2;
	} else if (cp1 && l > 0) {
	    if (!Fwarn && wenv) {
		(void) fprintf(stderr,
		    "%s: WARNING: ignoring environment: %s=", Pn, HASENVDC);
		safestrprt(cp1, stderr, 1);
	    }
	    wenv = 0;
	}
# endif	/* defined(HASENVDC) */

# if	defined(HASSYSDC)
/*
 * If HASSYSDC is defined, record the path of the system-wide device
 * cache file, unless the mode is write.
 */
	if (rw != 2)
	    DCpath[2] = HASSYSDC;
	else
	    DCpath[2] = (char *)NULL;
# endif	/* defined(HASSYSDC) */

# if	defined(HASPERSDC)
/*
 * If HASPERSDC is defined, form a personal device cache path by
 * interpreting the conversions specified in it.
 *
 * Get (HASPERSDCPATH) from the environment and add it to the home directory
 * path, if possible.
 */
	for (cp1 = HASPERSDC, endf = i = 0; *cp1 && !endf; cp1++) {
	    if (*cp1 != '%') {

	    /*
	     * If the format character isn't a `%', copy it.
	     */
		if (i < (int)sizeof(buf)) {
		    buf[i++] = *cp1;
		    continue;
		} else {
		    ierr = 2;
		    break;
		}
	     }
	/*
	 * `%' starts a conversion; the next character specifies
	 * the conversion type.
	 */
	    cp1++;
	    switch (*cp1) {

	    /*
	     * Two consecutive `%' characters convert to one `%'
	     * character in the output.
	     */

	    case '%':
		if (i < (int)sizeof(buf))
			buf[i++] = '%';
		else
			ierr = 2;
		break;

	    /*
	     * ``%0'' defines a root boundary.  If the effective
	     * (setuid-root) or real UID of the process is root, any
	     * path formed to this point is discarded and path formation
	     * begins with the next character.
	     *
	     * If neither the effective nor the real UID is root, path
	     * formation ends.
	     *
	     * This allows HASPERSDC to specify one path for non-root
	     * UIDs and another for the root (effective or real) UID.
	     */

	    case '0':
		if (Setuidroot || !Myuid)
		    i = 0;
		else
		    endf = 1;
		break;

	    /*
	     * ``%h'' converts to the home directory.
	     */
	
	    case 'h':
		if (!p && !(p = getpwuid(Myuid))) {
		    if (!Fwarn)
			(void) fprintf(stderr,
			    "%s: WARNING: can't get home dir for UID: %d\n",
			    Pn, (int)Myuid);
		    ierr = 1;
		    break;
		}
		if ((i + (l = strlen(p->pw_dir))) >= (int)sizeof(buf)) {
		    ierr = 2;
		    break;
		}
		(void) strcpy(&buf[i], p->pw_dir);
		i += l;
		if (i > 0 && buf[i - 1] == '/' && *(cp1 + 1)) {

		/*
		 * If the home directory ends in a '/' and the next format
		 * character is a '/', delete the '/' at the end of the home
		 * directory.
		 */
		    i--;
		    buf[i] = '\0';
		}
		break;

	    /*
	     * ``%l'' converts to the full host name.
	     *
	     * ``%L'' converts to the first component (characters up
	     * to the first `.') of the host name.
	     */

	    case 'l':
	    case 'L':
		if (gethostname(hn, sizeof(hn) - 1) < 0) {
		    if (!Fwarn)
			(void) fprintf(stderr,
			    "%s: WARNING: no gethostname for %%l or %%L: %s\n",
			    Pn, strerror(errno));
		    ierr = 1;
		    break;
		}
		hn[sizeof(hn) - 1] = '\0';
		if (*cp1 == 'L' && (cp2 = strchr(hn, '.')) && cp2 > hn)
		    *cp2 = '\0';
		j = strlen(hn);
		if ((j + i) < (int)sizeof(buf)) {
		    (void) strcpy(&buf[i], hn);
		    i += j;
		} else
		    ierr = 2;
		break;

	    /*
	     * ``%p'' converts to the contents of LSOFPERSDCPATH, followed
	     * by a '/'.
	     *
	     * It is ignored when:
	     *
	     *    The lsof process is setuid-root;
	     *    The real UID of the lsof process is 0;
	     *    The mode is write and the process is setgid.
	     */

	    case 'p':

#  if	defined(HASPERSDCPATH)
		if ((cp2 = getenv(HASPERSDCPATH))
		&&  (l = strlen(cp2)) > 0
		&&  !Setuidroot
		&&  Myuid
		&&  (rw == 1 || !Setgid))
		{
		    if (i && buf[i - 1] == '/' && *cp2 == '/') {
			cp2++;
			l--;
		    }
		    if ((i + l) < ((int)sizeof(buf) - 1)) {
			(void) strcpy(&buf[i], cp2);
			i += l;
			if (buf[i - 1] != '/') {
			    if (i < ((int)sizeof(buf) - 2)) {
				buf[i++] = '/';
				buf[i] = '\0';
			    } else
				ierr = 2;
			}
		    } else
			ierr = 2;
		} else {
		    if (cp2 && l > 0)  {
			if (!Fwarn && wpp) {
			    (void) fprintf(stderr,
				"%s: WARNING: ignoring environment: %s",
				Pn, HASPERSDCPATH);
			    safestrprt(cp2, stderr, 1);
			} 
			wpp = 0;
		    }
		}
#  else	/* !defined(HASPERSDCPATH) */
		if (!Fwarn && wpp)
		    (void) fprintf(stderr,
			"%s: WARNING: HASPERSDCPATH disabled: %s\n",
			Pn, HASPERSDC);
		ierr = 1;
		wpp = 0;
#  endif	/* defined(HASPERSDCPATH) */

		break;

	    /*
	     * ``%u'' converts to the login name of the real UID of the
	     * lsof process.
	     */

	    case 'u':
		if (!p && !(p = getpwuid(Myuid))) {
		    if (!Fwarn)
			(void) fprintf(stderr,
			    "%s: WARNING: can't get login name for UID: %d\n",
			    Pn, (int)Myuid);
		    ierr = 1;
		    break;
		}
		if ((i + (l = strlen(p->pw_name))) >= (int)sizeof(buf)) {
		    ierr = 2;
		    break;
		}
		(void) strcpy(&buf[i], p->pw_name);
		i += l;
		break;

	    /*
	     * ``%U'' converts to the real UID of the lsof process.
	     */

	    case 'U':
		(void) snpf(hn, sizeof(hn), "%d", (int)Myuid);
		if ((i + (l = strlen(hn))) >= (int)sizeof(buf))
		    ierr = 2;
		else {
		    (void) strcpy(&buf[i], hn);
		    i += l;
		}
		break;
	    default:
		if (!Fwarn)
		    (void) fprintf(stderr,
			"%s: WARNING: bad conversion (%%%c): %s\n",
			Pn, *cp1, HASPERSDC);
		ierr = 1;
	    }
	    if (endf || ierr > 1)
		break;
	}
	if (ierr) {

	/*
	 * If there was an intermediate error of some type, handle it.
	 * A type 1 intermediate error has already been noted with a
	 * warning message.  A type 2 intermediate error requires the
	 * issuing of a buffer overlow warning message.
	 */
	    if (ierr == 2 && !Fwarn)
		(void) fprintf(stderr,
	 	    "%s: WARNING: device cache path too large: %s\n",
		    Pn, HASPERSDC);
	    i = 0;
	}
	buf[i] = '\0';
/*
 * If there is one, allocate space for the personal device cache path,
 * copy buf[] to it, and store its pointer in DCpath[3].
 */
	if (i) {
	    if (!(cp1 = mkstrcpy(buf, (MALLOC_S *)NULL))) {
		(void) fprintf(stderr,
		    "%s: no space for device cache path: ", Pn);
		safestrprt(buf, stderr, 1);
		merr = 1;
	    } else
		DCpath[3] = cp1;
	}
# endif	/* defined(HASPERSDC) */

/*
 * Quit if there was a malloc() error.  The appropriate error message
 * will have been issued to stderr.
 */
	if (merr)
	    Exit(1);
/*
 * Return the index of the first defined path.  Since DCpath[] is arranged
 * in priority order, searching it beginning to end follows priority.
 * Return an error indication if the search discloses no path name.
 */
	for (i = 0; i < MAXDCPATH; i++) {
	    if (DCpath[i])
		return(i);
	}
	if (!Fwarn && npw)
	    (void) fprintf(stderr,
		"%s: WARNING: can't form any device cache path\n", Pn);
	return(-1);
}


/*
 * open_dcache() - open device cache file
 */

int
open_dcache(m, r, s)
	int m;			/* mode: 1 = read; 2 = write */
	int r;			/* create DCpath[] if 0, reuse if 1 */
	struct stat *s;		/* stat() receiver */
{
	char buf[128];
	char *w = (char *)NULL;
/*
 * Get the device cache file paths.
 */
	if (!r) {
	    if ((DCpathX = dcpath(m, 1)) < 0)
		return(1);
	}
/*
 * Switch to the requested open() action.
 */
	switch (m) {
	case 1:

	/*
	 * Check for access permission.
	 */
	    if (!is_readable(DCpath[DCpathX], 0)) {
		if (DCpathX == 2 && errno == ENOENT)
		    return(2);
		if (!Fwarn)
		    (void) fprintf(stderr, ACCESSERRFMT,
			Pn, DCpath[DCpathX], strerror(errno));
		return(1);
	    }
	/*
	 * Open for reading.
	 */
	    if ((DCfd = open(DCpath[DCpathX], O_RDONLY, 0)) < 0) {
		if (DCstate == 3 && errno == ENOENT)
		    return(1);

cant_open:
		    (void) fprintf(stderr,
			"%s: WARNING: can't open %s: %s\n",
			Pn, DCpath[DCpathX], strerror(errno));
		return(1);
	    }
	    if (stat(DCpath[DCpathX], s) != 0) {

cant_stat:
		if (!Fwarn)
		    (void) fprintf(stderr,
			"%s: WARNING: can't stat(%s): %s\n",
			Pn, DCpath[DCpathX], strerror(errno));
close_exit:
		(void) close(DCfd);
		DCfd = -1;
		return(1);
	    }
	    if ((int)(s->st_mode & 07777) != ((DCpathX == 2) ? 0644 : 0600)) {
		(void) snpf(buf, sizeof(buf), "doesn't have %04o modes",
	    	    (DCpathX == 2) ? 0644 : 0600);
		w = buf;
	    } else if ((s->st_mode & S_IFMT) != S_IFREG)
		w = "isn't a regular file";
	    else if (!s->st_size)
		w = "is empty";
	    if (w) {
		if (!Fwarn)
		    (void) fprintf(stderr,
			"%s: WARNING: %s %s.\n", Pn, DCpath[DCpathX], w);
		goto close_exit;
	    }
	    return(0);
	case 2:

	/*
	 * Open for writing: first unlink any previous version; then
	 * open exclusively, specifying it's an error if the file exists.
	 */
	    if (unlink(DCpath[DCpathX]) < 0) {
		if (errno != ENOENT) {
		    if (!Fwarn)
			(void) fprintf(stderr,
			    "%s: WARNING: can't unlink %s: %s\n",
			    Pn, DCpath[DCpathX], strerror(errno));
		    return(1);
		}
	    }
	    if ((DCfd = open(DCpath[DCpathX], O_RDWR|O_CREAT|O_EXCL, 0600)) < 0)
		goto cant_open;
	/*
	 * If the real user is not root, but the process is setuid-root,
	 * change the ownerships of the file to the real ones.
	 */
	    if (Myuid && Setuidroot) {

# if	defined(DVCH_CHOWN)
	 	if (chown(DCpath[DCpathX], Myuid, getgid()) < 0)
# else	/* !defined(DVCH_CHOWN) */
	 	if (fchown(DCfd, Myuid, getgid()) < 0)
# endif	/* defined(DVCH_CHOWN) */

		{
		    if (!Fwarn)
			(void) fprintf(stderr,
			     "%s: WARNING: can't change ownerships of %s: %s\n",
			     Pn, DCpath[DCpathX], strerror(errno));
	 	}
	    }
	    if (!Fwarn && DCstate != 1 && !DCunsafe)
		(void) fprintf(stderr,
		    "%s: WARNING: created device cache file: %s\n",
			Pn, DCpath[DCpathX]);
	    if (stat(DCpath[DCpathX], s) != 0) {
		(void) unlink(DCpath[DCpathX]);
		goto cant_stat;
	    }
	    return(0);
	default:

	/*
	 * Oops!
	 */
	    (void) fprintf(stderr, "%s: internal error: open_dcache=%d\n",
		Pn, m);
	    Exit(1);
	}
	return(1);
}


/*
 * read_dcache() - read device cache file
 */

int
read_dcache()
{
	char buf[MAXPATHLEN*2], cbuf[64], *cp;
	int i, len, ov;
	struct stat sb, devsb;
/*
 * Open the device cache file.
 *
 * If the open at HASSYSDC fails because the file doesn't exist, and
 * the real UID of this process is not zero, try to open a device cache
 * file at HASPERSDC.
 */
	if ((ov = open_dcache(1, 0, &sb)) != 0) {
	    if (DCpathX == 2) {
		if (ov == 2 && DCpath[3]) {
		    DCpathX = 3;
		    if (open_dcache(1, 1, &sb) != 0)
			return(1);
		} else
		    return(1);
	    } else
		return(1);
	}
/*
 * If the open device cache file's last mtime/ctime isn't greater than
 * DVCH_DEVPATH's mtime/ctime, ignore it, unless -Dr was specified.
 */
	if (stat(DVCH_DEVPATH, &devsb) != 0) {
	    if (!Fwarn)
		(void) fprintf(stderr,
		    "%s: WARNING: can't stat(%s): %s\n",
		    Pn, DVCH_DEVPATH, strerror(errno));
	} else {
	    if (sb.st_mtime <= devsb.st_mtime || sb.st_ctime <= devsb.st_ctime)
		DCunsafe = 1;
	}
	if (!(DCfs = fdopen(DCfd, "r"))) {
	    if (!Fwarn)
		(void) fprintf(stderr,
		    "%s: WARNING: can't fdopen(%s)\n", Pn, DCpath[DCpathX]);
	    (void) close(DCfd);
	    DCfd = -1;
	    return(1);
	}
/*
 * Read the section count line; initialize the CRC table;
 * validate the section count line.
 */
	if (!fgets(buf, sizeof(buf), DCfs)) {

cant_read:
	    if (!Fwarn)
		(void) fprintf(stderr,
		    "%s: WARNING: can't fread %s: %s\n", Pn, DCpath[DCpathX],
		    strerror(errno));
read_close:
		(void) fclose(DCfs);
		DCfd = -1;
		DCfs = (FILE *)NULL;
		(void) clr_devtab();

# if	defined(DCACHE_CLR)
		(void) DCACHE_CLR();
# endif	/* defined(DCACHE_CLR) */

		return(1);
	}
	(void) crcbld();
	DCcksum = 0;
	(void) crc(buf, strlen(buf), &DCcksum);
	i = 1;
	cp = "";

# if	defined(HASBLKDEV)
	i++;
	cp = "s";
# endif	/* defined(HASBLKDEV) */

# if	defined(DCACHE_CLONE)
	i++;
	cp = "s";
# endif	/* defined(DCACHE_CLONE) */

# if	defined(DCACHE_PSEUDO)
	i++;
	cp = "s";
# endif	/* defined(DCACHE_PSEUDO) */

	(void) snpf(cbuf, sizeof(cbuf), "%d section%s", i, cp);
	len = strlen(cbuf);
	(void) snpf(&cbuf[len], sizeof(cbuf) - len, ", dev=%lx\n",
		    (long)DevDev);
	if (!strncmp(buf, cbuf, len) && (buf[len] == '\n')) {
	    if (!Fwarn) {
		(void) fprintf(stderr,
		    "%s: WARNING: no /dev device in %s: line ", Pn,
		    DCpath[DCpathX]);
		safestrprt(buf, stderr, 1+4+8);
	    }
	    goto read_close;
	}
	if (strcmp(buf, cbuf)) {
	    if (!Fwarn) {
		(void) fprintf(stderr,
		    "%s: WARNING: bad section count line in %s: line ",
		    Pn, DCpath[DCpathX]);
		safestrprt(buf, stderr, 1+4+8);
	    }
	    goto read_close;
	}
/*
 * Read device section header and validate it.
 */
	if (!fgets(buf, sizeof(buf), DCfs))
	    goto cant_read;
	(void) crc(buf, strlen(buf), &DCcksum);
	len = strlen("device section: ");
	if (strncmp(buf, "device section: ", len) != 0) {

read_dhdr:
	    if (!Fwarn) {
		(void) fprintf(stderr,
		    "%s: WARNING: bad device section header in %s: line ",
		    Pn, DCpath[DCpathX]);
		safestrprt(buf, stderr, 1+4+8);
	    }
	    goto read_close;
	}
/*
 * Compute the device count; allocate Sdev[] and Devtp[] space.
 */
	if ((Ndev = atoi(&buf[len])) < 1)
	    goto read_dhdr;
	alloc_dcache();
/*
 * Read the device lines and store their information in Devtp[].
 * Construct the Sdev[] pointers to Devtp[].
 */
	for (i = 0; i < Ndev; i++) {
	    if (!fgets(buf, sizeof(buf), DCfs)) {
		if (!Fwarn)
		    (void) fprintf(stderr,
			"%s: WARNING: can't read device %d from %s\n",
			Pn, i + 1, DCpath[DCpathX]);
		goto read_close;
	    }
	    (void) crc(buf, strlen(buf), &DCcksum);
	/*
	 * Convert hexadecimal device number.
	 */
	    if (!(cp = x2dev(buf, &Devtp[i].rdev)) || *cp != ' ') {
		if (!Fwarn) {
		    (void) fprintf(stderr,
			"%s: device %d: bad device in %s: line ",
			Pn, i + 1, DCpath[DCpathX]);
		    safestrprt(buf, stderr, 1+4+8);
		}
		goto read_close;
	    }
	/*
	 * Convert inode number.
	 */
	    for (cp++, Devtp[i].inode = (INODETYPE)0; *cp != ' '; cp++) {
		if (*cp < '0' || *cp > '9') {
		    if (!Fwarn) {
			(void) fprintf(stderr,
			    "%s: WARNING: device %d: bad inode # in %s: line ",
			    Pn, i + 1, DCpath[DCpathX]);
			safestrprt(buf, stderr, 1+4+8);
		    }
		    goto read_close;
		}
		Devtp[i].inode = (INODETYPE)((Devtp[i].inode * 10)
			       + (int)(*cp - '0'));
	    }
	/*
	 * Get path name; allocate space for it; copy it; store the
	 * pointer in Devtp[]; clear verify status; construct the Sdev[]
	 * pointer to Devtp[].
	 */
	    if ((len = strlen(++cp)) < 2 || *(cp + len - 1) != '\n') {
		if (!Fwarn) {
		    (void) fprintf(stderr,
			"%s: WARNING: device %d: bad path in %s: line ",
			Pn, i + 1, DCpath[DCpathX]);
		    safestrprt(buf, stderr, 1+4+8);
		 }
		 goto read_close;
	    }
	    *(cp + len - 1) = '\0';
	    if (!(Devtp[i].name = mkstrcpy(cp, (MALLOC_S *)NULL))) {
		(void) fprintf(stderr,
		    "%s: device %d: no space for path: line ", Pn, i + 1);
		safestrprt(buf, stderr, 1+4+8);
		Exit(1);
	    }
	    Devtp[i].v = 0;
	    Sdev[i] = &Devtp[i];
	}

# if	defined(HASBLKDEV)
/*
 * Read block device section header and validate it.
 */
	if (!fgets(buf, sizeof(buf), DCfs))
	    goto cant_read;
	(void) crc(buf, strlen(buf), &DCcksum);
	len = strlen("block device section: ");
	if (strncmp(buf, "block device section: ", len) != 0) {
	    if (!Fwarn) {
		(void) fprintf(stderr,
		    "%s: WARNING: bad block device section header in %s: line ",
		    Pn, DCpath[DCpathX]);
		safestrprt(buf, stderr, 1+4+8);
	    }
	    goto read_close;
	}
/*
 * Compute the block device count; allocate BSdev[] and BDevtp[] space.
 */
	if ((BNdev = atoi(&buf[len])) > 0) {
	    alloc_bdcache();
	/*
	 * Read the block device lines and store their information in BDevtp[].
	 * Construct the BSdev[] pointers to BDevtp[].
	 */
	    for (i = 0; i < BNdev; i++) {
		if (!fgets(buf, sizeof(buf), DCfs)) {
		    if (!Fwarn)
			(void) fprintf(stderr,
			    "%s: WARNING: can't read block device %d from %s\n",
			    Pn, i + 1, DCpath[DCpathX]);
		    goto read_close;
		}
		(void) crc(buf, strlen(buf), &DCcksum);
	    /*
	     * Convert hexadecimal device number.
	     */
		if (!(cp = x2dev(buf, &BDevtp[i].rdev)) || *cp != ' ') {
		    if (!Fwarn) {
			(void) fprintf(stderr,
			    "%s: block dev %d: bad device in %s: line ",
			    Pn, i + 1, DCpath[DCpathX]);
			safestrprt(buf, stderr, 1+4+8);
		    }
		    goto read_close;
		}
	    /*
	     * Convert inode number.
	     */
		for (cp++, BDevtp[i].inode = (INODETYPE)0; *cp != ' '; cp++) {
		    if (*cp < '0' || *cp > '9') {
		      if (!Fwarn) {
			(void) fprintf(stderr,
			  "%s: WARNING: block dev %d: bad inode # in %s: line ",
			  Pn, i + 1, DCpath[DCpathX]);
			safestrprt(buf, stderr, 1+4+8);
		      }
		      goto read_close;
		    }
		    BDevtp[i].inode = (INODETYPE)((BDevtp[i].inode * 10)
				    + (int)(*cp - '0'));
		}
	    /*
	     * Get path name; allocate space for it; copy it; store the
	     * pointer in BDevtp[]; clear verify status; construct the BSdev[]
	     * pointer to BDevtp[].
	     */
		if ((len = strlen(++cp)) < 2 || *(cp + len - 1) != '\n') {
		    if (!Fwarn) {
			(void) fprintf(stderr,
			    "%s: WARNING: block dev %d: bad path in %s: line",
			    Pn, i + 1, DCpath[DCpathX]);
			safestrprt(buf, stderr, 1+4+8);
		    }
		    goto read_close;
		}
	        *(cp + len - 1) = '\0';
		if (!(BDevtp[i].name = mkstrcpy(cp, (MALLOC_S *)NULL))) {
		    (void) fprintf(stderr,
			"%s: block dev %d: no space for path: line", Pn, i + 1);
		    safestrprt(buf, stderr, 1+4+8);
		    Exit(1);
		}
	        BDevtp[i].v = 0;
	        BSdev[i] = &BDevtp[i];
	    }
	}
# endif	/* defined(HASBLKDEV) */

# if	defined(DCACHE_CLONE)
/*
 * Read the clone section.
 */
	if (DCACHE_CLONE(1))
	    goto read_close;
# endif	/* defined(DCACHE_CLONE) */

# if	defined(DCACHE_PSEUDO)
/*
 * Read the pseudo section.
 */
	if (DCACHE_PSEUDO(1))
	    goto read_close;
# endif	/* defined(DCACHE_PSEUDO) */

/*
 * Read and check the CRC section; it must be the last thing in the file.
 */
	(void) snpf(cbuf, sizeof(cbuf), "CRC section: %x\n", DCcksum);
	if (!fgets(buf, sizeof(buf), DCfs) || strcmp(buf, cbuf) != 0) {
	    if (!Fwarn) {
		(void) fprintf(stderr,
		    "%s: WARNING: bad CRC section in %s: line ",
		    Pn, DCpath[DCpathX]);
		safestrprt(buf, stderr, 1+4+8);
	    }
	    goto read_close;
	}
	if (fgets(buf, sizeof(buf), DCfs)) {
	    if (!Fwarn) {
		(void) fprintf(stderr,
		    "%s: WARNING: data follows CRC section in %s: line ",
		    Pn, DCpath[DCpathX]);
		safestrprt(buf, stderr, 1+4+8);
	    }
	    goto read_close;
	}
/*
 * Check one device entry at random -- the randomness based on our
 * PID.
 */
	i = (int)(Mypid % Ndev);
	if (stat(Devtp[i].name, &sb) != 0

# if	defined(DVCH_EXPDEV)
	||  expdev(sb.st_rdev) != Devtp[i].rdev
# else	/* !defined(DVCH_EXPDEV) */
	||  sb.st_rdev != Devtp[i].rdev
# endif	/* defined(DVCH_EXPDEV) */

	|| sb.st_ino != Devtp[i].inode) {
	    if (!Fwarn)
		(void) fprintf(stderr,
			"%s: WARNING: device cache mismatch: %s\n",
			Pn, Devtp[i].name);
	    goto read_close;
	}
/*
 * Close the device cache file and return OK.
 */
	(void) fclose(DCfs);
	DCfd = -1;
	DCfs = (FILE *)NULL;
	return(0);
}


# if	defined(DCACHE_CLONE_LOCAL)
/*
 * rw_clone_sect() - read/write the device cache file clone section
 */

static int
rw_clone_sect(m)
	int m;				/* mode: 1 = read; 2 = write */
{
	char buf[MAXPATHLEN*2], *cp, *cp1;
	struct clone *c;
	struct l_dev *dp;
	int i, j, len, n;

	if (m == 1) {

	/*
	 * Read the clone section header and validate it.
	 */
	    if (!fgets(buf, sizeof(buf), DCfs)) {

bad_clone_sect:
		if (!Fwarn) {
		    (void) fprintf(stderr,
			"%s: bad clone section header in %s: line ",
			Pn, DCpath[DCpathX]);
		    safestrprt(buf, stderr, 1+4+8);
		}
		return(1);
	    }
	    (void) crc(buf, strlen(buf), &DCcksum);
	    len = strlen("clone section: ");
	    if (strncmp(buf, "clone section: ", len) != 0)
		goto bad_clone_sect;
	    if ((n = atoi(&buf[len])) < 0)
		goto bad_clone_sect;
	/*
	 * Read the clone section lines and create the Clone list.
	 */
	    for (i = 0; i < n; i++) {
		if (fgets(buf, sizeof(buf), DCfs) == NULL) {
		    if (!Fwarn) {
			(void) fprintf(stderr,
			    "%s: no %d clone line in %s: line ", Pn, i + 1,
			    DCpath[DCpathX]);
			safestrprt(buf, stderr, 1+4+8);
		    }
		    return(1);
		}
		(void) crc(buf, strlen(buf), &DCcksum);
	    /*
	     * Assemble Devtp[] index and make sure it's correct.
	     */
		for (cp = buf, j = 0; *cp != ' '; cp++) {
		    if (*cp < '0' || *cp > '9') {

bad_clone_index:
			if (!Fwarn) {
			    (void) fprintf(stderr,
				"%s: clone %d: bad cached device index: line ",
				Pn, i + 1);
			    safestrprt(buf, stderr, 1+4+8);
			}
			return(1);
		    }
		    j = (j * 10) + (int)(*cp - '0');
		}
		if (j < 0 || j >= Ndev || (cp1 = strchr(++cp, '\n')) == NULL)
		    goto bad_clone_index;
		if (strncmp(cp, Devtp[j].name, (cp1 - cp)) != 0)
		    goto bad_clone_index;
	    /*
	     * Allocate and complete a clone structure.
	     */
		if (!(c = (struct clone *)malloc(sizeof(struct clone)))) {
		    (void) fprintf(stderr,
			"%s: clone %d: no space for cached clone: line ", Pn,
			i + 1);
		    safestrprt(buf, stderr, 1+4+8);
		    Exit(1);
		}
		c->dx = j;
		c->next = Clone;
		Clone = c;
	    }
	    return(0);
	} else if (m == 2) {

	/*
	 * Write the clone section header.
	 */
	    for (c = Clone, n = 0; c; c = c->next, n++)
		;
	    (void) snpf(buf, sizeof(buf), "clone section: %d\n", n);
	    if (wr2DCfd(buf, &DCcksum))
		return(1);
	/*
	 * Write the clone section lines.
	 */
	    for (c = Clone; c; c = c->next) {
		for (dp = &Devtp[c->dx], j = 0; j < Ndev; j++) {
		    if (dp == Sdev[j])
			break;
		}
		if (j >= Ndev) {
		    if (!Fwarn) {
			(void) fprintf(stderr,
			    "%s: can't make index for clone: ", Pn);
			safestrprt(dp->name, stderr, 1);
		    }
		    (void) unlink(DCpath[DCpathX]);
		    (void) close(DCfd);
		    DCfd = -1;
		    return(1);
		}
		(void) snpf(buf, sizeof(buf), "%d %s\n", j, dp->name);
		if (wr2DCfd(buf, &DCcksum))
		    return(1);
	    }
	    return(0);
	}
/*
 * A shouldn't-happen case: mode neither 1 nor 2.
 */
	(void) fprintf(stderr, "%s: internal rw_clone_sect error: %d\n",
	    Pn, m);
	Exit(1);
	return(1);		/* This useless return(1) keeps some
				 * compilers happy. */
}
# endif	/* defined(DCACHE_CLONE_LOCAL) */


/*
 * write_dcache() - write device cache file
 */

void
write_dcache()
{
	char buf[MAXPATHLEN*2], *cp;
	struct l_dev *dp;
	int i;
	struct stat sb;
/*
 * Open the cache file; set up the CRC table; write the section count.
 */
	if (open_dcache(2, 0, &sb))
    		return;
	i = 1;
	cp = "";

# if	defined(HASBLKDEV)
	i++;
	cp = "s";
# endif	/* defined(HASBLKDEV) */

# if	defined(DCACHE_CLONE)
	i++;
	cp = "s";
# endif	/* defined(DCACHE_CLONE) */

# if	defined(DCACHE_PSEUDO)
	i++;
	cp = "s";
# endif	/* defined(DCACHE_PSEUDO) */

	(void) snpf(buf, sizeof(buf), "%d section%s, dev=%lx\n", i, cp,
	    (long)DevDev);
	(void) crcbld();
	DCcksum = 0;
	if (wr2DCfd(buf, &DCcksum))
		return;
/*
 * Write the device section from the contents of Sdev[] and Devtp[].
 */
	(void) snpf(buf, sizeof(buf), "device section: %d\n", Ndev);
	if (wr2DCfd(buf, &DCcksum))
	    return;
	for (i = 0; i < Ndev; i++) {
	    dp = Sdev[i];
	    (void) snpf(buf, sizeof(buf), "%lx %ld %s\n", (long)dp->rdev,
			(long)dp->inode, dp->name);
	    if (wr2DCfd(buf, &DCcksum))
		return;
	}

# if	defined(HASBLKDEV)
/*
 * Write the block device section from the contents of BSdev[] and BDevtp[].
 */
	(void) snpf(buf, sizeof(buf), "block device section: %d\n", BNdev);
	if (wr2DCfd(buf, &DCcksum))
	    return;
	if (BNdev) {
	    for (i = 0; i < BNdev; i++) {
		dp = BSdev[i];
		(void) snpf(buf, sizeof(buf), "%lx %ld %s\n", (long)dp->rdev,
		    (long)dp->inode, dp->name);
		if (wr2DCfd(buf, &DCcksum))
		    return;
	    }
	}
# endif	/* defined(HASBLKDEV) */

# if	defined(DCACHE_CLONE)
/*
 * Write the clone section.
 */
	if (DCACHE_CLONE(2))
	    return;
# endif	/* defined(DCACHE_CLONE) */

# if	defined(DCACHE_PSEUDO)
/*
 * Write the pseudo section.
 */
	if (DCACHE_PSEUDO(2))
	    return;
# endif	/* defined(DCACHE_PSEUDO) */

/*
 * Write the CRC section and close the file.
 */
	(void) snpf(buf, sizeof(buf), "CRC section: %x\n", DCcksum);
	if (wr2DCfd(buf, (unsigned *)NULL))
		return;
	if (close(DCfd) != 0) {
	    if (!Fwarn)
		(void) fprintf(stderr,
		    "%s: WARNING: can't close %s: %s\n",
		    Pn, DCpath[DCpathX], strerror(errno));
	    (void) unlink(DCpath[DCpathX]);
	    DCfd = -1;
	}
	DCfd = -1;
/*
 * If the previous reading of the previous device cache file marked it
 * "unsafe," drop that marking and record that the device cache file was
 * rebuilt.
 */
	if (DCunsafe) {
	    DCunsafe = 0;
	    DCrebuilt = 1;
	}
}


/*
 * wr2DCfd() - write to the DCfd file descriptor
 */

int
wr2DCfd(b, c)
	char *b;			/* buffer */
	unsigned *c;			/* checksum receiver */
{
	int bl, bw;

	bl = strlen(b);
	if (c)
	    (void) crc(b, bl, c);
	while (bl > 0) {
	    if ((bw = write(DCfd, b, bl)) < 0) {
		if (!Fwarn)
		    (void) fprintf(stderr,
			"%s: WARNING: can't write to %s: %s\n",
			Pn, DCpath[DCpathX], strerror(errno));
		(void) unlink(DCpath[DCpathX]);
		(void) close(DCfd);
		DCfd = -1;
		return(1);
	    }
	    b += bw;
	    bl -= bw;
	}
	return(0);
}
#else	/* !defined(HASDCACHE) */
char dvch_d1[] = "d"; char *dvch_d2 = dvch_d1;
#endif	/* defined(HASDCACHE) */
