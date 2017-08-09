/* Copyright (C) 1992-1998 Free Software Foundation, Inc.
This file is part of the GNU C Library.

The GNU C Library is free software; you can redistribute it and/or
modify it under the terms of the GNU Lesser General Public License as
published by the Free Software Foundation; either version 3 of the
License, or (at your option) any later version.

The GNU C Library is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
Library General Public License for more details.

You should have received a copy of the GNU Lesser General Public
License along with the GNU C Library; see the file COPYING.LIB.  If
not, see <http://www.gnu.org/licenses/>.  */

/* Modified to use with samba by Jeremy Allison, 8th July 1995. */

#include "replace.h"
#include "system/filesys.h"
#include "system/wait.h"
#include "system/terminal.h"
#include "system/passwd.h"

/*
 * Define additional missing types
 */
#ifndef HAVE_SIG_ATOMIC_T_TYPE
typedef int sig_atomic_t;
#endif

#ifndef SIGCLD
#define SIGCLD SIGCHLD
#endif

#ifdef SYSV_TERMIO 

/* SYSTEM V TERMIO HANDLING */

static struct termio t;

#define ECHO_IS_ON(t) ((t).c_lflag & ECHO)
#define TURN_ECHO_OFF(t) ((t).c_lflag &= ~ECHO)
#define TURN_ECHO_ON(t) ((t).c_lflag |= ECHO)

#ifndef TCSAFLUSH
#define TCSAFLUSH 1
#endif

#ifndef TCSANOW
#define TCSANOW 0
#endif

static int tcgetattr(int fd, struct termio *_t)
{
	return ioctl(fd, TCGETA, _t);
}

static int tcsetattr(int fd, int flags, struct termio *_t)
{
	if(flags & TCSAFLUSH)
		ioctl(fd, TCFLSH, TCIOFLUSH);
	return ioctl(fd, TCSETS, _t);
}

#elif !defined(TCSAFLUSH)

/* BSD TERMIO HANDLING */

static struct sgttyb t;  

#define ECHO_IS_ON(t) ((t).sg_flags & ECHO)
#define TURN_ECHO_OFF(t) ((t).sg_flags &= ~ECHO)
#define TURN_ECHO_ON(t) ((t).sg_flags |= ECHO)

#define TCSAFLUSH 1
#define TCSANOW 0

static int tcgetattr(int fd, struct sgttyb *_t)
{
	return ioctl(fd, TIOCGETP, (char *)_t);
}

static int tcsetattr(int fd, int flags, struct sgttyb *_t)
{
	return ioctl(fd, TIOCSETP, (char *)_t);
}

#else /* POSIX TERMIO HANDLING */
#define ECHO_IS_ON(t) ((t).c_lflag & ECHO)
#define TURN_ECHO_OFF(t) ((t).c_lflag &= ~ECHO)
#define TURN_ECHO_ON(t) ((t).c_lflag |= ECHO)

static struct termios t;
#endif /* SYSV_TERMIO */

static void catch_signal(int signum, void (*handler)(int ))
{
#ifdef HAVE_SIGACTION
	struct sigaction act;
	struct sigaction oldact;

	memset(&act, 0, sizeof(act));

	act.sa_handler = handler;
#ifdef SA_RESTART
	/*
	 * We *want* SIGALRM to interrupt a system call.
	 */
	if(signum != SIGALRM)
		act.sa_flags = SA_RESTART;
#endif
	sigemptyset(&act.sa_mask);
	sigaddset(&act.sa_mask,signum);
	sigaction(signum,&act,&oldact);
#else /* !HAVE_SIGACTION */
	/* FIXME: need to handle sigvec and systems with broken signal() */
	signal(signum, handler);
#endif
}

static sig_atomic_t gotintr;
static int in_fd = -1;

/***************************************************************
 Signal function to tell us were ^C'ed.
****************************************************************/

static void gotintr_sig(int signum)
{
	gotintr = 1;
	if (in_fd != -1)
		close(in_fd); /* Safe way to force a return. */
	in_fd = -1;
}

char *rep_getpass(const char *prompt)
{
	FILE *in, *out;
	int echo_off;
	static char buf[256];
	static size_t bufsize = sizeof(buf);
	size_t nread;

	/* Catch problematic signals */
	catch_signal(SIGINT, gotintr_sig);

	/* Try to write to and read from the terminal if we can.
		If we can't open the terminal, use stderr and stdin.  */

	in = fopen ("/dev/tty", "w+");
	if (in == NULL) {
		in = stdin;
		out = stderr;
	} else {
		out = in;
	}

	setvbuf(in, NULL, _IONBF, 0);

	/* Turn echoing off if it is on now.  */

	if (tcgetattr (fileno (in), &t) == 0) {
		if (ECHO_IS_ON(t)) {
			TURN_ECHO_OFF(t);
			echo_off = tcsetattr (fileno (in), TCSAFLUSH, &t) == 0;
			TURN_ECHO_ON(t);
		} else {
			echo_off = 0;
		}
	} else {
		echo_off = 0;
	}

	/* Write the prompt.  */
	fputs(prompt, out);
	fflush(out);

	/* Read the password.  */
	buf[0] = 0;
	if (!gotintr) {
		in_fd = fileno(in);
		if (fgets(buf, bufsize, in) == NULL) {
			buf[0] = 0;
		}
	}
	nread = strlen(buf);
	if (nread) {
		if (buf[nread - 1] == '\n')
			buf[nread - 1] = '\0';
	}

	/* Restore echoing.  */
	if (echo_off) {
		if (gotintr && in_fd == -1) {
			in = fopen ("/dev/tty", "w+");
		}
		if (in != NULL)
			tcsetattr (fileno (in), TCSANOW, &t);
	}

	fprintf(out, "\n");
	fflush(out);

	if (in && in != stdin) /* We opened the terminal; now close it.  */
		fclose(in);

	/* Catch problematic signals */
	catch_signal(SIGINT, SIG_DFL);

	if (gotintr) {
		printf("Interrupted by signal.\n");
		fflush(stdout);
		exit(1);
	}
	return buf;
}
