/*
 * ntfstime.h - NTFS time related functions.  Part of the Linux-NTFS project.
 *
 * Copyright (c) 2005      Anton Altaparmakov
 * Copyright (c) 2005-2007 Yura Pakhuchiy
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
 * along with this program (in the main directory of the Linux-NTFS
 * distribution in the file COPYING); if not, write to the Free Software
 * Foundation,Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#ifndef _NTFS_NTFSTIME_H
#define _NTFS_NTFSTIME_H

#ifdef HAVE_TIME_H
#include <time.h>
#endif

#include "types.h"

#define NTFS_TIME_OFFSET ((s64)(369 * 365 + 89) * 24 * 3600 * 10000000)

/**
 * ntfs2utc - Convert an NTFS time to Unix time
 * @ntfs_time:  An NTFS time in 100ns units since 1601
 *
 * NTFS stores times as the number of 100ns intervals since January 1st 1601 at
 * 00:00 UTC.  This system will not suffer from Y2K problems until ~57000AD.
 *
 * Return:  n  A Unix time (number of seconds since 1970)
 */
static __inline__ time_t ntfs2utc(sle64 ntfs_time)
{
	return (sle64_to_cpu(ntfs_time) - (NTFS_TIME_OFFSET)) / 10000000;
}

/**
 * utc2ntfs - Convert Linux time to NTFS time
 * @utc_time:  Linux time to convert to NTFS
 *
 * Convert the Linux time @utc_time to its corresponding NTFS time.
 *
 * Linux stores time in a long at present and measures it as the number of
 * 1-second intervals since 1st January 1970, 00:00:00 UTC.
 *
 * NTFS uses Microsoft's standard time format which is stored in a s64 and is
 * measured as the number of 100 nano-second intervals since 1st January 1601,
 * 00:00:00 UTC.
 *
 * Return:  n  An NTFS time (100ns units since Jan 1601)
 */
static __inline__ sle64 utc2ntfs(time_t utc_time)
{
	/* Convert to 100ns intervals and then add the NTFS time offset. */
	return cpu_to_sle64((s64)utc_time * 10000000 + NTFS_TIME_OFFSET);
}

#endif /* _NTFS_NTFSTIME_H */
