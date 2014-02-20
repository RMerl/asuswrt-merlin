/*
 * LTnlink.c -- Lsof Test nlink tests
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
 * Pre-definitions that may be changed by specific dialects
 */

#define	DO_TEST			/* do the test */


/*
 * Dialect-specific items
 */


#if	defined(LT_DIAL_darwin)
/*
 * Darwin-specific items
 */

# if	defined(LT_KMEM)
#undef	DO_TEST
# endif	/* defined(LT_KMEM) */

#endif	/* defined(LT_DIAL_darwin) */

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
_PROTOTYPE(static char *FindFile,(char *opt, int *ff, int ie, LTdev_t *tfdc,
				  char *ibuf, char *xlnk, char *szbuf));


/*
 * Main program
 */

int
main(argc, argv)
    int argc;				/* argument count */
    char *argv[];			/* arguments */
{
    char buf[2048];			/* temporary buffer */
    int do_unlink = 1;			/* do the unlink test section */
    char *em;				/* error message pointer */
    int ff;				/* FindFile() file-found flag */
    char ibuf[32];			/* inode number in ASCII */
    char *opt;				/* lsof option */
    int sz;				/* file size */
    char szbuf[32];			/* file size in ASCII */
    LTdev_t tfdc;			/* device components */
    struct stat tfsb;			/* test file stat(2) buffer */
    int ti, tj;				/* temporary indexes */
    char xlnk[32];			/* expected link count in ASCII */
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
 * Quit if lsof for this dialect doesn't support adequate nlink reporting.
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
 * Process the file path.
 */
    if (!(Path = LTopt_p)) {

    /*
     * The file path was not supplied, so make one.
     */
	(void) snprintf(buf, sizeof(buf) - 1, "./config.LTnlink%ld",
	    (long)MyPid);
	buf[sizeof(buf) - 1] = '\0';
	Path = MkStrCpy(buf, &ti);
    }
/*
 * Create the test file.
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
 * Stat(2) the test file.
 */
    if (stat(Path, &tfsb)) {
	(void) snprintf(buf, sizeof(buf) - 1,
	    "ERROR!!!  can't stat(2) %s: %s", Path, strerror(errno));
	buf[sizeof(buf) - 1] = '\0';
	PrtMsgX(buf, Pn, cleanup, 1);
    }
/*
 * Set the test file status to open and linked.
 *
 * Get the test file's parameters:
 *
 *    * device paramters in LTdev_t form;
 *    * inode number in ASCII;
 *    * link count in ASCII;
 *    * file size in ASCII.
 */
    if ((em = ConvStatDev(&tfsb.st_dev, &tfdc)))
	PrtMsgX(em, Pn, cleanup, 1);
    (void) snprintf(ibuf, sizeof(ibuf) - 1, "%u", (unsigned int)tfsb.st_ino);
    ibuf[sizeof(szbuf) - 1] = '\0';
    (void) snprintf(xlnk, sizeof(xlnk) - 1, "%d", tfsb.st_nlink);
    ibuf[sizeof(szbuf) - 1] = '\0';
    (void) snprintf(szbuf, sizeof(szbuf) - 1, "%d", sz);
    szbuf[sizeof(szbuf) - 1] = '\0';
/*
 * See if the file is on an NFS file system.
 */
    (void) FindFile("-Na", &ff, 1, &tfdc, ibuf, xlnk, szbuf);
    if (ff) {

    /*
     * The file was found on an NFS file system.
     */
	(void) snprintf(buf, sizeof(buf) - 1,
	    "WARNING!!!  Test file %s is NFS mounted.", Path);
	(void) PrtMsg(buf, Pn);
	(void) PrtMsg(
	    "  As a result this test probably won't be able to unlink it and",
	    Pn);
	(void) PrtMsg(
	    "  find its open and unlinked instance with lsof's +L option.",
	    Pn);
	(void) PrtMsg(
	    "  Therefore, that section of this test has been disabled.\n",
	    Pn);
	(void) PrtMsg(
	    "  Hint: supply a path with the -p option to a file in a non-NFS",
	    Pn);
	(void) PrtMsg(
	    "  file system that this test can write and unlink.\n",
	    Pn);
	(void) PrtMsg(
	    "  See 00FAQ and 00TEST for more information.",
	    Pn);
	do_unlink = 0;
    }
/*
 * Find the test file.
 */
    if ((em = FindFile("+L", &ff, 0, &tfdc, ibuf, xlnk, szbuf)))
	(void) PrtMsgX(em, Pn, cleanup, 1);
/*
 * If the unlink test is enabled, do it.
 */
    if (do_unlink) {
	(void) unlink(Path);
	for (opt = "+L1", ti = 0, tj = 30; ti < tj; ti++) {

	/*
	 * Wait a while for the link count to be updated before concluding
	 * lsof can't find the unlinked file.  Use "+L1" for only the first
	 * third of the tries, then switch to "+L".
	 */
	    if ((ti + ti + ti) >= tj)
		opt = "+L";
	    if (!(em = FindFile(opt, &ff, 0, &tfdc, ibuf, "0", szbuf)))
		break;
	    if (ti)
		(void) printf(".");
	    else
		(void) printf("waiting for link count update: .");
	    (void) fflush(stdout);
	    (void) sleep(2);
	}
	if (ti) {

	/*
	 * End the delay message.
	 */
	    printf("\n");
	    (void) fflush(stdout);
	    MsgStat = 1;
	}
	if (em)
	    (void) PrtMsgX(em, Pn, cleanup, 1);
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
    }
    if (Path)
	(void) unlink(Path);
}


/*
 * FindFile() -- find a file with lsof
 */

static char *
FindFile(opt, ff, ie, tfdc, ibuf, xlnk, szbuf)
    char *opt;				/* additional lsof options */
    int *ff;				/* file-found response receptor */
    int ie;				/* ignore errors if == 1 */
    LTdev_t *tfdc;			/* test file device components */
    char *ibuf;				/* inode number in ASCII */
    char *xlnk;				/* expected link count */
    char *szbuf;				/* file size in ASCII */
{
    char buf[2048];			/* temporary buffer */
    char *cem;				/* current error message pointer */
    LTfldo_t *cmdp;			/* command pointer */
    LTfldo_t *devp;			/* device pointer */
    LTfldo_t *fop;			/* field output pointer */
    LTfldo_t *inop;			/* inode number pointer */
    LTdev_t lsofdc;			/* lsof device components */
    int nf;				/* number of fields */
    LTfldo_t *nlkp;			/* nlink pointer */
    char *opv[4];			/* option vector for ExecLsof() */
    char *pem = (char *)NULL;		/* previous error message pointer */
    pid_t pid;				/* PID */
    int pids = 0;			/* PID found status */
    LTfldo_t *szp;			/* size pointer */
    char *tcp;				/* temporary character pointer */
    int ti;				/* temporary integer */
    LTfldo_t *typ;			/* file type pointer */
/*
 * Check the argument pointers.
 *
 * Set the file-found response false.
 */
    if (!ff || !ibuf || !szbuf || !tfdc || !xlnk)
	(void) PrtMsgX("ERROR!!!  missing argument to FindFile()",
		       Pn, cleanup, 1);
    *ff = 0;
/*
 * Complete the option vector and start lsof execution.
 */
    ti = 0;
    if (opt && *opt)
	opv[ti++] = opt;

#if	defined(USE_LSOF_C_OPT)
    opv[ti++] = "-C";
#endif	/* defined(USE_LSOF_C_OPT) */

    if (strcmp(xlnk, "0"))
	opv[ti++] = Path;
    opv[ti] = (char *)NULL;
    if ((cem = ExecLsof(opv))) {
 	if (ie)
	    return((char *)NULL);
	return(cem);
    }
/*
 * Read lsof output.
 */
    while (!*ff && (fop = RdFrLsof(&nf, &cem))) {
	if (cem) {
 	    if (ie)
		return((char *)NULL);
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
	 * Scan for device, inode, nlink, size and type fields.
	 */
	    devp = inop = nlkp = szp = typ = (LTfldo_t *)NULL;
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
	    if (!devp || !inop || !szp || !typ)
		break;
	    if (strcasecmp(typ->v, "reg") && strcasecmp(typ->v, "vreg"))
		break;
	    if ((cem = ConvLsofDev(devp->v, &lsofdc))) {
		if (pem)
		    (void) PrtMsg(pem, Pn);
		pem = cem;
		break;
	    }
	    if ((tfdc->maj != lsofdc.maj)
	    ||  (tfdc->min != lsofdc.min)
	    ||  (tfdc->unit != lsofdc.unit)
	    ||  strcmp(inop->v, ibuf)
	    ) {
		break;
	    }
	/*
	 * Indicate the file was found.
	 */
	    *ff = 1;
	/*
	 * Check the size and link count.
	 */
	    if (!szp) {
		(void) snprintf(buf, sizeof(buf) - 1,
		    "ERROR!!!  lsof didn't report a file size for %s", Path);
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
	    if (!nlkp) {
		if (strcmp(xlnk, "0")) {

		/*
		 * If lsof returned no link count and the expected return is
		 * not "0", it's an error.  Otherwise, interpret no link count
		 * as equivalent to a "0" link count.
		 */
		    (void) snprintf(buf, sizeof(buf) - 1,
			"ERROR!!!  lsof didn't report a link count for %s",
			Path);
		    buf[sizeof(buf) - 1] = '\0';
		    cem = MkStrCpy(buf, &ti);
		    if (pem)
			(void) PrtMsg(pem, Pn);
		    pem = cem;
		    break;
		}
	    } else {
		if (strcmp(nlkp->v, xlnk)) {
		    (void) snprintf(buf, sizeof(buf) - 1,
			"ERROR!!!  wrong link count: expected %s, got %s",
			xlnk, nlkp->v);
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
	 * pointer unless errors are being ignored.  (The previous error
	 * message pointer  will be NULL if no error was detected.)
	 */
	    (void) StopLsof();
 	    if (ie)
		return((char *)NULL);
	    return(pem);
	}
    }
/*
 * Clean up and return.
 */
    (void) StopLsof();
    if (!*ff && !ie) {
	if (pem)
	    (void) PrtMsg(pem, Pn);
	(void) snprintf(buf, sizeof(buf) - 1,
	    "ERROR!!!  %s test file %s not found by lsof",
	    strcmp(xlnk, "0") ? "linked" : "unlinked",
	    Path);
	buf[sizeof(buf) - 1] = '\0';
	pem = MkStrCpy(buf, &ti);
    }
    if (ie)
	return((char *)NULL);
    return(pem);
}
