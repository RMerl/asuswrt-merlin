/* vi: set sw=4 ts=4: */
/*
 * linux32/linux64 allows for changing uname emulation.
 *
 * Copyright 2002 Andi Kleen, SuSE Labs.
 *
 * Licensed under GPLv2 or later, see file LICENSE in this source tree.
 */

//usage:#define setarch_trivial_usage
//usage:       "personality PROG ARGS"
//usage:#define setarch_full_usage "\n\n"
//usage:       "Personality may be:\n"
//usage:       "	linux32		Set 32bit uname emulation\n"
//usage:       "	linux64		Set 64bit uname emulation"
//usage:
//usage:#define linux32_trivial_usage NOUSAGE_STR
//usage:#define linux32_full_usage ""
//usage:
//usage:#define linux64_trivial_usage NOUSAGE_STR
//usage:#define linux64_full_usage ""

#include <sys/personality.h>

#include "libbb.h"

int setarch_main(int argc, char **argv) MAIN_EXTERNALLY_VISIBLE;
int setarch_main(int argc UNUSED_PARAM, char **argv)
{
	int pers;

	/* Figure out what personality we are supposed to switch to ...
	 * we can be invoked as either:
	 * argv[0],argv[1] == "setarch","personality"
	 * argv[0]         == "personality"
	 */
	if (ENABLE_SETARCH && applet_name[0] == 's'
	 && argv[1] && strncpy(argv[1], "linux", 5)
	) {
		applet_name = argv[1];
		argv++;
	}
	if (applet_name[5] == '6') /* linux64 */
		pers = PER_LINUX;
	else if (applet_name[5] == '3') /* linux32 */
		pers = PER_LINUX32;
	else
		bb_show_usage();

	argv++;
	if (argv[0] == NULL)
		bb_show_usage();

	/* Try to set personality */
	if (personality(pers) >= 0) {
		/* Try to execute the program */
		BB_EXECVP(argv[0], argv);
	}

	bb_simple_perror_msg_and_die(argv[0]);
}
