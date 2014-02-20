/*
 * LTszoff.c -- Lsof Test small file (< 32 bits) size and offset tests
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
 * Pre-definitions that might be undefined by dialects
 */

#define	OFFTST_STAT	1		/* offset tests status */


#if	defined(LT_DIAL_linux)
/*
 * Linux-specific items
 */

#undef	OFFTST_STAT
#define	OFFTST_STAT	0		/* Linux lsof may not be able to report
					 * offsets -- see the function
					 * ck_Linux_offset_support() */

_PROTOTYPE(static int ck_Linux_offset_support,(void));
#endif	/* defined(LT_DIAL_linux) */


/*
 * Local definitions
 */

#define	TYTST_SZ	0	/* size test type */
#define	TYTST_0to	1	/* 0t offset test type */
#define	TYTST_0xo	2	/* 0x offset test type */
#define TSTFSZ		32768	/* test file size */


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
_PROTOTYPE(static char *testlsof,(int tt, char *opt, char *xval));


/*
 * Main program
 */

int
main(argc, argv)
    int argc;				/* argument count */
    char *argv[];			/* arguments */
{
    char buf[2048];			/* temporary buffer */
    int do_offt = OFFTST_STAT;		/* do offset tests if == 1 */
    char *em;				/* error message pointer */
    int ti;				/* temporary index */
    char *tcp;				/* temporary character pointer */
    char *tstsz = (char *)NULL;		/* size test status */
    char *tst0to = (char *)NULL;	/* offset 0t form test */
    char *tst0xo = (char *)NULL;	/* offset 0x form test */
    int xv = 0;				/* exit value */
    char xbuf[64];			/* expected value buffer */
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
 * If a path was supplied in an "-p path" option, use it.  Otherwise construct
 * a path in the CWD.
 */
    if (!(Path = LTopt_p)) {
	(void) snprintf(buf, sizeof(buf) - 1, "./config.LTszoff%ld",
	(long)MyPid);
	buf[sizeof(buf) - 1] = '\0';
	Path = MkStrCpy(buf, &ti);
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
 * Write the test file to its expected size.
 */
    for (ti = 0; ti < sizeof(buf); ti++) {
	buf[ti] = (char)(ti & 0xff);
    }
    for (ti = 0; ti < TSTFSZ; ti += sizeof(buf)) {
	if (write(Fd, buf, sizeof(buf)) != sizeof(buf)) {
	    (void) fprintf(stderr, "ERROR!!!  can't write %d bytes to %s\n",
		(int)sizeof(buf), Path);
	     goto print_file_error;
	}
    }
/*
 * Fsync() the file.
 */
    if (fsync(Fd)) {
	(void) fprintf(stderr, "ERROR!!!  can't fsync %s\n", Path);
	goto print_file_error;
    }
/*
 * Do the tests.  Skip offset tests as indicated.
 */
    (void) snprintf(xbuf, sizeof(xbuf) - 1, "%d", TSTFSZ);
    xbuf[sizeof(xbuf) - 1] = '\0';
    if ((tstsz = testlsof(TYTST_SZ, "-s", xbuf)))
	(void) PrtMsg(tstsz, Pn);
    if (do_offt) {
	(void) snprintf(xbuf, sizeof(xbuf) - 1, "0t%d", TSTFSZ);
	xbuf[sizeof(xbuf) - 1] = '\0';
	if ((tst0to = testlsof(TYTST_0to, "-o", xbuf)))
	    (void) PrtMsg(tst0to, Pn);
	(void) snprintf(xbuf, sizeof(xbuf) - 1, "0x%x", TSTFSZ);
	xbuf[sizeof(xbuf) - 1] = '\0';
	if ((tst0xo = testlsof(TYTST_0xo, "-oo2", xbuf)))
	    (void) PrtMsg(tst0to, Pn);
    } else {
	PrtMsg("WARNING!!!  lsof can't return file offsets for this dialect,",
	   Pn);
	PrtMsg("  so offset tests have been disabled.", Pn);
    }
/*
 * Compute exit value and exit.
 */
    if (tstsz || tst0to || tst0xo) {
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
	(void) close(Fd);
	Fd = -1;
	if (Path) {
	    (void) unlink(Path);
	    Path = (char *)NULL;
	}
    }
}


/*
 * testlsof() -- test the open file with lsof
 */

static char *
testlsof(tt, opt, xval)
    int tt;				/* test type -- TYTST_* symbol */
    char *opt;				/* extra lsof options */
    char *xval;				/* expected value */
{
    char buf[2048];			/* temporary buffer */
    char *cem;				/* current error message pointer */
    LTfldo_t *cmdp;			/* command pointer */
    LTfldo_t *devp;			/* device pointer */
    int ff = 0;				/* file found status */
    LTfldo_t *fop;			/* field output pointer */
    char ibuf[64];			/* inode number buffer */
    LTfldo_t *inop;			/* inode number pointer */
    LTdev_t lsofdc;			/* lsof device components */
    int nf;				/* number of fields */
    LTfldo_t *offp;			/* offset pointer */
    char *opv[4];			/* option vector for ExecLsof() */
    char *pem = (char *)NULL;		/* previous error message pointer */
    pid_t pid;				/* PID */
    int pids = 0;			/* PID found status */
    struct stat sb;			/* stat(2) buffer */
    LTdev_t stdc;			/* stat(2) device components */
    LTfldo_t *szp;			/* size pointer */
    char *tcp;				/* temporary character pointer */
    int ti;				/* temporary integer */
    char *tnm1, *tnm2;			/* test names */
    int ts = 0;				/* test status flag */
    LTfldo_t *typ;			/* file type pointer */
/*
 * Check the test type.
 */
    switch (tt) {
    case TYTST_SZ:
	tnm1 = "";
	tnm2 = " size";
	break;
    case TYTST_0to:
	tnm1 = " 0t";
	tnm2 = " offset";
	break;
    case TYTST_0xo:
	tnm1 = " 0x";
	tnm2 = " offset";
	break;
    default:
	(void) snprintf(buf, sizeof(buf) - 1,
	    "ERROR!!!  illegal test type: %d", tt);
	buf[sizeof(buf) - 1] = '\0';
	(void) PrtMsgX(buf, Pn, cleanup, 1);
    }
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
	PrtMsgX(buf, Pn, cleanup, 1);
    (void) snprintf(ibuf, sizeof(ibuf) - 1, "%u", (unsigned int)sb.st_ino);
    ibuf[sizeof(ibuf) - 1] = '\0';
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
    if ((cem = ExecLsof(opv)))
	return(cem);
/*
 * Read lsof output.
 */
    while (!ff && !cem && (fop = RdFrLsof(&nf, &cem))) {
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
	 * Scan for device, inode, offset, size and type fields.
	 */
	    devp = inop = offp = szp = typ = (LTfldo_t *)NULL;
	    for (fop++, ti = 1; ti < nf; fop++, ti++) {
		switch (fop->ft) {
		case LSOF_FID_DEVN:
		    devp = fop;
		    break;
		case LSOF_FID_INODE:
		    inop = fop;
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
	 * The specified file has been located.  Do the specified test.
	 */
	    ff = 1;
	    fop = (tt == TYTST_SZ) ? szp : offp;
	    if (!fop) {
		(void) snprintf(buf, sizeof(buf) - 1,
		    "ERROR!!! %s%s test, but no lsof%s", tnm1, tnm2, tnm2);
		ts = 1;
	    } else if (strcmp(fop->v, xval)) {
		(void) snprintf(buf, sizeof(buf) - 1,
		    "ERROR!!! %s%s mismatch: expected %s, got %s",
		    tnm1, tnm2, xval, fop->v);
		ts = 1;
	    }
	    if (ts) {
		buf[sizeof(buf) - 1] = '\0';
		cem = MkStrCpy(buf, &ti);
		if (pem)
		    (void) PrtMsg(pem, Pn);
		pem = cem;
	    }
	    break;
	}
    }
    (void) StopLsof();
    if (!ff) {
	(void) snprintf(buf, sizeof(buf) - 1,
	    "ERROR!!!  test file %s not found by lsof", Path);
	buf[sizeof(buf) - 1] = '\0';
	cem = MkStrCpy(buf, &ti);
	if (pem)
	    (void) PrtMsg(pem, Pn);
	return(cem);
    }
    return(pem);
}
