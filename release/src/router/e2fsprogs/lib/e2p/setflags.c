/*
 * setflags.c		- Set a file flags on an ext2 file system
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

#if HAVE_ERRNO_H
#include <errno.h>
#endif
#include <sys/types.h>
#include <sys/stat.h>
#if HAVE_EXT2_IOCTLS
#include <sys/ioctl.h>
#endif

#include "e2p.h"

/*
 * Deal with lame glibc's that define this function without actually
 * implementing it.  Can you say "attractive nuisance", boys and girls?
 * I knew you could!
 */
#ifdef __linux__
#undef HAVE_CHFLAGS
#endif

int setflags (int fd, unsigned long flags)
{
	struct stat buf;
#if HAVE_CHFLAGS
	unsigned long bsd_flags = 0;

#ifdef UF_IMMUTABLE
	if (flags & EXT2_IMMUTABLE_FL)
		bsd_flags |= UF_IMMUTABLE;
#endif
#ifdef UF_APPEND
	if (flags & EXT2_APPEND_FL)
		bsd_flags |= UF_APPEND;
#endif
#ifdef UF_NODUMP
	if (flags & EXT2_NODUMP_FL)
		bsd_flags |= UF_NODUMP;
#endif

	return fchflags (fd, bsd_flags);
#else
#if HAVE_EXT2_IOCTLS
	int	f;

	if (!fstat(fd, &buf) &&
	    !S_ISREG(buf.st_mode) && !S_ISDIR(buf.st_mode)) {
		errno = EOPNOTSUPP;
		return -1;
	}
	f = (int) flags;
	return ioctl (fd, EXT2_IOC_SETFLAGS, &f);
#endif /* HAVE_EXT2_IOCTLS */
#endif
	errno = EOPNOTSUPP;
	return -1;
}
