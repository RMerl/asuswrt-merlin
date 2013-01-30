/* vi: set sw=4 ts=4: */
/*
 * setsid.c -- execute a command in a new session
 * Rick Sladkey <jrs@world.std.com>
 * In the public domain.
 *
 * 1999-02-22 Arkadiusz Mickiewicz <misiek@pld.ORG.PL>
 * - added Native Language Support
 *
 * 2001-01-18 John Fremlin <vii@penguinpowered.com>
 * - fork in case we are process group leader
 *
 * 2004-11-12 Paul Fox
 * - busyboxed
 */

//usage:#define setsid_trivial_usage
//usage:       "PROG ARGS"
//usage:#define setsid_full_usage "\n\n"
//usage:       "Run PROG in a new session. PROG will have no controlling terminal\n"
//usage:       "and will not be affected by keyboard signals (Ctrl-C etc).\n"
//usage:       "See setsid(2) for details."

#include "libbb.h"

int setsid_main(int argc, char **argv) MAIN_EXTERNALLY_VISIBLE;
int setsid_main(int argc UNUSED_PARAM, char **argv)
{
	if (!argv[1])
		bb_show_usage();

	/* setsid() is allowed only when we are not a process group leader.
	 * Otherwise our PID serves as PGID of some existing process group
	 * and cannot be used as PGID of a new process group. */
	if (setsid() < 0) {
		pid_t pid = fork_or_rexec(argv);
		if (pid != 0) {
			/* parent */
			/* TODO:
			 * we can waitpid(pid, &status, 0) and then even
			 * emulate exitcode, making the behavior consistent
			 * in both forked and non forked cases.
			 * However, the code is larger and upstream
			 * does not do such trick.
			 */
			exit(EXIT_SUCCESS);
		}

		/* child */
		/* now there should be no error: */
		setsid();
	}

	argv++;
	BB_EXECVP_or_die(argv);
}
