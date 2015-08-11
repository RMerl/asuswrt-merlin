/* $Id: os_timestamp_linux_kernel.c 3553 2011-05-05 06:14:19Z nanang $ */
/* 
 * Copyright (C) 2008-2011 Teluu Inc. (http://www.teluu.com)
 * Copyright (C) 2003-2008 Benny Prijono <benny@prijono.org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA 
 */
#include <pj/os.h>
#include <linux/time.h>

#if 0
PJ_DEF(pj_status_t) pj_get_timestamp(pj_timestamp *ts)
{
    ts->u32.hi = 0;
    ts->u32.lo = jiffies;
    return 0;
}

PJ_DEF(pj_status_t) pj_get_timestamp_freq(pj_timestamp *freq)
{
    freq->u32.hi = 0;
    freq->u32.lo = HZ;
    return 0;
}
#elif 0
PJ_DEF(pj_status_t) pj_get_timestamp(pj_timestamp *ts)
{
    struct timespec tv;
    
    tv = CURRENT_TIME;

    ts->u64 = tv.tv_sec;
    ts->u64 *= NSEC_PER_SEC;
    ts->u64 += tv.tv_nsec;

    return PJ_SUCCESS;
}

PJ_DEF(pj_status_t) pj_get_timestamp_freq(pj_timestamp *freq)
{
    freq->u32.hi = 0;
    freq->u32.lo = NSEC_PER_SEC;
    return 0;
}
#else
PJ_DEF(pj_status_t) pj_get_timestamp(pj_timestamp *ts)
{
    struct timeval tv;
    
    do_gettimeofday(&tv);

    ts->u64 = tv.tv_sec;
    ts->u64 *= USEC_PER_SEC;
    ts->u64 += tv.tv_usec;

    return PJ_SUCCESS;
}

PJ_DEF(pj_status_t) pj_get_timestamp_freq(pj_timestamp *freq)
{
    freq->u32.hi = 0;
    freq->u32.lo = USEC_PER_SEC;
    return 0;
}

#endif

