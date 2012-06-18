/*
 * compat.c
 *
 * Compatibility functions for different OSes
 *
 * $Id: compat.c,v 1.6 2005/08/22 00:48:34 quozl Exp $
 */

#if HAVE_CONFIG_H
#include "config.h"
#endif

#include "compat.h"

#ifndef HAVE_STRLCPY
#include <string.h>
#include <stdio.h>

void strlcpy(char *dst, const char *src, size_t size)
{
	strncpy(dst, src, size - 1);
	dst[size - 1] = '\0';
}
#endif

#ifndef HAVE_MEMMOVE
void *memmove(void *dst, const void *src, size_t size)
{
	bcopy(src, dst, size);
	return dst;
}
#endif

#ifndef HAVE_OPENPTY
/*
 * Finds a free PTY/TTY pair.
 *
 * This is derived from C.S. Ananian's pty.c that was with his pptp client.
 *
 *************************************************************************
 * pty.c - find a free pty/tty pair.
 *         inspired by the xterm source.
 *         NOTE: This is very likely to be highly non-portable.
 *         C. Scott Ananian <cananian@alumni.princeton.edu>
 *
 * Heavily modified to chage from getpseudopty() to openpty().
 */

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#if HAVE_SYSLOG_H
#include <syslog.h>
#else
#include "our_syslog.h"
#endif

int openpty(int *master, int *slave, char *name, void *unused1, void *unused2)
{
	int devindex = 0, letter = 0;
	int fd1, fd2;
	char ttydev[PTYMAX], ptydev[TTYMAX];

	syslog(LOG_DEBUG, "CTRL: Allocating pty/tty pair");
	strcpy(ttydev, TTYDEV);
	strcpy(ptydev, PTYDEV);
	while (PTYCHAR1[letter]) {
		ttydev[TTYMAX - 3] = ptydev[PTYMAX - 3] = PTYCHAR1[letter];
		while (PTYCHAR2[devindex]) {
			ttydev[TTYMAX - 2] = ptydev[PTYMAX - 2] = PTYCHAR2[devindex];
			if ((fd1 = open(ptydev, O_RDWR)) >= 0) {
				if ((fd2 = open(ttydev, O_RDWR)) >= 0) {
					goto out;
				} else {
					close(fd1);
				}
			}
			devindex++;
		}
		devindex = 0;
		letter++;
	}
	syslog(LOG_ERR, "CTRL: Failed to allocate pty");
	return -1;		/* Unable to allocate pty */

      out:
	syslog(LOG_INFO, "CTRL: Allocated pty/tty pair (%s,%s)", ptydev, ttydev);
	if (master)
		*master = fd1;
	if (slave)
		*slave = fd2;
	if (name)
		strcpy(name, ttydev);	/* no way to bounds check */
	return 0;
}
#endif

#ifndef HAVE_STRERROR
char *strerror(int errnum) {
	static char buf[16];
	sprintf(buf, "Error %d", errnum);
	return buf;
}
#endif

#ifndef HAVE_SETPROCTITLE
#include "inststr.h"
#endif

#define __USE_BSD 1
#include <stdarg.h>
#include <stdio.h>

void my_setproctitle(int argc, char **argv, const char *format, ...) {
       char proctitle[64];
       va_list parms;
       va_start(parms, format);
       vsnprintf(proctitle, sizeof(proctitle), format, parms);

#ifndef HAVE_SETPROCTITLE
       inststr(argc, argv, proctitle);
#else
       setproctitle(proctitle);
#endif
       va_end(parms);
}

/* signal to pipe delivery implementation */
#include <unistd.h>
#include <fcntl.h>
#include <signal.h>

/* pipe private to process */
static int sigpipe[2];

/* create a signal pipe, returns 0 for success, -1 with errno for failure */
int sigpipe_create()
{
  int rc;
  
  rc = pipe(sigpipe);
  if (rc < 0) return rc;
  
  fcntl(sigpipe[0], F_SETFD, FD_CLOEXEC);
  fcntl(sigpipe[1], F_SETFD, FD_CLOEXEC);
  
#ifdef O_NONBLOCK
#define FLAG_TO_SET O_NONBLOCK
#else
#ifdef SYSV
#define FLAG_TO_SET O_NDELAY
#else /* BSD */
#define FLAG_TO_SET FNDELAY
#endif
#endif
  
  rc = fcntl(sigpipe[1], F_GETFL);
  if (rc != -1)
    rc = fcntl(sigpipe[1], F_SETFL, rc | FLAG_TO_SET);
  if (rc < 0) return rc;
  return 0;
#undef FLAG_TO_SET
}

/* generic handler for signals, writes signal number to pipe */
void sigpipe_handler(int signum)
{
  write(sigpipe[1], &signum, sizeof(signum));
  signal(signum, sigpipe_handler);
}

/* assign a signal number to the pipe */
void sigpipe_assign(int signum)
{
  struct sigaction sa;

  memset(&sa, 0, sizeof(sa));
  sa.sa_handler = sigpipe_handler;
  sigaction(signum, &sa, NULL);
}

/* return the signal pipe read file descriptor for select(2) */
int sigpipe_fd()
{
  return sigpipe[0];
}

/* read and return the pending signal from the pipe */
int sigpipe_read()
{
  int signum;
  read(sigpipe[0], &signum, sizeof(signum));
  return signum;
}

void sigpipe_close()
{
  close(sigpipe[0]);
  close(sigpipe[1]);
}

