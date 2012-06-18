/* vi: set sw=4 ts=4: */
/*
 * Print UUIDs on all filesystems
 *
 * Copyright (C) 2008 Denys Vlasenko.
 *
 * Licensed under GPLv2, see file LICENSE in this tarball for details.
 */

#include "libbb.h"
#include "volume_id.h"

int blkid_main(int argc, char **argv) MAIN_EXTERNALLY_VISIBLE;
int blkid_main(int argc UNUSED_PARAM, char **argv UNUSED_PARAM)
{
	display_uuid_cache();
	return 0;
}
