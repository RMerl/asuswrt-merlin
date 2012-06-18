/* vi: set sw=4 ts=4: */
/*
 * Support functions for mounting devices by label/uuid
 *
 * Copyright (C) 2006 by Jason Schoon <floydpink@gmail.com>
 * Some portions cribbed from e2fsprogs, util-linux, dosfstools
 *
 * Licensed under GPLv2 or later, see file LICENSE in this tarball for details.
 */

#include "libbb.h"
#include "volume_id.h"

int findfs_main(int argc, char **argv) MAIN_EXTERNALLY_VISIBLE;
int findfs_main(int argc UNUSED_PARAM, char **argv)
{
	char *dev = *++argv;

	if (!dev)
		bb_show_usage();

	if (strncmp(dev, "/dev/", 5) == 0) {
		/* Just pass any /dev/xxx name right through.
		 * This might aid in some scripts being able
		 * to call this unconditionally */
		dev = NULL;
	} else {
		/* Otherwise, handle LABEL=xxx and UUID=xxx,
		 * fail on anything else */
		if (!resolve_mount_spec(argv))
			bb_show_usage();
	}

	if (*argv != dev) {
		puts(*argv);
		return 0;
	}
	return 1;
}
