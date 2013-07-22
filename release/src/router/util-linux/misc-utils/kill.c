/*
 * Copyright (c) 1988, 1993, 1994
 *	The Regents of the University of California.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */
/*
 *  oct 5 1994 -- almost entirely re-written to allow for process names.
 *  modifications (c) salvatore valente <svalente@mit.edu>
 *  may be used / modified / distributed under the same terms as the original.
 *
 *  1999-02-22 Arkadiusz Mi¶kiewicz <misiek@pld.ORG.PL>
 *  - added Native Language Support
 *
 *  1999-11-13 aeb Accept signal numers 128+s.
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <ctype.h>		/* for isdigit() */
#include <unistd.h>
#include <signal.h>

#include "c.h"
#include "kill.h"
#include "nls.h"
#include "strutils.h"

struct signv {
	char *name;
	int val;
} sys_signame[] = {
	/* POSIX signals */
	{ "HUP",	SIGHUP },	/* 1 */
	{ "INT",	SIGINT }, 	/* 2 */
	{ "QUIT",	SIGQUIT }, 	/* 3 */
	{ "ILL",	SIGILL }, 	/* 4 */
	{ "ABRT",	SIGABRT }, 	/* 6 */
	{ "FPE",	SIGFPE }, 	/* 8 */
	{ "KILL",	SIGKILL }, 	/* 9 */
	{ "SEGV",	SIGSEGV }, 	/* 11 */
	{ "PIPE",	SIGPIPE }, 	/* 13 */
	{ "ALRM",	SIGALRM }, 	/* 14 */
	{ "TERM",	SIGTERM }, 	/* 15 */
	{ "USR1",	SIGUSR1 }, 	/* 10 (arm,i386,m68k,ppc), 30 (alpha,sparc*), 16 (mips) */
	{ "USR2",	SIGUSR2 }, 	/* 12 (arm,i386,m68k,ppc), 31 (alpha,sparc*), 17 (mips) */
	{ "CHLD",	SIGCHLD }, 	/* 17 (arm,i386,m68k,ppc), 20 (alpha,sparc*), 18 (mips) */
	{ "CONT",	SIGCONT }, 	/* 18 (arm,i386,m68k,ppc), 19 (alpha,sparc*), 25 (mips) */
	{ "STOP",	SIGSTOP },	/* 19 (arm,i386,m68k,ppc), 17 (alpha,sparc*), 23 (mips) */
	{ "TSTP",	SIGTSTP },	/* 20 (arm,i386,m68k,ppc), 18 (alpha,sparc*), 24 (mips) */
	{ "TTIN",	SIGTTIN },	/* 21 (arm,i386,m68k,ppc,alpha,sparc*), 26 (mips) */
	{ "TTOU",	SIGTTOU },	/* 22 (arm,i386,m68k,ppc,alpha,sparc*), 27 (mips) */
	/* Miscellaneous other signals */
#ifdef SIGTRAP
	{ "TRAP",	SIGTRAP },	/* 5 */
#endif
#ifdef SIGIOT
	{ "IOT",	SIGIOT }, 	/* 6, same as SIGABRT */
#endif
#ifdef SIGEMT
	{ "EMT",	SIGEMT }, 	/* 7 (mips,alpha,sparc*) */
#endif
#ifdef SIGBUS
	{ "BUS",	SIGBUS },	/* 7 (arm,i386,m68k,ppc), 10 (mips,alpha,sparc*) */
#endif
#ifdef SIGSYS
	{ "SYS",	SIGSYS }, 	/* 12 (mips,alpha,sparc*) */
#endif
#ifdef SIGSTKFLT
	{ "STKFLT",	SIGSTKFLT },	/* 16 (arm,i386,m68k,ppc) */
#endif
#ifdef SIGURG
	{ "URG",	SIGURG },	/* 23 (arm,i386,m68k,ppc), 16 (alpha,sparc*), 21 (mips) */
#endif
#ifdef SIGIO
	{ "IO",		SIGIO },	/* 29 (arm,i386,m68k,ppc), 23 (alpha,sparc*), 22 (mips) */
#endif
#ifdef SIGPOLL
	{ "POLL",	SIGPOLL },	/* same as SIGIO */
#endif
#ifdef SIGCLD
	{ "CLD",	SIGCLD },	/* same as SIGCHLD (mips) */
#endif
#ifdef SIGXCPU
	{ "XCPU",	SIGXCPU },	/* 24 (arm,i386,m68k,ppc,alpha,sparc*), 30 (mips) */
#endif
#ifdef SIGXFSZ
	{ "XFSZ",	SIGXFSZ },	/* 25 (arm,i386,m68k,ppc,alpha,sparc*), 31 (mips) */
#endif
#ifdef SIGVTALRM
	{ "VTALRM",	SIGVTALRM },	/* 26 (arm,i386,m68k,ppc,alpha,sparc*), 28 (mips) */
#endif
#ifdef SIGPROF
	{ "PROF",	SIGPROF },	/* 27 (arm,i386,m68k,ppc,alpha,sparc*), 29 (mips) */
#endif
#ifdef SIGPWR
	{ "PWR",	SIGPWR },	/* 30 (arm,i386,m68k,ppc), 29 (alpha,sparc*), 19 (mips) */
#endif
#ifdef SIGINFO
	{ "INFO",	SIGINFO },	/* 29 (alpha) */
#endif
#ifdef SIGLOST
	{ "LOST",	SIGLOST }, 	/* 29 (arm,i386,m68k,ppc,sparc*) */
#endif
#ifdef SIGWINCH
	{ "WINCH",	SIGWINCH },	/* 28 (arm,i386,m68k,ppc,alpha,sparc*), 20 (mips) */
#endif
#ifdef SIGUNUSED
	{ "UNUSED",	SIGUNUSED },	/* 31 (arm,i386,m68k,ppc) */
#endif
};

int main (int argc, char *argv[]);
extern char *mybasename(char *);
int signame_to_signum (char *sig);
int arg_to_signum (char *arg, int mask);
void nosig (char *name);
void printsig (int sig);
void printsignals (FILE *fp);
int usage (int status);
int kill_verbose (char *procname, int pid, int sig);

extern int *get_pids (char *, int);

static char *progname;

#ifdef HAVE_SIGQUEUE
static int use_sigval;
static union sigval sigdata;
#endif

int main (int argc, char *argv[])
{
    int errors, numsig, pid;
    char *ep, *arg, *p;
    int do_pid, do_kill, check_all;
    int *pids, *ip;

    progname = argv[0];
    if ((p = strrchr(progname, '/')) != NULL)
	    progname = p+1;

    setlocale(LC_ALL, "");
    bindtextdomain(PACKAGE, LOCALEDIR);
    textdomain(PACKAGE);

    numsig = SIGTERM;
    do_pid = (! strcmp (progname, "pid")); 	/* Yecch */
    do_kill = 0;
    check_all = 0;

    /*  loop through the arguments.
	actually, -a is the only option can be used with other options.
	`kill' is basically a one-option-at-most program.  */
    for (argc--, argv++; argc > 0; argc--, argv++) {
	arg = *argv;
	if (*arg != '-') {
	    break;
	}
	if (! strcmp (arg, "--")) {
	    argc--, argv++;
	    break;
	}
	if (! strcmp (arg, "-v") || ! strcmp (arg, "-V") ||
	    ! strcmp (arg, "--version")) {
	    printf(_("%s from %s\n"), progname, PACKAGE_STRING);
	    return 0;
	}
	if (! strcmp (arg, "-a")) {
	    check_all++;
	    continue;
	}
	if (! strcmp (arg, "-l")) {
	    if (argc < 2) {
		printsignals (stdout);
		return 0;
	    }
	    if (argc > 2) {
		return usage (1);
	    }
	    /* argc == 2, accept "kill -l $?" */
	    arg = argv[1];
	    if ((numsig = arg_to_signum (arg, 1)) < 0) {
		fprintf (stderr, _("%s: unknown signal %s\n"), progname, arg);
		return 1;
	    }
	    printsig (numsig);
	    return 0;
	}
	if (! strcmp (arg, "-p")) {
	    do_pid++;
	    if (do_kill)
		return usage (1);
	    continue;
	}
	if (! strcmp (arg, "-s")) {
	    if (argc < 2) {
		return usage (1);
	    }
	    do_kill++;
	    if (do_pid)
		return usage (1);
	    argc--, argv++;
	    arg = *argv;
	    if ((numsig = arg_to_signum (arg, 0)) < 0) {
		nosig (arg);
		return 1;
	    }
	    continue;
	}
	if (! strcmp (arg, "-q")) {
	    if (argc < 2)
		return usage (1);
	    argc--, argv++;
	    arg = *argv;
#ifdef HAVE_SIGQUEUE
	    sigdata.sival_int = strtol_or_err(arg, _("failed to parse sigval"));
	    use_sigval = 1;
#endif
	    continue;
	}
	/*  `arg' begins with a dash but is not a known option.
	    so it's probably something like -HUP, or -1/-n
	    try to deal with it.
	    -n could be signal n, or pid -n (i.e. process group n).
	    In case of doubt POSIX tells us to assume a signal.
	    If a signal has been parsed, assume it's a pid, break */
	if (do_kill)
	  break;
	arg++;
	if ((numsig = arg_to_signum (arg, 0)) < 0) {
	    return usage (1);
	}
	do_kill++;
	if (do_pid)
	    return usage (1);
	continue;
    }

    if (! *argv) {
	return usage (1);
    }
    if (do_pid) {
	numsig = -1;
    }

    /*  we're done with the options.
	the rest of the arguments should be process ids and names.
	kill them.  */
    for (errors = 0; (arg = *argv) != NULL; argv++) {
	pid = strtol (arg, &ep, 10);
	if (! *ep)
	    errors += kill_verbose (arg, pid, numsig);
	else {
	    pids = get_pids (arg, check_all);
	    if (! pids) {
		errors++;
		fprintf (stderr, _("%s: can't find process \"%s\"\n"),
			 progname, arg);
		continue;
	    }
	    for (ip = pids; *ip >= 0; ip++)
		errors += kill_verbose (arg, *ip, numsig);
	    free (pids);
	}
    }
    return (errors);
}

#ifdef SIGRTMIN
int rtsig_to_signum(char *sig)
{
	int num, maxi = 0;
	char *ep = NULL;

	if (strncasecmp(sig, "min+", 4) == 0)
		sig += 4;
	else if (strncasecmp(sig, "max-", 4) == 0) {
		sig += 4;
		maxi = 1;
	}

	if (!isdigit(*sig))
		return -1;

	errno = 0;
	num = strtol(sig, &ep, 10);
	if (!ep || sig == ep || errno || num < 0)
		return -1;

	num = maxi ? SIGRTMAX - num : SIGRTMIN + num;

	if (num < SIGRTMIN || num > SIGRTMAX)
		return -1;

	return num;
}
#endif

int signame_to_signum (char *sig)
{
    size_t n;

    if (! strncasecmp (sig, "sig", 3))
	sig += 3;

#ifdef SIGRTMIN
    /* RT signals */
    if (!strncasecmp(sig, "rt", 2))
	return rtsig_to_signum(sig + 2);
#endif
    /* Normal sugnals */
    for (n = 0; n < ARRAY_SIZE(sys_signame); n++) {
	if (! strcasecmp (sys_signame[n].name, sig))
	    return sys_signame[n].val;
    }
    return (-1);
}

int arg_to_signum (char *arg, int maskbit)
{
    int numsig;
    char *ep;

    if (isdigit (*arg)) {
	numsig = strtol (arg, &ep, 10);
	if (numsig >= NSIG && maskbit && (numsig & 128) != 0)
	    numsig -= 128;
	if (*ep != 0 || numsig < 0 || numsig >= NSIG)
	    return (-1);
	return (numsig);
    }
    return (signame_to_signum (arg));
}

void nosig (char *name)
{
    fprintf (stderr, _("%s: unknown signal %s; valid signals:\n"), progname, name);
    printsignals (stderr);
}

void printsig (int sig)
{
    size_t n;

    for (n = 0; n < ARRAY_SIZE(sys_signame); n++) {
	if (sys_signame[n].val == sig) {
	    printf ("%s\n", sys_signame[n].name);
	    return;
	}
    }
#ifdef SIGRTMIN
    if (sig >= SIGRTMIN && sig <= SIGRTMAX) {
	printf ("RT%d\n", sig - SIGRTMIN);
	return;
    }
#endif
    printf("%d\n", sig);
}

void printsignals (FILE *fp)
{
    size_t n, lth, lpos = 0;

    for (n = 0; n < ARRAY_SIZE(sys_signame); n++) {
	lth = 1+strlen(sys_signame[n].name);
	if (lpos+lth > 72) {
	    fputc ('\n', fp);
	    lpos = 0;
	} else if (lpos)
	    fputc (' ', fp);
	lpos += lth;
	fputs (sys_signame[n].name, fp);
    }
#ifdef SIGRTMIN
    fputs (" RT<N> RTMIN+<N> RTMAX-<N>", fp);
#endif
    fputc ('\n', fp);
}

int usage (int status)
{
    FILE *fp;

    fp = (status == 0 ? stdout : stderr);
    fprintf (fp, _("usage: %s [ -s signal | -p ] [ -a ] pid ...\n"), progname);
    fprintf (fp, _("       %s -l [ signal ]\n"), progname);
    return status;
}

int kill_verbose (char *procname, int pid, int sig)
{
    int rc;

    if (sig < 0) {
	printf ("%d\n", pid);
	return 0;
    }
#ifdef HAVE_SIGQUEUE
    if (use_sigval)
	rc = sigqueue(pid, sig, sigdata);
    else
#endif
	rc = kill (pid, sig);

    if (rc < 0) {
	fprintf (stderr, "%s ", progname);
	perror (procname);
	return 1;
    }
    return 0;
}
