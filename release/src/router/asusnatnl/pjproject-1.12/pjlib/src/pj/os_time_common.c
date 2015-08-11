/* $Id: os_time_common.c 3553 2011-05-05 06:14:19Z nanang $ */
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
#include <pj/compat/time.h>
#include <pj/errno.h>


///////////////////////////////////////////////////////////////////////////////

PJ_DEF(pj_status_t) pj_time_decode(const pj_time_val *tv, pj_parsed_time *pt)
{
    struct tm *local_time;

    PJ_CHECK_STACK();

    local_time = localtime((time_t*)&tv->sec);

    pt->year = local_time->tm_year+1900;
    pt->mon = local_time->tm_mon;
    pt->day = local_time->tm_mday;
    pt->hour = local_time->tm_hour;
    pt->min = local_time->tm_min;
    pt->sec = local_time->tm_sec;
    pt->wday = local_time->tm_wday;
    pt->msec = tv->msec;

    return PJ_SUCCESS;
}

/**
 * Encode parsed time to time value.
 */
PJ_DEF(pj_status_t) pj_time_encode(const pj_parsed_time *pt, pj_time_val *tv)
{
    struct tm local_time;

    local_time.tm_year = pt->year-1900;
    local_time.tm_mon = pt->mon;
    local_time.tm_mday = pt->day;
    local_time.tm_hour = pt->hour;
    local_time.tm_min = pt->min;
    local_time.tm_sec = pt->sec;
    local_time.tm_isdst = 0;
    
    tv->sec = mktime(&local_time);
    tv->msec = pt->msec;

    return PJ_SUCCESS;
}

/**
 * Convert local time to GMT.
 */
PJ_DEF(pj_status_t) pj_time_local_to_gmt(pj_time_val *tv)
{
    PJ_UNUSED_ARG(tv);
    return PJ_EBUG;
}

/**
 * Convert GMT to local time.
 */
PJ_DEF(pj_status_t) pj_time_gmt_to_local(pj_time_val *tv)
{
    PJ_UNUSED_ARG(tv);
    return PJ_EBUG;
}


