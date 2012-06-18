/* vi: set sw=4 ts=4: */
/*
 * setlogcons: Send kernel messages to the current console or to console N
 *
 * Copyright (C) 2006 by Jan Kiszka <jan.kiszka@web.de>
 *
 * Based on setlogcons (kbd-1.12) by Andries E. Brouwer
 *
 * Licensed under GPLv2 or later, see file LICENSE in this tarball for details.
 */

#include "libbb.h"

int setlogcons_main(int argc, char **argv) MAIN_EXTERNALLY_VISIBLE;
int setlogcons_main(int argc UNUSED_PARAM, char **argv)
{
	struct {
		char fn;
		char subarg;
	} arg = { 11, /* redirect kernel messages */
			  0   /* to specified console (current as default) */
			};

	if (argv[1])
		arg.subarg = xatou_range(argv[1], 0, 63);

	xioctl(xopen(VC_1, O_RDONLY), TIOCLINUX, &arg);

	return EXIT_SUCCESS;
}
