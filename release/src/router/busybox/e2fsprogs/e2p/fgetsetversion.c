/* vi: set sw=4 ts=4: */
/*
 * fgetversion.c	- Get a file version on an ext2 file system
 * fsetversion.c	- Set a file version on an ext2 file system
 *
 *
 * Copyright (C) 1993, 1994  Remy Card <card@masi.ibp.fr>
 *                           Laboratoire MASI, Institut Blaise Pascal
 *                           Universite Pierre et Marie Curie (Paris VI)
 *
 * This file can be redistributed under the terms of the GNU Library General
 * Public License
 */

/*
 * History:
 * 93/10/30	- Creation
 */

#ifdef HAVE_ERRNO_H
#include <errno.h>
#endif
#ifdef HAVE_UNISTD_H
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

/*
   To do fsetversion:     unsigned long *ptr_version must be set to NULL.
		      and unsigned long version must be set to a value
   To do fgetversion:     unsigned long *ptr_version must NOT be set to NULL
		      and unsigned long version is ignored.
	TITO.
*/

int fgetsetversion (const char * name, unsigned long * get_version, unsigned long set_version)
{
#ifdef HAVE_EXT2_IOCTLS
	int fd, r, ver, save_errno = 0;

	fd = open (name, OPEN_FLAGS);
	if (fd == -1)
		return -1;
	if (!get_version) {
		ver = (int) set_version;
		r = ioctl (fd, EXT2_IOC_SETVERSION, &ver);
	} else {
		r = ioctl (fd, EXT2_IOC_GETVERSION, &ver);
		*get_version = ver;
	}
	if (r == -1)
		save_errno = errno;
	close (fd);
	if (save_errno)
		errno = save_errno;
	return r;
#else /* ! HAVE_EXT2_IOCTLS */
	errno = EOPNOTSUPP;
	return -1;
#endif /* ! HAVE_EXT2_IOCTLS */
}
