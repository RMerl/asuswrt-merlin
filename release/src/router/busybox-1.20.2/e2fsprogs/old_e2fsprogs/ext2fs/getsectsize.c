/* vi: set sw=4 ts=4: */
/*
 * getsectsize.c --- get the sector size of a device.
 *
 * Copyright (C) 1995, 1995 Theodore Ts'o.
 * Copyright (C) 2003 VMware, Inc.
 *
 * %Begin-Header%
 * This file may be redistributed under the terms of the GNU Public
 * License.
 * %End-Header%
 */

#include <stdio.h>
#if HAVE_UNISTD_H
#include <unistd.h>
#endif
#if HAVE_ERRNO_H
#include <errno.h>
#endif
#include <fcntl.h>
#ifdef HAVE_LINUX_FD_H
#include <sys/ioctl.h>
#include <linux/fd.h>
#endif

#if defined(__linux__) && defined(_IO) && !defined(BLKSSZGET)
#define BLKSSZGET  _IO(0x12,104)/* get block device sector size */
#endif

#include "ext2_fs.h"
#include "ext2fs.h"

/*
 * Returns the number of blocks in a partition
 */
errcode_t ext2fs_get_device_sectsize(const char *file, int *sectsize)
{
	int	fd;

#ifdef CONFIG_LFS
	fd = open64(file, O_RDONLY);
#else
	fd = open(file, O_RDONLY);
#endif
	if (fd < 0)
		return errno;

#ifdef BLKSSZGET
	if (ioctl(fd, BLKSSZGET, sectsize) >= 0) {
		close(fd);
		return 0;
	}
#endif
	*sectsize = 0;
	close(fd);
	return 0;
}
