/*
 * LTbasic.c -- Lsof Test basic tests
 *
 * The basic tests measure the finding by lsof of its own open CWD, open
 * executable (when possible), and open /dev/kmem files.
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
 * Local definitions
 */


/*
 * Globals
 */

char *Pn = (char *)NULL;	/* program name */


/*
 * Local function prototypes
 */

_PROTOTYPE(static void cleanup,(void));
_PROTOTYPE(static char *tstlsof,(char **texec, char **tkmem, char **tproc));


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
    char *texec = (char *)NULL;		/* lsof executable test result */
    char *tkmem = (char *)NULL;		/* /dev/kmem test result */
    char *tproc = (char *)NULL;		/* lsof process test result */
    int xv = 0;				/* exit value */
/*
 * Get program name and PID, issue start message, and build space prefix.
 */
    if ((Pn = strrchr(argv[0], '/')))
	Pn++;
    else
	Pn = argv[0];
    (void) printf("%s ... ", Pn);
    (void) fflush(stdout);
    PrtMsg((char *)NULL, Pn);
/*
 * Process arguments.
 */
    if (ScanArg(argc, argv, "h", Pn))
	xv = 1;
    if (xv || LTopt_h) {
	(void) PrtMsg("usage: [-h]", Pn);
	PrtMsgX      ("       -h       print help (this panel)", Pn, cleanup,
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
 * Test lsof.
 */
    if ((em = tstlsof(&texec, &tkmem, &tproc)))
	PrtMsg(em, Pn);
    if (texec)
	PrtMsg(texec, Pn);
    if (tkmem)
	PrtMsg(tkmem, Pn);
    if (tproc)
	PrtMsg(tproc, Pn);
/*
 * Compute exit value and exit.
 */
    if (em || texec || tkmem || tproc) {
	if (strcmp(LT_DEF_LSOF_PATH, LsofPath)) {
	    PrtMsg (" ", Pn);
	    PrtMsg ("Hint: you used the LT_LSOF_PATH environment variable to",
		Pn);
	    PrtMsg ("  specify this path to the lsof executable:\n", Pn);
	    (void) snprintf(buf, sizeof(buf) - 1, "      %s\n", LsofPath);
	    buf[sizeof(buf) - 1] = '\0';
	    PrtMsg (buf, Pn);
	    PrtMsgX("  Make sure its revision is 4.63 or higher.",
		Pn, cleanup, 1);
	} else
	    PrtMsgX("", Pn, cleanup, 1);
    }
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
 * tstlsof() -- test for the lsof process
 */

static char *
tstlsof(texec, tkmem, tproc)
    char **texec;			/* result of the executable test */
    char **tkmem;			/* result of the /dev/kmem test */
    char **tproc;			/* result of the lsof process test */
{
    char buf[2048];			/* temporary buffer */
    char *cem;				/* current error message pointer */
    LTfldo_t *cmdp;			/* command pointer */
    LTdev_t cwddc;			/* CWD device components */
    struct stat cwdsb;			/* CWD stat(2) buffer */
    LTfldo_t *devp;			/* device pointer */
    int execs = 0;			/* executable status */
    int fdn;				/* FD is a number */
    LTfldo_t *fdp;			/* file descriptor pointer */
    LTfldo_t *fop;			/* field output pointer */
    char ibuf[64];			/* inode string buffer */
    LTfldo_t *inop;			/* inode number pointer */
    LTdev_t kmemdc;			/* /dev/kmem device components */
    int kmems = 0;			/* kmem status */
    struct stat kmemsb;			/* /dev/kmem stat(2) buffer */
    LTdev_t lsofdc;			/* lsof device components */
    struct stat lsofsb;			/* lsof stat(2) buffer */
    int nf;				/* number of fields */
    char *opv[4];			/* option vector for ExecLsof() */
    char *pem = (char *)NULL;		/* previous error message */
    pid_t pid;				/* PID */
    int pids = 0;			/* PID found status */
    int procs = 0;			/* process status */
    LTfldo_t *rdevp;			/* raw device pointer */
    char *tcp;				/* temporary character pointer */
    int ti;				/* temporary integer */
    LTdev_t tmpdc;			/* temporary device components */
    LTfldo_t *typ;			/* file type pointer */
    int xwhile;				/* exit while() flag */

/*
 * Get lsof executable's stat(2) information.
 */
    if (stat(LsofPath, &lsofsb)) {
	(void) snprintf(buf, sizeof(buf) - 1, "ERROR!!!  stat(%s): %s",
	    LsofPath, strerror(errno));
	buf[sizeof(buf) - 1] = '\0';
	cem = MkStrCpy(buf, &ti);
	if (pem)
	    (void) PrtMsg(pem, Pn);
	pem = cem;
	execs = 1;
    } else if ((cem = ConvStatDev(&lsofsb.st_dev, &lsofdc))) {
	if (pem)
	    (void) PrtMsg(pem, Pn);
	pem = cem;
	execs = 1;
    }

#if	defined(LT_KMEM)
/*
 * Get /dev/kmem's stat(2) information.
 */
    if (stat("/dev/kmem", &kmemsb)) {
	(void) snprintf(buf, sizeof(buf) - 1,
	    "ERROR!!!  can't stat(2) /dev/kmem: %s", strerror(errno));
	buf[sizeof(buf) - 1] = '\0';
	cem = MkStrCpy(buf, &ti);
	if (pem)
	    (void) PrtMsg(pem, Pn);
	pem = cem;
	kmems = 1;
    } else if ((cem = ConvStatDev(&kmemsb.st_rdev, &kmemdc))) {
	if (pem)
	    (void) PrtMsg(pem, Pn);
	pem = cem;
	kmems = 1;
    }
#else	/* !defined(LT_KMEM) */
    kmems = 1;
#endif	/* defined(LT_KMEM) */

/*
 * Get CWD's stat(2) information.
 */
    if (stat(".", &cwdsb)) {
	(void) snprintf(buf, sizeof(buf) - 1, "ERROR!!!  stat(.): %s",
	    strerror(errno));
	buf[sizeof(buf) - 1] = '\0';
	cem = MkStrCpy(buf, &ti);
	if (pem)
	    (void) PrtMsg(pem, Pn);
	pem = cem;
	procs = 1;
    } else if ((cem = ConvStatDev(&cwdsb.st_dev, &cwddc))) {
	if (pem)
	    (void) PrtMsg(pem, Pn);
	pem = cem;
	procs = 1;
    }

/*
 * Complete the option vector and start lsof execution.
 */
    ti = 0;

#if	defined(USE_LSOF_C_OPT)
    opv[ti++] = "-C";
#endif	/* defined(USE_LSOF_C_OPT) */

#if	defined(USE_LSOF_X_OPT)
    opv[ti++] = "-X";
#endif	/* defined(USE_LSOF_X_OPT) */

    opv[ti++] = "-clsof";
    opv[ti] = (char *)NULL;
    if ((cem = ExecLsof(opv))) {
	if (pem)
	    (void) PrtMsg(pem, Pn);
	return(cem);
    }
/*
 * Read lsof output.
 */
    xwhile = execs + kmems + procs;
    while ((xwhile < 3) && (fop = RdFrLsof(&nf, &cem))) {
	if (pem)
	    (void) PrtMsg(pem, Pn);
	pem = cem;
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
	 * This is a file descriptor line.  Scan its fields.
	 */
	    if (!pids)
		break;
	    devp = inop = rdevp = typ = (LTfldo_t *)NULL;
	    fdp = fop;
	    for (fop++, ti = 1; ti < nf; fop++, ti++) {
		switch(fop->ft) {
		case LSOF_FID_DEVN:
		    devp = fop;
		    break;
		case LSOF_FID_INODE:
		    inop = fop;
		    break;
		case LSOF_FID_RDEV:
		    rdevp = fop;
		    break;
		case LSOF_FID_TYPE:
		    typ = fop;
		    break;
		}
	    }
	/*
	 * A file descriptor line has been processes.
	 *
	 * Set the descriptor's numeric status.
	 *
	 * Check descriptor by FD type.
	 */

	    for (fdn = 0, tcp = fdp->v; *tcp; tcp++) {
		if (!isdigit((unsigned char)*tcp)) {
		    fdn = -1;
		    break;
		}
		fdn = (fdn * 10) + (int)(*tcp - '0');
	    }
	    if (!procs
	    &&  (fdn == -1)
	    &&  !strcasecmp(fdp->v, "cwd")
	    &&  typ
	    &&  (!strcasecmp(typ->v, "DIR") || !strcasecmp(typ->v, "VDIR"))
	    ) {

	    /*
	     * This is the CWD for the process.  Make sure its information
	     * matches what stat(2) said about the CWD.
	     */
		if (!devp || !inop)
		    break;
		if ((cem = ConvLsofDev(devp->v, &tmpdc))) {
		    if (pem)
			(void) PrtMsg(pem, Pn);
		    pem = cem;
		    break;
		}
		(void) snprintf(ibuf, sizeof(ibuf) - 1, "%u",
		    (unsigned int)cwdsb.st_ino);
		ibuf[sizeof(ibuf) - 1] = '\0';
		if ((tmpdc.maj == cwddc.maj)
		&&  (tmpdc.min == cwddc.min)
		&&  (tmpdc.unit == cwddc.unit)
		&&  !strcmp(inop->v, ibuf)
		) {
		    procs = 1;
		    xwhile++;
		}
		break;
	    }
	    if (!kmems
	    &&  (fdn >= 0)
	    &&  typ
	    &&  (!strcasecmp(typ->v, "CHR") || !strcasecmp(typ->v, "VCHR"))
	    ) {

	    /*
	     * /dev/kmem hasn't been found and this is an open character device
	     * file with a numeric descriptor.
	     *
	     * See if it is /dev/kmem.
	     */
		if (!inop || !rdevp)
		    break;
		if ((cem = ConvLsofDev(rdevp->v, &tmpdc))) {
		    if (pem)
			(void) PrtMsg(pem, Pn);
		    pem = cem;
		    break;
		}
		(void) snprintf(ibuf, sizeof(ibuf) - 1, "%u",
		    (unsigned int)kmemsb.st_ino);
		ibuf[sizeof(ibuf) - 1] = '\0';
		if ((tmpdc.maj == kmemdc.maj)
		&&  (tmpdc.min == kmemdc.min)
		&&  (tmpdc.unit == kmemdc.unit)
		&&  !strcmp(inop->v, ibuf)
		) {
		    kmems = 1;
		    xwhile++;
		}
		break;
	    }
	    if (!execs
	    &&  (fdn == -1)
	    &&  typ
	    &&  (!strcasecmp(typ->v, "REG") || !strcasecmp(typ->v, "VREG"))
	    ) {

	    /*
	     * If this is a regular file with a non-numeric FD, it may be the
	     * executable.
	     */
		if (!devp || !inop)
		    break;
	        if ((cem = ConvLsofDev(devp->v, &lsofdc))) {
		    if (pem)
			(void) PrtMsg(pem, Pn);
		    pem = cem;
		    break;
		}
		(void) snprintf(ibuf, sizeof(ibuf) - 1, "%u",
		    (unsigned int)lsofsb.st_ino);
		ibuf[sizeof(ibuf) - 1] = '\0';
		if ((tmpdc.maj == lsofdc.maj)
		&&  (tmpdc.min == lsofdc.min)
		&&  (tmpdc.unit == lsofdc.unit)
		&&  !strcmp(inop->v, ibuf)
		) {
		    execs = 1;
		    xwhile++;
		}
	    }
	}
    }
    (void) StopLsof();
    if (!execs)
	*texec = "ERROR!!!  open lsof executable wasn't found.";
    if (!kmems)
	*tkmem = "ERROR!!!  open lsof /dev/kmem usage wasn't found.";
    if (!procs)
	*tproc = "ERROR!!!  lsof process wasn't found.";
    return(pem);
}
