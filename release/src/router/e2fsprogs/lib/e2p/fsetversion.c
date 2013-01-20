/*
 * fsetversion.c	- Set a file version on an ext2 file system
 *
 * Copyright (C) 1993, 1994  Remy Card <card@masi.ibp.fr>
 *                           Laboratoire MASI, Institut Blaise Pascal
 *                           Universite Pierre et Marie Curie (Paris VI)
 *
 * %Begin-Header%
 * This file may be redistributed under the terms of the GNU Library
 * General Public License, version 2.
 * %End-Header%
 */

/*
 * History:
 * 93/10/30	- Creation
 */

#define _LARGEFILE_SOURCE
#define _LARGEFILE64_SOURCE

#if HAVE_ERRNO_H
#include <errno.h>
#endif
#if HAVE_UNISTD_H
#include <unistd.h>
#endif
#include <fcntl.h>
#include <sys/ioctl.h>

#include "e2p.h"

#ifdef O_LARGEFILE
#define OPEN_FLAGS (O_RDONLY|O_NONBLOCK|O_LARGEFILE)
#else
#define OPEN_FLAGS (O_RDONLY|O_NONBLOCK)
#endif

int fsetversion (const char * name, unsigned long version)
{
#if HAVE_EXT2_IOCTLS
#if !APPLE_DARWIN
	int fd, r, ver, save_errno = 0;

	fd = open (name, OPEN_FLAGS);
	if (fd == -1)
		return -1;
	ver = (int) version;
	r = ioctl (fd, EXT2_IOC_SETVERSION, &ver);
	if (r == -1)
		save_errno = errno;
	close (fd);
	if (save_errno)
		errno = save_errno;
	return r;
#else
   int ver = (int)version;
   return syscall(SYS_fsctl, name, EXT2_IOC_SETVERSION, &ver, 0);
#endif
#else /* ! HAVE_EXT2_IOCTLS */
	extern int errno;
	errno = EOPNOTSUPP;
	return -1;
#endif /* ! HAVE_EXT2_IOCTLS */
}
