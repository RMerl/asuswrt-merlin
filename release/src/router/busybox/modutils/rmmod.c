/* vi: set sw=4 ts=4: */
/*
 * Mini rmmod implementation for busybox
 *
 * Copyright (C) 1999-2004 by Erik Andersen <andersen@codepoet.org>
 * Copyright (C) 2008 Timo Teras <timo.teras@iki.fi>
 *
 * Licensed under GPLv2 or later, see file LICENSE in this source tree.
 */

//applet:IF_RMMOD(APPLET(rmmod, BB_DIR_SBIN, BB_SUID_DROP))

//usage:#if !ENABLE_MODPROBE_SMALL
//usage:#define rmmod_trivial_usage
//usage:       "[-wfa] [MODULE]..."
//usage:#define rmmod_full_usage "\n\n"
//usage:       "Unload kernel modules\n"
//usage:     "\n	-w	Wait until the module is no longer used"
//usage:     "\n	-f	Force unload"
//usage:     "\n	-a	Remove all unused modules (recursively)"
//usage:#define rmmod_example_usage
//usage:       "$ rmmod tulip\n"
//usage:#endif

#include "libbb.h"
#include "modutils.h"

int rmmod_main(int argc, char **argv) MAIN_EXTERNALLY_VISIBLE;
int rmmod_main(int argc UNUSED_PARAM, char **argv)
{
	int n;
	unsigned flags = O_NONBLOCK | O_EXCL;

	/* Parse command line. */
	n = getopt32(argv, "wfas"); // -s ignored
	argv += optind;
	if (n & 1)  // --wait
		flags &= ~O_NONBLOCK;
	if (n & 2)  // --force
		flags |= O_TRUNC;
	if (n & 4) {
		/* Unload _all_ unused modules via NULL delete_module() call */
		if (bb_delete_module(NULL, flags) != 0 && errno != EFAULT)
			bb_perror_msg_and_die("rmmod");
		return EXIT_SUCCESS;
	}

	if (!*argv)
		bb_show_usage();

	n = ENABLE_FEATURE_2_4_MODULES && get_linux_version_code() < KERNEL_VERSION(2,6,0);
	while (*argv) {
		char modname[MODULE_NAME_LEN];
		const char *bname;

		bname = bb_basename(*argv++);
		if (n)
			safe_strncpy(modname, bname, MODULE_NAME_LEN);
		else
			filename2modname(bname, modname);
		if (bb_delete_module(modname, flags))
			bb_error_msg_and_die("can't unload '%s': %s",
					     modname, moderror(errno));
	}

	return EXIT_SUCCESS;
}
