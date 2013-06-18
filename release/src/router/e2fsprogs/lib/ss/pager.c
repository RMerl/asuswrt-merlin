/*
 * Pager: Routines to create a "more" running out of a particular file
 * descriptor.
 *
 * Copyright 1987, 1988 by MIT Student Information Processing Board
 *
 * Permission to use, copy, modify, and distribute this software and
 * its documentation for any purpose is hereby granted, provided that
 * the names of M.I.T. and the M.I.T. S.I.P.B. not be used in
 * advertising or publicity pertaining to distribution of the software
 * without specific, written prior permission.  M.I.T. and the
 * M.I.T. S.I.P.B. make no representations about the suitability of
 * this software for any purpose.  It is provided "as is" without
 * express or implied warranty.
 */

#include "config.h"
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif
#ifdef HAVE_ERRNO_H
#include <errno.h>
#else
extern int errno;
#endif

#include "ss_internal.h"
#include <stdio.h>
#include <sys/types.h>
#include <sys/file.h>
#include <signal.h>
#ifdef HAVE_SYS_PRCTL_H
#include <sys/prctl.h>
#else
#define PR_GET_DUMPABLE 3
#endif
#if (!defined(HAVE_PRCTL) && defined(linux))
#include <sys/syscall.h>
#endif

static char MORE[] = "more";
extern char *_ss_pager_name;
extern char *getenv PROTOTYPE((const char *));

char *ss_safe_getenv(const char *arg)
{
	if ((getuid() != geteuid()) || (getgid() != getegid()))
		return NULL;
#if HAVE_PRCTL
	if (prctl(PR_GET_DUMPABLE, 0, 0, 0, 0) == 0)
		return NULL;
#else
#if (defined(linux) && defined(SYS_prctl))
	if (syscall(SYS_prctl, PR_GET_DUMPABLE, 0, 0, 0, 0) == 0)
		return NULL;
#endif
#endif

#ifdef HAVE___SECURE_GETENV
	return __secure_getenv(arg);
#else
	return getenv(arg);
#endif
}

/*
 * this needs a *lot* of work....
 *
 * run in same process
 * handle SIGINT sensibly
 * allow finer control -- put-page-break-here
 */

#ifndef NO_FORK
int ss_pager_create(void)
{
	int filedes[2];

	if (pipe(filedes) != 0)
		return(-1);

	switch(fork()) {
	case -1:
		return(-1);
	case 0:
		/*
		 * Child; dup read half to 0, close all but 0, 1, and 2
		 */
		if (dup2(filedes[0], 0) == -1)
			exit(1);
		ss_page_stdin();
	default:
		/*
		 * Parent:  close "read" side of pipe, return
		 * "write" side.
		 */
		(void) close(filedes[0]);
		return(filedes[1]);
	}
}
#else /* don't fork */
int ss_pager_create()
{
    int fd;
    fd = open("/dev/tty", O_WRONLY, 0);
    return fd;
}
#endif

static int write_all(int fd, char *buf, size_t count)
{
	ssize_t ret;
	int c = 0;

	while (count > 0) {
		ret = write(fd, buf, count);
		if (ret < 0) {
			if ((errno == EAGAIN) || (errno == EINTR))
				continue;
			return -1;
		}
		count -= ret;
		buf += ret;
		c += ret;
	}
	return c;
}

void ss_page_stdin()
{
	int i;
	sigset_t mask;

	for (i = 3; i < 32; i++)
		(void) close(i);
	(void) signal(SIGINT, SIG_DFL);
	sigprocmask(SIG_BLOCK, 0, &mask);
	sigdelset(&mask, SIGINT);
	sigprocmask(SIG_SETMASK, &mask, 0);
	if (_ss_pager_name == (char *)NULL) {
		if ((_ss_pager_name = ss_safe_getenv("PAGER")) == (char *)NULL)
			_ss_pager_name = MORE;
	}
	(void) execlp(_ss_pager_name, _ss_pager_name, (char *) NULL);
	{
		/* minimal recovery if pager program isn't found */
		char buf[80];
		register int n;
		while ((n = read(0, buf, 80)) > 0)
			write_all(1, buf, n);
	}
	exit(errno);
}
