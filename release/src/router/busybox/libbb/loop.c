/* vi: set sw=4 ts=4: */
/*
 * Utility routines.
 *
 * Copyright (C) 1999-2004 by Erik Andersen <andersen@codepoet.org>
 * Copyright (C) 2005 by Rob Landley <rob@landley.net>
 *
 * Licensed under GPLv2 or later, see file LICENSE in this source tree.
 */
#include "libbb.h"
#include <linux/version.h>

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,0)

/* For 2.6, use the cleaned up header to get the 64 bit API. */
// Commented out per Rob's request
//# include "fix_u32.h" /* some old toolchains need __u64 for linux/loop.h */
# include <linux/loop.h>
typedef struct loop_info64 bb_loop_info;
# define BB_LOOP_SET_STATUS LOOP_SET_STATUS64
# define BB_LOOP_GET_STATUS LOOP_GET_STATUS64

#else

/* For 2.4 and earlier, use the 32 bit API (and don't trust the headers) */
/* Stuff stolen from linux/loop.h for 2.4 and earlier kernels */
# include <linux/posix_types.h>
# define LO_NAME_SIZE        64
# define LO_KEY_SIZE         32
# define LOOP_SET_FD         0x4C00
# define LOOP_CLR_FD         0x4C01
# define BB_LOOP_SET_STATUS  0x4C02
# define BB_LOOP_GET_STATUS  0x4C03
typedef struct {
	int                lo_number;
	__kernel_dev_t     lo_device;
	unsigned long      lo_inode;
	__kernel_dev_t     lo_rdevice;
	int                lo_offset;
	int                lo_encrypt_type;
	int                lo_encrypt_key_size;
	int                lo_flags;
	char               lo_file_name[LO_NAME_SIZE];
	unsigned char      lo_encrypt_key[LO_KEY_SIZE];
	unsigned long      lo_init[2];
	char               reserved[4];
} bb_loop_info;
#endif

char* FAST_FUNC query_loop(const char *device)
{
	int fd;
	bb_loop_info loopinfo;
	char *dev = NULL;

	fd = open(device, O_RDONLY);
	if (fd >= 0) {
		if (ioctl(fd, BB_LOOP_GET_STATUS, &loopinfo) == 0) {
			dev = xasprintf("%"OFF_FMT"u %s", (off_t) loopinfo.lo_offset,
					(char *)loopinfo.lo_file_name);
		}
		close(fd);
	}

	return dev;
}

int FAST_FUNC del_loop(const char *device)
{
	int fd, rc;

	fd = open(device, O_RDONLY);
	if (fd < 0)
		return 1;
	rc = ioctl(fd, LOOP_CLR_FD, 0);
	close(fd);

	return rc;
}

/* Returns 0 if mounted RW, 1 if mounted read-only, <0 for error.
   *device is loop device to use, or if *device==NULL finds a loop device to
   mount it on and sets *device to a strdup of that loop device name.  This
   search will re-use an existing loop device already bound to that
   file/offset if it finds one.
 */
int FAST_FUNC set_loop(char **device, const char *file, unsigned long long offset, int ro)
{
	char dev[LOOP_NAMESIZE];
	char *try;
	bb_loop_info loopinfo;
	struct stat statbuf;
	int i, dfd, ffd, mode, rc = -1;

	/* Open the file.  Barf if this doesn't work.  */
	mode = ro ? O_RDONLY : O_RDWR;
	ffd = open(file, mode);
	if (ffd < 0) {
		if (mode != O_RDONLY) {
			mode = O_RDONLY;
			ffd = open(file, mode);
		}
		if (ffd < 0)
			return -errno;
	}

	/* Find a loop device.  */
	try = *device ? *device : dev;
	/* 1048575 is a max possible minor number in Linux circa 2010 */
	for (i = 0; rc && i < 1048576; i++) {
		sprintf(dev, LOOP_FORMAT, i);

		IF_FEATURE_MOUNT_LOOP_CREATE(errno = 0;)
		if (stat(try, &statbuf) != 0 || !S_ISBLK(statbuf.st_mode)) {
			if (ENABLE_FEATURE_MOUNT_LOOP_CREATE
			 && errno == ENOENT
			 && try == dev
			) {
				/* Node doesn't exist, try to create it.  */
				if (mknod(dev, S_IFBLK|0644, makedev(7, i)) == 0)
					goto try_to_open;
			}
			/* Ran out of block devices, return failure.  */
			rc = -ENOENT;
			break;
		}
 try_to_open:
		/* Open the sucker and check its loopiness.  */
		dfd = open(try, mode);
		if (dfd < 0 && errno == EROFS) {
			mode = O_RDONLY;
			dfd = open(try, mode);
		}
		if (dfd < 0)
			goto try_again;

		rc = ioctl(dfd, BB_LOOP_GET_STATUS, &loopinfo);

		/* If device is free, claim it.  */
		if (rc && errno == ENXIO) {
			memset(&loopinfo, 0, sizeof(loopinfo));
			safe_strncpy((char *)loopinfo.lo_file_name, file, LO_NAME_SIZE);
			loopinfo.lo_offset = offset;
			/* Associate free loop device with file.  */
			if (ioctl(dfd, LOOP_SET_FD, ffd) == 0) {
				if (ioctl(dfd, BB_LOOP_SET_STATUS, &loopinfo) == 0)
					rc = 0;
				else
					ioctl(dfd, LOOP_CLR_FD, 0);
			}

		/* If this block device already set up right, re-use it.
		   (Yes this is racy, but associating two loop devices with the same
		   file isn't pretty either.  In general, mounting the same file twice
		   without using losetup manually is problematic.)
		 */
		} else
		if (strcmp(file, (char *)loopinfo.lo_file_name) != 0
		 || offset != loopinfo.lo_offset
		) {
			rc = -1;
		}
		close(dfd);
 try_again:
		if (*device) break;
	}
	close(ffd);
	if (rc == 0) {
		if (!*device)
			*device = xstrdup(dev);
		return (mode == O_RDONLY); /* 1:ro, 0:rw */
	}
	return rc;
}
