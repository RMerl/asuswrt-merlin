/* vi: set sw=4 ts=4: */
/*
 * vlock implementation for busybox
 *
 * Copyright (C) 2000 by spoon <spoon@ix.netcom.com>
 * Written by spoon <spon@ix.netcom.com>
 *
 * Licensed under GPLv2 or later, see file LICENSE in this tarball for details.
 */

/* Shoutz to Michael K. Johnson <johnsonm@redhat.com>, author of the
 * original vlock.  I snagged a bunch of his code to write this
 * minimalistic vlock.
 */
/* Fixed by Erik Andersen to do passwords the tinylogin way...
 * It now works with md5, sha1, etc passwords. */

#include <sys/vt.h>
#include "libbb.h"

static void release_vt(int signo UNUSED_PARAM)
{
	/* If -a, param is 0, which means:
	 * "no, kernel, we don't allow console switch away from us!" */
	ioctl(STDIN_FILENO, VT_RELDISP, (unsigned long) !option_mask32);
}

static void acquire_vt(int signo UNUSED_PARAM)
{
	/* ACK to kernel that switch to console is successful */
	ioctl(STDIN_FILENO, VT_RELDISP, VT_ACKACQ);
}

int vlock_main(int argc, char **argv) MAIN_EXTERNALLY_VISIBLE;
int vlock_main(int argc UNUSED_PARAM, char **argv)
{
	struct vt_mode vtm;
	struct termios term;
	struct termios oterm;
	struct vt_mode ovtm;
	struct passwd *pw;

	pw = xgetpwuid(getuid());
	opt_complementary = "=0"; /* no params! */
	getopt32(argv, "a");

	/* Ignore some signals so that we don't get killed by them */
	bb_signals(0
		+ (1 << SIGTSTP)
		+ (1 << SIGTTIN)
		+ (1 << SIGTTOU)
		+ (1 << SIGHUP )
		+ (1 << SIGCHLD) /* paranoia :) */
		+ (1 << SIGQUIT)
		+ (1 << SIGINT )
		, SIG_IGN);

	/* We will use SIGUSRx for console switch control: */
	/* 1: set handlers */
	signal_SA_RESTART_empty_mask(SIGUSR1, release_vt);
	signal_SA_RESTART_empty_mask(SIGUSR2, acquire_vt);
	/* 2: unmask them */
	sig_unblock(SIGUSR1);
	sig_unblock(SIGUSR2);

	/* Revert stdin/out to our controlling tty
	 * (or die if we have none) */
	xmove_fd(xopen(CURRENT_TTY, O_RDWR), STDIN_FILENO);
	xdup2(STDIN_FILENO, STDOUT_FILENO);

	xioctl(STDIN_FILENO, VT_GETMODE, &vtm);
	ovtm = vtm;
	/* "console switches are controlled by us, not kernel!" */
	vtm.mode = VT_PROCESS;
	vtm.relsig = SIGUSR1;
	vtm.acqsig = SIGUSR2;
	ioctl(STDIN_FILENO, VT_SETMODE, &vtm);

	tcgetattr(STDIN_FILENO, &oterm);
	term = oterm;
	term.c_iflag &= ~BRKINT;
	term.c_iflag |= IGNBRK;
	term.c_lflag &= ~ISIG;
	term.c_lflag &= ~(ECHO | ECHOCTL);
	tcsetattr_stdin_TCSANOW(&term);

	do {
		printf("Virtual console%s locked by %s.\n",
				option_mask32 /*o_lock_all*/ ? "s" : "",
				pw->pw_name);
		if (correct_password(pw)) {
			break;
		}
		bb_do_delay(FAIL_DELAY);
		puts("Password incorrect");
	} while (1);

	ioctl(STDIN_FILENO, VT_SETMODE, &ovtm);
	tcsetattr_stdin_TCSANOW(&oterm);
	fflush_stdout_and_exit(EXIT_SUCCESS);
}
