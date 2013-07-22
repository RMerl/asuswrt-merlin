/*
 * llseek.c -- stub calling the llseek system call
 *
 * Copyright (C) 1994, 1995, 1996, 1997 Theodore Ts'o.
 *
 * %Begin-Header%
 * This file may be redistributed under the terms of the
 * GNU Lesser General Public License.
 * %End-Header%
 */

#define _LARGEFILE_SOURCE
#define _LARGEFILE64_SOURCE

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

#include "blkidP.h"

#ifdef __linux__

#if defined(HAVE_LSEEK64) && defined(HAVE_LSEEK64_PROTOTYPE)

#define my_llseek lseek64

#elif defined(HAVE_LLSEEK)
#include <syscall.h>

#ifndef HAVE_LLSEEK_PROTOTYPE
extern long long llseek(int fd, long long offset, int origin);
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
static int _llseek(unsigned int, unsigned long, unsigned long,
		   blkid_loff_t *, unsigned int);

static _syscall5(int, _llseek, unsigned int, fd, unsigned long, offset_high,
		 unsigned long, offset_low, blkid_loff_t *, result,
		 unsigned int, origin)
#endif

static blkid_loff_t my_llseek(int fd, blkid_loff_t offset, int origin)
{
	blkid_loff_t result;
	int retval;

#ifndef __i386__
	retval = _llseek(fd, ((unsigned long long) offset) >> 32,
			 ((unsigned long long)offset) & 0xffffffff,
			 &result, origin);
#else
	retval = syscall(__NR__llseek, fd, ((unsigned long long) offset) >> 32,
			 ((unsigned long long)offset) & 0xffffffff,
			 &result, origin);
#endif
	return (retval == -1 ? (blkid_loff_t) retval : result);
}

#endif	/* __alpha__ || __ia64__ */

#endif /* HAVE_LLSEEK */

blkid_loff_t blkid_llseek(int fd, blkid_loff_t offset, int whence)
{
	blkid_loff_t result;
	static int do_compat = 0;

	if ((sizeof(off_t) >= sizeof(blkid_loff_t)) ||
	    (offset < ((blkid_loff_t) 1 << ((sizeof(off_t)*8) -1))))
		return lseek(fd, (off_t) offset, whence);

	if (do_compat) {
		errno = EOVERFLOW;
		return -1;
	}

	result = my_llseek(fd, offset, whence);
	if (result == -1 && errno == ENOSYS) {
		/*
		 * Just in case this code runs on top of an old kernel
		 * which does not support the llseek system call
		 */
		do_compat++;
		errno = EOVERFLOW;
	}
	return result;
}

#else /* !linux */

#ifndef EOVERFLOW
#ifdef EXT2_ET_INVALID_ARGUMENT
#define EOVERFLOW EXT2_ET_INVALID_ARGUMENT
#else
#define EOVERFLOW 112
#endif
#endif

blkid_loff_t blkid_llseek(int fd, blkid_loff_t offset, int origin)
{
#if defined(HAVE_LSEEK64) && defined(HAVE_LSEEK64_PROTOTYPE)
	return lseek64 (fd, offset, origin);
#else
	if ((sizeof(off_t) < sizeof(blkid_loff_t)) &&
	    (offset >= ((blkid_loff_t) 1 << ((sizeof(off_t)*8) - 1)))) {
		errno = EOVERFLOW;
		return -1;
	}
	return lseek(fd, (off_t) offset, origin);
#endif
}

#endif	/* linux */


