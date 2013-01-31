/* vi: set sw=4 ts=4: */
/*
 * nice implementation for busybox
 *
 * Copyright (C) 2005  Manuel Novoa III  <mjn3@codepoet.org>
 *
 * Licensed under GPLv2 or later, see file LICENSE in this source tree.
 */

//usage:#define nice_trivial_usage
//usage:       "[-n ADJUST] [PROG ARGS]"
//usage:#define nice_full_usage "\n\n"
//usage:       "Change scheduling priority, run PROG\n"
//usage:     "\n	-n ADJUST	Adjust priority by ADJUST"

#include <sys/resource.h>
#include "libbb.h"

int nice_main(int argc, char **argv) MAIN_EXTERNALLY_VISIBLE;
int nice_main(int argc, char **argv)
{
	int old_priority, adjustment;

	old_priority = getpriority(PRIO_PROCESS, 0);

	if (!*++argv) { /* No args, so (GNU) output current nice value. */
		printf("%d\n", old_priority);
		fflush_stdout_and_exit(EXIT_SUCCESS);
	}

	adjustment = 10;  /* Set default adjustment. */

	if (argv[0][0] == '-') {
		if (argv[0][1] == 'n') { /* -n */
			if (argv[0][2]) { /* -nNNNN (w/o space) */
				argv[0] += 2; argv--; argc++;
			}
		} else { /* -NNN (NNN may be negative) == -n NNN */
			argv[0] += 1; argv--; argc++;
		}
		if (argc < 4) {  /* Missing priority and/or utility! */
			bb_show_usage();
		}
		adjustment = xatoi_range(argv[1], INT_MIN/2, INT_MAX/2);
		argv += 2;
	}

	{  /* Set our priority. */
		int prio = old_priority + adjustment;

		if (setpriority(PRIO_PROCESS, 0, prio) < 0) {
			bb_perror_msg_and_die("setpriority(%d)", prio);
		}
	}

	BB_EXECVP_or_die(argv);
}
