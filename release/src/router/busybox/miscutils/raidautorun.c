/* vi: set sw=4 ts=4: */
/*
 * raidautorun implementation for busybox
 *
 * Copyright (C) 2006 Bernhard Reutner-Fischer
 *
 * Licensed under the GPL v2 or later, see the file LICENSE in this tarball.
 *
 */

#include "libbb.h"

#include <linux/major.h>
#include <linux/raid/md_u.h>

int raidautorun_main(int argc, char **argv) MAIN_EXTERNALLY_VISIBLE;
int raidautorun_main(int argc UNUSED_PARAM, char **argv)
{
	xioctl(xopen(single_argv(argv), O_RDONLY), RAID_AUTORUN, NULL);
	return EXIT_SUCCESS;
}
