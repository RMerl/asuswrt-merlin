/* vi: set sw=4 ts=4: */
/*
 * script implementation for busybox
 *
 * pascal.bellard@ads-lu.com
 *
 * Based on code from util-linux v 2.12r
 * Copyright (c) 1980
 * The Regents of the University of California.  All rights reserved.
 *
 * Licensed under GPLv2 or later, see file LICENSE in this source tree.
 */

//usage:#define script_trivial_usage
//usage:       "[-afq" IF_SCRIPTREPLAY("t") "] [-c PROG] [OUTFILE]"
//usage:#define script_full_usage "\n\n"
//usage:       "	-a	Append output"
//usage:     "\n	-c PROG	Run PROG, not shell"
//usage:     "\n	-f	Flush output after each write"
//usage:     "\n	-q	Quiet"
//usage:	IF_SCRIPTREPLAY(
//usage:     "\n	-t	Send timing to stderr"
//usage:	)

#include "libbb.h"
#include "common_bufsiz.h"

int script_main(int argc, char **argv) MAIN_EXTERNALLY_VISIBLE;
int script_main(int argc UNUSED_PARAM, char **argv)
{
	int opt;
	int mode;
	int child_pid;
	int attr_ok; /* NB: 0: ok */
	int winsz_ok;
	int pty;
	char pty_line[GETPTY_BUFSIZE];
	struct termios tt, rtt;
	struct winsize win;
	const char *fname = "typescript";
	const char *shell;
	char shell_opt[] = "-i";
	char *shell_arg = NULL;
	enum {
		OPT_a = (1 << 0),
		OPT_c = (1 << 1),
		OPT_f = (1 << 2),
		OPT_q = (1 << 3),
		OPT_t = (1 << 4),
	};

#if ENABLE_LONG_OPTS
	static const char getopt_longopts[] ALIGN1 =
		"append\0"  No_argument       "a"
		"command\0" Required_argument "c"
		"flush\0"   No_argument       "f"
		"quiet\0"   No_argument       "q"
		IF_SCRIPTREPLAY("timing\0" No_argument "t")
		;

	applet_long_options = getopt_longopts;
#endif

	opt_complementary = "?1"; /* max one arg */
	opt = getopt32(argv, "ac:fq" IF_SCRIPTREPLAY("t") , &shell_arg);
	//argc -= optind;
	argv += optind;
	if (argv[0]) {
		fname = argv[0];
	}
	mode = O_CREAT|O_TRUNC|O_WRONLY;
	if (opt & OPT_a) {
		mode = O_CREAT|O_APPEND|O_WRONLY;
	}
	if (opt & OPT_c) {
		shell_opt[1] = 'c';
	}
	if (!(opt & OPT_q)) {
		printf("Script started, file is %s\n", fname);
	}

	shell = get_shell_name();

	/* Some people run "script ... 0>&-".
	 * Our code assumes that STDIN_FILENO != pty.
	 * Ensure STDIN_FILENO is not closed:
	 */
	bb_sanitize_stdio();

	pty = xgetpty(pty_line);

	/* get current stdin's tty params */
	attr_ok = tcgetattr(0, &tt);
	winsz_ok = ioctl(0, TIOCGWINSZ, (char *)&win);

	rtt = tt;
	cfmakeraw(&rtt);
	rtt.c_lflag &= ~ECHO;
	tcsetattr(0, TCSAFLUSH, &rtt);

	/* "script" from util-linux exits when child exits,
	 * we wouldn't wait for EOF from slave pty
	 * (output may be produced by grandchildren of child) */
	signal(SIGCHLD, record_signo);

	/* TODO: SIGWINCH? pass window size changes down to slave? */

	child_pid = xvfork();

	if (child_pid) {
		/* parent */
		struct pollfd pfd[2];
		int outfd, count, loop;
		double oldtime = ENABLE_SCRIPTREPLAY ? time(NULL) : 0;
		smallint fd_count = 2;
#define buf bb_common_bufsiz1
		setup_common_bufsiz();

		outfd = xopen(fname, mode);
		pfd[0].fd = pty;
		pfd[0].events = POLLIN;
		pfd[1].fd = STDIN_FILENO;
		pfd[1].events = POLLIN;
		ndelay_on(pty); /* this descriptor is not shared, can do this */
		/* ndelay_on(STDIN_FILENO); - NO, stdin can be shared! Pity :( */

		/* copy stdin to pty master input,
		 * copy pty master output to stdout and file */
		/* TODO: don't use full_write's, use proper write buffering */
		while (fd_count && !bb_got_signal) {
			/* not safe_poll! we want SIGCHLD to EINTR poll */
			if (poll(pfd, fd_count, -1) < 0 && errno != EINTR) {
				/* If child exits too quickly, we may get EIO:
				 * for example, try "script -c true" */
				break;
			}
			if (pfd[0].revents) {
				errno = 0;
				count = safe_read(pty, buf, COMMON_BUFSIZE);
				if (count <= 0 && errno != EAGAIN) {
					/* err/eof from pty: exit */
					goto restore;
				}
				if (count > 0) {
					if (ENABLE_SCRIPTREPLAY && (opt & OPT_t)) {
						struct timeval tv;
						double newtime;

						gettimeofday(&tv, NULL);
						newtime = tv.tv_sec + (double) tv.tv_usec / 1000000;
						fprintf(stderr, "%f %u\n", newtime - oldtime, count);
						oldtime = newtime;
					}
					full_write(STDOUT_FILENO, buf, count);
					full_write(outfd, buf, count);
					if (opt & OPT_f) {
						fsync(outfd);
					}
				}
			}
			if (pfd[1].revents) {
				count = safe_read(STDIN_FILENO, buf, COMMON_BUFSIZE);
				if (count <= 0) {
					/* err/eof from stdin: don't read stdin anymore */
					pfd[1].revents = 0;
					fd_count--;
				} else {
					full_write(pty, buf, count);
				}
			}
		}
		/* If loop was exited because SIGCHLD handler set bb_got_signal,
		 * there still can be some buffered output. But dont loop forever:
		 * we won't pump orphaned grandchildren's output indefinitely.
		 * Testcase: running this in script:
		 *      exec dd if=/dev/zero bs=1M count=1
		 * must have "1+0 records in, 1+0 records out" captured too.
		 * (util-linux's script doesn't do this. buggy :) */
		loop = 999;
		/* pty is in O_NONBLOCK mode, we exit as soon as buffer is empty */
		while (--loop && (count = safe_read(pty, buf, COMMON_BUFSIZE)) > 0) {
			full_write(STDOUT_FILENO, buf, count);
			full_write(outfd, buf, count);
		}
 restore:
		if (attr_ok == 0)
			tcsetattr(0, TCSAFLUSH, &tt);
		if (!(opt & OPT_q))
			printf("Script done, file is %s\n", fname);
		return EXIT_SUCCESS;
	}

	/* child: make pty slave to be input, output, error; run shell */
	close(pty); /* close pty master */
	/* open pty slave to fd 0,1,2 */
	close(0);
	xopen(pty_line, O_RDWR); /* uses fd 0 */
	xdup2(0, 1);
	xdup2(0, 2);
	/* copy our original stdin tty's parameters to pty */
	if (attr_ok == 0)
		tcsetattr(0, TCSAFLUSH, &tt);
	if (winsz_ok == 0)
		ioctl(0, TIOCSWINSZ, (char *)&win);
	/* set pty as a controlling tty */
	setsid();
	ioctl(0, TIOCSCTTY, 0 /* 0: don't forcibly steal */);

	/* Non-ignored signals revert to SIG_DFL on exec anyway */
	/*signal(SIGCHLD, SIG_DFL);*/
	execl(shell, shell, shell_opt, shell_arg, (char *) NULL);
	bb_simple_perror_msg_and_die(shell);
}
