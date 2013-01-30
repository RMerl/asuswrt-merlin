/* vi: set sw=4 ts=4: */
/*
 * Disallocate virtual terminal(s)
 *
 * Copyright (C) 2003 by Tito Ragusa <farmatito@tiscali.it>
 * Copyright (C) 1999-2004 by Erik Andersen <andersen@codepoet.org>
 *
 * Licensed under GPLv2 or later, see file LICENSE in this source tree.
 */

/* no options, no getopt */

//usage:#define deallocvt_trivial_usage
//usage:       "[N]"
//usage:#define deallocvt_full_usage "\n\n"
//usage:       "Deallocate unused virtual terminal /dev/ttyN"

#include "libbb.h"

/* From <linux/vt.h> */
enum { VT_DISALLOCATE = 0x5608 }; /* free memory associated to vt */

int deallocvt_main(int argc, char **argv) MAIN_EXTERNALLY_VISIBLE;
int deallocvt_main(int argc UNUSED_PARAM, char **argv)
{
	/* num = 0 deallocate all unused consoles */
	int num = 0;

	if (argv[1]) {
		if (argv[2])
			bb_show_usage();
		num = xatou_range(argv[1], 1, 63);
	}

	/* double cast suppresses "cast to ptr from int of different size" */
	xioctl(get_console_fd_or_die(), VT_DISALLOCATE, (void *)(ptrdiff_t)num);
	return EXIT_SUCCESS;
}
