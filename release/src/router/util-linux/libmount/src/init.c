/*
 * Copyright (C) 2008 Karel Zak <kzak@redhat.com>
 *
 * This file may be redistributed under the terms of the
 * GNU Lesser General Public License.
 */

/**
 * SECTION: init
 * @title: Library initialization
 * @short_description: initialize debuging
 */

#include <stdarg.h>

#include "mountP.h"

int libmount_debug_mask;

/**
 * mnt_init_debug:
 * @mask: debug mask (0xffff to enable full debuging)
 *
 * If the @mask is not specified then this function reads
 * LIBMOUNT_DEBUG environment variable to get the mask.
 *
 * Already initialized debugging stuff cannot be changed. It does not
 * have effect to call this function twice.
 */
void mnt_init_debug(int mask)
{
	if (libmount_debug_mask & MNT_DEBUG_INIT)
		return;
	if (!mask) {
		char *str = getenv("LIBMOUNT_DEBUG");
		if (str)
			libmount_debug_mask = strtoul(str, 0, 0);
	} else
		libmount_debug_mask = mask;

	if (libmount_debug_mask)
		printf("libmount: debug mask set to 0x%04x.\n",
				libmount_debug_mask);
	libmount_debug_mask |= MNT_DEBUG_INIT;
}
