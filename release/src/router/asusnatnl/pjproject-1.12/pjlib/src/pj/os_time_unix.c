/* $Id: os_time_unix.c 3553 2011-05-05 06:14:19Z nanang $ */
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
#include <pj/errno.h>
#include <pj/compat/time.h>

#if defined(PJ_HAS_UNISTD_H) && PJ_HAS_UNISTD_H!=0
#    include <unistd.h>
#endif

#include <errno.h>

///////////////////////////////////////////////////////////////////////////////

PJ_DEF(pj_status_t) pj_gettimeofday(pj_time_val *p_tv)
{
    struct timeval the_time;
    int rc;

    PJ_CHECK_STACK();

    rc = gettimeofday(&the_time, NULL);
    if (rc != 0)
	return PJ_RETURN_OS_ERROR(pj_get_native_os_error());

    p_tv->sec = the_time.tv_sec;
    p_tv->msec = the_time.tv_usec / 1000;
    return PJ_SUCCESS;
}

