/* $Id: os_time_linux_kernel.c 3553 2011-05-05 06:14:19Z nanang $ */
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

///////////////////////////////////////////////////////////////////////////////

PJ_DEF(pj_status_t) pj_gettimeofday(pj_time_val *tv)
{
    struct timeval tval;
  
    do_gettimeofday(&tval);
    tv->sec = tval.tv_sec;
    tv->msec = tval.tv_usec / 1000;

    return 0;
}

PJ_DEF(pj_status_t) pj_gettimeofday2(pj_time_val2 *tv)
{
    struct timeval tval;
  
    do_gettimeofday(&tval);
    tv->sec = tval.tv_sec;
    tv->usec = tval.tv_usec;

    return 0;
}

PJ_DEF(pj_status_t) pj_time_decode(const pj_time_val *tv, pj_parsed_time *pt)
{
    pt->year = 2005;
    pt->mon = 8;
    pt->day = 20;
    pt->hour = 16;
    pt->min = 30;
    pt->sec = 30;
    pt->wday = 3;
    pt->yday = 200;
    pt->msec = 777;

    return -1;
}

/**
 * Encode parsed time to time value.
 */
PJ_DEF(pj_status_t) pj_time_encode(const pj_parsed_time *pt, pj_time_val *tv);

/**
 * Convert local time to GMT.
 */
PJ_DEF(pj_status_t) pj_time_local_to_gmt(pj_time_val *tv);

/**
 * Convert GMT to local time.
 */
PJ_DEF(pj_status_t) pj_time_gmt_to_local(pj_time_val *tv);


