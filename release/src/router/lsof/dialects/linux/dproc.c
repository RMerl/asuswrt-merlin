/*
 * dproc.c - Linux process access functions for /proc-based lsof
 */


/*
 * Copyright 1997 Purdue Research Foundation, West Lafayette, Indiana
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
"@(#) Copyright 1997 Purdue Research Foundation.\nAll rights reserved.\n";
static char *rcsid = "$Id: dproc.c,v 1.22 2008/10/21 16:17:21 abe Exp $";
#endif

#include "lsof.h"


/*
 * Local definitions
 */

#define	FDINFO_FLAGS		1	/* fdinfo flags available */
#define	FDINFO_POS		2	/* fdinfo position available */
#define FDINFO_ALL		(FDINFO_FLAGS | FDINFO_POS)
#define	LSTAT_TEST_FILE		"/"
#define LSTAT_TEST_SEEK		1

#if	!defined(ULLONG_MAX)
#define	ULLONG_MAX		18446744073709551615ULL
#endif	/* !defined(ULLONG_MAX) */


/*
 * Local structures
 */

struct l_fdinfo {
	int flags;			/* flags: line value */
	off_t pos;			/* pos: line value */
};


/*
 * Local variables
 */

static short Cckreg;			/* conditional status of regular file
					 * checking:
					 *     0 = unconditionally check
					 *     1 = conditionally check */
static short Ckscko;			/* socket file only checking status:
					 *     0 = none
					 *     1 = check only socket files */


/*
 * Local function prototypes
 */

_PROTOTYPE(static int get_fdinfo,(char *p, struct l_fdinfo *fi));
_PROTOTYPE(static int getlinksrc,(char *ln, char *src, int srcl));
_PROTOTYPE(static int nm2id,(char *nm, int *id, int *idl));
_PROTOTYPE(static int read_id_stat,(int ty, char *p, int id, char **cmd,
				    int *ppid, int *pgid));
_PROTOTYPE(static void process_proc_map,(char *p, struct stat *s, int ss));
_PROTOTYPE(static int process_id,(char *idp, int idpl, char *cmd, UID_ARG uid,
				  int pid, int ppid, int pgid));
_PROTOTYPE(static int statEx,(char *p, struct stat *s, int *ss));
_PROTOTYPE(static int get_other_thread,(int pid, char **tid));


#if	defined(HASSELINUX)
_PROTOTYPE(static int cmp_cntx_eq,(char *pcntx, char *ucntx));


#include <fnmatch.h>


/*
 * cmp_cntx_eq -- compare program and user security contexts
 */

static int
cmp_cntx_eq(pcntx, ucntx)
	char *pcntx;			       /* program context */
	char *ucntx;			       /* user supplied context */
{
	return !fnmatch(ucntx, pcntx, 0);
}


/*
 * enter_cntx_arg() - enter name ecurity context argument
 */

int
enter_cntx_arg(cntx)
	char *cntx;			       /* context */
{
	cntxlist_t *cntxp;
/*
 * Search the argument list for a duplicate.
 */
	for (cntxp = CntxArg; cntxp; cntxp = cntxp->next) {
	    if (!strcmp(cntxp->cntx, cntx)) {
		if (!Fwarn) {
		    (void) fprintf(stderr, "%s: duplicate context: %s\n",
			Pn, cntx);
		}
		return(1);
	    }
	}
/*
 * Create and link a new context argument list entry.
 */
	if (!(cntxp = (cntxlist_t *)malloc((MALLOC_S)sizeof(cntxlist_t)))) {
	    (void) fprintf(stderr, "%s: no space for context: %s\n", Pn, cntx);
	    Exit(1);
	}
	cntxp->f = 0;
	cntxp->cntx = cntx;
	cntxp->next = CntxArg;
	CntxArg = cntxp;
	return(0);
}
#endif	/* defined(HASSELINUX) */


/*
 * gather_proc_info() -- gather process information
 */

void
gather_proc_info()
{
	char *cmd;
	struct dirent *dp;
	struct stat sb;
	int lwp, n, nl, pgid, pid, ppid, rv, tx;
	char *tid = NULL;
	static char *lwppath = (char *)NULL;
	static int lwppathl = 0;
	static char *path = (char *)NULL;
	static int pathl = 0;
	static char *pidpath = (char *)NULL;
	static MALLOC_S pidpathl = 0;
	static MALLOC_S pidx = 0;
	static DIR *ps = (DIR *)NULL;
	static char *taskpath = (char *)NULL;
	static int taskpathl = 0;
	DIR *ts;
	UID_ARG uid;

/*
 * Do one-time setup.
 */
	if (!pidpath) {
	    pidx = strlen(PROCFS) + 1;
	    pidpathl = pidx + 64 + 1;	/* 64 is growth room */
	    if (!(pidpath = (char *)malloc(pidpathl))) {
		(void) fprintf(stderr,
		    "%s: can't allocate %d bytes for \"%s/\"<pid>\n",
		    Pn, pidpathl, PROCFS);
		Exit(1);
	    }
	    (void) snpf(pidpath, pidpathl, "%s/", PROCFS);
	}
/*
 * Get lock and net information.
 */
	(void) make_proc_path(pidpath, pidx, &path, &pathl, "locks");
	(void) get_locks(path);
	(void) make_proc_path(pidpath, pidx, &path, &pathl, "net/");
	(void) set_net_paths(path, strlen(path));
/*
 * If only socket files have been selected, or socket files have been selected
 * ANDed with other selection options, enable the skipping of regular files.
 *
 * If socket files and some process options have been selected, enable
 * conditional skipping of regular file; i.e., regular files will be skipped
 * unless they belong to a process selected by one of the specified options.
 */
	if (Selflags & SELNW) {

	/*
	 * Some network files selection options have been specified.
	 */
	    if (Fand || !(Selflags & ~SELNW)) {

	    /*
	     * Selection ANDing or only network file options have been
	     * specified, so set unconditional skipping of regular files
	     * and socket file only checking.
	     */
		Cckreg = 0;
		Ckscko = 1;
	    } else {

	    /*
	     * If ORed file selection options have been specified, or no ORed
	     * process selection options have been specified, enable
	     * unconditional file checking and clear socket file only checking.
	     *
	     * If only ORed process selection options have been specified,
	     * enable conditional file skipping and socket file only checking.
	     */
		if ((Selflags & SELFILE) || !(Selflags & SELPROC))
		    Cckreg = Ckscko = 0;
		else
		    Cckreg = Ckscko = 1;
	    }
	} else {

	/*
	 * No network file selection options were specified.  Enable
	 * unconditional file checking and clear socket file only checking.
	 */
	    Cckreg = Ckscko = 0;
	}
/*
 * Read /proc, looking for PID directories.  Open each one and
 * gather its process and file information.
 */
	if (!ps) {
	    if (!(ps = opendir(PROCFS))) {
		(void) fprintf(stderr, "%s: can't open %s\n", Pn, PROCFS);
		Exit(1);
	    }
	} else
	    (void) rewinddir(ps);
	while ((dp = readdir(ps))) {
	    if (nm2id(dp->d_name, &pid, &n))
		continue;

	    tid = NULL;
	    if (get_other_thread(pid, &tid) < 0)
		continue;
	    if (tid)
		n += sizeof("task/") + strlen(tid);

	/*
	 * Build path to PID's directory.
	 */
	    if ((pidx + n + 1 + 1) > pidpathl) {
		pidpathl = pidx + n + 1 + 1 + 64;
		if (!(pidpath = (char *)realloc((MALLOC_P *)pidpath, pidpathl)))
		{
		    (void) fprintf(stderr,
			"%s: can't allocate %d bytes for \"%s/%s/\"\n",
			Pn, pidpathl, PROCFS, dp->d_name);
		    Exit(1);
		}
	    }
	    if (tid) {
		/* /proc/<pid> is useless (zombie), we have to use /proc/<pid>/task/<tid>
		 * where is still running thread
		 */
		(void) snpf(pidpath + pidx, pidpathl - pidx, "%s/task/%s/", dp->d_name, tid);
		free(tid);
	    } else
		(void) snpf(pidpath + pidx, pidpathl - pidx, "%s/", dp->d_name);
	    n += (pidx + 1);
	/*
	 * Process the PID's stat info.
	 */
	    if (stat(pidpath, &sb))
		continue;
	    uid = (UID_ARG)sb.st_uid;
	/*
	 * Process the PID's process information.
	 */
	    (void) make_proc_path(pidpath, n, &path, &pathl, "stat");
	    rv = read_id_stat(0, path, pid, &cmd, &ppid, &pgid);
	    if (rv == 1)
		continue;
	    else if (rv == 0) {
		(void) process_id(pidpath, n, cmd, uid, pid, ppid, pgid);
		continue;
	    }
	/*
	 * The process is a zombie.  Check for a non-zombie task.
	 */
	    (void) make_proc_path(pidpath, n, &taskpath, &taskpathl, "task");
	    tx = n + 4;
	    if ((ts = opendir(taskpath))) {

	    /*
	     * Process the PID's tasks (lightweight processes.)  Record the
	     * open files of the first one whose LWP ID does not match the PID
	     * and which is not a itself a zombie.
	     */
		while ((dp = readdir(ts))) {

		/*
		 * Get the LWP ID.  Skip the task if its LWP ID matches the
		 * process PID.
		 */
		    if (nm2id(dp->d_name, &lwp, &nl))
			continue;
		    if  (lwp == pid)
			continue;
		/*
		 * Check the LWP state.
		 */
		    if (read_id_stat(1, path, lwp, &cmd, &ppid, &pgid))
			continue;
		/*
		 * Attempt to record the LWP.
		 */
		    if ((tx + 1 + nl + 1) > lwppathl) {
			lwppathl = tx + 1 + n + 1 + 64;
			if (lwppath)
			    lwppath = (char *)realloc((MALLOC_P *)lwppath,
						      lwppathl);
			else
			    lwppath = (char *)malloc((MALLOC_S)lwppathl);
			if (!lwppath) {
			    (void) fprintf(stderr,
				"%s: can't allocate %d task bytes", Pn,
				lwppathl);
			    (void) fprintf(stderr,
				" for \"%s/%s/\"\n", taskpath, dp->d_name);
			    Exit(1);
			}
		    }
		    (void) snpf(lwppath, lwppathl, "%s/%s/", taskpath,
			dp->d_name);
		    if (!process_id(lwppath, (tx + 1 + nl+ 1), cmd, uid, pid,
				    ppid, pgid))
		    {
			break;
		    }
		}
		(void) closedir(ts);
	    }
	}
}


/*
 * get_fdinfo() - get values from /proc/<PID>fdinfo/FD
 */

static int
get_fdinfo(p, fi)
	char *p;			/* path to fdinfo file */
	struct l_fdinfo *fi;		/* pointer to local fdinfo values
					 * return structure */
{
	char buf[MAXPATHLEN + 1], *ep, **fp;
	FILE *fs;
	int rv = 0;
	unsigned long ul;
	unsigned long long ull;
/*
 * Signal no values returned (0) if no fdinfo pointer was provided or if the
 * fdinfo path can't be opened.
 */
	if (!fi)
	    return(0);
	if (!p || !*p || !(fs = fopen(p, "r")))
	    return(0);
/*
 * Read the fdinfo file.
 */
	while (fgets(buf, sizeof(buf), fs)) {
	    if (get_fields(buf, (char *)NULL, &fp, (int *)NULL, 0) < 2)
		continue;
	    if (!fp[0] || !*fp[0] || !fp[1] || !*fp[1])
		continue;
	    if (!strcmp(fp[0], "flags:")) {

	    /*
	     * Process a "flags:" line.
	     */
		ep = (char *)NULL;
		if ((ul = strtoul(fp[1], &ep, 0)) == ULONG_MAX
		||  !ep || *ep)
		    continue;
		fi->flags = (unsigned int)ul;
		if ((rv |= FDINFO_FLAGS) == FDINFO_ALL)
		    break;
	    } else if (!strcmp(fp[0], "pos:")) {

	    /*
	     * Process a "pos:" line.
	     */
		ep = (char *)NULL;
		if ((ull = strtoull(fp[1], &ep, 0)) == ULLONG_MAX
		||  !ep || *ep)
		    continue;
		fi->pos = (off_t)ull;
		if ((rv |= FDINFO_POS) == FDINFO_ALL)
		    break;
	    }
	}
	fclose(fs);
/*
 * Signal via the return value what information was obtained. (0 == none)
 */
	return(rv);
}


/*
 * getlinksrc() - get the source path name for the /proc/<PID>/fd/<FD> link
 */


static int
getlinksrc(ln, src, srcl)
	char *ln;			/* link path */
	char *src;			/* link source path return address */
	int srcl;			/* length of src[] */
{
	char *cp;
	int ll;

	if ((ll = readlink(ln, src, srcl - 1)) < 1
	||  ll >= srcl)
	    return(-1);
	src[ll] = '\0';
	if (*src == '/')
	    return(ll);
	if ((cp = strchr(src, ':'))) {
	    *cp = '\0';
	    ll = strlen(src);
	}
	return(ll);
}


/*
 * initialize() - perform all initialization
 */

void
initialize()
{
	int fd;
	struct l_fdinfo fi;
	char path[MAXPATHLEN];
	struct stat sb;
/*
 * Test for -i and -X option conflict.
 */
	if (Fxopt && (Fnet || Nwad)) {
	    (void) fprintf(stderr, "%s: -i is useless when -X is specified.\n",
		Pn);
	    usage(1, 0, 0);
	}
/*
 * Open LSTAT_TEST_FILE and seek to byte LSTAT_TEST_SEEK, then lstat the
 * /proc/<PID>/fd/<FD> for LSTAT_TEST_FILE to see what position is reported.
 * If the result is LSTAT_TEST_SEEK, enable offset reporting.
 *
 * If the result isn't LSTAT_TEST_SEEK, next check the fdinfo file for the
 * open LSTAT_TEST_FILE file descriptor.  If it exists and contains a "pos:"
 * value, and if the value is LSTAT_TEST_SEEK, enable offset reporting.
 */
	if ((fd = open(LSTAT_TEST_FILE, O_RDONLY)) >= 0) {
	    if (lseek(fd, (off_t)LSTAT_TEST_SEEK, SEEK_SET)
	    == (off_t)LSTAT_TEST_SEEK) {
		(void) snpf(path, sizeof(path), "%s/%d/fd/%d", PROCFS, Mypid,
			    fd);
		if (!lstat(path, &sb)) {
		    if (sb.st_size == (off_t)LSTAT_TEST_SEEK)
			OffType = 1;
		}
	    }
	    if (!OffType) {
		(void) snpf(path, sizeof(path), "%s/%d/fdinfo/%d", PROCFS,
			    Mypid, fd);
		if (get_fdinfo(path, &fi) & FDINFO_POS) {
		    if (fi.pos == (off_t)LSTAT_TEST_SEEK)
			OffType = 2;
		}
	    }
	    (void) close(fd);
	}
	if (!OffType) {
	    if (Foffset && !Fwarn)
		(void) fprintf(stderr,
		    "%s: WARNING: can't report offset; disregarding -o.\n",
		    Pn);
	    Foffset = 0;
	    Fsize = 1;
	}
	if (Fsv && (OffType != 2)) {
	    if (!Fwarn && FsvByf)
		(void) fprintf(stderr,
		    "%s: WARNING: can't report file flags; disregarding +f.\n",
		    Pn);
	    Fsv = 0;
	}
/*
 * Make sure the local mount info table is loaded if doing anything other
 * than just Internet lookups.  (HasNFS is defined during the loading of the
 * local mount table.)
 */
	if (Selinet == 0)
	    (void) readmnt();
}


/*
 * make_proc_path() - make a path in a /proc directory
 *
 * entry:
 *	pp = pointer to /proc prefix
 *	lp = length of prefix
 *	np = pointer to malloc'd buffer to receive new file's path
 *	nl = length of new file path buffer
 *	sf = new path's suffix
 *
 * return: length of new path
 *	np = updated with new path
 *	nl = updated with new path length
 */

int
make_proc_path(pp, pl, np, nl, sf)
	char *pp;			/* path prefix -- e.g., /proc/<pid>/ */
	int pl;				/* strlen(pp) */
	char **np;			/* malloc'd receiving buffer */
	int *nl;			/* strlen(*np) */
	char *sf;			/* suffix of new path */
{
	char *cp;
	MALLOC_S rl, sl;

	sl = strlen(sf);
	if ((rl = pl + sl + 1) > *nl) {
	    if ((cp = *np))
		cp = (char *)realloc((MALLOC_P *)cp, rl);
	    else
		cp = (char *)malloc(rl);
	    if (!cp) {
		(void) fprintf(stderr,
		    "%s: can't allocate %d bytes for %s%s\n",
		    Pn, rl, pp, sf);
		Exit(1);
	    }
	    *nl = rl;
	    *np = cp;
	}
	(void) snpf(*np, *nl, "%s", pp);
	(void) snpf(*np + pl, *nl - pl, "%s", sf);
	return(rl - 1);
}


/*
 * nm2id() - convert a name to an integer ID
 */

static int
nm2id(nm, id, idl)
	char *nm;			/* pointer to name */
	int *id;			/* pointer to ID receiver */
	int *idl;			/* pointer to ID length receiver */
{
	register int tid, tidl;

	for (*id = *idl = tid = tidl = 0; *nm; nm++) {

#if	defined(__STDC__)	/* { */
	    if (!isdigit((unsigned char)*nm))
#else	/* !defined(__STDC__)	   } { */
	    if (!isascii(*nm) || !isdigit((unsigned char)*cp))
#endif	/* defined(__STDC__)	   } */

		{
		    return(1);
		}
		tid = tid * 10 + (int)(*nm - '0');
		tidl++;
	}
	*id = tid;
	*idl = tidl;
	return(0);
}


/*
 * open_proc_stream() -- open a /proc stream
 */

FILE *
open_proc_stream(p, m, buf, sz, act)
	char *p;			/* pointer to path to open */
	char *m;			/* pointer to mode -- e.g., "r" */
	char **buf;			/* pointer tp setvbuf() address
					 * (NULL if none) */
	size_t *sz;			/* setvbuf() size (0 if none or if
					 * getpagesize() desired */
	int act;			/* fopen() failure action:
					 *     0 : return (FILE *)NULL
					 *   <>0 : fprintf() an error message
					 *         and Exit(1)
					 */
{
	FILE *fs;			/* opened stream */
	static size_t psz = (size_t)0;	/* page size */
	size_t tsz;			/* temporary size */
/*
 * Open the stream.
 */
	if (!(fs = fopen(p, m))) {
	    if (!act)
		return((FILE *)NULL);
	    (void) fprintf(stderr, "%s: can't fopen(%s, \"%s\"): %s\n",
		Pn, p, m, strerror(errno));
	    Exit(1);
	}
/*
 * Return the stream if no buffer change is required.
 */
	if (!buf)
	    return(fs);
/*
 * Determine the buffer size required.
 */
	if (!(tsz = *sz)) {
	    if (!psz)
		psz = getpagesize();
	    tsz = psz;
	}
/*
 * Allocate a buffer for the stream, as required.
 */
	if (!*buf) {
	    if (!(*buf = (char *)malloc((MALLOC_S)tsz))) {
		(void) fprintf(stderr,
		    "%s: can't allocate %d bytes for %s stream buffer\n",
		    Pn, (int)tsz, p);
		Exit(1);
	    }
	    *sz = tsz;
	}
/*
 * Assign the buffer to the stream.
 */
	if (setvbuf(fs, *buf, _IOFBF, tsz)) {
	    (void) fprintf(stderr, "%s: setvbuf(%s)=%d failure: %s\n",
		Pn, p, (int)tsz, strerror(errno));
	    Exit(1);
	}
	return(fs);
}


/*
 * process_id - process ID: PID or LWP
 *
 * return:  0 == ID processed
 *          1 == ID not processed
 */

static int
process_id(idp, idpl, cmd, uid, pid, ppid, pgid)
	char *idp;			/* pointer to ID's path */
	int idpl;			/* pointer to ID's path length */
	char *cmd;			/* pointer to ID's command */
	UID_ARG uid;			/* ID's UID */
	int pid;			/* ID's PID */
	int ppid;			/* parent PID */
	int pgid;			/* parent GID */
{
	int av;
	static char *dpath = (char *)NULL;
	static int dpathl = 0;
	short enls, enss, lnk, oty, pn, pss, sf;
	int fd, i, ls, n, ss, sv;
	struct l_fdinfo fi;
	DIR *fdp;
	struct dirent *fp;
	static char *ipath = (char *)NULL;
	static int ipathl = 0;
	int j = 0;
	struct stat lsb, sb;
	char nmabuf[MAXPATHLEN + 1], pbuf[MAXPATHLEN + 1];
	static char *path = (char *)NULL;
	static int pathl = 0;
	static char *pathi = (char *)NULL;
	static int pathil = 0;
	int txts = 0;

#if	defined(HASSELINUX)
	cntxlist_t *cntxp;
#endif	/* defined(HASSELINUX) */

/*
 * See if process is excluded.
 */
	if (is_proc_excl(pid, pgid, uid, &pss, &sf)
	||  is_cmd_excl(cmd, &pss, &sf))
	    return(1);
	if (Cckreg) {

	/*
	 * If conditional checking of regular files is enabled, enable
	 * socket file only checking, based on the process' selection
	 * status.
	 */
	    Ckscko = (sf & SELPROC) ? 0 : 1;
	}
	alloc_lproc(pid, pgid, ppid, uid, cmd, (int)pss, (int)sf);
	Plf = (struct lfile *)NULL;
/*
 * Process the ID's current working directory info.
 */
	if (!Ckscko) {
	    (void) make_proc_path(idp, idpl, &path, &pathl, "cwd");
	    alloc_lfile(CWD, -1);
	    if (getlinksrc(path, pbuf, sizeof(pbuf)) < 1) {
		if (!Fwarn) {
		    (void) memset((void *)&sb, 0, sizeof(sb));
		    lnk = ss = 0;
		    (void) snpf(nmabuf, sizeof(nmabuf), "(readlink: %s)",
			strerror(errno));
		    nmabuf[sizeof(nmabuf) - 1] = '\0';
		    (void) add_nma(nmabuf, strlen(nmabuf));
		    pn = 1;
		} else
		    pn = 0;
	    } else {
		lnk = pn = 1;
		ss = SB_ALL;
		if (HasNFS) {
		    if ((sv = statsafely(path, &sb)))
			sv = statEx(pbuf, &sb, &ss);
		} else
		    sv = stat(path, &sb);
		if (sv) {
		    ss = 0;
		    if (!Fwarn) {
			(void) snpf(nmabuf, sizeof(nmabuf), "(stat: %s)",
			    strerror(errno));
			nmabuf[sizeof(nmabuf) - 1] = '\0';
			(void) add_nma(nmabuf, strlen(nmabuf));
		    }
		}
	    }
	    if (pn) {
		(void) process_proc_node(lnk ? pbuf : path,
					 &sb, ss,
					 (struct stat *)NULL, 0);
		if (Lf->sf)
		    link_lfile();
	    }
	}
/*
 * Process the ID's root directory info.
 */
	if (!Ckscko) {
	    (void) make_proc_path(idp, idpl, &path, &pathl, "root");
	    alloc_lfile(RTD, -1);
	    if (getlinksrc(path, pbuf, sizeof(pbuf)) < 1) {
		if (!Fwarn) {
		    (void) memset((void *)&sb, 0, sizeof(sb));
		    lnk = ss = 0;
		    (void) snpf(nmabuf, sizeof(nmabuf), "(readlink: %s)",
			strerror(errno));
		    nmabuf[sizeof(nmabuf) - 1] = '\0';
		    (void) add_nma(nmabuf, strlen(nmabuf));
		    pn = 1;
		} else
		    pn = 0;
	    } else {
		lnk = pn = 1;
		ss = SB_ALL;
		if (HasNFS) {
		    if ((sv = statsafely(path, &sb)))
			sv = statEx(pbuf, &sb, &ss);
		} else
		    sv = stat(path, &sb);
		if (sv) {
		    ss = 0;
		    if (!Fwarn) {
			(void) snpf(nmabuf, sizeof(nmabuf), "(stat: %s)",
			    strerror(errno));
			nmabuf[sizeof(nmabuf) - 1] = '\0';
			(void) add_nma(nmabuf, strlen(nmabuf));
		    }
		}
	    }
	    if (pn) {
		(void) process_proc_node(lnk ? pbuf : path,
					 &sb, ss,
					 (struct stat *)NULL, 0);
		if (Lf->sf)
		    link_lfile();
	    }
	}
/*
 * Process the ID's execution info.
 */
	if (!Ckscko) {
	    (void) make_proc_path(idp, idpl, &path, &pathl, "exe");
	    alloc_lfile("txt", -1);
	    if (getlinksrc(path, pbuf, sizeof(pbuf)) < 1) {
		(void) memset((void *)&sb, 0, sizeof(sb));
		lnk = ss = 0;
		if (!Fwarn) {
		    if ((errno != ENOENT) || uid) {
			(void) snpf(nmabuf, sizeof(nmabuf), "(readlink: %s)",
			    strerror(errno));
			nmabuf[sizeof(nmabuf) - 1] = '\0';
			(void) add_nma(nmabuf, strlen(nmabuf));
		    }
		    pn = 1;
		} else
		    pn = 0;
	    } else {
		lnk = pn = 1;
		ss = SB_ALL;
		if (HasNFS) {
		    if ((sv = statsafely(path, &sb))) {
			sv = statEx(pbuf, &sb,  &ss);
			if (!sv && (ss & SB_DEV) && (ss & SB_INO))
			    txts = 1;
		    }
		} else
		    sv = stat(path, &sb);
		if (sv) {
		    ss = 0;
		    if (!Fwarn) {
			(void) snpf(nmabuf, sizeof(nmabuf), "(stat: %s)",
			    strerror(errno));
			nmabuf[sizeof(nmabuf) - 1] = '\0';
			(void) add_nma(nmabuf, strlen(nmabuf));
		    }
		} else
		    txts = 1;
	    }
	    if (pn) {
		(void) process_proc_node(lnk ? pbuf : path,
					 &sb, ss,
					 (struct stat *)NULL, 0);
		if (Lf->sf)
		    link_lfile();
	    }
	}
/*
 * Process the ID's memory map info.
 */
	if (!Ckscko) {
	    (void) make_proc_path(idp, idpl, &path, &pathl, "maps");
	    (void) process_proc_map(path, txts ? &sb : (struct stat *)NULL,
				    txts ? ss : 0);
	}

#if	defined(HASSELINUX)
/*
 * Process the PID's SELinux context.
 */
	if (Fcntx) {

	/*
	 * If the -Z (cntx) option was specified, match the valid contexts.
	 */
	    errno = 0;
	    if (getpidcon(pid, &Lp->cntx) == -1) {
		Lp->cntx = (char *)NULL;
		if (!Fwarn) {
		    (void) snpf(nmabuf, sizeof(nmabuf),
			"(getpidcon: %s)", strerror(errno));
		    if (!(Lp->cntx = strdup(nmabuf))) {
			(void) fprintf(stderr,
			    "%s: no context error space: PID %ld",
			    Pn, (long)Lp->pid);
			Exit(1);
		    }
		}
	    } else if (CntxArg) {

	    /*
	     * See if context includes the process.
	     */
		for (cntxp = CntxArg; cntxp; cntxp = cntxp->next) {
		    if (cmp_cntx_eq(Lp->cntx, cntxp->cntx)) {
			cntxp->f = 1;
			Lp->pss |= PS_PRI;
			Lp->sf |= SELCNTX;
			break;
		    }
		}
	    }
	}
#endif	/* defined(HASSELINUX) */

/*
 * Process the ID's file descriptor directory.
 */
	if ((i = make_proc_path(idp, idpl, &dpath, &dpathl, "fd/")) < 3)
	    return(0);
	dpath[i - 1] = '\0';
	if ((OffType == 2)
	&&  ((j = make_proc_path(idp, idpl, &ipath, &ipathl, "fdinfo/")) >= 7))
	    oty = 1;
	else
	    oty = 0;
	if (!(fdp = opendir(dpath))) {
	    if (!Fwarn) {
		(void) snpf(nmabuf, sizeof(nmabuf), "%s (opendir: %s)",
		    dpath, strerror(errno));
		alloc_lfile("NOFD", -1);
		nmabuf[sizeof(nmabuf) - 1] = '\0';
		(void) add_nma(nmabuf, strlen(nmabuf));
		link_lfile();
	    }
	    return(0);
	}
	dpath[i - 1] = '/';
	while ((fp = readdir(fdp))) {
	    if (nm2id(fp->d_name, &fd, &n))
		continue;
	    (void) make_proc_path(dpath, i, &path, &pathl, fp->d_name);
	    (void) alloc_lfile((char *)NULL, fd);
	    if (getlinksrc(path, pbuf, sizeof(pbuf)) < 1) {
		(void) memset((void *)&sb, 0, sizeof(sb));
		lnk = ss = 0;
		if (!Fwarn) {
		    (void) snpf(nmabuf, sizeof(nmabuf), "(readlink: %s)",
			strerror(errno));
		    nmabuf[sizeof(nmabuf) - 1] = '\0';
		    (void) add_nma(nmabuf, strlen(nmabuf));
		    pn = 1;
		} else
		    pn = 0;
	    } else {
		lnk = 1;
		if (HasNFS) {
		    if (lstatsafely(path, &lsb)) {
			(void) statEx(pbuf, &lsb, &ls);
		        enls = errno;
		    } else {
			enls = 0;
			ls = SB_ALL;
		    }
		    if (statsafely(path, &sb)) {
			(void) statEx(pbuf, &sb, &ss);
			enss = errno;
		    } else {
			enss = 0;
			ss = SB_ALL;
		    }
		} else {
		    ls = lstat(path, &lsb) ? 0 : SB_ALL;
		    enls = errno;
		    ss = stat(path, &sb) ? 0 : SB_ALL;
		    enss = errno;
		}
		if (!ls && !Fwarn) {
		    (void) snpf(nmabuf, sizeof(nmabuf), "lstat: %s)",
			strerror(enls));
		    nmabuf[sizeof(nmabuf) - 1] = '\0';
		    (void) add_nma(nmabuf, strlen(nmabuf));
		}
		if (!ss && !Fwarn) {
		    (void) snpf(nmabuf, sizeof(nmabuf), "(stat: %s)",
			strerror(enss));
		    nmabuf[sizeof(nmabuf) - 1] = '\0';
		    (void) add_nma(nmabuf, strlen(nmabuf));
		}
		if (Ckscko) {
		    if ((ss & SB_MODE) && ((sb.st_mode & S_IFMT) == S_IFSOCK))
			pn = 1;
		    else
			pn = 0;
		} else
		    pn = 1;
	    }
	    if (pn) {
		if (oty) {
		    (void) make_proc_path(ipath, j, &pathi, &pathil,
					  fp->d_name);
		    if ((av = get_fdinfo(pathi, &fi)) & FDINFO_POS) {
			ls |= SB_SIZE;
			lsb.st_size = fi.pos;
		    } else
			ls &= ~SB_SIZE;

#if	!defined(HASNOFSFLAGS)
		    if ((av & FDINFO_FLAGS) && (Fsv & FSV_FG)) {
			Lf->ffg = (long)fi.flags;
			Lf->fsv |= FSV_FG;
		     }
# endif	/* !defined(HASNOFSFLAGS) */

		}
		process_proc_node(lnk ? pbuf : path,
				  &sb, ss, &lsb, ls);
		if (Lf->sf)
		    link_lfile();
	    }
	}
	(void) closedir(fdp);
	return(0);
}

/* fill tid if the initial thread is zombie,
 * but other thread still alive
 *
 * returns -1=error, 0=nothing, 1=ok
 */
static int
get_other_thread(pid, tid)
	int pid;
	char **tid;
{
	char path[MAXPATHLEN];
	DIR *tdp;
	struct dirent *td;
	char pstate;
	FILE *f;
	int _pid;
	int re = 0, x;

	snpf(path, sizeof(path), "%s/%d/stat", PROCFS, pid);
	if (!(f = fopen(path, "r")))
		return -1;
	x = fscanf(f, "%d %*s %c", &_pid, &pstate);
	fclose(f);
	if (x!=2)
		return -1;
	if (_pid != pid)
		return -1;		/* corrupted /proc? */
	if (pstate!='Z')
		return 0;		/* ignore normal proceses */

	snpf(path, sizeof(path), "%s/%d/task", PROCFS, pid);

	/* open /proc/<pid>/task */
	if (!(tdp = opendir(path)))
		return 0;		/* kernel < 2.6.x */

	/* look for first alive thread */
	while ((td = readdir(tdp))) {
		if (strcmp(td->d_name, ".")==0 || strcmp(td->d_name, "..")==0)
			continue;

		/* /proc/<pid>/task/<tid>/stat */
		snpf(path, sizeof(path), "%s/%d/task/%s/stat", PROCFS, pid, td->d_name);
		if (!(f = fopen(path, "r")))
			continue;
		x = fscanf(f, "%*d %*s %c", &pstate);
		fclose(f);
		if (x!=1)
			continue;
		if (pstate!='Z') {
			re = 1;
			*tid = strdup(td->d_name);
			break;
		}
	}
	closedir(tdp);
	return re;
}

/*
 * process_proc_map() - process the memory map of a process
 */

static void
process_proc_map(p, s, ss)
	char *p;			/* path to process maps file */
	struct stat *s;			/* executing text file state buffer */
	int ss;				/* *s status -- i.e., SB_* values */
{
	char buf[MAXPATHLEN + 1], *ep, fmtbuf[32], **fp, nmabuf[MAXPATHLEN + 1];
	dev_t dev;
	int ds, en, i, mss, nf, sv;
	int eb = 6;
	INODETYPE inode;
	MALLOC_S len;
	long maj, min;
	FILE *ms;
	int ns = 0;
	struct stat sb;
	struct saved_map {
	    dev_t dev;
	    INODETYPE inode;
	};
	static struct saved_map *sm = (struct saved_map *)NULL;
	static int sma = 0;
	static char *vbuf = (char *)NULL;
	static size_t vsz = (size_t)0;
/*
 * Open the /proc/<pid>/maps file, assign a page size buffer to its stream,
 * and read it/
 */
	if (!(ms = open_proc_stream(p, "r", &vbuf, &vsz, 0)))
	    return;
	while (fgets(buf, sizeof(buf), ms)) {
	    if ((nf = get_fields(buf, ":", &fp, &eb, 1)) < 7)
		continue;			/* not enough fields */
	    if (!fp[6] || !*fp[6])
		continue;			/* no path name */
	/*
	 * See if the path ends in " (deleted)".  If it does, strip the
	 * " (deleted)" characters and remember that they were there.
	 */
	    if (((ds = (int)strlen(fp[6])) > 10)
	    &&  !strcmp(fp[6] + ds - 10, " (deleted)"))
	    {
		*(fp[6] + ds - 10) = '\0';
	    } else
		ds = 0;
	/*
	 * Assemble the major and minor device numbers.
	 */
	    ep = (char *)NULL;
	    if (!fp[3] || !*fp[3]
	    ||  (maj = strtol(fp[3], &ep, 16)) == LONG_MIN || maj == LONG_MAX
	    ||  !ep || *ep)
		continue;
	    ep = (char *)NULL;
	    if (!fp[4] || !*fp[4]
	    ||  (min = strtol(fp[4], &ep, 16)) == LONG_MIN || min == LONG_MAX
	    ||  !ep || *ep)
		continue;
	/*
	 * Assemble the device and inode numbers.  If they are both zero, skip
	 * the entry.
	 */
	    dev = (dev_t)makedev((int)maj, (int)min);
	    if (!fp[5] || !*fp[5])
		continue;
	    ep = (char *)NULL;
	    if ((inode = strtoull(fp[5], &ep, 0)) == ULLONG_MAX
	    ||  !ep || *ep)
		continue;
	    if (!dev && !inode)
		continue;
	/*
	 * See if the device + inode pair match that of the executable.
	 * If they do, skip this map entry.
	 */
	    if (s && (ss & SB_DEV) && (ss & SB_INO)
	    &&  (dev == s->st_dev) && (inode == (INODETYPE)s->st_ino))
		continue;
	/*
	 * See if this device + inode pair has already been processed as
	 * a map entry.
	 */
	    for (i = 0; i < ns; i++) {
		if (dev == sm[i].dev && inode == sm[i].inode)
		    break;
	    }
	    if (i < ns)
		continue;
	/*
	 * Record the processing of this map entry's device and inode pair.
	 */
	    if (ns >= sma) {
		sma += 10;
		len = (MALLOC_S)(sma * sizeof(struct saved_map));
		if (sm)
		    sm = (struct saved_map *)realloc(sm, len);
		else
		    sm = (struct saved_map *)malloc(len);
		if (!sm) {
		    (void) fprintf(stderr,
			"%s: can't allocate %d bytes for saved maps, PID %d\n",
			Pn, len, Lp->pid);
		    Exit(1);
		}
	    }
	    sm[ns].dev = dev;
	    sm[ns++].inode = inode;
	/*
	 * Allocate space for the mapped file, then get stat(2) information
	 * for it.
	 */
	    alloc_lfile("mem", -1);
	    if (HasNFS) {
		sv = statsafely(fp[6], &sb);
	    } else
		sv = stat(fp[6], &sb);
	    if (sv) {
		en = errno;
	    /*
	     * Applying stat(2) to the file failed, so manufacture a partial
	     * stat(2) reply from the process' maps file entry.
	     *
	     * If the file has been deleted, reset its type to "DEL"; otherwise
	     * generate a stat() error name addition.
	     */
		(void) memset((void *)&sb, 0, sizeof(sb));
		sb.st_dev = dev;
		sb.st_ino = (ino_t)inode;
		sb.st_mode = S_IFREG;
		mss = SB_DEV | SB_INO | SB_MODE;
		if (ds)
		    alloc_lfile("DEL", -1);
		else {
		    (void) snpf(nmabuf, sizeof(nmabuf), "(stat: %s)",
			strerror(en));
		    nmabuf[sizeof(nmabuf) - 1] = '\0';
		    (void) add_nma(nmabuf, strlen(nmabuf));
		}
	    } else if ((sb.st_dev != dev) || ((INODETYPE)sb.st_ino != inode)) {

	    /*
	     * The stat(2) device and inode numbers don't match those obtained
	     * from the process' maps file.
	     *
	     * If the file has been deleted, reset its type to "DEL"; otherwise
	     * generate inconsistency name additions.
	     *
	     * Manufacture a partial stat(2) reply from the maps file
	     * information.
	     */
		if (ds)
		    alloc_lfile("DEL", -1);
		else if (!Fwarn) {
		    char *sep;

		    if (sb.st_dev != dev) {
			(void) snpf(nmabuf, sizeof(nmabuf),
			    "(path dev=%d,%d%s",
			    GET_MAJ_DEV(sb.st_dev), GET_MIN_DEV(sb.st_dev),
			    ((INODETYPE)sb.st_ino == inode) ? ")" : ",");
			nmabuf[sizeof(nmabuf) - 1] = '\0';
			(void) add_nma(nmabuf, strlen(nmabuf));
			sep = "";
		    } else
			sep = "(path ";
		    if ((INODETYPE)sb.st_ino != inode) {
			(void) snpf(fmtbuf, sizeof(fmtbuf), "%%sinode=%s)",
			    InodeFmt_d);
			(void) snpf(nmabuf, sizeof(nmabuf), fmtbuf,
			    sep, (INODETYPE)sb.st_ino);
			nmabuf[sizeof(nmabuf) - 1] = '\0';
			(void) add_nma(nmabuf, strlen(nmabuf));
		    }
		}
		(void) memset((void *)&sb, 0, sizeof(sb));
		sb.st_dev = dev;
		sb.st_ino = (ino_t)inode;
		sb.st_mode = S_IFREG;
		mss = SB_DEV | SB_INO | SB_MODE;
	    } else
		mss = SB_ALL;
	/*
	 * Record the file's information.
	 */
	    process_proc_node(fp[6], &sb, mss, (struct stat *)NULL, 0);
	    if (Lf->sf)
		link_lfile();
	}
	(void) fclose(ms);
}


/*
 * read_id_stat() - read ID (PID or LWP ID) status
 *
 * return: -1 == ID is a zombie
 *          0 == ID OK
 *          1 == ID unavailable
 */

static int
read_id_stat(ty, p, id, cmd, ppid, pgid)
	int ty;				/* type: 0 == PID, 1 == LWP */
	char *p;			/* path to status file */
	int id;				/* ID: PID or LWP */
	char **cmd;			/* malloc'd command name */
	int *ppid;			/* returned parent PID for PID type */
	int *pgid;			/* returned process group ID for PID
					 * type */
{
	char buf[MAXPATHLEN], *cp, *cp1, **fp;
	static char *cbf = (char *)NULL;
	static MALLOC_S cbfa = 0;
	FILE *fs;
	MALLOC_S len;
	int nf;
	static char *vbuf = (char *)NULL;
	static size_t vsz = (size_t)0;
/*
 * Open the stat file path, assign a page size buffer to its stream,
 * and read the file's first line.
 */
	if (!(fs = open_proc_stream(p, "r", &vbuf, &vsz, 0)))
	    return(1);
	cp = fgets(buf, sizeof(buf), fs);
	(void) fclose(fs);
	if (!cp)
	    return(1);
/*
 * Separate the line into fields on white space separators.  Expect five fields
 * for a PID type and three for an LWP type.
 */
	if ((nf = get_fields(buf, (char *)NULL, &fp, (int *)NULL, 0))
	<  (ty ? 5 : 3))
	{
	    return(1);
	}
/*
 * Get the command name from the second field.  Strip a starting '(' and
 * an ending ')'.  Allocate space to hold the result and return the space
 * pointer.
 */
	if (!(cp = fp[1]))
	    return(1);
	if (cp && *cp == '(')
	    cp++;
	if ((cp1 = strrchr(cp, ')')))
	    *cp1 = '\0';
	if ((len = strlen(cp) + 1) > cbfa) {
	     cbfa = len;
	     if (cbf)
		cbf = (char *)realloc((MALLOC_P *)cbf, cbfa);
	     else
		cbf = (char *)malloc(cbfa);
	     if (!cbf) {
		(void) fprintf(stderr,
		    "%s: can't allocate %d bytes for command \"%s\"\n",
		    Pn, cbfa, cp);
		Exit(1);
	     }
	}
	(void) snpf(cbf, len, "%s", cp);
	*cmd = cbf;
/*
 * If the type is PID, convert and return parent process (fourth field)
 * and process group (fifth field) IDs.
 */
	if (!ty) {
	    if (fp[3] && *fp[3])
		*ppid = atoi(fp[3]);
	    else
		return(1);
	    if (fp[4] && *fp[4])
		*pgid = atoi(fp[4]);
	    else
		return(1);
	}
/*
 * Check the state in the third field.  If it is 'Z', return that indication.
 */
	return((fp[2] && !strcmp(fp[2], "Z")) ? -1 : 0);
}


/*
 * statEx() - extended stat() to get device numbers when a "safe" stat has
 *	      failed and the system has an NFS mount
 *
 * Note: this function was suggested by Paul Szabo as a way to get device
 *       numbers for NFS files when an NFS mount point has the root_squash
 *       option set.  In that case, even if lsof is setuid(root), the identity
 *	 of its requests to stat() NFS files lose root permission and may fail.
 *
 *	 This function should be used only when links have been successfully
 *	 resolved in the /proc path by getlinksrc().
 */

static int
statEx(p, s, ss)
	char *p;			/* file path */
	struct stat *s;			/* stat() result -- NULL if none
					 * wanted */
	int *ss;			/* stat() status --  SB_* values */
{
	static size_t ca = 0;
	static char *cb = NULL;
	char *cp;
	int ensv = ENOENT;
	struct stat sb;
	int st = 0;
	size_t sz;
/*
 * Make a copy of the path.
 */
	sz = strlen(p);
	if ((sz + 1) > ca) {
	    if (cb)
		cb = (char *)realloc((MALLOC_P *)cb, sz + 1);
	    else
		cb = (char *)malloc(sz + 1);
	    if (!cb) {
		(void) fprintf(stderr,
		    "%s: PID %ld: no statEx path space: %s\n",
		    Pn, (long)Lp->pid, p);
		Exit(1);
	    }
	    ca = sz + 1;
	}
	(void) strcpy(cb, p);
/*
 * Trim trailing leaves from the end of the path one at a time and do s safe
 * stat() on each trimmed result.  Stop when a safe stat() succeeds or doesn't
 * fail because of EACCES or EPERM.
 */
	for (cp = strrchr(cb, '/'); cp && (cp != cb);) {
	    *cp = '\0';
	    if (!statsafely(cb, &sb)) {
		st = 1;
		break;
	    }
	    ensv = errno;
	    if ((ensv != EACCES) && (ensv != EPERM))
		break;
	    cp = strrchr(cb, '/');
	}
/*
 * If a stat() on a trimmed result succeeded, form partial results containing
 * only the device and raw device numbers.
 */
	memset((void *)s, 0, sizeof(struct stat));
	if (st) {
	    errno = 0;
	    s->st_dev = sb.st_dev;
	    s->st_rdev = sb.st_rdev;
	    *ss = SB_DEV | SB_RDEV;
	    return(0);
	}
	errno = ensv;
	*ss = 0;
	return(1);
}
