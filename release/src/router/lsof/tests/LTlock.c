/*
 * LTlock.c -- Lsof Test locking tests
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
#include "lsof_fields.h"


#if	defined(LT_DIAL_aix)
/*
 * AIX-specific items
 */

#define	USE_FCNTL
#endif	/* defined(LT_DIAL_aix) */


#if	defined(LT_DIAL_bsdi)
/*
 * BSDI-specific items
 */

#define	USE_FCNTL
#endif	/* defined(LT_DIAL_bsdi) */


#if	defined(LT_DIAL_darwin)
/*
 * Darwin-specific items
 */

/*
 * There is no Darwin USE_* definition, because lock support in lsof for
 * Darwin is inadequate for this test.
 */
#endif	/* defined(LT_DIAL_darwin) */


#if	defined(LT_DIAL_du)
/*
 * DEC_OSF/1|Digital_UNIX|Tru64_UNIX-specific items
 */

#define	USE_FCNTL
#endif	/* defined(LT_DIAL_du) */


#if	defined(LT_DIAL_freebsd)
/*
 * FreeBSD-specific items
 */

#define	USE_FCNTL
#endif	/* defined(LT_DIAL_freebsd) */


#if	defined(LT_DIAL_linux)
/*
 * Linux-specific items
 */

#define	USE_FCNTL
#endif	/* defined(LT_DIAL_linux) */


#if	defined(LT_DIAL_netbsd)
/*
 * NetBSD-specific items
 */

#define	USE_FCNTL
#endif	/* defined(LT_DIAL_netbsd) */


#if	defined(LT_DIAL_openbsd)
/*
 * OpenBSD-specific items
 */

#define	USE_FCNTL
#endif	/* defined(LT_DIAL_openbsd) */


#if	defined(LT_DIAL_hpux)
/*
 * HP-UX-specific items
 */

#define	USE_FCNTL
#endif	/* defined(LT_DIAL_hpux) */


#if	defined(LT_DIAL_ns)
/*
 * NEXTSTEP-specific items
 */

#define	USE_FLOCK
#endif	/* defined(LT_DIAL_ns) */


#if	defined(LT_DIAL_osr)
/*
 * OSR-specific items
 */

#define	USE_FCNTL
#endif	/* defined(LT_DIAL_osr) */


#if	defined(LT_DIAL_ou)
/*
 * OpenUNIX-specific items
 */

#define	USE_FCNTL
#endif	/* defined(LT_DIAL_ou) */


#if	defined(LT_DIAL_openbsd)
/*
 * OpenBSD-specific items
 */

#define	USE_FCNTL
#endif	/* defined(LT_DIAL_openbsd) */


#if	defined(LT_DIAL_solaris)
/*
 * Solaris-specific items
 */

#define	USE_FCNTL
#endif	/* defined(solaris) */


#if	defined(LT_DIAL_uw)
/*
 * UnixWare-specific items
 */

#define	USE_FCNTL
#endif	/* defined(LT_DIAL_uw) */


#if	!defined(USE_FLOCK) && !defined(USE_FCNTL)
/*
 * Here begins the version of this program for dialects that don't support
 * flock() or fcntl() locking.
 */


/*
 * Main program for dialects that don't support flock() of fcntl() locking.
 */

int
main(argc, argv)
	int argc;			/* argument count */
	char *argv[];			/* arguments */
{
    char *pn;			/* program name */
/*
 * Get program name and issue error message.
 */
    if ((pn = (char *)strrchr(argv[0], '/')))
	pn++;
    else
	pn = argv[0];
    (void) printf("%s ... %s\n", pn, LT_DONT_DO_TEST);
    return(0);
}
#else	/* defined(USE_FLOCK) || defined(USE_FCNTL) */


/*
 * Local definitions
 */

#define	FULL_EX_LOCK	0	/* get a full file exclusive lock */
#define	FULL_SH_LOCK	1	/* get a full file shared lock */
#define	PART_EX_LOCK	2	/* get a partial file exclusive lock */
#define	PART_SH_LOCK	3	/* get a partial file shared lock */


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
_PROTOTYPE(static char *lkfile,(int ty));
_PROTOTYPE(static char *tstwlsof,(char *opt, char *xlk));
_PROTOTYPE(static char *unlkfile,(int ty));


/*
 * Main program for dialects that support locking tests.
 */

int
main(argc, argv)
    int argc;				/* argument count */
    char *argv[];			/* arguments */
{
    char buf[2048];			/* temporary buffer */
    char *em;				/* error message pointer */
    int ti;				/* temporary index */
    char *tcp;				/* temporary character pointer */
    int tlen;				/* temporary length -- e.g., as
					 * returned by MkStrCpy() */
    char *tstR = (char *)NULL;		/* "R" lock test result */
    char *tstr = (char *)NULL;		/* "r" lock test result */
    char *tstW = (char *)NULL;		/* "W" lock test result */
    char *tstw = (char *)NULL;		/* "w" lock test result */
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
    (void) PrtMsg((char *)NULL, Pn);
/*
 * Process arguments.
 */
    if (ScanArg(argc, argv, "hp:", Pn))
	xv = 1;
    if (xv || LTopt_h) {
	(void) PrtMsg ("usage: [-h] [-p path]", Pn);
	(void) PrtMsg ("       -h       print help (this panel)", Pn);
	(void) PrtMsgX("       -p path  define test file path", Pn, cleanup,
		       xv);
    }
/*
 * See if lsof can be executed and can access kernel memory.
 */
    if ((em = IsLsofExec()))
	(void) PrtMsgX(em, Pn, cleanup, 1);
    if ((em = CanRdKmem()))
	(void) PrtMsgX(em, Pn, cleanup, 1);
/*
 * If a path was supplied in an "-p path" option, use it.  Otherwise construct
 * a path in the CWD.
 */
    if (!(Path = LTopt_p)) {
	(void) snprintf(buf, sizeof(buf), "./config.LTlock%ld",
	(long)MyPid);
	buf[sizeof(buf) - 1] = '\0';
	Path = MkStrCpy(buf, &tlen);
    }
/*
 * Fill buffer for writing to the test file.
 */
    for (ti = 0; ti < sizeof(buf); ti++) {
	buf[ti] = (char)(ti & 0xff);
    }
/*
 * Open a new test file at the specified path.
 */
    (void) unlink(Path);
    if ((Fd = open(Path, O_RDWR|O_CREAT, 0600)) < 0) {
	(void) fprintf(stderr, "ERROR!!!  can't open %s\n", Path);

print_file_error:

	MsgStat = 1;
	(void) snprintf(buf, sizeof(buf) - 1, "      Errno %d: %s",
	    errno, strerror(errno));
	buf[sizeof(buf) - 1] = '\0';
	(void) PrtMsgX(buf, Pn, cleanup, 1);
    }
/*
 * Write a buffer load at the beginning of the file.
 */
    if (write(Fd, buf, sizeof(buf)) != sizeof(buf)) {
	(void) fprintf(stderr,
	    "ERROR!!!  can't write %d bytes to the beginning of %s\n",
	    (int)sizeof(buf), Path);
	goto print_file_error;
    }
/*
 * Fsync() the file.
 */
    if (fsync(Fd)) {
	(void) fprintf(stderr, "ERROR!!!  can't fsync %s\n", Path);
	goto print_file_error;
    }
/*
 * Quit (with a hint) if the test file is on an NFS file system.
 */
    if (!tstwlsof("-wNa", " ")) {
	(void) printf("ERROR!!!  %s is NFS-mounted.\n", Path);
	MsgStat = 1;
	(void) PrtMsg ("Lsof can't report lock information on files that", Pn);
	(void) PrtMsg ("are located on file systems mounted from a remote", Pn);
	(void) PrtMsg ("NFS server.\n", Pn);
        (void) PrtMsg ("Hint: try using \"-p path\" to supply a path in a", Pn);
        (void) PrtMsg ("non-NFS file system.\n", Pn);
	(void) PrtMsgX("See 00FAQ and 00TEST for more information.", Pn,
		       cleanup, 1);
    }
/*
 * Get an exclusive lock on the entire file and test it with lsof.
 */
    if ((em = lkfile(FULL_EX_LOCK)))
	(void) PrtMsgX(em, Pn, cleanup, 1);
    if ((tstW = tstwlsof("-w", "W")))
	(void) PrtMsg(tstW, Pn);
/*
 * Get a shared lock on the entire file and test it with lsof.
 */
    if ((em = unlkfile(FULL_EX_LOCK)))
	(void) PrtMsgX(em, Pn, cleanup, 1);
    if ((em = lkfile(FULL_SH_LOCK)))
	(void) PrtMsgX(em, Pn, cleanup, 1);
    if ((tstR = tstwlsof("-w", "R")))
	(void) PrtMsg(tstR, Pn);

# if	defined(USE_FLOCK)
/*
 * If using flock(), skip the byte lock tests.
 */
    tstr = tstw = (char *)NULL;
# endif	/* defined(USE_FLOCK) */

# if	defined(USE_FCNTL)
/*
 * If using fcntl(), do exclusive and shared byte lock tests,
 */
    if ((em = unlkfile(FULL_SH_LOCK)))
	(void) PrtMsgX(em, Pn, cleanup, 1);
    if ((em = lkfile(PART_EX_LOCK)))
	(void) PrtMsgX(em, Pn, cleanup, 1);
    if ((tstw = tstwlsof("-w", "w")))
	(void) PrtMsg(tstw, Pn);
    if ((em = unlkfile(PART_EX_LOCK)))
	(void) PrtMsgX(em, Pn, cleanup, 1);
    if ((em = lkfile(PART_SH_LOCK)))
	(void) PrtMsgX(em, Pn, cleanup, 1);
    if ((tstr = tstwlsof("-w", "r")))
	(void) PrtMsg(tstr, Pn);
# endif	/* defined(USE_FCNTL) */

/*
 * Compute exit value and exit.
 */
    if (tstr || tstR || tstw || tstW) {
	tcp = (char *)NULL;
	xv = 1;
    } else {
	tcp = "OK";
	xv = 0;
    }
    (void) PrtMsgX(tcp, Pn, cleanup, xv);
    return(0);
}


/*
 * cleanup() -- release resources
 */

static void
cleanup()
{
    if (Fd >= 0) {
	(void) close(Fd);
	Fd = -1;
	if (Path) {
	    (void) unlink(Path);
	    Path = (char *)NULL;
	}
    }
}


/* 
 * lkfile() -- lock the test file
 */

static char *
lkfile(ty)
    int ty;				/* a *_*_LOCK requested */
{
    char buf[2048];			/* temporary buffer */
    int ti;				/* temporary integer */

# if	defined(USE_FLOCK)
    int flf;				/* flock() function */
# endif	/* defined(USE_FLOCK) */

# if	defined(USE_FCNTL)
    struct flock fl;			/* flock control structure */
/*
 * Check fcntl() lock request.
 */
    (void) memset((void *)&fl, 0, sizeof(fl));
    switch(ty) {
    case FULL_EX_LOCK:
	fl.l_type = F_WRLCK;
	break;
    case FULL_SH_LOCK:
	fl.l_type = F_RDLCK;
	break;
    case PART_EX_LOCK:
	fl.l_type = F_WRLCK;
	fl.l_len = (off_t)1;
	break;
    case PART_SH_LOCK:
	fl.l_type = F_RDLCK;
	fl.l_len = (off_t)1;
	break;
    default:
	(void) snprintf(buf, sizeof(buf) - 1,
	    "ERROR!!!  unknown lock type: %d", ty);
	buf[sizeof(buf) - 1] = '\0';
	return(MkStrCpy(buf, &ti));
    }
/*
 * Lock test file with fcntl().
 */
    if (fcntl(Fd, F_SETLK, &fl) != -1)
	return((char *)NULL);
    (void) snprintf(buf, sizeof(buf) - 1, "ERROR!!!  fcntl() lock error: %s",
	strerror(errno));
    buf[sizeof(buf) - 1] = '\0';
    return(MkStrCpy(buf, &ti));
# endif	/* defined(USE_FCNTL) */

# if	defined(USE_FLOCK)
/*
 * Check flock() lock request.
 */
    switch(ty) {
    case FULL_EX_LOCK:
	flf = LOCK_EX;
	break;
    case FULL_SH_LOCK:
	flf = LOCK_SH;
	break;
    case PART_EX_LOCK:
    case PART_SH_LOCK:
	return("ERROR!!!  flock() doesn't support partial locks");
	break;
    default:
	(void) snprintf(buf, sizeof(buf) - 1,
	    "ERROR!!!  unknown flock() type: %d", ty);
	buf[sizeof(buf) - 1] = '\0';
	return(MkStrCpy(buf, &ti));
    }
/*
 * Acquire lock.
 */
    if (!flock(Fd, flf))
	return((char *)NULL);
    (void) snprintf(buf, sizeof(buf) - 1,
	"ERROR!!!  flock() %s lock failed: %s",
	(flf == LOCK_EX) ? "exclusive" : "shared",
	strerror(errno));
    buf[sizeof(buf) - 1] = '\0';
    return(MkStrCpy(buf, &ti));
# endif	/* defined(USE_FLOCK) */

}


/*
 * tstwlsof() -- test the open file with lsof
 */

static char *
tstwlsof(opt, xlk)
    char *opt;				/* extra lsof options */
    char *xlk;				/* expected lock value */
{
    char buf[2048];			/* temporary buffer */
    LTfldo_t *cmdp;			/* command pointer */
    LTfldo_t *devp;			/* device pointer */
    char *cem;				/* current error message pointer */
    int ff = 0;				/* file found status */
    LTfldo_t *fop;			/* field output pointer */
    LTfldo_t *inop;			/* inode number pointer */
    LTfldo_t *lkp;			/* lock pointer */
    LTdev_t lsofdc;			/* lsof device components */
    int nf;				/* number of fields */
    LTfldo_t *nmp;			/* file name pointer */
    char *opv[4];			/* option vector for ExecLsof() */
    char *pem = (char *)NULL;		/* previous error message pointer */
    pid_t pid;				/* PID */
    int pids = 0;			/* PID found status */
    struct stat sb;			/* stat(2) buffer */
    LTdev_t stdc;			/* stat(2) device components */
    char *tcp;				/* temporary character pointer */
    int ti;				/* temporary integer */
    LTfldo_t *typ;			/* file type pointer */
/*
 * Make sure there is an expected lock value.
 */
    if (!xlk || !*xlk)
	(void) PrtMsgX("ERROR!!!  no expected lock value", Pn, cleanup, 1);
/*
 * Get test file's information.
 */
    if (stat(Path, &sb)) {
	(void) snprintf(buf, sizeof(buf) - 1,
	    "ERROR!!!  can't stat(2) %s: %s", Path, strerror(errno));
	buf[sizeof(buf) - 1] = '\0';
	(void) PrtMsgX(buf, Pn, cleanup, 1);
    }
/*
 * Extract components from test file's device number.
 */
    if ((cem = ConvStatDev(&sb.st_dev, &stdc)))
	(void) PrtMsgX(cem, Pn, cleanup, 1);
/*
 * Complete the option vector and start lsof execution.
 */
    ti = 0;
    if (opt && *opt)
	opv[ti++] = opt;

#if	defined(USE_LSOF_C_OPT)
    opv[ti++] = "-C";
#endif	/* defined(USE_LSOF_C_OPT) */

    opv[ti++] = Path;
    opv[ti] = (char *)NULL;
    if ((cem = ExecLsof(opv)))
	return(cem);
/*
 * Read lsof output.
 */
    while (!ff && (fop = RdFrLsof(&nf, &cem))) {
	if (cem) {
	    if (pem)
		(void) PrtMsg(pem, Pn);
	    return(cem);
	}
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
	 * This is a file descriptor line.  Make sure its number matches the
	 * test file's descriptor number.
	 *
	 * Scan for lock and name fields.
	 */
	    if (!pids)
		break;
	    for (ti = 0, tcp = fop->v; *tcp; tcp++) {

	    /*
	     * Convert file descriptor to a number.
	     */
		if (*tcp == ' ')
		    continue;
		if (((int)*tcp < (int)'0') || ((int)*tcp > (int)'9')) {
		    ti = -1;
		    break;
		}
		ti = (ti * 10) + (int)*tcp - (int)'0'; 
	    }
	    if (Fd != ti)
		break;
	    devp = inop = lkp = nmp = (LTfldo_t *)NULL;
	    for (fop++, ti = 1; ti < nf; fop++, ti++) {
		switch(fop->ft) {
		case LSOF_FID_DEVN:
		    devp = fop;
		    break;
		case LSOF_FID_INODE:
		    inop = fop;
		    break;
		case LSOF_FID_LOCK:
		    lkp = fop;
		    break;
		case LSOF_FID_NAME:
		    nmp = fop;
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
	    (void) snprintf(buf, sizeof(buf) - 1, "%u",
		(unsigned int)sb.st_ino);
	    buf[sizeof(buf) - 1] = '\0';
	    if (strcmp(inop->v, buf))
		break;
	/*
	 * The specified file has been located.  Check its lock status.
	 */
	    ff = 1;
	    if (!lkp || strcmp(lkp->v, xlk)) {
		if (pem)
		    (void) PrtMsg(pem, Pn);
		(void) snprintf(buf, sizeof(buf) - 1,
		    "lock mismatch: expected %s, got \"%s\"", xlk,
		    lkp ? lkp->v : "(none)");
		pem = MkStrCpy(buf, &ti);
	    }
	    break;
	}
    }
    (void) StopLsof();
    if (!ff) {
	if (pem)
	   (void) PrtMsg(pem, Pn);
	(void) snprintf(buf, sizeof(buf) - 1,
	    "lock test file %s not found by lsof", Path);
	buf[sizeof(buf) - 1] = '\0';
	return(MkStrCpy(buf, &ti));
    }
    return(pem);
}


/*
 * unlkfile() -- unlock the test file
 */

static char *
unlkfile(ty)
    int ty;				/* current *_*_LOCK lock typ */
{
    char buf[2048];			/* temporary buffer */
    int ti;				/* temporary integer */

# if	defined(USE_FCNTL)
    struct flock fl;			/* flock control structure */
/*
 * Check current fcntl() lock type.
 */
    (void) memset((void *)&fl, 0, sizeof(fl));
    switch(ty) {
    case FULL_EX_LOCK:
    case FULL_SH_LOCK:
	break;
    case PART_EX_LOCK:
    case PART_SH_LOCK:
	fl.l_len = (off_t)1;
	break;
    default:
	(void) snprintf(buf, sizeof(buf) - 1,
	    "ERROR!!!  unknown unlock type: %d", ty);
	buf[sizeof(buf) - 1] = '\0';
	return(MkStrCpy(buf, &ti));
    }
/*
 * Unlock test file with fcntl().
 */
    fl.l_type = F_UNLCK;
    if (fcntl(Fd, F_SETLK, &fl) != -1)
	return((char *)NULL);
    (void) snprintf(buf, sizeof(buf) - 1, "ERROR!!!  fcntl() unlock error: %s",
	strerror(errno));
    buf[sizeof(buf) - 1] = '\0';
    return(MkStrCpy(buf, &ti));
# endif	/* defined(USE_FCNTL) */

# if	defined(USE_FLOCK)
/*
 * Check current flock() lock type.
 */
    switch(ty) {
    case FULL_EX_LOCK:
    case FULL_SH_LOCK:
	break;
    default:
	(void) snprintf(buf, sizeof(buf) - 1,
	    "ERROR!!!   unknown unlock type: %s", ty);
	buf[sizeof(buf) - 1] = '\0';
	return(MkStrCpy(buf, &ti));
    }
/*
 * Unlock file with flock().
 */
    if (!flock(Fd, LOCK_UN))
	return((char *)NULL);
    (void) snprintf(buf, sizeof(buf) - 1, "ERROR!!!  flock() unlock error: %s",
	strerror(errno));
    return(MkStrCpy(buf, &ti));
# endif	/* defined(USE_FLOCK) */

}
#endif	/* !defined(USE_FLOCK) && !defined(USE_FCNTL) */
