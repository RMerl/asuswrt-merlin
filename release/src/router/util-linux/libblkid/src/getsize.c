/*
 * getsize.c --- get the size of a partition.
 *
 * Copyright (C) 1995, 1995 Theodore Ts'o.
 * Copyright (C) 2010 Karel Zak <kzak@redhat.com>
 *
 * %Begin-Header%
 * This file may be redistributed under the terms of the
 * GNU Lesser General Public License.
 * %End-Header%
 */

#include <stdio.h>
#include <sys/stat.h>
#include <sys/types.h>

#include "blkidP.h"

/**
 * blkid_get_dev_size:
 * @fd: file descriptor
 *
 * Returns: size (in bytes) of the block device or size of the regular file or 0.
 */
blkid_loff_t blkid_get_dev_size(int fd)
{
	unsigned long long bytes;

	if (blkdev_get_size(fd, &bytes))
		return 0;

	return bytes;
}

