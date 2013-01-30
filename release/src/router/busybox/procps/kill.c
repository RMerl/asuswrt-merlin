/* vi: set sw=4 ts=4: */
/*
 * Mini kill/killall[5] implementation for busybox
 *
 * Copyright (C) 1995, 1996 by Bruce Perens <bruce@pixar.com>.
 * Copyright (C) 1999-2004 by Erik Andersen <andersen@codepoet.org>
 *
 * Licensed under the GPL v2 or later, see the file LICENSE in this tarball.
 */

#include "libbb.h"

/* Note: kill_main is directly called from shell in order to implement
 * kill built-in. Shell substitutes job ids with process groups first.
 *
 * This brings some complications:
 *
 * + we can't use xfunc here
 * + we can't use applet_name
 * + we can't use bb_show_usage
 * (Above doesn't apply for killall[5] cases)
 *
 * kill %n gets translated into kill ' -<process group>' by shell (note space!)
 * This is needed to avoid collision with kill -9 ... syntax
 */

int kill_main(int argc, char **argv)
{
	char *arg;
	pid_t pid;
	int signo = SIGTERM, errors = 0, quiet = 0;
#if !ENABLE_KILLALL && !ENABLE_KILLALL5
#define killall 0
#define killall5 0
#else
/* How to determine who we are? find 3rd char from the end:
 * kill, killall, killall5
 *  ^i       ^a        ^l  - it's unique
 * (checking from the start is complicated by /bin/kill... case) */
	const char char3 = argv[0][strlen(argv[0]) - 3];
#define killall (ENABLE_KILLALL && char3 == 'a')
#define killall5 (ENABLE_KILLALL5 && char3 == 'l')
#endif

	/* Parse any options */
	argc--;
	arg = *++argv;

	if (argc < 1 || arg[0] != '-') {
		goto do_it_now;
	}

	/* The -l option, which prints out signal names.
	 * Intended usage in shell:
	 * echo "Died of SIG`kill -l $?`"
	 * We try to mimic what kill from coreutils-6.8 does */
	if (arg[1] == 'l' && arg[2] == '\0') {
		if (argc == 1) {
			/* Print the whole signal list */
			print_signames();
			return 0;
		}
		/* -l <sig list> */
		while ((arg = *++argv)) {
			if (isdigit(arg[0])) {
				signo = bb_strtou(arg, NULL, 10);
				if (errno) {
					bb_error_msg("unknown signal '%s'", arg);
					return EXIT_FAILURE;
				}
				/* Exitcodes >= 0x80 are to be treated
				 * as "killed by signal (exitcode & 0x7f)" */
				puts(get_signame(signo & 0x7f));
				/* TODO: 'bad' signal# - coreutils says:
				 * kill: 127: invalid signal
				 * we just print "127" instead */
			} else {
				signo = get_signum(arg);
				if (signo < 0) {
					bb_error_msg("unknown signal '%s'", arg);
					return EXIT_FAILURE;
				}
				printf("%d\n", signo);
			}
		}
		/* If they specified -l, we are all done */
		return EXIT_SUCCESS;
	}

	/* The -q quiet option */
	if (killall && arg[1] == 'q' && arg[2] == '\0') {
		quiet = 1;
		arg = *++argv;
		argc--;
		if (argc < 1)
			bb_show_usage();
		if (arg[0] != '-')
			goto do_it_now;
	}

	arg++; /* skip '-' */

	/* -o PID? (if present, it always is at the end of command line) */
	if (killall5 && arg[0] == 'o')
		goto do_it_now;

	if (argc > 1 && arg[0] == 's' && arg[1] == '\0') { /* -s SIG? */
		argc--;
		arg = *++argv;
	} /* else it must be -SIG */
	signo = get_signum(arg);
	if (signo < 0) { /* || signo > MAX_SIGNUM ? */
		bb_error_msg("bad signal name '%s'", arg);
		return EXIT_FAILURE;
	}
	arg = *++argv;
	argc--;

 do_it_now:
	pid = getpid();

	if (killall5) {
		pid_t sid;
		procps_status_t* p = NULL;
		int ret = 0;

		/* Find out our session id */
		sid = getsid(pid);
		/* Stop all processes */
		kill(-1, SIGSTOP);
		/* Signal all processes except those in our session */
		while ((p = procps_scan(p, PSSCAN_PID|PSSCAN_SID))) {
			int i;

			if (p->sid == (unsigned)sid
			 || p->pid == (unsigned)pid
			 || p->pid == 1)
				continue;

			/* All remaining args must be -o PID options.
			 * Check p->pid against them. */
			for (i = 0; i < argc; i++) {
				pid_t omit;

				arg = argv[i];
				if (arg[0] != '-' || arg[1] != 'o') {
					bb_error_msg("bad option '%s'", arg);
					ret = 1;
					goto resume;
				}
				arg += 2;
				if (!arg[0] && argv[++i])
					arg = argv[i];
				omit = bb_strtoi(arg, NULL, 10);
				if (errno) {
					bb_error_msg("bad pid '%s'", arg);
					ret = 1;
					goto resume;
				}
				if (p->pid == omit)
					goto dont_kill;
			}
			kill(p->pid, signo);
 dont_kill: ;
		}
 resume:
		/* And let them continue */
		kill(-1, SIGCONT);
		return ret;
	}

	/* Pid or name is required for kill/killall */
	if (argc < 1) {
		bb_error_msg("you need to specify whom to kill");
		return EXIT_FAILURE;
	}

	if (killall) {
		/* Looks like they want to do a killall.  Do that */
		while (arg) {
			pid_t* pidList;

			pidList = find_pid_by_name(arg);
			if (*pidList == 0) {
				errors++;
				if (!quiet)
					bb_error_msg("%s: no process killed", arg);
			} else {
				pid_t *pl;

				for (pl = pidList; *pl; pl++) {
					if (*pl == pid)
						continue;
					if (kill(*pl, signo) == 0)
						continue;
					errors++;
					if (!quiet)
						bb_perror_msg("can't kill pid %d", (int)*pl);
				}
			}
			free(pidList);
			arg = *++argv;
		}
		return errors;
	}

	/* Looks like they want to do a kill. Do that */
	while (arg) {
		/* Support shell 'space' trick */
		if (arg[0] == ' ')
			arg++;
		pid = bb_strtoi(arg, NULL, 10);
		if (errno) {
			bb_error_msg("bad pid '%s'", arg);
			errors++;
		} else if (kill(pid, signo) != 0) {
			bb_perror_msg("can't kill pid %d", (int)pid);
			errors++;
		}
		arg = *++argv;
	}
	return errors;
}
