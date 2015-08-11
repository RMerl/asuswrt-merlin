/* $Id: config.c 3553 2011-05-05 06:14:19Z nanang $ */
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
#include <pj/config.h>
#include <pj/log.h>
#include <pj/ioqueue.h>

static const char *id = "config.c";

#define PJ_MAKE_VERSION1(a,b,c,d) 	#a "." #b "." #c d
#define PJ_MAKE_VERSION2(a,b,c,d)	PJ_MAKE_VERSION1(a,b,c,d)

PJ_DEF_DATA(const char*) PJ_VERSION = PJ_MAKE_VERSION2(PJ_VERSION_NUM_MAJOR,
						       PJ_VERSION_NUM_MINOR,
						       PJ_VERSION_NUM_REV,
						       PJ_VERSION_NUM_EXTRA);

/*
 * Get PJLIB version string.
 */
PJ_DEF(const char*) pj_get_version(void)
{
    return PJ_VERSION;
}

PJ_DEF(void) pj_dump_config(void)
{
    PJ_LOG(3, (id, "PJLIB (c)2008-2009 Teluu Inc."));
    PJ_LOG(3, (id, "Dumping configurations:"));
    PJ_LOG(3, (id, " PJ_VERSION                : %s", PJ_VERSION));
    PJ_LOG(3, (id, " PJ_M_NAME                 : %s", PJ_M_NAME));
    PJ_LOG(3, (id, " PJ_HAS_PENTIUM            : %d", PJ_HAS_PENTIUM));
    PJ_LOG(3, (id, " PJ_OS_NAME                : %s", PJ_OS_NAME));
    PJ_LOG(3, (id, " PJ_CC_NAME/VER_(1,2,3)    : %s-%d.%d.%d", PJ_CC_NAME,
	       PJ_CC_VER_1, PJ_CC_VER_2, PJ_CC_VER_3));
    PJ_LOG(3, (id, " PJ_IS_(BIG/LITTLE)_ENDIAN : %s", 
	       (PJ_IS_BIG_ENDIAN?"big-endian":"little-endian")));
    PJ_LOG(3, (id, " PJ_HAS_INT64              : %d", PJ_HAS_INT64));
    PJ_LOG(3, (id, " PJ_HAS_FLOATING_POINT     : %d", PJ_HAS_FLOATING_POINT));
    PJ_LOG(3, (id, " PJ_DEBUG                  : %d", PJ_DEBUG));
    PJ_LOG(3, (id, " PJ_FUNCTIONS_ARE_INLINED  : %d", PJ_FUNCTIONS_ARE_INLINED));
    PJ_LOG(3, (id, " PJ_LOG_MAX_LEVEL          : %d", PJ_LOG_MAX_LEVEL));
    PJ_LOG(3, (id, " PJ_LOG_MAX_SIZE           : %d", PJ_LOG_MAX_SIZE));
    PJ_LOG(3, (id, " PJ_LOG_USE_STACK_BUFFER   : %d", PJ_LOG_USE_STACK_BUFFER));
    PJ_LOG(3, (id, " PJ_POOL_DEBUG             : %d", PJ_POOL_DEBUG));
    PJ_LOG(3, (id, " PJ_HAS_POOL_ALT_API       : %d", PJ_HAS_POOL_ALT_API));
    PJ_LOG(3, (id, " PJ_HAS_TCP                : %d", PJ_HAS_TCP));
    PJ_LOG(3, (id, " PJ_MAX_HOSTNAME           : %d", PJ_MAX_HOSTNAME));
    PJ_LOG(3, (id, " ioqueue type              : %s", pj_ioqueue_name()));
    PJ_LOG(3, (id, " PJ_IOQUEUE_MAX_HANDLES    : %d", PJ_IOQUEUE_MAX_HANDLES));
    PJ_LOG(3, (id, " PJ_IOQUEUE_HAS_SAFE_UNREG : %d", PJ_IOQUEUE_HAS_SAFE_UNREG));
    PJ_LOG(3, (id, " PJ_HAS_THREADS            : %d", PJ_HAS_THREADS));
    PJ_LOG(3, (id, " PJ_LOG_USE_STACK_BUFFER   : %d", PJ_LOG_USE_STACK_BUFFER));
    PJ_LOG(3, (id, " PJ_HAS_SEMAPHORE          : %d", PJ_HAS_SEMAPHORE));
    PJ_LOG(3, (id, " PJ_HAS_EVENT_OBJ          : %d", PJ_HAS_EVENT_OBJ));
    PJ_LOG(3, (id, " PJ_ENABLE_EXTRA_CHECK     : %d", PJ_ENABLE_EXTRA_CHECK));
    PJ_LOG(3, (id, " PJ_HAS_EXCEPTION_NAMES    : %d", PJ_HAS_EXCEPTION_NAMES));
    PJ_LOG(3, (id, " PJ_MAX_EXCEPTION_ID       : %d", PJ_MAX_EXCEPTION_ID));
    PJ_LOG(3, (id, " PJ_EXCEPTION_USE_WIN32_SEH: %d", PJ_EXCEPTION_USE_WIN32_SEH));
    PJ_LOG(3, (id, " PJ_TIMESTAMP_USE_RDTSC:   : %d", PJ_TIMESTAMP_USE_RDTSC));
    PJ_LOG(3, (id, " PJ_OS_HAS_CHECK_STACK     : %d", PJ_OS_HAS_CHECK_STACK));
    PJ_LOG(3, (id, " PJ_HAS_HIGH_RES_TIMER     : %d", PJ_HAS_HIGH_RES_TIMER));
}

