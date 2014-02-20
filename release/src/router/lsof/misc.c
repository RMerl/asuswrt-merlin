/*
 * misc.c - common miscellaneous functions for lsof
 */


/*
 * Copyright 1994 Purdue Research Foundation, West Lafayette, Indiana
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

#ifndef lint
static char copyright[] =
"@(#) Copyright 1994 Purdue Research Foundation.\nAll rights reserved.\n";
static char *rcsid = "$Id: misc.c,v 1.26 2008/10/21 16:21:41 abe Exp $";
#endif


#include "lsof.h"

#if	defined(HASWIDECHAR) && defined(WIDECHARINCL)
#include WIDECHARINCL
#endif	/* defined(HASWIDECHAR) && defined(WIDECHARINCL) */


/*
 * Local definitions
 */

#if	!defined(MAXSYMLINKS)
#define	MAXSYMLINKS	32
#endif	/* !defined(MAXSYMLINKS) */


/*
 * Local function prototypes
 */

_PROTOTYPE(static void closePipes,(void));
_PROTOTYPE(static int dolstat,(char *path, char *buf, int len));
_PROTOTYPE(static int dostat,(char *path, char *buf, int len));
_PROTOTYPE(static int doreadlink,(char *path, char *buf, int len));
_PROTOTYPE(static int doinchild,(int (*fn)(), char *fp, char *rbuf, int rbln));

#if	defined(HASINTSIGNAL)
_PROTOTYPE(static int handleint,(int sig));
#else	/* !defined(HASINTSIGNAL) */
_PROTOTYPE(static void handleint,(int sig));
#endif	/* defined(HASINTSIGNAL) */

_PROTOTYPE(static char *safepup,(unsigned int c, int *cl));


/*
 * Local variables
 */

static pid_t Cpid = 0;			/* child PID */
static jmp_buf Jmp_buf;			/* jump buffer */
static int Pipes[] =			/* pipes for child process */
	{ -1, -1, -1, -1 };
static int CtSigs[] = { 0, SIGINT, SIGKILL };
					/* child termination signals (in order
					 * of application) -- the first is a
					 * dummy to allow pipe closure to
					 * cause the child to exit */
#define	NCTSIGS	(sizeof(CtSigs) / sizeof(int))


#if	defined(HASNLIST)
/*
 * build-Nl() - build kernel name list table
 */

static struct drive_Nl *Build_Nl = (struct drive_Nl *)NULL;
					/* the default Drive_Nl address */

void
build_Nl(d)
	struct drive_Nl *d;		/* data to drive the construction */
{
	struct drive_Nl *dp;
	int i, n;

	for (dp = d, n = 0; dp->nn; dp++, n++)
	    ;
	if (n < 1) {
	    (void) fprintf(stderr,
		"%s: can't calculate kernel name list length\n", Pn);
	    Exit(1);
	}
	if (!(Nl = (struct NLIST_TYPE *)calloc((n + 1),
					       sizeof(struct NLIST_TYPE))))
	{
	    (void) fprintf(stderr,
		"%s: can't allocate %d bytes to kernel name list structure\n",
		Pn, (int)((n + 1) * sizeof(struct NLIST_TYPE)));
	    Exit(1);
	}
	for (dp = d, i = 0; i < n; dp++, i++) {
	    Nl[i].NL_NAME = dp->knm;
	}
	Nll = (int)((n + 1) * sizeof(struct NLIST_TYPE));
	Build_Nl = d;
}
#endif	/* defined(HASNLIST) */


/*
 * childx() - make child process exit (if possible)
 */

void
childx()
{
	static int at, sx;
	pid_t wpid;

	if (Cpid > 1) {

	/*
	 * First close the pipes to and from the child.  That should cause the
	 * child to exit.  Compute alarm time shares.
	 */
	    (void) closePipes();
	    if ((at = TmLimit / NCTSIGS) < TMLIMMIN)
		at = TMLIMMIN;
	/*
	 * Loop, waiting for the child to exit.  After the first pass, help
	 * the child exit by sending it signals.
	 */
	    for (sx = 0; sx < NCTSIGS; sx++) {
		if (setjmp(Jmp_buf)) {

		/*
		 * An alarm has rung.  Disable further alarms.
		 *
		 * If there are more signals to send, continue the signal loop.
		 *
		 * If the last signal has been sent, issue a warning (unless
		 * warninge have been suppressed) and exit the signal loop.
		 */
		    (void) alarm(0);
		    (void) signal(SIGALRM, SIG_DFL);
		    if (sx < (NCTSIGS - 1))
			continue;
		    if (!Fwarn)
			(void) fprintf(stderr,
			    "%s: WARNING -- child process %d may be hung.\n",
			    Pn, (int)Cpid);
		    break;
	        }
	    /*
	     * Send the next signal to the child process, after the first pass
	     * through the loop.
	     *
	     * Wrap the wait() with an alarm.
	     */
		if (sx)
		    (void) kill(Cpid, CtSigs[sx]);
		(void) signal(SIGALRM, handleint);
		(void) alarm(at);
		wpid = (pid_t) wait(NULL);
		(void) alarm(0);
		(void) signal(SIGALRM, SIG_DFL);
		if (wpid == Cpid)
		    break;
	    }
	    Cpid = 0;
	}
}


/*
 * closePipes() - close open pipe file descriptors
 */

static void
closePipes()
{
	int i;

	for (i = 0; i < 4; i++) {
	    if (Pipes[i] >= 0) {
		(void) close(Pipes[i]);
		Pipes[i] = -1;
	    }
	}
}


/*
 * compdev() - compare Devtp[] entries
 */

int
compdev(a1, a2)
	COMP_P *a1, *a2;
{
	struct l_dev **p1 = (struct l_dev **)a1;
	struct l_dev **p2 = (struct l_dev **)a2;

	if ((dev_t)((*p1)->rdev) < (dev_t)((*p2)->rdev))
	    return(-1);
	if ((dev_t)((*p1)->rdev) > (dev_t)((*p2)->rdev))
	    return(1);
	if ((INODETYPE)((*p1)->inode) < (INODETYPE)((*p2)->inode))
	    return(-1);
	if ((INODETYPE)((*p1)->inode) > (INODETYPE)((*p2)->inode))
	    return(1);
	return(strcmp((*p1)->name, (*p2)->name));
}


/*
 * doinchild() -- do a function in a child process
 */

static int
doinchild(fn, fp, rbuf, rbln)
	int (*fn)();			/* function to perform */
	char *fp;			/* function parameter */
	char *rbuf;			/* response buffer */
	int rbln;			/* response buffer length */
{
	int en, rv;
/*
 * Check reply buffer size.
 */
	if (!Fovhd && rbln > MAXPATHLEN) {
	    (void) fprintf(stderr,
		"%s: doinchild error; response buffer too large: %d\n",
		Pn, rbln);
	    Exit(1);
	}
/*
 * Set up to handle an alarm signal; handle an alarm signal; build
 * pipes for exchanging information with a child process; start the
 * child process; and perform functions in the child process.
 */
	if (!Fovhd) {
	    if (setjmp(Jmp_buf)) {

	    /*
	     * Process an alarm that has rung.
	     */
		(void) alarm(0);
		(void) signal(SIGALRM, SIG_DFL);
		(void) childx();
		errno = ETIMEDOUT;
		return(1);
	    } else if (!Cpid) {

	    /*
	     * Create pipes to exchange function information with a child
	     * process.
	     */
		if (pipe(Pipes) < 0 || pipe(&Pipes[2]) < 0) {
		    (void) fprintf(stderr, "%s: can't open pipes: %s\n",
			Pn, strerror(errno));
		    Exit(1);
		}
	    /*
	     * Fork a child to execute functions.
	     */
		if ((Cpid = fork()) == 0) {

		/*
		 * Begin the child process.
		 */

		    int fd, nd, r_al, r_rbln;
		    char r_arg[MAXPATHLEN+1], r_rbuf[MAXPATHLEN+1];
		    int (*r_fn)();
		/*
		 * Close all open file descriptors except Pipes[0] and
		 * Pipes[3].
		 */
		    for (fd = 0, nd = GET_MAX_FD(); fd < nd; fd++) {
			if (fd == Pipes[0] || fd == Pipes[3])
			    continue;
			(void) close(fd);
			if (fd == Pipes[1])
			    Pipes[1] = -1;
			else if (fd == Pipes[2])
			    Pipes[2] = -1;
		    }
		    if (Pipes[1] >= 0) {
			(void) close(Pipes[1]);
			Pipes[1] = -1;
		    }
		    if (Pipes[2] >= 0) {
			(void) close(Pipes[2]);
			Pipes[2] = -1;
		    }
		/*
		 * Read function requests, process them, and return replies.
		 */
		    for (;;) {
			if (read(Pipes[0], (char *)&r_fn, sizeof(r_fn))
			    != (int)sizeof(r_fn)
			||  read(Pipes[0], (char *)&r_al, sizeof(int))
			    != (int)sizeof(int)
			||  r_al < 1
			||  r_al > (int)sizeof(r_arg)
			||  read(Pipes[0], r_arg, r_al) != r_al
			||  read(Pipes[0], (char *)&r_rbln, sizeof(r_rbln))
			    != (int)sizeof(r_rbln)
			||  r_rbln < 1 || r_rbln > (int)sizeof(r_rbuf))
			    break;
			rv = r_fn(r_arg, r_rbuf, r_rbln);
			en = errno;
			if (write(Pipes[3], (char *)&rv, sizeof(rv))
			    != sizeof(rv)
			||  write(Pipes[3], (char *)&en, sizeof(en))
			    != sizeof(en)
			||  write(Pipes[3], r_rbuf, r_rbln) != r_rbln)
			    break;
		    }
		    (void) _exit(0);
		}
	    /*
	     * Continue in the parent process to finish the setup.
	     */
		if (Cpid < 0) {
		    (void) fprintf(stderr, "%s: can't fork: %s\n",
			Pn, strerror(errno));
		    Exit(1);
		}
		(void) close(Pipes[0]);
		(void) close(Pipes[3]);
		Pipes[0] = Pipes[3] = -1;
	    }
	}
	if (!Fovhd) {
	    int len;

	/*
	 * Send a function to the child and wait for the response.
	 */
	    len  = strlen(fp) + 1;
	    (void) signal(SIGALRM, handleint);
	    (void) alarm(TmLimit);
	    if (write(Pipes[1], (char *)&fn, sizeof(fn)) != sizeof(fn)
	    ||  write(Pipes[1], (char *)&len, sizeof(len)) != sizeof(len)
	    ||  write(Pipes[1], fp, len) != len
	    ||  write(Pipes[1], (char *)&rbln, sizeof(rbln)) != sizeof(rbln)
	    ||  read(Pipes[2], (char *)&rv, sizeof(rv)) != sizeof(rv)
	    ||  read(Pipes[2], (char *)&en, sizeof(en)) != sizeof(en)
	    ||  read(Pipes[2], rbuf, rbln) != rbln) {
		(void) alarm(0);
		(void) signal(SIGALRM, SIG_DFL);
		(void) childx();
		errno = ECHILD;
		return(-1);
	    }
	} else {

	/*
	 * Do the operation directly -- not in a child.
	 */
	    (void) signal(SIGALRM, handleint);
	    (void) alarm(TmLimit);
	    rv = fn(fp, rbuf, rbln);
	    en = errno;
	}
/*
 * Function completed, response collected -- complete the operation.
 */
	(void) alarm(0);
	(void) signal(SIGALRM, SIG_DFL);
	errno = en;
	return(rv);
}


/*
 * dolstat() - do an lstat() function
 */

static int
dolstat(path, rbuf, rbln)
	char *path;			/* path */
	char *rbuf;			/* response buffer */
	int rbln;			/* response buffer length */

/* ARGSUSED */

{
	return(lstat(path, (struct stat *)rbuf));
}


/*
 * doreadlink() -- do a readlink() function
 */

static int
doreadlink(path, rbuf, rbln)
	char *path;			/* path */
	char *rbuf;			/* response buffer */
	int rbln;			/* response buffer length */
{
	return(readlink(path, rbuf, rbln));
}


/*
 * dostat() - do a stat() function
 */

static int
dostat(path, rbuf, rbln)
	char *path;			/* path */
	char *rbuf;			/* response buffer */
	int rbln;			/* response buffer length */

/* ARGSUSED */

{
	return(stat(path, (struct stat *)rbuf));
}


#if	defined(WILLDROPGID)
/*
 * dropgid() - drop setgid permission
 */

void
dropgid()
{
	if (!Setuidroot && Setgid) {
	    if (setgid(Mygid) < 0) {
		(void) fprintf(stderr, "%s: can't setgid(%d): %s\n",
		    Pn, (int)Mygid, strerror(errno));
		Exit(1);
	    }
	    Setgid = 0;
	}
}
#endif	/* defined(WILLDROPGID) */


/*
 * enter_dev_ch() - enter device characters in file structure
 */

void
enter_dev_ch(m)
	char *m;
{
	char *mp;

	if (!m || *m == '\0')
	    return;
	if (!(mp = mkstrcpy(m, (MALLOC_S *)NULL))) {
	    (void) fprintf(stderr, "%s: no more dev_ch space at PID %d: \n",
		Pn, Lp->pid);
	    safestrprt(m, stderr, 1);
	    Exit(1);
	}
	if (Lf->dev_ch)
	   (void) free((FREE_P *)Lf->dev_ch);
	Lf->dev_ch = mp;
}


/*
 * enter_IPstate() -- enter a TCP or UDP state
 */

void
enter_IPstate(ty, nm, nr)
	char *ty;			/* type -- TCP or UDP */
	char *nm;			/* state name (may be NULL) */
	int nr;				/* state number */
{

#if	defined(USE_LIB_PRINT_TCPTPI)
	TcpNstates = nr;
#else	/* !defined(USE_LIB_PRINT_TCPTPI) */

	int al, i, j, oc, nn, ns, off, tx;
	char *cp;
	MALLOC_S len;
/*
 * Check the type name and set the type index.
 */
	if (!ty) {
	    (void) fprintf(stderr,
		"%s: no type specified to enter_IPstate()\n", Pn);
	    Exit(1);
	}
	if (!strcmp(ty, "TCP"))
	    tx = 0;
	else if (!strcmp(ty, "UDP"))
	    tx = 1;
	else {
	    (void) fprintf(stderr, "%s: unknown type for enter_IPstate: %s\n",
		Pn, ty);
	    Exit(1);
	}
/*
 * If the name argument is NULL, reduce the allocated table to its minimum
 * size.
 */
	if (!nm) {
	    if (tx) {
		if (UdpSt) {
		    if (!UdpNstates) {
			(void) free((MALLOC_P *)UdpSt);
			UdpSt = (char **)NULL;
		    }
		    if (UdpNstates < UdpStAlloc) {
			len = (MALLOC_S)(UdpNstates * sizeof(char *));
			if (!(UdpSt = (char **)realloc((MALLOC_P *)UdpSt, len)))
			{
			    (void) fprintf(stderr,
				"%s: can't reduce UdpSt[]\n", Pn);
			    Exit(1);
			}
		    }
		    UdpStAlloc = UdpNstates;
		}
	    } else {
		if (TcpSt) {
		    if (!TcpNstates) {
			(void) free((MALLOC_P *)TcpSt);
			TcpSt = (char **)NULL;
		    }
		    if (TcpNstates < TcpStAlloc) {
			len = (MALLOC_S)(TcpNstates * sizeof(char *));
			if (!(TcpSt = (char **)realloc((MALLOC_P *)TcpSt, len)))
			{
			    (void) fprintf(stderr,
				"%s: can't reduce TcpSt[]\n", Pn);
			    Exit(1);
			}
		    }
		    TcpStAlloc = TcpNstates;
		}
	    }
	    return;
	}
/*
 * Check the name and number.
 */
	if ((len = (size_t)strlen(nm)) < 1) {
	    (void) fprintf(stderr,
		"%s: bad %s name (\"%s\"), number=%d\n", Pn, ty, nm, nr);
	    Exit(1);
	}
/*
 * Make a copy of the name.
 */
	if (!(cp = mkstrcpy(nm, (MALLOC_S *)NULL))) {
	    (void) fprintf(stderr,
		"%s: enter_IPstate(): no %s space for %s\n",
		Pn, ty, nm);
	    Exit(1);
	}
/*
 * Set the necessary offset for using nr as an index.  If it is
 * a new offset, adjust previous entries.
 */
	if ((nr < 0) && ((off = -nr) > (tx ? UdpStOff : TcpStOff))) {
	    if (tx ? UdpSt : TcpSt) {

	    /*
	     * A new, larger offset (smaller negative state number) could mean
	     * a previously allocated state table must be enlarged and its
	     * previous entries moved.
	     */
		oc = off - (tx ? UdpStOff : TcpStOff);
		al = tx ? UdpStAlloc : TcpStAlloc;
		ns = tx ? UdpNstates : TcpNstates;
		if ((nn = ns + oc) >= al) {
		    while ((nn + 5) > al) {
			al += TCPUDPALLOC;
		    }
		    len = (MALLOC_S)(al * sizeof(char *));
		    if (tx) {
			if (!(UdpSt = (char **)realloc((MALLOC_P *)UdpSt, len)))
			    goto no_IP_space;
			UdpStAlloc = al;
		    } else {
			if (!(TcpSt = (char **)realloc((MALLOC_P *)TcpSt, len)))
			    goto no_IP_space;
			TcpStAlloc = al;
		    }
		    for (i = 0, j = oc; i < oc; i++, j++) {
			if (tx) {
			    if (i < UdpNstates)
				UdpSt[j] = UdpSt[i];
			    UdpSt[i] = (char *)NULL;
			} else {
			    if (i < TcpNstates)
				TcpSt[j] = TcpSt[i];
			    TcpSt[i] = (char *)NULL;
			}
		    }
		    if (tx)
			UdpNstates += oc;
		    else
			TcpNstates += oc;
		}
	    }
	    if (tx)
		UdpStOff = off;
	    else
		TcpStOff = off;
	}
/*
 * Enter name as {Tc|Ud}pSt[nr + {Tc|Ud}pStOff].
 *
 * Allocate space, as required.
 */
	al = tx ? UdpStAlloc : TcpStAlloc;
	off = tx ? UdpStOff : TcpStOff;
	nn = nr + off + 1;
	if (nn > al) {
	    i = tx ? UdpNstates : TcpNstates;
	    while ((nn + 5) > al) {
		al += TCPUDPALLOC;
	    }
	    len = (MALLOC_S)(al * sizeof(char *));
	    if (tx) {
		if (UdpSt)
		    UdpSt = (char **)realloc((MALLOC_P *)UdpSt, len);
		else
		    UdpSt = (char **)malloc(len);
		if (!UdpSt) {

no_IP_space:

		    (void) fprintf(stderr, "%s: no %s state space\n", Pn, ty);
		    Exit(1);
		}
		UdpNstates = nn;
		UdpStAlloc = al;
	    } else {
		if (TcpSt)
		    TcpSt = (char **)realloc((MALLOC_P *)TcpSt, len);
		else
		    TcpSt = (char **)malloc(len);
		if (!TcpSt)
		    goto no_IP_space;
		TcpNstates = nn;
		TcpStAlloc = al;
	    }
	    while (i < al) {
		if (tx)
		    UdpSt[i] = (char *)NULL;
		else
		    TcpSt[i] = (char *)NULL;
		i++;
	    }
	} else {
	    if (tx) {
		if (nn > UdpNstates)
		    UdpNstates = nn;
	    } else {
		if (nn > TcpNstates)
		    TcpNstates = nn;
	    }
	}
	if (tx) {
	    if (UdpSt[nr + UdpStOff]) {

dup_IP_state:

		(void) fprintf(stderr,
		    "%s: duplicate %s state %d (already %s): %s\n",
		    Pn, ty, nr,
		    tx ? UdpSt[nr + UdpStOff] : TcpSt[nr + TcpStOff],
		    nm);
	 	Exit(1);
	    }
	    UdpSt[nr + UdpStOff] = cp;
	} else {
	    if (TcpSt[nr + TcpStOff])
		goto dup_IP_state;
	    TcpSt[nr + TcpStOff] = cp;
	}
#endif	/* defined(USE_LIB_PRINT_TCPTPI) */

}


/*
 * enter_nm() - enter name in local file structure
 */

void
enter_nm(m)
	char *m;
{
	char *mp;

	if (!m || *m == '\0')
	    return;
	if (!(mp = mkstrcpy(m, (MALLOC_S *)NULL))) {
	    (void) fprintf(stderr, "%s: no more nm space at PID %d for: ",
		Pn, Lp->pid);
	    safestrprt(m, stderr, 1);
	    Exit(1);
	}
	if (Lf->nm)
	    (void) free((FREE_P *)Lf->nm);
	Lf->nm = mp;
}


/*
 * Exit() - do a clean exit()
 */

void
Exit(xv)
	int xv;				/* exit() value */
{
	(void) childx();

#if	defined(HASDCACHE)
	if (DCrebuilt && !Fwarn)
	    (void) fprintf(stderr, "%s: WARNING: %s was updated.\n",
		Pn, DCpath[DCpathX]);
#endif	/* defined(HASDCACHE) */

	exit(xv);
}


#if	defined(HASNLIST)
/*
 * get_Nl_value() - get Nl value for nickname
 */

int
get_Nl_value(nn, d, v)
	char *nn;			/* nickname of requested entry */
	struct drive_Nl *d;		/* drive_Nl table that built Nl
					 * (if NULL, use Build_Nl) */
	KA_T *v;			/* returned value (if NULL,
					 * return nothing) */
{
	int i;

	if (!Nl || !Nll)
	    return(-1);
	if (!d)
	    d = Build_Nl;
	for (i = 0; d->nn; d++, i++) {
	    if (strcmp(d->nn, nn) == 0) {
		if (v)
		    *v = (KA_T)Nl[i].n_value;
		return(i);
	    }
	}
	return(-1);
}
#endif	/* defined(HASNLIST) */


/*
 * handleint() - handle an interrupt
 */

#if	defined(HASINTSIGNAL)
static int
#else
static void
#endif

/* ARGSUSED */

handleint(sig)
	int sig;
{
	longjmp(Jmp_buf, 1);
}


/*
 * hashbyname() - hash by name
 */

int
hashbyname(nm, mod)
	char *nm;			/* pointer to NUL-terminated name */
	int mod;			/* hash modulus */
{
	int i, j;

	for (i = j = 0; *nm; nm++) {
	    i ^= (int)*nm << j;
	    if (++j > 7)
		j = 0;
	}
	return(((int)(i * 31415)) & (mod - 1));
}


/*
 * is_nw_addr() - is this network address selected?
 */

int
is_nw_addr(ia, p, af)
	unsigned char *ia;		/* Internet address */
	int p;				/* port */
	int af;				/* address family -- e.g., AF_INET,
					 * AF_INET6 */
{
	struct nwad *n;

	if (!(n = Nwad))
	    return(0);
	for (; n; n = n->next) {
	    if (n->proto) {
		if (strcasecmp(n->proto, Lf->iproto) != 0)
		    continue;
	    }
	    if (af && n->af && af != n->af)
		continue;

#if	defined(HASIPv6)
	    if (af == AF_INET6) {
		if (n->a[15] || n->a[14] || n->a[13] || n->a[12]
		||  n->a[11] || n->a[10] || n->a[9]  || n->a[8]
		||  n->a[7]  || n->a[6]  || n->a[5]  || n->a[4]
		||  n->a[3]  || n->a[2]  || n->a[1]  || n->a[0]) {
		    if (ia[15] != n->a[15] || ia[14] != n->a[14]
		    ||  ia[13] != n->a[13] || ia[12] != n->a[12]
		    ||  ia[11] != n->a[11] || ia[10] != n->a[10]
		    ||  ia[9]  != n->a[9]  || ia[8]  != n->a[8]
		    ||  ia[7]  != n->a[7]  || ia[6]  != n->a[6]
		    ||  ia[5]  != n->a[5]  || ia[4]  != n->a[4]
		    ||  ia[3]  != n->a[3]  || ia[2]  != n->a[2]
		    ||  ia[1]  != n->a[1]  || ia[0]  != n->a[0])
			continue;
		}
	    } else if (af == AF_INET)
#endif	/* defined(HASIPv6) */

	    {
		if (n->a[3] || n->a[2] || n->a[1] || n->a[0]) {
		    if (ia[3] != n->a[3] || ia[2] != n->a[2]
		    ||  ia[1] != n->a[1] || ia[0] != n->a[0])
			continue;
		}
	    }

#if	defined(HASIPv6)
	    else
		continue;
#endif	/* defined(HASIPv6) */

	    if (n->sport == -1 || (p >= n->sport && p <= n->eport)) {
		n->f = 1;
		return(1);
	    }
	}
	return(0);
}


/*
 * mkstrcpy() - make a string copy in malloc()'d space
 *
 * return: copy pointer
 *	   copy length (optional)
 */

char *
mkstrcpy(src, rlp)
	char *src;			/* source */
	MALLOC_S *rlp;			/* returned length pointer (optional)
					 * The returned length is an strlen()
					 * equivalent */
{
	MALLOC_S len;
	char *ns;

	len = (MALLOC_S)(src ? strlen(src) : 0);
	ns = (char *)malloc(len + 1);
	if (ns) {
	    if (src)
		(void) snpf(ns, len + 1, "%s", src);
	    else
		*ns = '\0';
	}
	if (rlp)
	    *rlp = len;
	return(ns);
}


/*
 * mkstrcat() - make a catenated copy of up to three strings under optional
 *		string-by-string count control
 *
 * return: copy pointer
 *	   copy string length (optional)
 */

char *
mkstrcat(s1, l1, s2, l2, s3, l3, clp)
	char *s1;			/* source string 1 */
	int l1;				/* length of string 1 (-1 if none) */
	char *s2;			/* source string 2 */
	int l2;				/* length of string 2 (-1 if none) */
	char *s3;			/* source string 3 (optional) */
	int l3	;			/* length of string 3 (-1 if none) */
	MALLOC_S *clp;			/* pointer to return of copy length
					 * (optional) */
{
	MALLOC_S cl, len1, len2, len3;
	char *cp;

	if (s1)
	    len1 = (MALLOC_S)((l1 >= 0) ? l1 : strlen(s1));
	else
	    len1 = (MALLOC_S)0;
	if (s2)
	    len2 = (MALLOC_S)((l2 >= 0) ? l2 : strlen(s2));
	else
	    len2 = (MALLOC_S)0;
	if (s3)
	    len3 = (MALLOC_S)((l3 >= 0) ? l3 : strlen(s3));
	else
	    len3 = (MALLOC_S)0;
	cl = len1 + len2 + len3;
	if ((cp = (char *)malloc(cl + 1))) {
	    char *tp = cp;

	    if (s1 && len1) {
		(void) strncpy(tp, s1, len1);
		tp += len1;
	    }
	    if (s2 && len2) {
		(void) strncpy(tp, s2, len2);
		tp += len2;
	    }
	    if (s3 && len3) {
		(void) strncpy(tp, s3, len3);
		tp += len3;
	    }
	    *tp = '\0';
	}
	if (clp)
	    *clp = cl;
	return(cp);
}


/*
 * is_readable() -- is file readable
 */

int
is_readable(path, msg)
	char *path;			/* file path */
	int msg;			/* issue warning message if 1 */
{
	if (access(path, R_OK) < 0) {
	    if (!Fwarn && msg == 1)
		(void) fprintf(stderr, ACCESSERRFMT, Pn, path, strerror(errno));
	    return(0);
	}
	return(1);
}


/*
 * lstatsafely() - lstat path safely (i. e., with timeout)
 */

int
lstatsafely(path, buf)
	char *path;			/* file path */
	struct stat *buf;		/* stat buffer address */
{
	if (Fblock) {
	    if (!Fwarn) 
		(void) fprintf(stderr,
		    "%s: avoiding stat(%s): -b was specified.\n",
		    Pn, path);
	    errno = EWOULDBLOCK;
	    return(1);
	}
	return(doinchild(dolstat, path, (char *)buf, sizeof(struct stat)));
}


/*
 * Readlink() - read and interpret file system symbolic links
 */

char *
Readlink(arg)
	char *arg;			/* argument to be interpreted */
{
	char abuf[MAXPATHLEN+1];
	int alen;
	char *ap;
	char *argp1, *argp2;
	int i, len, llen, slen;
	char lbuf[MAXPATHLEN+1];
	static char *op = (char *)NULL;
	static int ss = 0;
	char *s1;
	static char **stk = (char **)NULL;
	static int sx = 0;
	char tbuf[MAXPATHLEN+1];
/*
 * See if avoiding kernel blocks.
 */
	if (Fblock) {
	    if (!Fwarn) {
		(void) fprintf(stderr, "%s: avoiding readlink(", Pn);
		safestrprt(arg, stderr, 0);
		(void) fprintf(stderr, "): -b was specified.\n");
	    }
	    op = (char *)NULL;
	    return(arg);
	}
/*
 * Save the original path.
 */
	if (!op)
	    op = arg;
/*
 * Evaluate each component of the argument for a symbolic link.
 */
	for (alen = 0, ap = abuf, argp1 = argp2 = arg; *argp2; argp1 = argp2 ) {
	    for (argp2 = argp1 + 1; *argp2 && *argp2 != '/'; argp2++)
		;
	    if ((len = argp2 - arg) >= (int)sizeof(tbuf)) {

path_too_long:
		if (!Fwarn) {
		    (void) fprintf(stderr,
			"%s: readlink() path too long: ", Pn);
		    safestrprt(op ? op : arg, stderr, 1);
		}
		op = (char *)NULL;
		return((char *)NULL);
	    }
	    (void) strncpy(tbuf, arg, len);
	    tbuf[len] = '\0';
	/*
	 * Dereference a symbolic link.
	 */
	    if ((llen=doinchild(doreadlink,tbuf,lbuf,sizeof(lbuf) - 1)) >= 0) {

	    /*
	     * If the link is a new absolute path, replace
	     * the previous assembly with it.
	     */
		if (lbuf[0] == '/') {
		    (void) strncpy(abuf, lbuf, llen);
		    ap = &abuf[llen];
		    *ap = '\0';
		    alen = llen;
		    continue;
		}
		lbuf[llen] = '\0';
		s1 = lbuf;
	    } else {
		llen = argp2 - argp1;
		s1 = argp1;
	    }
	/*
	 * Make sure two components are separated by a `/'.
	 *
	 * If the first component is not a link, don't force
	 * a leading '/'.
	 *
	 * If the first component is a link and the source of
	 * the link has a leading '/', force a leading '/'.
	 */
	    if (*s1 == '/')
		slen = 1;
	    else {
		if (alen > 0) {

		/*
		 * This is not the first component.
		 */
		    if (abuf[alen - 1] == '/')
			slen = 1;
		    else
			slen = 2;
		} else {

		/*
		 * This is the first component.
		 */
		    if (s1 == lbuf && tbuf[0] == '/')
			slen = 2;
		    else
			slen = 1;
		}
	    }
	/*
	 * Add to the path assembly.
	 */
	    if ((alen + llen + slen) >= (int)sizeof(abuf))
		goto path_too_long;
	    if (slen == 2)
		*ap++ = '/';
	    (void) strncpy(ap, s1, llen);
	    ap += llen;
	    *ap = '\0';
	    alen += (llen + slen - 1);
	}
/*
 * If the assembled path and argument are the same, free all but the
 * last string in the stack, and return the argument.
 */
	if (strcmp(arg, abuf) == 0) {
	    for (i = 0; i < sx; i++) {
		if (i < (sx - 1))
		    (void) free((FREE_P *)stk[i]);
		stk[i] = (char *)NULL;
	    }
	    sx = 0;
	    op = (char *)NULL;
	    return(arg);
	}
/*
 * If the assembled path and argument are different, add it to the
 * string stack, then Readlink() it.
 */
	if (!(s1 = mkstrcpy(abuf, (MALLOC_S *)NULL))) {

no_readlink_space:

	    (void) fprintf(stderr, "%s: no Readlink string space for ", Pn);
	    safestrprt(abuf, stderr, 1);
	    Exit(1);
	}
	if (sx >= MAXSYMLINKS) {

	/*
	 * If there are too many symbolic links, report an error, clear
	 * the stack, and return no path.
	 */
	    if (!Fwarn) {
		(void) fprintf(stderr,
		    "%s: too many (> %d) symbolic links in readlink() path: ",
			Pn, MAXSYMLINKS);
		safestrprt(op ? op : arg, stderr, 1);
	    }
	    for (i = 0; i < sx; i++) {
		(void) free((FREE_P *)stk[i]);
		stk[i] = (char *)NULL;
	    }
	    (void) free((FREE_P *)stk);
	    stk = (char **)NULL;
	    ss = sx = 0;
	    op = (char *)NULL;
	    return((char *)NULL);
	}
	if (++sx > ss) {
	    if (!stk)
		stk = (char **)malloc((MALLOC_S)(sizeof(char *) * sx));
	    else
		stk = (char **)realloc((MALLOC_P *)stk,
					(MALLOC_S)(sizeof(char *) * sx));
	    if (!stk)
		goto no_readlink_space;
	    ss = sx;
	}
	stk[sx - 1] = s1;
	return(Readlink(s1));
}


#if	defined(HASSTREAMS)
/*
 * readstdata() - read stream's stdata structure
 */

int
readstdata(addr, buf)
	KA_T addr;			/* stdata address in kernel*/
	struct stdata *buf;		/* buffer addess */
{
	if (!addr
	||  kread(addr, (char *)buf, sizeof(struct stdata))) {
	    (void) snpf(Namech, Namechl, "no stream data in %s",
		print_kptr(addr, (char *)NULL, 0));
	    return(1);
	}
	return(0);
}


/*
 * readsthead() - read stream head
 */

int
readsthead(addr, buf)
	KA_T addr;			/* starting queue pointer in kernel */
	struct queue *buf;		/* buffer for queue head */
{
	KA_T qp;

	if (!addr) {
	    (void) snpf(Namech, Namechl, "no stream queue head");
	    return(1);
	}
	for (qp = addr; qp; qp = (KA_T)buf->q_next) {
	    if (kread(qp, (char *)buf, sizeof(struct queue))) {
		(void) snpf(Namech, Namechl, "bad stream queue link at %s",
		    print_kptr(qp, (char *)NULL, 0));
		return(1);
	    }
	}
	return(0);
}


/*
 * readstidnm() - read stream module ID name
 */

int
readstidnm(addr, buf, len)
	KA_T addr;			/* module ID name address in kernel */
	char *buf;			/* receiving buffer address */
	READLEN_T len;			/* buffer length */
{
	if (!addr || kread(addr, buf, len)) {
	    (void) snpf(Namech, Namechl, "can't read module ID name from %s",
		print_kptr(addr, (char *)NULL, 0));
	    return(1);
	}
	return(0);
}


/*
 * readstmin() - read stream's module info
 */

int
readstmin(addr, buf)
	KA_T addr;			/* module info address in kernel */
	struct module_info *buf;	/* receiving buffer address */
{
	if (!addr || kread(addr, (char *)buf, sizeof(struct module_info))) {
	    (void) snpf(Namech, Namechl, "can't read module info from %s",
		print_kptr(addr, (char *)NULL, 0));
	    return(1);
	}
	return(0);
}


/*
 * readstqinit() - read stream's queue information structure
 */

int
readstqinit(addr, buf)
	KA_T addr;			/* queue info address in kernel */
	struct qinit *buf;		/* receiving buffer address */
{
	if (!addr || kread(addr, (char *)buf, sizeof(struct qinit))) {
	    (void) snpf(Namech, Namechl, "can't read queue info from %s",
		print_kptr(addr, (char *)NULL, 0));
	    return(1);
	}
	return(0);
}
#endif	/* HASSTREAMS */


/*
 * safepup() - safely print an unprintable character -- i.e., print it in a
 *	       printable form
 *
 * return: char * to printable equivalent
 *	   cl = strlen(printable equivalent)
 */

static char *
safepup(c, cl)
	unsigned int c;			/* unprintable (i.e., !isprint())
					 * character */
	int *cl;			/* returned printable strlen -- NULL if
					 * no return needed */
{
	int len;
	char *rp;
	static char up[8];

	if (c < 0x20) {
	    switch (c) {
	    case '\b':
		rp = "\\b";
		break;
	    case '\f':
		rp = "\\f";
		break;
	    case '\n':
		rp = "\\n";
		break;
	    case '\r':
		rp = "\\r";
		break;
	    case '\t':
		rp = "\\t";
		break;
	    default:
		(void) snpf(up, sizeof(up), "^%c", c + 0x40);
		rp = up;
	    }
	    len = 2;
	} else if (c == 0xff) {
	    rp = "^?";
	    len = 2;
	} else {
	    (void) snpf(up, sizeof(up), "\\x%02x", (int)(c & 0xff));
	    rp = up;
	    len = 4;
	}
	if (cl)
	    *cl = len;
	return(rp);
}


/*
 * safestrlen() - calculate a "safe" string length -- i.e., compute space for
 *		  non-printable characters when printed in a printable form
 */

int
safestrlen(sp, flags)
	char *sp;			/* string pointer */
	int flags;			/* flags:
					 *   bit 0: 0 (0) = no NL
					 *	    1 (1) = add trailing NL
					 *	 1: 0 (0) = ' ' printable
					 *	    1 (2) = ' ' not printable
					 */
{
	char c;
	int len = 0;

	c = (flags & 2) ? ' ' : '\0';
	if (sp) {
	    for (; *sp; sp++) {
		if (!isprint((unsigned char)*sp) || *sp == c) {
		    if (*sp < 0x20 || (unsigned char)*sp == 0xff)
			len += 2;		/* length of \. or ^. form */
		    else
			len += 4;		/* length of "\x%02x" printf */
		} else
		    len++;
	    }
	}
	return(len);
}


/*
 * safestrprt() - print a string "safely" to the indicated stream -- i.e.,
 *		  print unprintable characters in a printable form
 */

void
safestrprt(sp, fs, flags)
	char *sp;			/* string to print pointer pointer */
	FILE *fs;			/* destination stream -- e.g., stderr
					 * or stdout */
	int flags;			/* flags:
					 *   bit 0: 0 (0) = no NL
					 *	    1 (1) = add trailing NL
					 *	 1: 0 (0) = ' ' printable
					 *	    1 (2) = ' ' not printable
					 *	 2: 0 (0) = print string as is
					 *	    1 (4) = surround string
					 *		    with '"'
					 *	 4: 0 (0) = print ending '\n'
					 *	    1 (8) = don't print ending
					 *		    '\n'
					 */
{
	char c;
	int lnc, lnt, sl;

#if	defined(HASWIDECHAR)
	wchar_t w;
	int wcmx = MB_CUR_MAX;
#else	/* !defined(HASWIDECHAR) */
	static int wcmx = 1;
#endif	/* defined(HASWIDECHAR) */

	c = (flags & 2) ? ' ' : '\0';
	if (flags & 4)
	    putc('"', fs);
	if (sp) {
	    for (sl = strlen(sp); *sp; sl -= lnc, sp += lnc) {

#if	defined(HASWIDECHAR)
		if (wcmx > 1) {
		    lnc = mblen(sp, sl);
		    if (lnc > 1) {
			if ((mbtowc(&w, sp, sl) == lnc) && iswprint(w)) {
			    for (lnt = 0; lnt < lnc; lnt++) {
				putc((int)*(sp + lnt), fs);
			    }
			} else {
			    for (lnt = 0; lnt < lnc; lnt++) {
			        fputs(safepup((unsigned int)*(sp + lnt),
					      (int *)NULL), fs);
			    }
			}
			continue;
		    } else
			lnc = 1;
		} else
		    lnc = 1;
#else	/* !defined(HASWIDECHAR) */
		lnc = 1;
#endif	/* defined(HASWIDECHAR) */

		if (isprint((unsigned char)*sp) && *sp != c)
		    putc((int)(*sp & 0xff), fs);
		else {
		    if ((flags & 8) && (*sp == '\n') && !*(sp + 1))
			break;
		    fputs(safepup((unsigned int)*sp, (int *)NULL), fs);
		}
	    }
	}
	if (flags & 4)
	    putc('"', fs);
	if (flags & 1)
	    putc('\n', fs);
}


/*
 * safestrprtn() - print a specified number of characters from a string
 *		   "safely" to the indicated stream
 */

void
safestrprtn(sp, len, fs, flags)
	char *sp;			/* string to print pointer pointer */
	int len;			/* safe number of characters to
					 * print */
	FILE *fs;			/* destination stream -- e.g., stderr
					 * or stdout */
	int flags;			/* flags:
					 *   bit 0: 0 (0) = no NL
					 *	    1 (1) = add trailing NL
					 *	 1: 0 (0) = ' ' printable
					 *	    1 (2) = ' ' not printable
					 *	 2: 0 (0) = print string as is
					 *	    1 (4) = surround string
					 *		    with '"'
					 *	 4: 0 (0) = print ending '\n'
					 *	    1 (8) = don't print ending
					 *		    '\n'
					 */
{
	char c, *up;
	int cl, i;

	if (flags & 4)
	    putc('"', fs);
	if (sp) {
	    c = (flags & 2) ? ' ' : '\0';
	    for (i = 0; i < len && *sp; sp++) {
		if (isprint((unsigned char)*sp) && *sp != c) {
		    putc((int)(*sp & 0xff), fs);
		    i++;
		} else {
		    if ((flags & 8) && (*sp == '\n') && !*(sp + 1))
			break;
		    up = safepup((unsigned int)*sp, &cl);
		    if ((i + cl) > len)
			break;
		    fputs(up, fs);
		    i += cl;
		}
	    }
	} else
	    i = 0;
	for (; i < len; i++)
	    putc(' ', fs);
	if (flags & 4)
	    putc('"', fs);
	if (flags & 1)
	    putc('\n', fs);
}


/*
 * statsafely() - stat path safely (i. e., with timeout)
 */

int
statsafely(path, buf)
	char *path;			/* file path */
	struct stat *buf;		/* stat buffer address */
{
	if (Fblock) {
	    if (!Fwarn) 
		(void) fprintf(stderr,
		    "%s: avoiding stat(%s): -b was specified.\n",
		    Pn, path);
	    errno = EWOULDBLOCK;
	    return(1);
	}
	return(doinchild(dostat, path, (char *)buf, sizeof(struct stat)));
}


/*
 * stkdir() - stack directory name
 */

void
stkdir(p)
	char *p;		/* directory path */
{
	MALLOC_S len;
/*
 * Provide adequate space for directory stack pointers.
 */
	if (Dstkx >= Dstkn) {
	    Dstkn += 128;
	    len = (MALLOC_S)(Dstkn * sizeof(char *));
	    if (!Dstk)
		Dstk = (char **)malloc(len);
	    else
		Dstk = (char **)realloc((MALLOC_P *)Dstk, len);
	    if (!Dstk) {
		(void) fprintf(stderr,
		    "%s: no space for directory stack at: ", Pn);
		safestrprt(p, stderr, 1);
		Exit(1);
	    }
	}
/*
 * Allocate space for the name, copy it there and put its pointer on the stack.
 */
	if (!(Dstk[Dstkx] = mkstrcpy(p, (MALLOC_S *)NULL))) {
	    (void) fprintf(stderr, "%s: no space for: ", Pn);
	    safestrprt(p, stderr, 1);
	    Exit(1);
	}
	Dstkx++;
}


/*
 * x2dev() - convert hexadecimal ASCII string to device number
 */

char *
x2dev(s, d)
	char *s;			/* ASCII string */
	dev_t *d;			/* device receptacle */
{
	char *cp, *cp1;
	int n;
	dev_t r;

/*
 * Skip an optional leading 0x.  Count the number of hex digits up to the end
 * of the string, or to a space, or to a comma.  Return an error if an unknown
 * character is encountered.  If the count is larger than (2 * sizeof(dev_t))
 * -- e.g., because of sign extension -- ignore excess leading hex 0xf digits,
 * but return an error if an excess leading digit isn't 0xf.
 */
	if  (strncasecmp(s, "0x", 2) == 0)
		s += 2;
	for (cp = s, n = 0; *cp; cp++, n++) {
	    if (isdigit((unsigned char)*cp))
		continue;
	    if ((unsigned char)*cp >= 'a' && (unsigned char)*cp <= 'f')
		continue;
	    if ((unsigned char)*cp >= 'A' && (unsigned char)*cp <= 'F')
		continue;
	    if (*cp == ' ' || *cp == ',')
		break;
	    return((char *)NULL);
	}
	if (!n)
	    return((char *)NULL);
	if (n > (2 * (int)sizeof(dev_t))) {
	    cp1 = s;
	    s += (n - (2 * sizeof(dev_t)));
	    while (cp1 < s) {
		if (*cp1 != 'f' && *cp1 != 'F')
		    return((char *)NULL);
		cp1++;
	    }
	}
/*
 * Assemble the validated hex digits of the device number, starting at a point
 * in the string relevant to sizeof(dev_t).
 */
	for (r = 0; s < cp; s++) {
	    r = r << 4;
	    if (isdigit((unsigned char)*s))
		r |= (unsigned char)(*s - '0') & 0xf;
	    else {
		if (isupper((unsigned char)*s))
		    r |= ((unsigned char)(*s - 'A') + 10) & 0xf;
		else
		    r |= ((unsigned char)(*s - 'a') + 10) & 0xf;
	    }
	}
	*d = r;
	return(s);
}
