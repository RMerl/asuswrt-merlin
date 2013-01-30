/* vi: set sw=4 ts=4: */
/*
 * Mini chvt implementation for busybox
 *
 * Copyright (C) 1999-2004 by Erik Andersen <andersen@codepoet.org>
 *
 * Licensed under GPLv2 or later, see file LICENSE in this source tree.
 */

//usage:#define chvt_trivial_usage
//usage:       "N"
//usage:#define chvt_full_usage "\n\n"
//usage:       "Change the foreground virtual terminal to /dev/ttyN"

#include "libbb.h"

int chvt_main(int argc, char **argv) MAIN_EXTERNALLY_VISIBLE;
int chvt_main(int argc UNUSED_PARAM, char **argv)
{
	int num = xatou_range(single_argv(argv), 1, 63);
	console_make_active(get_console_fd_or_die(), num);
	return EXIT_SUCCESS;
}
