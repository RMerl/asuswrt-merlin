/*
 * llseek.c -- stub calling the llseek system call
 *
 * Copyright (C) 1994, 1995, 1996, 1997 Theodore Ts'o.
 *
 * %Begin-Header%
 * This file may be redistributed under the terms of the GNU Library
 * General Public License, version 2.
 * %End-Header%
 */

#define _LARGEFILE_SOURCE
#define _LARGEFILE64_SOURCE

#include "config.h"
#if HAVE_SYS_TYPES_H
#include <sys/types.h>
#endif

#if HAVE_ERRNO_H
#include <errno.h>
#endif
#if HAVE_UNISTD_H
#include <unistd.h>
#endif
#ifdef __MSDOS__
#include <io.h>
#endif
#include "et/com_err.h"
#include "ext2fs/ext2_io.h"

#ifdef __linux__

#if defined(HAVE_LSEEK64) && defined(HAVE_LSEEK64_PROTOTYPE)

#define my_llseek lseek64

#else
#if defined(HAVE_LLSEEK)
#include <syscall.h>

#ifndef HAVE_LLSEEK_PROTOTYPE
extern long long llseek (int fd, long long offset, int origin);
#endif

#define my_llseek llseek

#else	/* ! HAVE_LLSEEK */

#if SIZEOF_LONG == SIZEOF_LONG_LONG

#define llseek lseek

#else /* SIZEOF_LONG != SIZEOF_LONG_LONG */

#include <linux/unistd.h>

#ifndef __NR__llseek
#define __NR__llseek            140
#endif

#ifndef __i386__
static int _llseek (unsigned int, unsigned long,
		   unsigned long, ext2_loff_t *, unsigned int);

static _syscall5(int,_llseek,unsigned int,fd,unsigned long,offset_high,
		 unsigned long, offset_low,ext2_loff_t *,result,
		 unsigned int, origin)
#endif

static ext2_loff_t my_llseek (int fd, ext2_loff_t offset, int origin)
{
	ext2_loff_t result;
	int retval;

#ifndef __i386__
	retval = _llseek(fd, ((unsigned long long) offset) >> 32,
#else
	retval = syscall(__NR__llseek, fd, (unsigned long long) (offset >> 32),
#endif
			  ((unsigned long long) offset) & 0xffffffff,
			&result, origin);
	return (retval == -1 ? (ext2_loff_t) retval : result);
}

#endif	/* __alpha__ || __ia64__ */

#endif /* HAVE_LLSEEK */
#endif /* defined(HAVE_LSEEK64) && defined(HAVE_LSEEK64_PROTOTYPE) */

ext2_loff_t ext2fs_llseek (int fd, ext2_loff_t offset, int origin)
{
	ext2_loff_t result;
	static int do_compat = 0;

	if ((sizeof(off_t) >= sizeof(ext2_loff_t)) ||
	    (offset < ((ext2_loff_t) 1 << ((sizeof(off_t)*8) -1))))
		return lseek(fd, (off_t) offset, origin);

	if (do_compat) {
		errno = EINVAL;
		return -1;
	}

	result = my_llseek (fd, offset, origin);
	if (result == -1 && errno == ENOSYS) {
		/*
		 * Just in case this code runs on top of an old kernel
		 * which does not support the llseek system call
		 */
		do_compat++;
		errno = EINVAL;
	}
	return result;
}

#else /* !linux */

#ifndef EINVAL
#define EINVAL EXT2_ET_INVALID_ARGUMENT
#endif

ext2_loff_t ext2fs_llseek (int fd, ext2_loff_t offset, int origin)
{
#if defined(HAVE_LSEEK64) && defined(HAVE_LSEEK64_PROTOTYPE)
	return lseek64 (fd, offset, origin);
#else
	if ((sizeof(off_t) < sizeof(ext2_loff_t)) &&
	    (offset >= ((ext2_loff_t) 1 << ((sizeof(off_t)*8) -1)))) {
		errno = EINVAL;
		return -1;
	}
	return lseek (fd, (off_t) offset, origin);
#endif
}

#endif 	/* linux */


