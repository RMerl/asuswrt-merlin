/*
 * LTbigf.c -- Lsof Test big file size and offset tests
 *
 * V. Abell
 * Purdue University
 */


/*
 * Copyright 2002 Purdue Research Foundation, West Lafayette, Indiana
 * 47907.  All rights reserved.
 *
 * Written by V. Abell.
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

#ifndef lint
static char copyright[] =
"@(#) Copyright 2002 Purdue Research Foundation.\nAll rights reserved.\n";
#endif

#include "LsofTest.h"

#if	!defined(LT_BIGF)

/*
 * Here begins the version of this program for dialects that don't support
 * large files.
 */


/*
 * Main program for dialects that don't support large files
 */

int
main(argc, argv)
	int argc;			/* argument count */
	char *argv[];			/* arguments */
{
    char *pn;			/* program name */
/*
 * Get program name and issue start and exit message.
 */
    if ((pn = (char *)strrchr(argv[0], '/')))
	    pn++;
    else
	    pn = argv[0];
	
    (void) printf("%s ... %s\n", pn, LT_DONT_DO_TEST);
    return(0);
}
#else	/* defined(LT_BIGF) */

/*
 * Here begins the version of this program for dialects that support
 * large files.
 */

#include "lsof_fields.h"


/*
 * Pre-definitions that may be changed by specific dialects
 */

#define	OFFTST_STAT	1		/* offset tests status */


#if	defined(LT_DIAL_aix)
/*
 * AIX-specific definitions
 */

#define	OFFSET_T	off64_t		/* define offset type */
#endif	/* defined(LT_DIAL_aix) */


#if	defined(LT_DIAL_bsdi)
/*
 * BSDI-specific definitions
 */

#define	OFFSET_T	off_t		/* define offset type */
#define	OPENF		open		/* define open function */
#define	SEEKF		lseek		/* define seek function */
#define	STATF		stat		/* define stat function */
#define	STATS		struct stat	/* define stat structure */
#endif	/* defined(LT_DIAL_bsdi) */


#if	defined(LT_DIAL_darwin)
/*
 * Darwin-specific definitions
 */

# if	LT_VERS>=900
#define	OFFSET_T	off_t		/* define offset type */
#define	OPENF		open		/* define open function */
#define	SEEKF		lseek		/* define seek function */
#define	STATF		stat		/* define stat function */
#define	STATS		struct stat	/* define stat structure */
# endif	/* LT_VERS>=900 */
#endif	/* defined(LT_DIAL_darwin) */


#if	defined(LT_DIAL_du)
/*
 * DEC_OSF/1|Digital_UNIX|Tru64_UNIX-specific items
 */

#define	OFFSET_T	off_t		/* define offset type */
#define	OPENF		open		/* define open function */
#define	SEEKF		lseek		/* define seek function */
#define	STATF		stat		/* define stat function */
#define	STATS		struct stat	/* define stat structure */
#endif	/* defined(LT_DIAL_du) */


#if	defined(LT_DIAL_freebsd)
/*
 * FreeBSD-specific definitions
 */

#define	OFFSET_T	off_t		/* define offset type */
#define	OPENF		open		/* define open function */
#define	SEEKF		lseek		/* define seek function */
#define	STATF		stat		/* define stat function */
#define	STATS		struct stat	/* define stat structure */
#endif	/* defined(LT_DIAL_freebsd) */


#if	defined(LT_DIAL_linux)
/*
 * Linux-specific definitions
 */

#undef	OFFTST_STAT
#define	OFFTST_STAT	0		/* Linux lsof may not be able to report
					 * offsets -- see the function
					 * ck_Linux_offset_support() */
#define	OFFSET_T	off_t		/* define offset type */
#define	OPENF		open		/* define open function */
#define	SEEKF		lseek		/* define seek function */
#define	STATF		stat		/* define stat function */
#define	STATS		struct stat	/* define stat structure */

_PROTOTYPE(static int ck_Linux_offset_support,(void));
#endif	/* defined(LT_DIAL_linux) */


#if	defined(LT_DIAL_hpux)
/*
 * HP-UX-specific definitions
 */

#define	OFFSET_T	off64_t		/* define offset type */
#endif	/* defined(LT_DIAL_hpux) */


#if	defined(LT_DIAL_netbsd)
/*
 * NetBSD-specific definitions
 */

#define	OFFSET_T	off_t		/* define offset type */
#define	OPENF		open		/* define open function */
#define	SEEKF		lseek		/* define seek function */
#define	STATF		stat		/* define stat function */
#define	STATS		struct stat	/* define stat structure */
#endif	/* defined(LT_DIAL_netbsd) */


#if	defined(LT_DIAL_openbsd)
/*
 * OpenBSD-specific definitions
 */

#define	OFFSET_T	off_t		/* define offset type */
#define	OPENF		open		/* define open function */
#define	SEEKF		lseek		/* define seek function */
#define	STATF		stat		/* define stat function */
#define	STATS		struct stat	/* define stat structure */
#endif	/* defined(LT_DIAL_openbsd) */


#if	defined(LT_DIAL_ou)
/*
 * OpenUNIX-specific items
 */

#include <signal.h>

#define	IGNORE_SIGXFSZ
#define	OFFSET_T	off64_t		/* define offset type */
#endif	/* defined(LT_DIAL_ou) */


#if	defined(LT_DIAL_solaris)
/*
 * Solaris-specific definitions
 */

#define	OFFSET_T	off64_t		/* define offset type */
#endif	/* defined(LT_DIAL_solaris) */


#if	defined(LT_DIAL_uw)
/*
 * UnixWare-specific items
 */

#include <signal.h>

#define	IGNORE_SIGXFSZ
#define	OFFSET_T	off64_t		/* define offset type */
#endif	/* defined(LT_DIAL_uw) */


/*
 * Local definitions
 */

#if	!defined(OPENF)
#define	OPENF		open64		/* open() function */
#endif	/* !defined(OPENF) */

#if	!defined(OFFSET_T)
#define	OFFSET_T unsigned long long	/* offset type */
#endif	/* !defined(OFFSET_T) */

#if	!defined(SEEKF)
#define	SEEKF		lseek64		/* seek() function */
# endif	/* !defined(SEEKF) */

#if	!defined(STATF)
#define	STATF		stat64		/* stat(2) structure */
#endif	/* !defined(STATF) */

#if	!defined(STATS)
#define	STATS		struct stat64	/* stat(2) structure */
#endif	/* !defined(STATS) */

#define	TST_OFFT	0		/* test offset in 0t decimal*/
#define	TST_OFFX	1		/* test offset in hex */
#define	TST_SZ		2		/* test size */


/*
 * Globals
 */

int Fd = -1;			/* test file descriptor; open if >= 0 */
pid_t MyPid = (pid_t)0;		/* PID of this process */
char *Path = (char *)NULL;	/* test file path; none if NULL */
char *Pn = (char *)NULL;	/* program name */


/*
 * Local function prototypes
 */

_PROTOTYPE(static void cleanup,(void));
_PROTOTYPE(static int tstwlsof,(int tt, char *opt, OFFSET_T sz));


/*
 * Main program for dialects that support large files
 */

int
main(argc, argv)
    int argc;				/* argument count */
    char *argv[];			/* arguments */
{
    char buf[2048];			/* temporary buffer */
    int do_offt = OFFTST_STAT;		/* do offset tests if == 1 */
    char *em;				/* error message pointer */
    int i;				/* temporary integer */
    int len;				/* string length */
    OFFSET_T sz = 0x140000000ll;	/* test file size */
    char szbuf[64];			/* size buffer */
    char *tcp;				/* temporary character pointer */
    int tofft = 0;			/* 0t offset test result */
    int toffx = 0;			/* 0x offset test result */
    int tsz = 0;			/* size test result */
    int xv = 0;				/* exit value */
/*
 * Get program name and PID, issue start message, and build space prefix.
 */
    if ((Pn = strrchr(argv[0], '/')))
	Pn++;
    else
	Pn = argv[0];
    MyPid = getpid();
    (void) printf("%s ... ", Pn);
    (void) fflush(stdout);
    PrtMsg((char *)NULL, Pn);
/*
 * Process arguments.
 */
    if (ScanArg(argc, argv, "hp:", Pn))
	xv = 1;
    if (xv || LTopt_h) {
	(void) PrtMsg("usage: [-h] [-p path]", Pn);
	PrtMsg       ("       -h       print help (this panel)", Pn);
	PrtMsgX      ("       -p path  define test file path", Pn, cleanup, xv);
    }

#if	defined(LT_DIAL_linux)
/*
 * If this is Linux, see if lsof can report file offsets.
 */
	do_offt = ck_Linux_offset_support();
#endif	/* defined(LT_DIAL_linux) */

/*
 * See if lsof can be executed and can access kernel memory.
 */
    if ((em = IsLsofExec()))
	(void) PrtMsgX(em, Pn, cleanup, 1);
    if ((em = CanRdKmem()))
	(void) PrtMsgX(em, Pn, cleanup, 1);
/*
 * Construct the path.  If LT_BIGSZOFF_PATH is defined in the environment,
 * use it. otherwise construct a path in the CWD. 
 */
    if (!(Path = LTopt_p)) {
	(void) snprintf(buf, sizeof(buf), "./config.LTbigf%ld",
	(long)MyPid);
	buf[sizeof(buf) - 1] = '\0';
	Path = MkStrCpy(buf, &len);
    }
/*
 * Fill buffer for writing to the test file.
 */
    for (i = 0; i < sizeof(buf); i++) {
	buf[i] = (char)(i & 0xff);
    }

#if	defined(IGNORE_SIGXFSZ)
/*
 * Ignore SIGXFSZ, if directed by a dialect-specific option.
 */
	(void) signal(SIGXFSZ, SIG_IGN);
#endif	/* defined(IGNORE_SIGXFSZ) */

/*
 * Open a new test file at the specified path.
 */
    (void) unlink(Path);
    if ((Fd = OPENF(Path, O_RDWR|O_CREAT, 0600)) < 0) {
	(void) fprintf(stderr, "ERROR!!!  can't open %s\n", Path);

print_hint:

    /*
     * Print a hint about the LT_BIGSZOFF_PATH environment variable.
     */

	MsgStat = 1;
	(void) snprintf(buf, sizeof(buf) - 1, "      Errno %d: %s",
	    errno, strerror(errno));
	buf[sizeof(buf) - 1] = '\0';
	(void) PrtMsg(buf, Pn);
	(void) PrtMsg("Hint: try using \"-p path\" to supply a path in a", Pn);
	(void) PrtMsg("file system that has large file support enabled.\n", Pn);
	(void) PrtMsg("Hint: try raising the process ulimit file block", Pn);
	(void) PrtMsg("size to a value that will permit this test to", Pn);
	(void) snprintf(szbuf, sizeof(szbuf) - 1, "%lld", (long long)sz);
	szbuf[sizeof(szbuf) - 1] = '\0';
	(void) snprintf(buf, sizeof(buf) - 1,
	    "write a file whose size appears to be %s", szbuf);
	buf[sizeof(buf) - 1] = '\0';
	(void) PrtMsg(buf, Pn);
	(void) PrtMsg("bytes.  (The file really isn't that big -- it", Pn);
	(void) PrtMsg("just has a large \"hole\" in its mid-section.)\n", Pn);
	(void) PrtMsgX("See 00FAQ and 00TEST for more information.", Pn,
		       cleanup, 1);
    }
/*
 * Write a buffer load at the beginning of the file.
 */
    if (SEEKF(Fd, (OFFSET_T)0, SEEK_SET) < 0) {
	(void) fprintf(stderr,
	    "ERROR!!!  can't seek to the beginning of %s\n", Path);
	goto print_hint;
    }
    if (write(Fd, buf, sizeof(buf)) != sizeof(buf)) {
	(void) fprintf(stderr,
	    "ERROR!!!  can't write %d bytes to the beginning of %s\n",
	    (int)sizeof(buf), Path);
	goto print_hint;
    }
/*
 * Write a buffer load near the end of the file to bring it to the
 * specified length.  Leave the file open so lsof can find it.
 */
    if (SEEKF(Fd, (OFFSET_T)(sz - sizeof(buf)), SEEK_SET) < 0) {
	(void) snprintf(szbuf, sizeof(szbuf) - 1, "%lld",
	    (unsigned long long)(sz - sizeof(buf)));
	(void) fprintf(stderr, "ERROR!!!  can't seek to %s in %s\n", szbuf,
	    Path);
	goto print_hint;
    }
    if (write(Fd, buf, sizeof(buf)) != sizeof(buf)) {
	(void) fprintf(stderr,
	    "ERROR!!!  can't write %d bytes near the end of %s\n",
	    (int)sizeof(buf), Path);
	goto print_hint;
    }
/*
 * Fsync() the file.
 */
    if (fsync(Fd)) {
	(void) fprintf(stderr, "ERROR!!!  can't fsync %s\n", Path);
	goto print_hint;
    }

/*
 * If this dialect can't report offsets, disable the offset tests.
 */
    if (!do_offt) {
	tofft = toffx = 1;
	PrtMsg("WARNING!!!  lsof can't return file offsets for this dialect,",
	    Pn);
	PrtMsg("  so offset tests have been disabled.", Pn);
    }
/*
 * Do file size test.
 */
    tsz = tstwlsof(TST_SZ, "-s", sz);
/*
 * If enabled, do offset tests.
 */
    if (!tofft)
	tofft = tstwlsof(TST_OFFT, "-oo20", sz);
    if (!toffx)
	toffx = tstwlsof(TST_OFFX, "-oo2", sz);
/*
 * Compute exit value and exit.
 */
    if ((tsz != 1) || (tofft != 1) || (toffx != 1)) {
	tcp = (char *)NULL;
	xv = 1;
    } else {
	tcp = "OK";
	xv = 0;
    }
    (void) PrtMsgX(tcp, Pn, cleanup, xv);
    return(0);
}


#if	defined(LT_DIAL_linux)
/*
 * ck_Linux_offset_support() -- see if lsof can report offsets for this
 *				Linux implementation
 */

static int
ck_Linux_offset_support()
{
	char buf[1024];			/* lsof output line buffer */
	int bufl = sizeof(buf);		/* size of buf[] */
	char *opv[5];			/* option vector for lsof */
	int rv = 1;			/* return value:
					 *     0 == no lsof offset support
					 *     1 == lsof offset support */
/*
 * Ask lsof to report the test's FD zero offset.
 */
	if (IsLsofExec())
	    return(0);
	opv[0] = "-o";
	snprintf(buf, bufl - 1, "-p%d", (int)getpid());
	opv[1] = buf;
	opv[2] = "-ad0";
	opv[3] = "+w";
	opv[4] = (char *)NULL;
	if (ExecLsof(opv))
	    return(0);
/*
 * Read the lsof output.  Look for a line with "WARNING: can't report offset"
 * in it.  If it is found, then this Linux lsof can't report offsets.
 */
	while(fgets(buf, bufl - 1, LsofFs)) {
	    if (strstr(buf, "WARNING: can't report offset")) {
		rv = 0;
		break;
	    }
	}
	(void) StopLsof();
	return(rv);
}
#endif	/* defined(LT_DIAL_linux) */


/*
 * cleanup() -- release resources
 */

static void
cleanup()
{
    if (Fd >= 0) {
/*
 * Close the test file.
 *
 * But first unlink it to discourage some kernel file system implementations
 * (e.g., HFS on Apple Darwin, aka Mac OS X) from trying to fill the file's
 * large holes.  (Filling can take a long time.)
 */
	if (Path) {
	    (void) unlink(Path);
	    Path = (char *)NULL;
	}
	(void) close(Fd);
	Fd = -1;
    }
}


/*
 * tstwlsof() -- test the open file with lsof
 */

static int
tstwlsof(tt, opt, sz)
    int tt;				/* test type -- i.e., TST_* */
    char *opt;				/* additional lsof options */
    OFFSET_T sz;			/* expected size (and offset) */
{
    char buf[2048], buf1[2048];		/* temporary buffers */
    LTfldo_t *cmdp;			/* command pointer */
    LTfldo_t *devp;			/* device pointer */
    char *em;				/* error message pointer */
    int ff = 0;				/* file found status */
    LTfldo_t *fop;			/* field output pointer */
    LTfldo_t *inop;			/* inode number pointer */
    LTdev_t lsofdc;			/* lsof device components */
    int nf;				/* number of fields */
    LTfldo_t *nmp;			/* file name pointer */
    LTfldo_t *offp;			/* file offset pointer */
    char *opv[4];			/* option vector for ExecLsof() */
    pid_t pid;				/* PID */
    int pids = 0;			/* PID found status */
    STATS sb;				/* stat(2) buffer */
    LTdev_t stdc;			/* stat(2) device components */
    LTfldo_t *szp;			/* file size pointer */
    LTfldo_t *tfop;			/* temporary field output pointer */
    int ti;				/* temporary index */
    LTfldo_t *typ;			/* file type pointer */
    int xv = 0;				/* exit value */
/*
 * Check the test type.
 */
    switch (tt) {
    case TST_OFFT:
    case TST_OFFX:
    case TST_SZ:
	break;
    default:
	(void) snprintf(buf, sizeof(buf) - 1,
	    "ERROR!!! unknown test type: %d", tt);
	buf[sizeof(buf) - 1] = '\0';
	(void) PrtMsgX(buf, Pn, cleanup, 1);
    }
/*
 * Get test file's information.
 */
    if (STATF(Path, &sb)) {
	(void) snprintf(buf, sizeof(buf) - 1,
	    "ERROR!!! can't stat(2) %s: %s", Path, strerror(errno));
	buf[sizeof(buf) - 1] = '\0';
	(void) PrtMsgX(buf, Pn, cleanup, 1);
    }
/*
 * Extract components from test file's device number.
 */
    if ((em = ConvStatDev(&sb.st_dev, &stdc))) {
	(void) PrtMsg(em, Pn);
	return(0);
    }
/*
 * Complete the option vector and start lsof execution.
 */
    ti = 0;
    if (opt && *opt)
	opv[ti++] = opt;

#if	defined(USE_LSOF_C_OPT)
    opv[ti++] = "-C";
#else	/* !defined(USE_LSOF_C_OPT) */
    opv[ti++] = "--";
#endif	/* defined(USE_LSOF_C_OPT) */

    opv[ti++] = Path;
    opv[ti] = (char *)NULL;
    if ((em = ExecLsof(opv))) {
	(void) PrtMsg(em, Pn);
	return(0);
    }
/*
 * Read lsof output.
 */
    while (!ff && (fop = RdFrLsof(&nf, &em))) {
	switch (fop->ft) {
	case LSOF_FID_PID:

	/*
	 * This is a process information line.
	 */
	    pid = (pid_t)atoi(fop->v);
	    pids = 1;
	    cmdp = (LTfldo_t *)NULL;
	    for (fop++, ti = 1; ti < nf; fop++, ti++) {
		switch (fop->ft) {
		case LSOF_FID_CMD:
		    cmdp = fop;
		    break;
		}
	    }
	    if (!cmdp || (pid != MyPid))
		pids = 0;
	    break;
	case LSOF_FID_FD:

	/*
	 * This is a file descriptor line.
	 *
	 * Scan for device number, inode number, name, offset, size, and type
	 * fields.
	 */
	    if (!pids)
		break;
	    devp = inop = nmp = offp = szp = typ = (LTfldo_t *)NULL;
	    for (fop++, ti = 1; ti < nf; fop++, ti++) {
		switch(fop->ft) {
		case LSOF_FID_DEVN:
		    devp = fop;
		    break;
		case LSOF_FID_INODE:
		    inop = fop;
		    break;
		case LSOF_FID_NAME:
		    nmp = fop;
		    break;
		case LSOF_FID_OFFSET:
		    offp = fop;
		    break;
		case LSOF_FID_SIZE:
		    szp = fop;
		    break;
		case LSOF_FID_TYPE:
		    typ = fop;
		    break;
		}
	    }
	/*
	 * Check the results of the file descriptor field scan.
	 *
	 * (Don't compare path names because of symbolic link interference.)
	 */
	    if (!devp || !inop || !nmp || !typ)
		break;
	    if (strcasecmp(typ->v, "reg") && strcasecmp(typ->v, "vreg"))
		break;
	    if (ConvLsofDev(devp->v, &lsofdc))
		break;
	    if ((stdc.maj != lsofdc.maj)
	    ||  (stdc.min != lsofdc.min)
	    ||  (stdc.unit != lsofdc.unit))
		break;
	    (void) snprintf(buf, sizeof(buf) - 1, "%llu",
		(unsigned long long)sb.st_ino);
	    buf[sizeof(buf) - 1] = '\0';
	    if (strcmp(inop->v, buf))
		break;
	/*
	 * The specifed file has been located.  Check its size or offset,
	 * according to the tt argument.
	 */
	    ff = 1;
	    switch (tt) {
	    case TST_OFFT:
	    case TST_SZ:

	    /*
	     * Test the size as an offset in decimal with a leading "0t", or
	     * test the size as a size in decimal.
	     */
		(void) snprintf(buf, sizeof(buf) - 1,
		    (tt == TST_SZ) ? "%llu" : "0t%llu",
		    (unsigned long long)sz);
		buf[sizeof(buf) - 1] = '\0';
		tfop = (tt == TST_SZ) ? szp : offp;
		if (!tfop || strcmp(tfop->v, buf)) {
		    (void) snprintf(buf1, sizeof(buf1) - 1,
			"%s mismatch: expected %s, got %s",
			(tt == TST_SZ) ? "size" : "0t offset",
			buf,
			tfop ? tfop->v : "nothing");
		    buf1[sizeof(buf1) - 1] = '\0';
		    (void) PrtMsg(buf1, Pn);
		    xv = 0;
		} else
		    xv = 1;
		break;
	    case TST_OFFX:

	    /*
	     * Test the size as an offset in hex.
	     */
		(void) snprintf(buf, sizeof(buf) - 1, "0x%llx",
		    (unsigned long long)sz);
		buf[sizeof(buf) - 1] = '\0';
		if (!offp || strcmp(offp->v, buf)) {
		    (void) snprintf(buf1, sizeof(buf1) - 1,
			"0x offset mismatch: expected %s, got %s",
			buf,
			offp ? offp->v : "nothing");
		    buf1[sizeof(buf1) - 1] = '\0';
		    (void) PrtMsg(buf1, Pn);
		    xv = 0;
		} else
		    xv = 1;
	    }
	    break;
	}
    }
    (void) StopLsof();
    if (em) {

    /*
     * RdFrLsof() encountered an error.
     */
	(void) PrtMsg(em, Pn);
	xv = 0;
    }
    if (!ff) {
	(void) snprintf(buf, sizeof(buf) - 1, "%s not found by lsof", Path);
	buf[sizeof(buf) - 1] = '\0';
	PrtMsg(buf, Pn);
	xv = 0;
    }
    return(xv);
}
#endif	/* defined(LT_BIG) */
