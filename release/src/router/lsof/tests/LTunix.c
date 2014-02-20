/*
 * LTunix.c -- Lsof Test UNIX domain socket test
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

#include <sys/socket.h>
#include <sys/un.h>


/*
 * Local definitions
 */

#if	!defined(MAXPATHLEN)
#define	MAXPATHLEN	1024		/* maximum path length */
#endif	/* !defined(MAXPATHLEN) */


/*
 * Globals
 */

pid_t MyPid = (pid_t)0;		/* PID of this process */
char *Pn = (char *)NULL;	/* program name */
int SpFd[2] = {-1,-1};		/* socket pair FDs */
char *Path[2] = {(char *)NULL, (char *)NULL};
				/* socket pair paths */


/*
 * Local function prototypes
 */

_PROTOTYPE(static void cleanup,(void));
_PROTOTYPE(static char *FindUsocks,(void));


/*
 * Main program
 */

int
main(argc, argv)
    int argc;				/* argument count */
    char *argv[];			/* arguments */
{
    char buf[2048];			/* temporary buffer */
    char cwd[MAXPATHLEN + 1];		/* CWD buffer */
    char *em;				/* error message pointer */
    int ti, tj;				/* temporary indexes */
    struct sockaddr_un ua;		/* UNIX socket address */
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
	(void) PrtMsg("usage: [-h]", Pn);
	PrtMsgX("       -h       print help (this panel)", Pn, cleanup, xv);
    }
/*
 * See if lsof can be executed and can access kernel memory.
 */
    if ((em = IsLsofExec()))
	(void) PrtMsgX(em, Pn, cleanup, 1);
    if ((em = CanRdKmem()))
	(void) PrtMsgX(em, Pn, cleanup, 1);
/*
 * Construct the socket paths.
 */

#if	defined(USE_GETCWD)
    if (!getcwd(cwd, sizeof(cwd)))
#else	/* ! defined(USE_GETCWD) */
    if (!getwd(cwd))
#endif	/* defined(USE_GETCWD) */

    {
	em = "ERROR!!!  can't get CWD";
	goto print_errno;
    }
    cwd[sizeof(cwd) - 1] = '\0';
    if ((strlen(cwd) + strlen("/config.LT#U9223372036854775807") + 1)
    > sizeof(ua.sun_path))
    {
	strncpy(cwd, "/tmp", sizeof(cwd) - 1);
    }
    for (ti = 0; ti < 2; ti++) {
	(void) snprintf(buf, sizeof(buf) - 1, "%s/config.LT%dU%ld", cwd, ti,
	    (long)MyPid);
	buf[sizeof(buf) - 1] = '\0';
	Path[ti] = MkStrCpy(buf, &tj);
	(void) unlink(Path[ti]);
    }
/*
 * Get two UNIX domain socket FDs.
 */
    for (ti = 0; ti < 2; ti++) {
	if ((SpFd[ti] = socket(AF_UNIX, SOCK_STREAM, PF_UNSPEC)) < 0) {
	    em = "socket";

print_errno_by_ti:

	    (void) snprintf(buf, sizeof(buf) - 1, "ERROR!!!  %s(%s) failure",
		em, Path[ti]);
	    buf[sizeof(buf) - 1] = '\0';
	    em = buf;

print_errno:

	    PrtMsg(em, Pn);
	    (void) snprintf(buf, sizeof(buf) - 1, "    Errno %d: %s", errno,
		strerror(errno));
	    buf[sizeof(buf) - 1] = '\0';
	    PrtMsgX(buf, Pn, cleanup, 1);
	}
    }
/*
 * Bind file system names to the sockets.
 */
    for (ti = 0; ti < 2; ti++) {
	(void) memset((void *)&ua, 0, sizeof(ua));
	ua.sun_family = AF_UNIX;
	(void) strncpy(ua.sun_path, Path[ti], sizeof(ua.sun_path));
	ua.sun_path[sizeof(ua.sun_path) - 1] = '\0';
	if (bind(SpFd[ti], (struct sockaddr *)&ua, sizeof(ua)) < 0) {
	    em = "bind";
	    goto print_errno_by_ti;
	}
    }
/*
 * Look for the open UNIX domain socket files with lsof.
 */
    if ((em = FindUsocks()))
	(void) PrtMsgX(em, Pn, cleanup, 1);
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
    int ti;

    for (ti = 0; ti < 2; ti++) {
	if (SpFd[ti] >= 0) {
	    (void) close(SpFd[ti]);
	    SpFd[ti] = -1;
	}
	if (Path[ti]) {
	    (void) unlink(Path[ti]);
	    (void) free((void *)Path[ti]);
	    Path[ti] = (char *)NULL;
	}
    }
}


/*
 * FindUsocks() -- find UNIX sockets with lsof
 */

static char *
FindUsocks()
{
    char buf[2048];			/* temporary buffer */
    char *cem;				/* current error message pointer */
    LTfldo_t *cmdp;			/* command pointer */
    int ff[2];				/* file-found flags */
    LTfldo_t *fop;			/* field output pointer */
    int nf;				/* number of fields */
    int nl;				/* name length */
    LTfldo_t *nmp;			/* name pointer */
    char *opv[5];			/* option vector for ExecLsof() */
    char *pem = (char *)NULL;		/* previous error message pointer */
    pid_t pid;				/* PID */
    int pids = 0;			/* PID found status */
    char *tcp;				/* temporary character pointer */
    int ti, tj;				/* temporary integers */
    LTfldo_t *typ;			/* file type pointer */
/*
 * Build the option vector and start lsof execution.
 */
    ff[0] = ff[1] = ti = 0;
    opv[ti++] = "-aU";
    opv[ti++] = "-p";
    (void) snprintf(buf, sizeof(buf) - 1, "%ld", (long)MyPid);
    buf[sizeof(buf) - 1] = '\0';
    opv[ti++] = MkStrCpy(buf, &tj);

#if	defined(USE_LSOF_C_OPT)
    opv[ti++] = "-C";
#endif	/* defined(USE_LSOF_C_OPT) */

    opv[ti] = (char *)NULL;
    if ((cem = ExecLsof(opv)))
	return(cem);
/*
 * Read lsof output.
 */
    while (((ff[0] + ff[1]) < 2) && (fop = RdFrLsof(&nf, &cem))) {
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
	 * This is a file descriptor line.  Make sure its number matches a
	 * test file descriptor number.
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
	    for (tj = 0; tj < 2; tj++) {
		if (ff[tj])
		    continue;
		if (SpFd[tj] == ti)
		    break;
	    }
	    if (tj >= 2)
		break;
	/*
	 * Scan for name and type.
	 */
	    nmp = typ  = (LTfldo_t *)NULL;
	    for (fop++, ti = 1; ti < nf; fop++, ti++) {
		switch (fop->ft) {
		case LSOF_FID_NAME:
		    nmp = fop;
		    break;
		case LSOF_FID_TYPE:
		    typ = fop;
		    break;
		}
	    }
	/*
	 * Check the type of the file.
	 */
	    if (!typ || strcasecmp(typ->v, "unix"))
		break;
	/*
	 * Look for the name.
	 */
	    if (!nmp)
		break;
	    nl = strlen(Path[tj]);
	    for (tcp = nmp->v; tcp; tcp = strchr(tcp + 1, '/')) {
		if (!strncmp(tcp, Path[tj], nl)) {

		/*
		 * Mark a file as found.
		 */
		    ff[tj] = 1;
		    break;
		}
	    }
	}
    }
/*
 * Clean up and return.
 */
    (void) StopLsof();
    for (ti = 0; ti < 2; ti++) {
	if (ff[tj])
	    continue;
	(void) snprintf(buf, sizeof(buf) - 1, "ERROR!!!  not found: %s",
	    Path[ti]);
	buf[sizeof(buf) - 1] = '\0';
	if (pem)
	    (void) PrtMsg(pem, Pn);
	pem = MkStrCpy(buf, &tj);
    }
    return(pem);
}
