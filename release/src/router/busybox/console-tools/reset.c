/* vi: set sw=4 ts=4: */
/*
 * Mini reset implementation for busybox
 *
 * Copyright (C) 1999-2004 by Erik Andersen <andersen@codepoet.org>
 * Written by Erik Andersen and Kent Robotti <robotti@metconnect.com>
 *
 * Licensed under GPLv2 or later, see file LICENSE in this source tree.
 */

/* BTW, which "standard" package has this utility? It doesn't seem
 * to be ncurses, coreutils, console-tools... then what? */

//usage:#define reset_trivial_usage
//usage:       ""
//usage:#define reset_full_usage "\n\n"
//usage:       "Reset the screen"

#include "libbb.h"

#define ESC "\033"

#if ENABLE_STTY
int stty_main(int argc, char **argv) MAIN_EXTERNALLY_VISIBLE;
#endif

int reset_main(int argc, char **argv) MAIN_EXTERNALLY_VISIBLE;
int reset_main(int argc UNUSED_PARAM, char **argv UNUSED_PARAM)
{
	static const char *const args[] = {
		"stty", "sane", NULL
	};

	/* no options, no getopt */

	if (/*isatty(STDIN_FILENO) &&*/ isatty(STDOUT_FILENO)) {
		/* See 'man 4 console_codes' for details:
		 * "ESC c"        -- Reset
		 * "ESC ( B"      -- Select G0 Character Set (B = US)
		 * "ESC [ 0 m"    -- Reset all display attributes
		 * "ESC [ J"      -- Erase to the end of screen
		 * "ESC [ ? 25 h" -- Make cursor visible
		 */
		printf(ESC"c" ESC"(B" ESC"[0m" ESC"[J" ESC"[?25h");
		/* http://bugs.busybox.net/view.php?id=1414:
		 * people want it to reset echo etc: */
#if ENABLE_STTY
		return stty_main(2, (char**)args);
#else
		execvp("stty", (char**)args);
#endif
	}
	return EXIT_SUCCESS;
}
