/*
 * LTdnlc.c -- Lsof Test Dynamic Name Lookup Cache test
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


/*
 * Pre-definitions that may be revoked by specific dialects
 */

#define	DO_TEST				/* do the test */


/*
 * Dialect-specific items
 */


#if	defined(LT_DIAL_aix)
/*
 * AIX-specific items
 */

#undef	DO_TEST
#endif	/* defined(LT_DIAL_aix) */


#if   defined(LT_DIAL_darwin)
/*
 * Darwin-specific items
 */

# if	LT_VERS<800
#undef        DO_TEST
# endif	/* LT_VERS<800 */
#endif        /* defined(LT_DIAL_darwin) */


/*
 * Local definitions
 */

#define	ATTEMPT_CT	5		/* number of lsof CWD lookup attempts */
#define	LSPATH		"/bin/ls"	/* path to ls(1) */
#define	SUCCESS_THRESH	50.0		/* success threshold */


/*
 * Globals
 */

pid_t MyPid = (pid_t)0;		/* PID of this process */
char *Pn = (char *)NULL;	/* program name */


/*
 * Local function prototypes
 */

_PROTOTYPE(static void cleanup,(void));
_PROTOTYPE(static char *FindLsofCwd,(int *ff, LTdev_t *cwddc, char *ibuf));


/*
 * Main program
 */

int
main(argc, argv)
    int argc;				/* argument count */
    char *argv[];			/* arguments */
{
    char buf[2048];			/* temporary buffer */
    char cwd[MAXPATHLEN + 1];		/* CWD */
    LTdev_t cwddc;			/* CWD device components */
    char *em;				/* error message pointer */
    int ff;				/* FindFile() file-found flag */
    int fpathct;			/* full path found count */
    char ibuf[32];			/* inode buffer */
    char lsbuf[2048 + MAXPATHLEN + 1];	/* ls(1) system() command */
    double pct;				/* performance percentage */
    struct stat sb;			/* CWD stat(2) results */
    int ti;				/* temporary index */
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
    if (ScanArg(argc, argv, "h", Pn))
	xv = 1;
    if (xv || LTopt_h) {
	(void) PrtMsg("usage: [-h] [-p path]", Pn);
	PrtMsgX("       -h       print help (this panel)", Pn, cleanup, xv);
    }

#if	!defined(DO_TEST)
/*
 * If the dialect has disabled the test, echo that result and exit with
 * a successful return code.
 */
    (void) PrtMsgX(LT_DONT_DO_TEST, Pn, cleanup, 0);
#endif	/* !defined(DO_TEST) */

/*
 * See if lsof can be executed and can access kernel memory.
 */
    if ((em = IsLsofExec()))
	(void) PrtMsgX(em, Pn, cleanup, 1);
    if ((em = CanRdKmem()))
	(void) PrtMsgX(em, Pn, cleanup, 1);
/*
 * Get the CWD and form the ls(1) system() command.
 */

#if	defined(USE_GETCWD)
    em = "getcwd";
    if (!getcwd(cwd, sizeof(cwd)))
#else	/* ! defined(USE_GETCWD) */
    em = "getwd";
    if (!getwd(cwd))
#endif	/* defined(USE_GETCWD) */

    {
	(void) snprintf(buf, sizeof(buf) - 1,
	    "ERROR!!!  %s() error: %s", em, strerror(errno));
	buf[sizeof(buf) - 1] = '\0';
	(void) PrtMsgX(buf, Pn, cleanup, 1);
    }
    (void) snprintf(lsbuf, sizeof(lsbuf) - 1, "%s %s > /dev/null 2>&1",
	LSPATH, cwd);
/*
 * Get the CWD stat(2) results.
 */
    if (stat(cwd, &sb)) {
	(void) snprintf(buf, sizeof(buf) - 1,
	    "ERROR!!!  stat(%s) error: %s", cwd, strerror(errno));
	buf[sizeof(buf) - 1] = '\0';
	(void) PrtMsgX(buf, Pn, cleanup, 1);
    }
    if ((em = ConvStatDev(&sb.st_dev, &cwddc)))
	PrtMsgX(em, Pn, cleanup, 1);
    (void) snprintf(ibuf, sizeof(ibuf) - 1, "%u", (unsigned int)sb.st_ino);
    ibuf[sizeof(ibuf) - 1] = '\0';
/*
 * Loop ATTEMPT_CT times.
 */
    for (fpathct = ti = 0; ti < ATTEMPT_CT; ti++) {

    /*
     * Call ls(1) to list the CWD to /dev/null.
     */
	(void) system(lsbuf);
    /*
     * Call lsof to look up its own CWD -- i.e., this one.
     */
	if ((em = FindLsofCwd(&ff, &cwddc, ibuf))) {

	/*
	 * FindLsofCwd() returned a message.  Decode it via ff.
	 */
	    if (ff == -1)
		PrtMsgX(em, Pn, cleanup, 1);
	    else if (ff == 1) {

	    /*
	     * This shouldn't happen.  If FindLsof() found lsof's CWD, it
	     * should set ff to one and return NULL.
	     */
		PrtMsgX("ERROR!!!  inconsistent FindLsofCwd() return", Pn,
		    cleanup, 1);
	    }
	} else if (ff == 1) {
	    fpathct++;
	}
    }
/*
 * Compute, display, and measure the success percentage.
 */
    pct = ((double)fpathct * (double)100.0) / (double)ATTEMPT_CT;
    PrtMsg((char *)NULL, Pn);
    (void) printf("%s found: %.2f%%\n", cwd, pct);	/* NeXT snpf.c has no
							 * %f support */
    MsgStat = 1;
    if (pct < (double)SUCCESS_THRESH) {
	PrtMsg("ERROR!!!  the find rate was too low.", Pn);
	if (!fpathct) {
	    (void) PrtMsg(
		"Hint: since the find rate is zero, it may be that this file",
		Pn);
	    (void) PrtMsg(
		"system does not fully participate in kernel DNLC processing",
		Pn);
	    (void) PrtMsg(
		"-- e.g., NFS file systems often do not, /tmp file systems",
		Pn);
	    (void) PrtMsg(
		"sometimes do not, Solaris loopback file systems do not.\n",
		Pn);
	    (void) PrtMsg(
		"As a work-around rebuild and test lsof on a file system that",
		Pn);
	    (void) PrtMsg(
		"fully participates in kernel DNLC processing.\n",
		Pn);
	    (void) PrtMsg("See 00FAQ and 00TEST for more information.", Pn);
	}
	exit(1);
    }
/*
 * Exit successfully.
 */
    (void) PrtMsgX("OK", Pn, cleanup, 0);
    return(0);
}


/*
 * cleanup() -- release resources
 */

static void
cleanup()
{
}


/*
 * FindLsofCwd() -- find the lsof CWD
 */

static char *
FindLsofCwd(ff, cwddc, ibuf)
    int *ff;				/* file-found response receptor */
    LTdev_t *cwddc;			/* CWD device components */
    char *ibuf;				/* CWD inode number in ASCII */
{
    char *cp;				/* temporary character pointer */
    char *cem;				/* current error message pointer */
    LTfldo_t *cmdp;			/* command pointer */
    LTdev_t devdc;			/* devp->v device components */
    LTfldo_t *devp;			/* device pointer */
    LTfldo_t *fop;			/* field output pointer */
    LTfldo_t *inop;			/* inode number pointer */
    int nf;				/* number of fields */
    LTfldo_t *nmp;			/* name pointer */
    char *opv[3];			/* option vector for ExecLsof() */
    char *pem = (char *)NULL;		/* previous error message pointer */
    pid_t pid;				/* PID */
    int pids = 0;			/* PID found status */
    int ti;				/* temporary integer */
    LTfldo_t *typ;			/* file type pointer */
/*
 * Check the argument pointers.
 *
 * Set the file-found response false.
 */
    if (!ff || !cwddc || !ibuf)
	(void) PrtMsgX("ERROR!!!  missing argument to FindFile()",
		       Pn, cleanup, 1);
    *ff = 0;
/*
 * Complete the option vector and start lsof execution.
 */
    opv[0] = "-clsof";
    opv[1] = "-adcwd";
    opv[2] = (char *)NULL;
    if ((cem = ExecLsof(opv))) {
	*ff = -1;
	return(cem);
    }
/*
 * Read lsof output.
 */
    while (!*ff && (fop = RdFrLsof(&nf, &cem))) {
	if (cem) {
	    if (pem)
		(void) PrtMsg(pem, Pn);
	    *ff = -1;
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
	    if (!cmdp || (pid != LsofPid))
		pids = 0;
	    break;
	case LSOF_FID_FD:

	/*
	 * This is a file descriptor line.  Make sure it's for the expected
	 * PID and its type is "cwd".
	 */
	    if (!pids)
		break;
	    if (strcasecmp(fop->v, "cwd"))
		break;
	/*
	 * Scan for device, inode, name, and type fields.
	 */
	    devp = inop = nmp = typ = (LTfldo_t *)NULL;
	    for (fop++, ti = 1; ti < nf; fop++, ti++) {
		switch (fop->ft) {
		case LSOF_FID_DEVN:
		    devp = fop;
		    break;
		case LSOF_FID_INODE:
		    inop = fop;
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
	 * Check the device, inode, and type of the file.
	 */
	    if (!devp || !inop || !nmp || !typ)
		break;
	    if (strcasecmp(typ->v, "dir") && strcasecmp(typ->v, "vdir"))
		break;
	    if ((cem = ConvLsofDev(devp->v, &devdc))) {
		if (pem)
		    (void) PrtMsg(pem, Pn);
		pem = cem;
		break;
	    }
	    if ((cwddc->maj != devdc.maj)
	    ||  (cwddc->min != devdc.min)
	    ||  (cwddc->unit != devdc.unit)
	    ||  strcmp(inop->v, ibuf)
	    ) {
		break;
	    }
	/*
	 * Check the name for spaces.  If it has none, set a file-found
	 * response.
	 */
	    if (!(cp = strchr(nmp->v, ' ')))
		*ff = 1;
	    else {

	    /*
	     * If a parenthesized file system name follows the space in the
	     * file's name, it probably is an NFS file system name and can
	     * be ignored.  Accordingly set a file-found response.
	     */
		if ((*(cp + 1) == '(') && *(cp + 2) && !strchr(cp + 2, ' ')) {
		    if ((cp = strchr(cp + 2, ')')) && !*(cp + 1))
			*ff = 1;
		}
	    }
	}
    }
/*
 * Clean up and return.
 */
    (void) StopLsof();
    if (pem) {
	*ff = -1;
	return(pem);
    }
    return((char *)NULL);
}
