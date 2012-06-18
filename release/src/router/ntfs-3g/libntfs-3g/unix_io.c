/**
 * unix_io.c - Unix style disk io functions. Originated from the Linux-NTFS project.
 *
 * Copyright (c) 2000-2006 Anton Altaparmakov
 *
 * This program/include file is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as published
 * by the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program/include file is distributed in the hope that it will be
 * useful, but WITHOUT ANY WARRANTY; without even the implied warranty
 * of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program (in the main directory of the NTFS-3G
 * distribution in the file COPYING); if not, write to the Free Software
 * Foundation,Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif
#ifdef HAVE_STDLIB_H
#include <stdlib.h>
#endif
#ifdef HAVE_STRING_H
#include <string.h>
#endif
#ifdef HAVE_ERRNO_H
#include <errno.h>
#endif
#ifdef HAVE_STDIO_H
#include <stdio.h>
#endif
#ifdef HAVE_SYS_TYPES_H
#include <sys/types.h>
#endif
#ifdef HAVE_SYS_STAT_H
#include <sys/stat.h>
#endif
#ifdef HAVE_FCNTL_H
#include <fcntl.h>
#endif
#ifdef HAVE_SYS_IOCTL_H
#include <sys/ioctl.h>
#endif
#ifdef HAVE_LINUX_FD_H
#include <linux/fd.h>
#endif

#include "types.h"
#include "mst.h"
#include "debug.h"
#include "device.h"
#include "logging.h"
#include "misc.h"

#define DEV_FD(dev)	(*(int *)dev->d_private)

/* Define to nothing if not present on this system. */
#ifndef O_EXCL
#	define O_EXCL 0
#endif

/**
 * fsync replacement which makes every effort to try to get the data down to
 * disk, using different means for different operating systems. Specifically,
 * it issues the proper fcntl for Mac OS X or does fsync where it is available
 * or as a last resort calls the fsync function. Information on this problem
 * was retrieved from:
 *   http://mirror.linux.org.au/pub/linux.conf.au/2007/video/talks/278.pdf
 */
static int ntfs_fsync(int fildes)
{
	int ret = -1;
#if defined(__APPLE__) || defined(__DARWIN__)
# ifndef F_FULLFSYNC
#  error "Mac OS X: F_FULLFSYNC is not defined. Either you didn't include fcntl.h or you're using an older, unsupported version of Mac OS X (pre-10.3)."
# endif
	/* 
	 * Apple has disabled fsync() for internal disk drives in OS X.
	 * To force a synchronization of disk contents, we use a Mac OS X
	 * specific fcntl, F_FULLFSYNC. 
	 */
	ret = fcntl(fildes, F_FULLFSYNC, NULL);
	if (ret) {
		/* 
		 * If we are not on a file system that supports this,
		 * then fall back to a plain fsync. 
		 */
		ret = fsync(fildes);
	}
#else
	ret = fsync(fildes);
#endif
	return ret;
}

/**
 * ntfs_device_unix_io_open - Open a device and lock it exclusively
 * @dev:
 * @flags:
 *
 * Description...
 *
 * Returns:
 */
static int ntfs_device_unix_io_open(struct ntfs_device *dev, int flags)
{
	struct flock flk;
	struct stat sbuf;
	int err;

	if (NDevOpen(dev)) {
		errno = EBUSY;
		return -1;
	}
	if (stat(dev->d_name, &sbuf)) {
		ntfs_log_perror("Failed to access '%s'", dev->d_name);
		return -1;
	}
	if (S_ISBLK(sbuf.st_mode))
		NDevSetBlock(dev);
	
	dev->d_private = ntfs_malloc(sizeof(int));
	if (!dev->d_private)
		return -1;
	/*
	 * Open file for exclusive access if mounting r/w.
	 * Fuseblk takes care about block devices.
	 */ 
	if (!NDevBlock(dev) && (flags & O_RDWR) == O_RDWR)
		flags |= O_EXCL;
	*(int*)dev->d_private = open(dev->d_name, flags);
	if (*(int*)dev->d_private == -1) {
		err = errno;
		goto err_out;
	}
	
	if ((flags & O_RDWR) != O_RDWR)
		NDevSetReadOnly(dev);
	
	memset(&flk, 0, sizeof(flk));
	if (NDevReadOnly(dev))
		flk.l_type = F_RDLCK;
	else
		flk.l_type = F_WRLCK;
	flk.l_whence = SEEK_SET;
	flk.l_start = flk.l_len = 0LL;
	if (fcntl(DEV_FD(dev), F_SETLK, &flk)) {
		err = errno;
		ntfs_log_perror("Failed to %s lock '%s'", NDevReadOnly(dev) ? 
				"read" : "write", dev->d_name);
		if (close(DEV_FD(dev)))
			ntfs_log_perror("Failed to close '%s'", dev->d_name);
		goto err_out;
	}
	
	NDevSetOpen(dev);
	return 0;
err_out:
	free(dev->d_private);
	dev->d_private = NULL;
	errno = err;
	return -1;
}

/**
 * ntfs_device_unix_io_close - Close the device, releasing the lock
 * @dev:
 *
 * Description...
 *
 * Returns:
 */
static int ntfs_device_unix_io_close(struct ntfs_device *dev)
{
	struct flock flk;

	if (!NDevOpen(dev)) {
		errno = EBADF;
		ntfs_log_perror("Device %s is not open", dev->d_name);
		return -1;
	}
	if (NDevDirty(dev))
		if (ntfs_fsync(DEV_FD(dev))) {
			ntfs_log_perror("Failed to fsync device %s", dev->d_name);
			return -1;
		}

	memset(&flk, 0, sizeof(flk));
	flk.l_type = F_UNLCK;
	flk.l_whence = SEEK_SET;
	flk.l_start = flk.l_len = 0LL;
	if (fcntl(DEV_FD(dev), F_SETLK, &flk))
		ntfs_log_perror("Could not unlock %s", dev->d_name);
	if (close(DEV_FD(dev))) {
		ntfs_log_perror("Failed to close device %s", dev->d_name);
		return -1;
	}
	NDevClearOpen(dev);
	free(dev->d_private);
	dev->d_private = NULL;
	return 0;
}

/**
 * ntfs_device_unix_io_seek - Seek to a place on the device
 * @dev:
 * @offset:
 * @whence:
 *
 * Description...
 *
 * Returns:
 */
static s64 ntfs_device_unix_io_seek(struct ntfs_device *dev, s64 offset,
		int whence)
{
	return lseek(DEV_FD(dev), offset, whence);
}

/**
 * ntfs_device_unix_io_read - Read from the device, from the current location
 * @dev:
 * @buf:
 * @count:
 *
 * Description...
 *
 * Returns:
 */
static s64 ntfs_device_unix_io_read(struct ntfs_device *dev, void *buf,
		s64 count)
{
	return read(DEV_FD(dev), buf, count);
}

/**
 * ntfs_device_unix_io_write - Write to the device, at the current location
 * @dev:
 * @buf:
 * @count:
 *
 * Description...
 *
 * Returns:
 */
static s64 ntfs_device_unix_io_write(struct ntfs_device *dev, const void *buf,
		s64 count)
{
	if (NDevReadOnly(dev)) {
		errno = EROFS;
		return -1;
	}
	NDevSetDirty(dev);
	return write(DEV_FD(dev), buf, count);
}

/**
 * ntfs_device_unix_io_pread - Perform a positioned read from the device
 * @dev:
 * @buf:
 * @count:
 * @offset:
 *
 * Description...
 *
 * Returns:
 */
static s64 ntfs_device_unix_io_pread(struct ntfs_device *dev, void *buf,
		s64 count, s64 offset)
{
	return pread(DEV_FD(dev), buf, count, offset);
}

/**
 * ntfs_device_unix_io_pwrite - Perform a positioned write to the device
 * @dev:
 * @buf:
 * @count:
 * @offset:
 *
 * Description...
 *
 * Returns:
 */
static s64 ntfs_device_unix_io_pwrite(struct ntfs_device *dev, const void *buf,
		s64 count, s64 offset)
{
	if (NDevReadOnly(dev)) {
		errno = EROFS;
		return -1;
	}
	NDevSetDirty(dev);
	return pwrite(DEV_FD(dev), buf, count, offset);
}

/**
 * ntfs_device_unix_io_sync - Flush any buffered changes to the device
 * @dev:
 *
 * Description...
 *
 * Returns:
 */
static int ntfs_device_unix_io_sync(struct ntfs_device *dev)
{
	int res = 0;
	
	if (!NDevReadOnly(dev)) {
		res = ntfs_fsync(DEV_FD(dev));
		if (res)
			ntfs_log_perror("Failed to sync device %s", dev->d_name);
		else
			NDevClearDirty(dev);
	}
	return res;
}

/**
 * ntfs_device_unix_io_stat - Get information about the device
 * @dev:
 * @buf:
 *
 * Description...
 *
 * Returns:
 */
static int ntfs_device_unix_io_stat(struct ntfs_device *dev, struct stat *buf)
{
	return fstat(DEV_FD(dev), buf);
}

/**
 * ntfs_device_unix_io_ioctl - Perform an ioctl on the device
 * @dev:
 * @request:
 * @argp:
 *
 * Description...
 *
 * Returns:
 */
static int ntfs_device_unix_io_ioctl(struct ntfs_device *dev, int request,
		void *argp)
{
	return ioctl(DEV_FD(dev), request, argp);
}

/**
 * Device operations for working with unix style devices and files.
 */
struct ntfs_device_operations ntfs_device_unix_io_ops = {
	.open		= ntfs_device_unix_io_open,
	.close		= ntfs_device_unix_io_close,
	.seek		= ntfs_device_unix_io_seek,
	.read		= ntfs_device_unix_io_read,
	.write		= ntfs_device_unix_io_write,
	.pread		= ntfs_device_unix_io_pread,
	.pwrite		= ntfs_device_unix_io_pwrite,
	.sync		= ntfs_device_unix_io_sync,
	.stat		= ntfs_device_unix_io_stat,
	.ioctl		= ntfs_device_unix_io_ioctl,
};
