/*
 * fsetflags.c		- Set a file flags on an ext2 file system
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

#include "config.h"
#if HAVE_ERRNO_H
#include <errno.h>
#endif
#if HAVE_UNISTD_H
#include <unistd.h>
#endif
#include <sys/types.h>
#include <sys/stat.h>
#if HAVE_EXT2_IOCTLS
#include <fcntl.h>
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

#ifdef O_LARGEFILE
#define OPEN_FLAGS (O_RDONLY|O_NONBLOCK|O_LARGEFILE)
#else
#define OPEN_FLAGS (O_RDONLY|O_NONBLOCK)
#endif

int fsetflags (const char * name, unsigned long flags)
{
#if HAVE_CHFLAGS && !(APPLE_DARWIN && HAVE_EXT2_IOCTLS)
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

	return chflags (name, bsd_flags);
#else /* !HAVE_CHFLAGS || (APPLE_DARWIN && HAVE_EXT2_IOCTLS) */
#if HAVE_EXT2_IOCTLS
	int fd, r, f, save_errno = 0;
	struct stat buf;

	if (!lstat(name, &buf) &&
	    !S_ISREG(buf.st_mode) && !S_ISDIR(buf.st_mode)) {
		goto notsupp;
	}
#if !APPLE_DARWIN
	fd = open (name, OPEN_FLAGS);
	if (fd == -1)
		return -1;
	f = (int) flags;
	r = ioctl (fd, EXT2_IOC_SETFLAGS, &f);
	if (r == -1)
		save_errno = errno;
	close (fd);
	if (save_errno)
		errno = save_errno;
#else /* APPLE_DARWIN */
	f = (int) flags;
	return syscall(SYS_fsctl, name, EXT2_IOC_SETFLAGS, &f, 0);
#endif /* !APPLE_DARWIN */
	return r;

notsupp:
#endif /* HAVE_EXT2_IOCTLS */
#endif
	errno = EOPNOTSUPP;
	return -1;
}
