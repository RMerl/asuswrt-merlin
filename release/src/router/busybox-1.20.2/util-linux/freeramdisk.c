/* vi: set sw=4 ts=4: */
/*
 * freeramdisk and fdflush implementations for busybox
 *
 * Copyright (C) 2000 and written by Emanuele Caratti <wiz@iol.it>
 * Adjusted a bit by Erik Andersen <andersen@codepoet.org>
 * Unified with fdflush by Tito Ragusa <farmatito@tiscali.it>
 *
 * Licensed under GPLv2, see file LICENSE in this source tree.
 */

//usage:#define freeramdisk_trivial_usage
//usage:       "DEVICE"
//usage:#define freeramdisk_full_usage "\n\n"
//usage:       "Free all memory used by the specified ramdisk"
//usage:
//usage:#define freeramdisk_example_usage
//usage:       "$ freeramdisk /dev/ram2\n"
//usage:
//usage:#define fdflush_trivial_usage
//usage:       "DEVICE"
//usage:#define fdflush_full_usage "\n\n"
//usage:       "Force floppy disk drive to detect disk change"

#include <sys/mount.h>
#include "libbb.h"

/* From <linux/fd.h> */
#define FDFLUSH  _IO(2,0x4b)

int freeramdisk_main(int argc, char **argv) MAIN_EXTERNALLY_VISIBLE;
int freeramdisk_main(int argc UNUSED_PARAM, char **argv)
{
	int fd;

	fd = xopen(single_argv(argv), O_RDWR);

	// Act like freeramdisk, fdflush, or both depending on configuration.
	ioctl_or_perror_and_die(fd, (ENABLE_FREERAMDISK && applet_name[1] == 'r')
			|| !ENABLE_FDFLUSH ? BLKFLSBUF : FDFLUSH, NULL, "%s", argv[1]);

	if (ENABLE_FEATURE_CLEAN_UP) close(fd);

	return EXIT_SUCCESS;
}
