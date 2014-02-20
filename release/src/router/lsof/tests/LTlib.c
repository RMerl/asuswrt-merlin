/*
 * LTlib.c -- the lsof test library
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


/*
 * Pre-defintions that may be changed by a specific dialect
 */

#define	X2DEV_T		unsigned int	/* cast for result of x2dev() */
#define	XDINDEV		8		/* number of hex digits in an lsof
					 * device field -- should be
					 * 2 X sizeof(X2DEV_T) */


#if	defined(LT_DIAL_aix)
/*
 * AIX-specific items
 */

#include <sys/sysmacros.h>

# if	defined(LT_AIXA) && LT_AIXA>=1

/*
 * Note: the DEVNO64 and ISDEVNO54 #define's come from <sys/sysmacros.h>, but
 * only when _KERNEL is #define'd.
 */

#undef	DEVNO64
#define	DEVNO64		0x8000000000000000LL
#undef	ISDEVNO64
#define	ISDEVNO64(d)	(((ulong)(d) & DEVNO64) ? 1 : 0)

/*
 * Define major and minor extraction macros that work on 64 bit AIX
 * architectures.
 */
 
#define	major_S(d)	(ISDEVNO64(d) ? major64(d) : minor(d & ~SDEV_REMOTE))
#define	minor_S(d)	(ISDEVNO64(d) ? (minor64(d) & ~SDEV_REMOTE) : minor(d))
#undef	X2DEV_T
#define	X2DEV_T		unsigned long long
#undef	XDINDEV
#define	XDINDEV		16
#define	major_X(dp, em)	major_S(x2dev(dp, em))
#define	minor_X(dp, em)	minor_S(x2dev(dp, em))
# endif	/* defined(LT_AIXA) && LT_AIXA>=1 */

#endif	/* defined(LT_DIAL_aix) */


#if	defined(LT_DIAL_bsdi)
/*
 * BSDI-specific items
 */

#define	minor_S(dev)	dv_subunit(dev)
#define	unit_S(dev)	dv_unit(dev)
#define	minor_X(dp, em)	dv_subunit(x2dev(dp, em))
#define	unit_X(dp, em)	dv_unit(x2dev(dp, em))	
#endif	/* defined(LT_DIAL_bsdi) */


#if	defined(LT_DIAL_osr)
/*
 * OpenUNIX-specific items
 */

#include <sys/sysmacros.h>
#endif	/* defined(LT_DIAL_osr) */


#if	defined(LT_DIAL_ou)
/*
 * OpenUNIX-specific items
 */

#include <sys/mkdev.h>
#endif	/* defined(LT_DIAL_ou) */


#if	defined(LT_DIAL_solaris)
/*
 * Solaris-specific items
 */

#include <sys/sysmacros.h>


/*
 * Define maximum major device number in a stat(2) dev_t
 */

# if	LT_VERS>=20501
#define LT_MJX	L_MAXMAJ	/* Get maximum major device number from
				 * <sys/sysmacros.h>. */
# else	/* LT_VERS<20501 */
#define	LT_MJX	0x3fff		/* Avoid <sys/sysmacros.h> when
				 * Solaris < 2.5.1. */
# endif /* LT_VERS>=20501 */

#define	major_S(dev)	((int)((dev >> L_BITSMINOR) & LT_MJX))
#define	minor_S(dev)	((int)(dev & L_MAXMIN))

# if	defined(LT_K64)

/*
 * Solaris 64 bit kernel
 */

#undef	X2DEV_T
#define	X2DEV_T		unsigned long long
#undef	XDINDEV
#define	XDINDEV		16

#define	major_X(dp, em)	((int)((x2dev(dp, em) >> 32) & 0xffffffff))
#define	minor_X(dp, em) ((int)(x2dev(dp, em) & 0xffffffff))
# else	/* !defined(LT_K64) */

/*
 * Solaris 32 bit kernel
 */

#define	major_X(dp, em)	((int)((x2dev(dp, em) >> L_BITSMINOR) & LT_MJX))
#define	minor_X(dp, em)	((int)(x2dev(dp, em) & L_MAXMIN))
# endif	/* LT_K64 */
#endif	/* defined(LT_DIAL_solaris) */


#if	defined(LT_DIAL_uw)
/*
 * UnixWare-specific items
 */

#include <sys/mkdev.h>
#endif	/* defined(LT_DIAL_uw) */


/*
 * Global variables
 */

int LsofFd = -1;			/* lsof pipe FD */
FILE *LsofFs = (FILE *)NULL;		/* stream for lsof pipe FD */
char *LsofPath = (char *)NULL;		/* path to lsof executable */
pid_t LsofPid = (pid_t)0;		/* PID of lsof child process */
int LTopt_h = 0;			/* "-h" option's switch value */
char *LTopt_p = (char *)NULL;		/* "-p path" option's path value */
int MsgStat = 0;			/* message status: 1 means prefix needs
					 * to be issued */


/*
 * Local static variables
 */

static int Afo = 0;			/* Fo[] structures allocated */
static char *GOv = (char *)NULL;	/* option `:' value pointer */
static int GOx1 = 1;			/* first opt[][] index */
static int GOx2 = 0;			/* second opt[][] index */
static LTfldo_t *Fo = (LTfldo_t *)NULL;	/* allocated LTfldo_t structures */
static int Ufo = 0;			/* Fo[] structures used */


/*
 * Local function prototypes
 */

_PROTOTYPE(static void closepipe,(void));
_PROTOTYPE(static void getlsofpath,(void));
_PROTOTYPE(static int GetOpt,(int ct, char *opt[], char *rules, char **em,
			 char *pn));
_PROTOTYPE(static X2DEV_T x2dev,(char *x, char **em));


/*
 * Default major, minor, and unit macros.
 */

#if	!defined(major_S)
#define	major_S		major
#endif	/* defined(major_S) */

#if	!defined(minor_S)
#define	minor_S		minor
#endif	/* defined(minor_S) */

#if	!defined(unit_S)
#define	unit_S(x)	0
#endif	/* defined(unit_S) */

#if	!defined(major_X)
#define	major_X(dp, em)	major(x2dev(dp, em))
#endif	/* defined(major_X) */

#if	!defined(minor_X)
#define	minor_X(dp, em)	minor(x2dev(dp, em))
#endif	/* defined(minor_X) */

#if	!defined(unit_X)
#define	unit_X(dp, em)	0
#endif	/* defined(unit_X) */


/*
 * CanRdKmem() -- can lsof read kernel memory devices?
 */

char *
CanRdKmem()
{

#if	defined(LT_KMEM)
    char buf[2048];			/* temporary buffer */
    char *dn;				/* memory device name */
    char *em;				/* error message pointer */
    int fd;				/* temporary file descriptor */
    struct stat sb;			/* memory device stat(2) buffer */
    int ti;				/* temporary integer */
/*
 * Get the lsof path.  If it is not the default, check no further.
 */
    (void) getlsofpath();
    if (!strcmp(LsofPath, LT_DEF_LSOF_PATH))
	return((char *)NULL);
/*
 * Check /dev/kmem access.
 */
    dn = "/dev/kmem";
    if (stat(dn, &sb)) {
	em = "stat";

kmem_error:

	(void) snprintf(buf, sizeof(buf) - 1,
	    "ERROR!!!  can't %s(%s): %s\n", em, dn, strerror(errno));
	buf[sizeof(buf) - 1] = '\0';
	return(MkStrCpy(buf, &ti));
    }
    if ((fd = open(dn, O_RDONLY, 0)) < 0) {
	em = "open";
	goto kmem_error;
    }
    (void) close(fd);
/*
 * Check /dev/mem access.
 */
    dn = "/dev/mem";
    if (stat(dn, &sb)) {

    /*
     * If /dev/mem can't be found, ignore the error.
     */
	return((char *)NULL);
    }
    if ((fd = open(dn, O_RDONLY, 0)) < 0) {
	em = "open";
	goto kmem_error;
    }
    (void) close(fd);
#endif	/* defined(LT_KMEM) */

    return((char *)NULL);
}


/*
 * closepipe() -- close pipe from lsof
 */

static void
closepipe()
{
    if (LsofFd >= 0) {

    /*
     * A pipe from lsof is open.  Close it and the associated stream.
     */
	if (LsofFs) {
	    (void) fclose(LsofFs);
	    LsofFs = (FILE *)NULL;
	}
	(void) close(LsofFd);
	LsofFd = -1;
    }
}


/*
 * ConvLsofDev() -- convert lsof device string
 *
 * Note: this function is dialect-specific.
 */

char *
ConvLsofDev(dev, ldev)
    char *dev;			/* lsof device string -- the value to the
				 * LSOF_FID_DEVN field of a LSOF_FID_FD block
				 * (see lsof_fields.h) */
    LTdev_t *ldev;		/* results are returned to this structure */
{
    char *dp;			/* device pointer */
    char *em;			/* error message pointer */
    int tlen;			/* temporary length */
/*
 * Check function arguments.
 *
 * Establish values for decoding the device string.
 */
    if (!dev)
	return("ERROR!!!  no ConvLsofDev() device");
    if (!ldev)
	return("ERROR!!!  no ConvLsofDev() result pointer");
    if (strncmp(dev, "0x", 2))
	return("ERROR!!!  no leading 0x in ConvLsofDev() device");
    dp = dev + 2;
    if (((tlen = (int)strlen(dp)) < 1) || (tlen > XDINDEV))
	return("ERROR!!!  bad ConvLsofDev() device length");
/*
 * Use the pre-defined *_X() macros to do the decomposition.
 */
    ldev->maj = (unsigned int)major_X(dp, &em);
    if (em)
	return(em);
    ldev->min = (unsigned int)minor_X(dp, &em);
    if (em)
	return(em);
    ldev->unit = (unsigned int)unit_X(dp, &em);
    return(em);
}


/*
 * ConvStatDev() -- convert stat(2) device number
 *
 * Note: this function is dialect-specific.
 */

char *
ConvStatDev(dev, ldev)
    dev_t *dev;			/* device number to be converted */
    LTdev_t *ldev;		/* results are returned to this structure */
{

/*
 * Check function arguments.
 */
    if (!dev)
	return("ERROR!!!  no ConvStatDev() device");
    if (!ldev)
	return("ERROR!!!  no ConvStatDev() result pointer");
/*
 * Use the pre-defined *_S() macros to do the decomposition.
 */
    ldev->maj = (unsigned int)major_S(*dev);    
    ldev->min = (unsigned int)minor_S(*dev);
    ldev->unit = (unsigned int)unit_S(*dev);
    return((char *)NULL);
}


/*
 * ExecLsof() -- execute lsof with full field output and a NUL field terminator
 *		 in a child process
 */

char *
ExecLsof(opt)
    char **opt;				/* lsof options -- a pointer to an
					 * array of character pointers,
					 * terminated by a NULL pointer */
{
    static char **av = (char **)NULL;	/* lsof argument vector, dynamically
					 * allocated */
    static int ava = 0;			/* **av entries allocated */
    char buf[2048];			/* temporary buffer */
    char *em;				/* error message pointer */
    int fd;				/* temporary file descriptor */
    int optc;				/* option count */
    int nf;				/* number of files */
    int p[2];				/* pipe FDs */
    char **tcpp;			/* temporary character pointer
					 * pointer */
    int ti;				/* temporary integer */
    int tlen;				/* temporary length */
    pid_t tpid;				/* temporary PID holder */
/*
 * It's an error if lsof is already in execution or if no lsof options
 * were supplied.
 */
    (void) getlsofpath();
    if (LsofPid)
	return("ERROR!!!  ExecLsof() says lsof is already in execution");
    if (!opt)
	return("ERROR!!!  no ExecLsof() option list");
    for (optc = 0, tcpp = opt; *tcpp; optc++, tcpp++)
	;
/*
 * Make sure lsof is executable.
 */
    if ((em = IsLsofExec()))
	return(em);
/*
 * Open a pipe through which lsof can return output.
 */
    if (pipe(p)) {
	(void) snprintf(buf, sizeof(buf) - 1,
	    "ERROR!!!  can't open pipe: %s", strerror(errno));
	return(MkStrCpy(buf, &ti));
    }
/*
 * Allocate and build an argument vector.  The first entry will be set
 * to "lsof", the second to "-wFr", and the third to "-F0".  Additional
 * entries will be set as supplied by the caller.
 */
    if ((optc + 4) > ava) {
	tlen = (int)(sizeof(char *) * (optc + 4));
	if (!av)
	    av = (char **)malloc(tlen);
	else
	    av = (char **)realloc((void *)av, tlen);
	if (!av) {
	    (void) snprintf(buf, sizeof(buf) - 1,
		"LTlib: ExecLsof() can't allocat pointers for %d arguments",
		optc + 4);
	    return(MkStrCpy(buf, &ti));
	}
	ava = optc + 4;
    }
    for (ti = 0, tcpp = opt; ti < (optc + 3); ti++) {
	switch(ti) {
	case 0:
	    av[ti] = "lsof";
	    break;
	case 1:
	    av[ti] = "-wFr";
	    break;
	case 2:
	    av[ti] = "-F0";
	    break;
	default:
	    av[ti] = *tcpp;
	    tcpp++;
	}
    }
    av[ti] = (char *)NULL;
/*
 * Fork a child process to run lsof.
 */
    switch((tpid = fork())) {
    case (pid_t)0:

    /*
     * This is the child process.
     *
     * First close all file descriptors except the output side of the pipe.
     *
     * Make the output side of the pipe STDOUT and STDERR.
     */
	for (fd = 0, nf = getdtablesize(); fd < nf; fd++) {
	    if (fd == p[1])
		continue;
	    (void) close(fd);
	}
	if (p[1] != 1)
	    (void) dup2(p[1], 1);
	if (p[1] != 2)
	    (void) dup2(p[1], 2);
	if ((p[1] != 1) && (p[1] != 2))
	    (void) close(p[1]);
    /*
     * Execute lsof.
     */
	(void) execv(LsofPath, av);
	_exit(0);				/* (Shouldn't get here.) */
    case (pid_t)-1:

    /*
     * A fork error occurred.  Form and return a message.
     */
	(void) snprintf(buf, sizeof(buf) - 1,
	    "ERROR!!!  ExecLsof() can't fork: %s", strerror(errno));
	buf[sizeof(buf) - 1] = '\0';
	return(MkStrCpy(buf, &ti));
    default:

    /*
     * This is the parent.
     *
     * Save the lsof child PID.
     *
     * Close the output side of the pipe.
     *
     * Save the input side of the pipe as LsofFd; open a stream for it.
     */
	LsofPid = tpid;
	(void) close(p[1]);
	LsofFd = p[0];
	if (!(LsofFs = fdopen(LsofFd, "r")))
	    return("ERROR!!!  ExecLsof() can't open stream to lsof output FD");
    }
/*
 * Wait a bit for lsof to start and put something in its pipe, then return
 * an "All is well." response.
 */
    sleep(1);
    return((char *)NULL);
}


/*
 * getlsofpath() -- get lsof path, either from LT_LSOF_PATH in the environment
 *		    or from LT_DEF_LSOF_PATH
 */

static void
getlsofpath()
{
    char *tcp;				/* temporary character pointer */
    int ti;				/* temporary integer */

    if (LsofPath)
	return;
    if ((tcp = getenv("LT_LSOF_PATH")))
	LsofPath = MkStrCpy(tcp, &ti);
    else
	LsofPath = LT_DEF_LSOF_PATH;
}


/*
 * GetOpt() -- Local get option
 *
 * Borrowed from lsof's main.c source file.
 *
 * Liberally adapted from the public domain AT&T getopt() source,
 * distributed at the 1985 UNIFORM conference in Dallas
 *
 * The modifications allow `?' to be an option character and allow
 * the caller to decide that an option that may be followed by a
 * value doesn't have one -- e.g., has a default instead.
 */

static int
GetOpt(ct, opt, rules, em, pn)
    int ct;				/* option count */
    char *opt[];			/* options */
    char *rules;			/* option rules */
    char **em;				/* error message return */
    char *pn;
{
    register int c;			/* character value */
    register char *cp = (char *)NULL;	/* character pointer */
    char embf[2048];			/* error message buffer */
    int tlen;				/* temporary message length from
					 * MkStrCpy() */

    *em = (char *)NULL;
    if (GOx2 == 0) {

    /*
     * Move to a new entry of the option array.
     *
     * EOF if:
     *
     *	Option list has been exhausted;
     *	Next option doesn't start with `-' or `+';
     *	Next option has nothing but `-' or `+';
     *	Next option is ``--'' or ``++''.
     */
	if (GOx1 >= ct
	||  (opt[GOx1][0] != '-' && opt[GOx1][0] != '+')
	||  !opt[GOx1][1])
	    return(EOF);
	if (strcmp(opt[GOx1], "--") == 0 || strcmp(opt[GOx1], "++") == 0) {
	    GOx1++;
	    return(EOF);
	}
	GOx2 = 1;
    }
/*
 * Flag `:' option character as an error.
 *
 * Check for a rule on this option character.
 */
    if ((c = opt[GOx1][GOx2]) == ':') {
	(void) snprintf(embf, sizeof(embf) - 1,
	    "ERROR!!!  colon is an illegal option character.");
	embf[sizeof(embf) - 1] = '\0';
	*em = MkStrCpy(embf, &tlen);
    } else if (!(cp = strchr(rules, c))) {
	(void) snprintf(embf, sizeof(embf) - 1,
	    "ERROR!!!  illegal option character: %c", c);
	embf[sizeof(embf) - 1] = '\0';
	*em = MkStrCpy(embf, &tlen);
    }
    if (*em) {

    /*
     * An error was detected.
     *
     * Advance to the next option character.
     *
     * Return the character causing the error.
     */
	if (opt[GOx1][++GOx2] == '\0') {
	    GOx1++;
	    GOx2 = 0;
	}
	return(c);
    }
    if (*(cp + 1) == ':') {

    /*
     * The option may have a following value.  The caller decides if it does.
     *
     * Don't indicate that an option of ``--'' is a possible value.
     *
     * Finally, on the assumption that the caller will decide that the possible
     * value belongs to the option, position to the option following the
     * possible value, so that the next call to GetOpt() will find it.
     */
	if(opt[GOx1][GOx2 + 1] != '\0') {
	    GOv = &opt[GOx1++][GOx2];
	} else if (++GOx1 >= ct)
	    GOv = (char *)NULL;
	else {
	    GOv = opt[GOx1];
	    if (strcmp(GOv, "--") == 0)
		GOv = (char *)NULL;
	    else
		GOx1++;
	}
	GOx2 = 0;
     } else {

    /*
     * The option character stands alone with no following value.
     *
     * Advance to the next option character.
     */
	if (opt[GOx1][++GOx2] == '\0') {
	    GOx2 = 0;
	    GOx1++;
	}
	GOv = (char *)NULL;
    }
/*
 * Return the option character.
 */
    return(c);
}


/*
 * IsLsofExec() -- see if lsof is executable
 */

char *
IsLsofExec()
{
    char buf[2048];			/* temporary buffer */
    int len;				/* temporary length */

    (void) getlsofpath();
    if (access(LsofPath, X_OK) < 0) {
	(void) snprintf(buf, sizeof(buf) - 1,
	    "ERROR!!!  can't execute %s: %s", LsofPath, strerror(errno));
        return(MkStrCpy(buf, &len));
    }
    return((char *)NULL);
}


/*
 * LTlibClean() -- clean up LTlib resource accesses
 */

void
LTlibClean()
{
    (void) StopLsof();
}


/*
 * MkStrCpy() -- make string copy
 */

char *
MkStrCpy(src, len)
    char *src;			/* string source to copy */
    int *len;			/* returned length allocation */
{
    char *rp;			/* return pointer */
    int srclen;			/* source string length */

    if (!src) {
	(void) fprintf(stderr, "ERROR!!!  no string supplied to MkStrCpy()\n");
	exit(1);
    }
    srclen = (int)strlen(src);
    *len = srclen++;
    if (!(rp = (char *)malloc(srclen))) {
	(void) fprintf(stderr, "ERROR!!!  MkStrCpy() -- no malloc() space");
	exit(1);
    }
    (void) strcpy(rp, src);
    return(rp);
}


/*
 * PrtMsg() -- print message
 */

void
PrtMsg(mp, pn)
    char *mp;				/* message pointer -- may be NULL to
					 * trigger space prefix initialization
					 */
    char *pn;				/* program name */
{
    static int pfxlen = -1;		/* prefix length, based on program */
					/* name -- computed on first call
					 * when pfxlen == -1 */
    static char *pfx = (char *)NULL;	/* prefix (spaces) */
    int ti;				/* temporary index */

    if (pfxlen == -1) {

    /*
     * This is the first call.  Compute the prefix length and build the
     * prefix.
     */
	if (!pn)
	    pfxlen = 0;
	else
	    pfxlen = (int)(strlen(pn));
	pfxlen += (int)strlen(" ... ");
	if (!(pfx = (char *)malloc(pfxlen + 1))) {
	    (void) printf( "ERROR!!!  not enough space for %d space prefix\n",
		pfxlen);
	    exit(1);
	}
	for (ti = 0; ti < pfxlen; ti++) {
	    pfx[ti] = ' ';
	}
	pfx[pfxlen] = '\0';
	MsgStat = 0;
    }
/*
 * Process the message.
 */
    if (MsgStat)
	(void) printf("%s", pfx);
    if (mp && *mp) {
	(void) printf("%s\n", mp);
	MsgStat = 1;
    }
}


/*
 * PrtMsgX() -- print message and exit
 */

void
PrtMsgX(mp, pn, f, xv)
    char *mp;				/* message pointer */
    char *pn;				/* program name */
    void (*f)();			/* clean-up function pointer */
    int xv;				/* exit value */
{
    if (mp)
	PrtMsg(mp, pn);
    if (f)
	(void) (*f)();
    (void) LTlibClean();
    exit(xv);
}


/*
 * RdFrLsof() -- read from lsof
 */

LTfldo_t *
RdFrLsof(nf, em)
    int *nf;				/* number of fields receiver */
    char **em;				/* error message pointer receiver */
{
    char buf[2048];			/* temporary buffer */
    int bufl = (int)sizeof(buf);	/* size of buf[] */
    char *blim = &buf[bufl - 1];	/* buf[] limit (last character
					 * address) */
    char *fsp;				/* field start pointer */
    char *tcp;				/* temporary character pointer */
    LTfldo_t *tfop;			/* temporary field output pointer */
    int ti;				/* temporary index */
    int tlen;				/* remporary length */
    char *vp;				/* value character pointer */
/*
 * Check for errors.
 */
    if (!em)
	return((LTfldo_t *)NULL);
    if (!nf) {
	*em = "ERROR!!!  RdFrLsof() not given a count return pointer";
	return((LTfldo_t *)NULL);
    }
    *em = (char *)NULL;
    *nf = 0;
/*
 * If fields are in use, release their resources.
 */
    for (ti = 0, tfop = Fo; (ti < Ufo); ti++, tfop++) {
	if (tfop->v)
	    (void) free((void *)tfop->v);
    }
    Ufo = 0;
/*
 * Read a line from lsof.
 */
    if (!fgets(buf, bufl - 2, LsofFs)) {

    /*
     * An lsof pipe EOF has been reached.  Indicate that with a NULL
     * pointer return, coupled with a NULL error message return pointer
     * (set above), and a field count of zero (set above).
     */
	return((LTfldo_t *)NULL);
    }
/*
 * Parse the lsof line, allocating field output structures as appropriate.
 *
 * It is expected that fields will end in a NUL ('\0') or a NL ('\0') and
 * that a NL ends all fields in the lsof line.
 */
    for (tcp = buf, Ufo = 0; (*tcp != '\n') && (tcp < blim); tcp++) {

    /*
     * Start a new field.  The first character is the LSOF_FID_*.
     *
     * First allocate an LTfldo_t structure.
     */
	if (Ufo >= Afo) {

	/*
	 * More LTfldo_t space is required.
	 */
	     Afo += LT_FLDO_ALLOC;
	     tlen = (int)(Afo * sizeof(LTfldo_t));
	     if (Fo)
		Fo = (LTfldo_t *)realloc(Fo, tlen);
	     else
		Fo = (LTfldo_t *)malloc(tlen);
	    if (!Fo) {

	    /*
	     * A serious error has occurred; no LTfldo_t space is available.
	     */
		(void) snprintf(buf, bufl,
		    "ERROR!!!  RdFrLsof() can't allocate %d pointer bytes",
		    tlen);
		*em = MkStrCpy(buf, &ti);
		*nf = -1;
		return((LTfldo_t *)NULL);
	    }
	}
	tfop = Fo + Ufo;
	tfop->v = (char *)NULL;
	Ufo++;
    /*
     * Save the LSOF_FID_* character.  Then compute the field value length,
     * and make a copy of it.
     */
	tfop->ft = *tcp++;
	fsp = tcp;
	tlen = 0;
	while (*tcp && (*tcp != '\n') && (tcp < blim)) {
	     tcp++;
	     tlen++;
	}
	if (!(vp = (char *)malloc(tlen + 1))) {

	/*
	 * A serious error has occurred; there's no space for the field value.
	 */
	    (void) snprintf(buf, bufl,
		"ERROR!!!  RdFrLsof() can't allocate %d field bytes", tlen + 1);
	    *em = MkStrCpy(buf, &ti);
	    *nf = -1;
	    return((LTfldo_t *)NULL);
	}
	(void) memcpy((void *)vp, (void *)fsp, tlen);
	vp[tlen] = '\0';
	tfop->v = vp;
	if (*tcp == '\n')
	    break;
	if (tcp >= blim) {

	/*
	 * The lsof line has no NL terminator; that's an error.
	 */
	    *em = "ERROR!!! RdFrLsof() didn't find a NL";
	    *nf = -1;
	    return((LTfldo_t *)NULL);
	}
    }
/*
 * The end of the lsof line has been reached.  If no fields were assembled,
 * return an error indicate.  Otherwise return the fields and their count.
 */
    if (!Ufo) {
	*em = "ERROR!!! RdFrLsof() read an empty lsof line";
	*nf = -1;
	return((LTfldo_t *)NULL);
    }
    *nf = Ufo;
    *em = (char *)NULL;
    return(Fo);
}


/*
 * ScanArg() -- scan arguments
 */

int
ScanArg(ac, av, opt, pn)
    int ac;				/* argument count */
    char *av[];				/* argument pointers */
    char *opt;				/* option string */
    char *pn;				/* program name */
{
    char *em;				/* pointer to error message returned by
					 * GetOpt() */
    char embf[2048];			/* error message buffer */
    int rv = 0;				/* return value */
    int tc;				/* temporary character value */
/*
 * Preset possible argument values.
 */
    LTopt_h = 0;
    if (LTopt_p) {
	(void) free((void *)LTopt_p);
	LTopt_p = (char *)NULL;
    }
/*
 * Process the options according to the supplied option string.
 */
    while ((tc = GetOpt(ac, av, opt, &em, pn)) != EOF) {
	if (em) {
	    rv = 1;
	    PrtMsg(em, pn);
	    continue;
	}
	switch (tc) {
	case 'h':
	    LTopt_h = 1;
	    break;
	case 'p':
	    if (!GOv || *GOv == '-' || *GOv == '+') {
		rv = 1;
		(void) PrtMsg("ERROR!!!  -p not followed by a path", pn);
	    } else
		LTopt_p = GOv;
	    break;
	default:
	    rv = 1;
	    (void) snprintf(embf, sizeof(embf) - 1,
		"ERROR!!!  unknown option: %c", tc);
	    PrtMsg(embf, pn);
	}
    }
    for (; GOx1 < ac; GOx1++) {

    /*
     * Report extraneous arguments.
     */
	rv = 1;
	(void) snprintf(embf, sizeof(embf) - 1,
	    "ERROR!!!  extraneous option: \"%s\"", av[GOx1]);
	PrtMsg(embf, pn);
    }
    return(rv);
}


/*
 * StopLsof() -- stop a running lsof process and close the pipe from it
 */

void
StopLsof()
{
    pid_t pid;

    if (LsofPid) {

    /*
     * An lsof child process may be active.  Wait for (or kill) it.
     */
	pid = wait3(NULL, WNOHANG, NULL);
	if (pid != LsofPid) {
	    (void) kill(LsofPid, SIGKILL);
	    sleep(2);
	    pid = wait3(NULL, WNOHANG, NULL);
	}
	LsofPid = (pid_t)0;
    }
    (void) closepipe();
}


/*
 * x2dev() -- convert hex string to device number
 */

static X2DEV_T
x2dev(x, em)
    char *x;				/* hex string */
    char **em;				/* error message receiver */
{
    char buf[2048];			/* temporary message buffer */
    int c;				/* character holder */
    X2DEV_T dev;			/* device number result */
    char *wx;				/* working hex string pointer */
    int xl;				/* hex string length */

    if (!x || !*x) {
	*em = "ERROR!!!  no hex string supplied to x2dev()";
	return(0);
    }
    wx = strncasecmp(x, "0x", 2) ? x : (x + 2);
    if (((xl = (int)strlen(wx)) < 1) || (xl > XDINDEV)) {
	(void) snprintf(buf, sizeof(buf) - 1,
	    "ERROR!!!  x2dev(\"%s\") bad length: %d", x, xl + 2);
	buf[sizeof(buf) - 1] = '\0';
	*em = MkStrCpy(buf, &c);
	return(0);
    }
/*
 * Assemble the device number result from the hex string.
 */
    for (dev = (X2DEV_T)0; *wx; wx++) {
	if (isdigit((unsigned char)*wx)) {
	    dev = (dev << 4) | (unsigned int)(((int)*wx - (int)'0') & 0xf);
	    continue;
	}
	c = (int) tolower((unsigned char)*wx);
	if ((c >= (int)'a') && (c <= (int)'f')) {
	    dev = (dev << 4) | (unsigned int)((c - 'a' + 10) & 0xf);
	    continue;
	}
	(void) snprintf(buf, sizeof(buf) - 1,
	    "ERROR!!!  x2dev(\"%s\") non-hex character: %c", x, c);
	*em = MkStrCpy(buf, &c);
    }
/*
 * Return result and no error indication.
 */
    *em = (char *)NULL;
    return(dev);
}
