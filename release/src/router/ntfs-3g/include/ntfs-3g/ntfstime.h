/*
 * ntfstime.h - NTFS time related functions.  Originated from the Linux-NTFS project.
 *
 * Copyright (c) 2005 Anton Altaparmakov
 * Copyright (c) 2005 Yura Pakhuchiy
 * Copyright (c) 2010 Jean-Pierre Andre
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

#ifndef _NTFS_NTFSTIME_H
#define _NTFS_NTFSTIME_H

#ifdef HAVE_TIME_H
#include <time.h>
#endif
#ifdef HAVE_SYS_STAT_H
#include <sys/stat.h>
#endif
#ifdef HAVE_GETTIMEOFDAY
#include <sys/time.h>
#endif

#include "types.h"

/*
 * assume "struct timespec" is not defined if st_mtime is not defined
 */
#if !defined(st_mtime) & !defined(__timespec_defined)
struct timespec {
	time_t tv_sec;
	long tv_nsec;
} ;
#endif

/*
 * There are four times more conversions of internal representation
 * to ntfs representation than any other conversion, so the most
 * efficient internal representation is ntfs representation
 * (with low endianness)
 */
typedef sle64 ntfs_time;

#define NTFS_TIME_OFFSET ((s64)(369 * 365 + 89) * 24 * 3600 * 10000000)

/**
 * ntfs2timespec - Convert an NTFS time to Unix time
 * @ntfs_time:  An NTFS time in 100ns units since 1601
 *
 * NTFS stores times as the number of 100ns intervals since January 1st 1601 at
 * 00:00 UTC.  This system will not suffer from Y2K problems until ~57000AD.
 *
 * Return:  A Unix time (number of seconds since 1970, and nanoseconds)
 */
static __inline__ struct timespec ntfs2timespec(ntfs_time ntfstime)
{
	struct timespec spec;
	s64 cputime;

	cputime = sle64_to_cpu(ntfstime);
	spec.tv_sec = (cputime - (NTFS_TIME_OFFSET)) / 10000000;
	spec.tv_nsec = (cputime - (NTFS_TIME_OFFSET)
			- (s64)spec.tv_sec*10000000)*100;
		/* force zero nsec for overflowing dates */
	if ((spec.tv_nsec < 0) || (spec.tv_nsec > 999999999))
		spec.tv_nsec = 0;
	return (spec);
}

/**
 * timespec2ntfs - Convert Linux time to NTFS time
 * @utc_time:  Linux time to convert to NTFS
 *
 * Convert the Linux time @utc_time to its corresponding NTFS time.
 *
 * Linux stores time in a long at present and measures it as the number of
 * 1-second intervals since 1st January 1970, 00:00:00 UTC
 * with a separated non-negative nanosecond value
 *
 * NTFS uses Microsoft's standard time format which is stored in a sle64 and is
 * measured as the number of 100 nano-second intervals since 1st January 1601,
 * 00:00:00 UTC.
 *
 * Return:  An NTFS time (100ns units since Jan 1601)
 */
static __inline__ ntfs_time timespec2ntfs(struct timespec spec)
{
	s64 units;

	units = (s64)spec.tv_sec * 10000000
				+ NTFS_TIME_OFFSET + spec.tv_nsec/100;
	return (cpu_to_le64(units));
}

/*
 *		Return the current time in ntfs format
 */

static __inline__ ntfs_time ntfs_current_time(void)
{
	struct timespec now;

#if defined(HAVE_CLOCK_GETTIME) || defined(HAVE_SYS_CLOCK_GETTIME)
	clock_gettime(CLOCK_REALTIME, &now);
#elif defined(HAVE_GETTIMEOFDAY)
	struct timeval microseconds;

	gettimeofday(&microseconds, (struct timezone*)NULL);
	now.tv_sec = microseconds.tv_sec;
	now.tv_nsec = microseconds.tv_usec*1000;
#else
	now.tv_sec = time((time_t*)NULL);
	now.tv_nsec = 0;
#endif
	return (timespec2ntfs(now));
}

#endif /* _NTFS_NTFSTIME_H */
