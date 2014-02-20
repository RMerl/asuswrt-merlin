/*
 * LTnfs.c -- Lsof Test NFS tests
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


#if	defined(LT_DIAL_darwin)
/*
 * Darwin-specific items
 */

# if LT_VERS<800
#undef	DO_TEST
# endif	/* LT_VERS<800 */
#endif	/* defined(LT_DIAL_darwin) */


/*
 * Globals
 */

int Fd = -1;			/* test file descriptor; open if >= 0 */
pid_t MyPid = (pid_t)0;		/* PID of this process */
int NFstat = 0;			/* NFS file status: 0 == closed
				 *		    1 == not created by this
				 *			 these and must not be
				 *			 unlinked
				 *		    2 == created by this test
				 *			 and must be unlinked
				 */
char *Path = (char *)NULL;	/* test file path; none if NULL */
char *Pn = (char *)NULL;	/* program name */


/*
 * Local function prototypes
 */

_PROTOTYPE(static void cleanup,(void));
_PROTOTYPE(static char *FindNFSfile,(int *ff, char *szbuf));


/*
 * Main program
 */

int
main(argc, argv)
    int argc;				/* argument count */
    char *argv[];			/* arguments */
{
    char buf[2048];			/* temporary buffer */
    char *em;				/* error message pointer */
    int ff;				/* FindNFSfile() file-found flag */
    int sz;				/* file size (if created) */
    char szbuf[32];			/* created test file size in ASCII */
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

#if	!defined(DO_TEST)
/*
 * If the dialect has disabled the test, echo that result and exit with
 * a successful return code.
 */
    (void) PrtMsgX(LT_DONT_DO_TEST, Pn, cleanup, 0);
#endif	/* !defined(DO_TEST) */

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
/*
 * See if lsof can be executed and can access kernel memory.
 */
    if ((em = IsLsofExec()))
	(void) PrtMsgX(em, Pn, cleanup, 1);
    if ((em = CanRdKmem()))
	(void) PrtMsgX(em, Pn, cleanup, 1);
/*
 * Process the file path and open it.
 */
    if ((Path = LTopt_p)) {

    /*
     * The file path was supplied.  Open the file read-only.
     */
	if ((Fd = open(Path, O_RDONLY, 0400)) < 0) {
	    (void) fprintf(stderr, "ERROR!!!  can't read-only open %s\n",
		Path);
	    goto print_file_error;
	}
    /*
     * Record that an existing file is being used.  Clear its ASCII size.
     */
	NFstat = 1;
	szbuf[0] = '\0';
    } else {

    /*
     * The file path wasn't supplied with -p, so generate one.
     */
	(void) snprintf(buf, sizeof(buf) - 1, "./config.LTnfs%ld",
	    (long)MyPid);
	buf[sizeof(buf) - 1] = '\0';
	Path = MkStrCpy(buf, &ti);
    /*
     * Open a new test file at the specified path.
     */
	(void) unlink(Path);
	if ((Fd = open(Path, O_RDWR|O_CREAT, 0600)) < 0) {
	    (void) fprintf(stderr, "ERROR!!!  can't create %s\n", Path);

print_file_error:

	    MsgStat = 1;
	    (void) snprintf(buf, sizeof(buf) - 1, "      Errno %d: %s",
		errno, strerror(errno));
	    buf[sizeof(buf) - 1] = '\0';
	    (void) PrtMsgX(buf, Pn, cleanup, 1);
	}
	NFstat = 2;
    /*
     * Write the test file to its expected size.
     */
	sz = sizeof(buf);
	for (ti = 0; ti < sz; ti++) {
	    buf[ti] = (char)(ti & 0xff);
	}
	if (write(Fd, buf, sz) != sz) {
	    (void) fprintf(stderr, "ERROR!!!  can't write %d bytes to %s\n",
		sz, Path);
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
    * Convert the file size to ASCII.
    */
	(void) snprintf(szbuf, sizeof(szbuf) - 1, "%d", sz);
	szbuf[sizeof(szbuf) - 1] = '\0';
    }
/*
 * Make sure the test file can be found on an NFS file system.
 */
    if ((em = FindNFSfile(&ff, szbuf))) {

    /*
     * Print the error message returned by FindNFSfile().
     */
	(void) PrtMsg(em, Pn);
	if (!ff) {

	/*
	 * If the file couldn't be found, print hints.
	 */
	    if (NFstat == 1) {
		(void) PrtMsg(
		    "Hint: this test must be able to open for read access",
		    Pn);
		(void) PrtMsg(
		    "the file at the path supplied with the -p option and",
		    Pn);
		(void) PrtMsg(
		    "that file must be a regular file (not a directory) on",
		    Pn);
		(void) PrtMsg(
		    "an NFS file system.\n",
		    Pn);
		(void) PrtMsgX(
		    "See 00FAQ and 00TEST for more information.",
		    Pn, cleanup, 1);
	    } else if (NFstat == 2) {
		(void) PrtMsg(
		    "Hint: the temporary path generated by this test might",
		    Pn);
		(void) PrtMsg(
		    "not be on an NFS file system, or this test might be",
		    Pn);
		(void) PrtMsg(
		    "unable to create a file on the NFS file system.\n",
		    Pn);
		(void) PrtMsg(
		    "As a work-around use the -p option to specify a path to",
		    Pn);
		(void) PrtMsg(
		    "a regular file (not a directory) on an NFS file system",
		    Pn);
		(void) PrtMsg(
		    "to which this test will have read access.\n",
		    Pn);
		(void) PrtMsgX(
		    "See 00FAQ and 00TEST for more information.",
		    Pn, cleanup, 1);
	    }
	}
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
    if (Fd >= 0) {
	(void) close(Fd);
	Fd = -1;
	if (Path) {
	    if (NFstat == 2)
		(void) unlink(Path);
	    Path = (char *)NULL;
	}
    }
}


/*
 * FindNFSfile() -- find the NFS file with lsof
 */

static char *
FindNFSfile(ff, szbuf)
    int *ff;				/* file-found response receptor */
    char *szbuf;			/* expected file size in ASCII (if
					 * the file was created by this test */
{
    char buf[2048];			/* temporary buffer */
    char *cem;				/* current error message pointer */
    LTfldo_t *cmdp;			/* command pointer */
    LTfldo_t *devp;			/* device pointer */
    LTfldo_t *fop;			/* field output pointer */
    char ibuf[64];			/* inode number buffer */
    LTfldo_t *inop;			/* inode number pointer */
    LTdev_t lsofdc;			/* lsof device components */
    int nf;				/* number of fields */
    char nlkbuf[32];			/* link count buffer */
    LTfldo_t *nlkp;			/* nlink pointer */
    char *opv[5];			/* option vector for ExecLsof() */
    char *pem = (char *)NULL;		/* previous error message pointer */
    pid_t pid;				/* PID */
    int pids = 0;			/* PID found status */
    struct stat sb;			/* stat(2) buffer */
    LTdev_t stdc;			/* stat(2) device components */
    LTfldo_t *szp;			/* size pointer */
    char *tcp;				/* temporary character pointer */
    int ti;				/* temporary integer */
    LTfldo_t *typ;			/* file type pointer */
/*
 * Check the argument pointers.
 *
 * Set the file-found response false.
 */
    if (!ff || !szbuf)
	(void) PrtMsgX("ERROR!!!  missing argument to FindNFSfile()",
		       Pn, cleanup, 1);
    *ff = 0;
/*
 * Get test file's information.
 */
    if (stat(Path, &sb)) {
	(void) snprintf(buf, sizeof(buf) - 1,
	    "ERROR!!!  can't stat(2) %s: %s", Path, strerror(errno));
	buf[sizeof(buf) - 1] = '\0';
	PrtMsgX(buf, Pn, cleanup, 1);
    }
/*
 * Extract components from test file's stat buffer.
 */
    if ((cem = ConvStatDev(&sb.st_dev, &stdc)))
	PrtMsgX(cem, Pn, cleanup, 1);
    (void) snprintf(ibuf, sizeof(ibuf) - 1, "%u", (unsigned int)sb.st_ino);
    ibuf[sizeof(ibuf) - 1] = '\0';
    (void) snprintf(nlkbuf, sizeof(nlkbuf) - 1, "%d", (int)sb.st_nlink);
    nlkbuf[sizeof(nlkbuf) - 1] = '\0';
/*
 * Complete the option vector and start lsof execution.
 */
    ti = 0;
    opv[ti++] = "-s";
    opv[ti++] = "-Na";

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
    while (!*ff && (fop = RdFrLsof(&nf, &cem))) {
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
	/*
	 * Scan for device, inode, nlink, offset, size and type fields.
	 */
	    devp = inop = nlkp, szp = typ = (LTfldo_t *)NULL;
	    for (fop++, ti = 1; ti < nf; fop++, ti++) {
		switch (fop->ft) {
		case LSOF_FID_DEVN:
		    devp = fop;
		    break;
		case LSOF_FID_INODE:
		    inop = fop;
		    break;
		case LSOF_FID_NLINK:
		    nlkp = fop;
		    break;
		case LSOF_FID_OFFSET:
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
	 * Check the device, inode, and type of the file.
	 */
	    if (!devp || !inop || !typ)
		break;
	    if (strcasecmp(typ->v, "reg") && strcasecmp(typ->v, "vreg"))
		break;
	    if ((cem = ConvLsofDev(devp->v, &lsofdc))) {
		if (pem)
		    (void) PrtMsg(pem, Pn);
		pem = cem;
		break;
	    }
	    if ((stdc.maj != lsofdc.maj)
	    ||  (stdc.min != lsofdc.min)
	    ||  (stdc.unit != lsofdc.unit)
	    ||  strcmp(inop->v, ibuf)
	    ) {
		break;
	    }
	/*
	 * Indicate the file was found.
	 */
	    *ff = 1;
	/*
	 * Check the link count.
	 */
	    if (!nlkp) {
		(void) snprintf(buf, sizeof(buf) - 1,
		    "ERROR!!!  lsof didn't report a link count for %s", Path);
		buf[sizeof(buf) - 1] = '\0';
		cem = MkStrCpy(buf, &ti);
		if (pem)
		    (void) PrtMsg(pem, Pn);
		pem = cem;
		break;
	    }
	    if (strcmp(nlkp->v, nlkbuf)) {
		(void) snprintf(buf, sizeof(buf) - 1,
		    "ERROR!!!  wrong link count: expected %s, got %s",
		    nlkbuf, nlkp->v);
		buf[sizeof(buf) - 1] = '\0';
		cem = MkStrCpy(buf, &ti);
		if (pem)
		    (void) PrtMsg(pem, Pn);
		pem = cem;
		break;
	    }
	/*
	 * If the file was created by this test, check its size.
	 */
	    if (NFstat == 2) {
		if (!szp) {
		    (void) snprintf(buf, sizeof(buf) - 1,
			"ERROR!!!  lsof didn't report a size for %s", Path);
		    buf[sizeof(buf) - 1] = '\0';
		    cem = MkStrCpy(buf, &ti);
		    if (pem)
			(void) PrtMsg(pem, Pn);
		    pem = cem;
		    break;
		}
		if (strcmp(szp->v, szbuf)) {
		    (void) snprintf(buf, sizeof(buf) - 1,
			"ERROR!!!  wrong file size: expected %s, got %s",
			szbuf, szp->v);
		    buf[sizeof(buf) - 1] = '\0';
		    cem = MkStrCpy(buf, &ti);
		    if (pem)
			(void) PrtMsg(pem, Pn);
		    pem = cem;
		    break;
		}
	    }
	/*
	 * The requested file was located.  Return the previous error message
	 * pointer.  (It will be NULL if no error was detected.)
	 */
	    (void) StopLsof();
	    return(pem);
	}
    }
/*
 * The test file wasn't found.
 */
    (void) StopLsof();
    if (pem)
	(void) PrtMsg(pem, Pn);
    (void) snprintf(buf, sizeof(buf) - 1,
	"ERROR!!!  test file %s not found by lsof", Path);
    buf[sizeof(buf) - 1] = '\0';
    return(MkStrCpy(buf, &ti));
}
