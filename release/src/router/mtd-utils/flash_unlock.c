/*
 * flash_{lock,unlock}
 *
 * utilities for locking/unlocking sectors of flash devices
 */

#ifndef PROGRAM_NAME
#define PROGRAM_NAME "flash_unlock"
#define FLASH_MSG    "unlock"
#define FLASH_UNLOCK 1
#else
#define FLASH_MSG    "lock"
#define FLASH_UNLOCK 0
#endif

#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <fcntl.h>
#include <time.h>
#include <sys/ioctl.h>
#include <sys/mount.h>
#include <string.h>

#include "common.h"
#include <mtd/mtd-user.h>

static void usage(int status)
{
	fprintf(status ? stderr : stdout,
		"Usage: %s <mtd device> [offset] [block count]\n\n"
		"If offset is not specified, it defaults to 0.\n"
		"If block count is not specified, it defaults to all blocks.\n",
		PROGRAM_NAME);
	exit(status);
}

int main(int argc, char *argv[])
{
	int fd, request;
	struct mtd_info_user mtdInfo;
	struct erase_info_user mtdLockInfo;
	int count;
	const char *dev;

	/* Parse command line options */
	if (argc < 2 || argc > 4)
		usage(1);
	if (!strcmp(argv[1], "-h") || !strcmp(argv[1], "--help"))
		usage(0);

	dev = argv[1];

	/* Get the device info to compare to command line sizes */
	fd = open(dev, O_RDWR);
	if (fd < 0)
		sys_errmsg_die("could not open: %s", dev);

	if (ioctl(fd, MEMGETINFO, &mtdInfo))
		sys_errmsg_die("could not get mtd info: %s", dev);

	/* Make sure user options are valid */
	if (argc > 2)
		mtdLockInfo.start = strtol(argv[2], NULL, 0);
	else
		mtdLockInfo.start = 0;
	if (mtdLockInfo.start > mtdInfo.size)
		errmsg_die("%#x is beyond device size %#x",
			mtdLockInfo.start, mtdInfo.size);

	if (argc > 3) {
		count = strtol(argv[3], NULL, 0);
		if (count == -1)
			mtdLockInfo.length = mtdInfo.size;
		else
			mtdLockInfo.length = mtdInfo.erasesize * count;
	} else
		mtdLockInfo.length = mtdInfo.size;
	if (mtdLockInfo.start + mtdLockInfo.length > mtdInfo.size)
		errmsg_die("range is more than device supports: %#x + %#x > %#x",
			mtdLockInfo.start, mtdLockInfo.length, mtdInfo.size);

	/* Finally do the operation */
	request = FLASH_UNLOCK ? MEMUNLOCK : MEMLOCK;
	if (ioctl(fd, request, &mtdLockInfo))
		sys_errmsg_die("could not %s device: %s\n",
			FLASH_MSG, dev);

	return 0;
}
