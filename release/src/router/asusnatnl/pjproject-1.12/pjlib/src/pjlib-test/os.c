/* $Id: os.c 3759 2011-09-21 04:27:58Z bennylp $ */
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
#include "test.h"
#include <pj/log.h>
#include <pj/os.h>

#if INCLUDE_OS_TEST
static int endianness_test32(void)
{
    union t
    {
	pj_uint32_t u32;
	pj_uint16_t u16[2];
	pj_uint8_t u8[4];
    } t;

    PJ_LOG(3,("", " Testing endianness.."));

    t.u32 = 0x11223344;

#if defined(PJ_IS_LITTLE_ENDIAN) && PJ_IS_LITTLE_ENDIAN
    PJ_LOG(3,("", "   Library is set to little endian"));

#  if defined(PJ_IS_BIG_ENDIAN) && PJ_IS_BIG_ENDIAN
#    error Error: Both PJ_IS_LITTLE_ENDIAN and PJ_IS_BIG_ENDIAN are set!
#  endif

    if ((t.u16[0] & 0xFFFF) != 0x3344 ||
	(t.u16[1] & 0xFFFF) != 0x1122)
    {
	PJ_LOG(3,("", "   Error: wrong 16bit values 0x%x and 0x%x",
		      (t.u16[0] & 0xFFFF), (t.u16[1] & 0xFFFF)));
	return 10;
    }

    if ((t.u8[0] & 0xFF) != 0x44 ||
	(t.u8[1] & 0xFF) != 0x33 ||
	(t.u8[2] & 0xFF) != 0x22 ||
	(t.u8[3] & 0xFF) != 0x11)
    {
	PJ_LOG(3,("", "   Error: wrong 8bit values"));
	return 12;
    }

#elif defined(PJ_IS_BIG_ENDIAN) && PJ_IS_BIG_ENDIAN
    PJ_LOG(3,("", "   Library is set to big endian"));

    if ((t.u16[0] & 0xFFFF) != 0x1122 ||
	(t.u16[1] & 0xFFFF) != 0x3344)
    {
	PJ_LOG(3,("", "   Error: wrong 16bit values 0x%x and 0x%x",
		      (t.u16[0] & 0xFFFF), (t.u16[1] & 0xFFFF)));
	return 20;
    }

    if ((t.u8[0] & 0xFF) != 0x11 ||
	(t.u8[1] & 0xFF) != 0x22 ||
	(t.u8[2] & 0xFF) != 0x33 ||
	(t.u8[3] & 0xFF) != 0x44)
    {
	PJ_LOG(3,("", "   Error: wrong 8bit values"));
	return 22;
    }

#  if defined(PJ_IS_LITTLE_ENDIAN) && PJ_IS_LITTLE_ENDIAN
#    error Error: Both PJ_IS_LITTLE_ENDIAN and PJ_IS_BIG_ENDIAN are set!
#  endif


#else
#    error Error: Endianness is not set properly!
#endif

    return 0;
}

int os_test(void)
{
    const pj_sys_info *si;
    int rc = 0;

    PJ_LOG(3,("", " Sys info:"));
    si = pj_get_sys_info();
    PJ_LOG(3,("", "   machine:  %s", si->machine.ptr));
    PJ_LOG(3,("", "   os_name:  %s", si->os_name.ptr));
    PJ_LOG(3,("", "   os_ver:   0x%x", si->os_ver));
    PJ_LOG(3,("", "   sdk_name: %s", si->sdk_name.ptr));
    PJ_LOG(3,("", "   sdk_ver:  0x%x", si->sdk_ver));
    PJ_LOG(3,("", "   info:     %s", si->info.ptr));

    rc = endianness_test32();

    return rc;
}

#else
int dummy_os_var;
#endif

