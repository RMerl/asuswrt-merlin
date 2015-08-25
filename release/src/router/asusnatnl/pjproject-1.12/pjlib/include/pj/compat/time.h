/* $Id: time.h 3553 2011-05-05 06:14:19Z nanang $ */
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
#ifndef __PJ_COMPAT_TIME_H__
#define __PJ_COMPAT_TIME_H__

/**
 * @file time.h
 * @brief Provides ftime() and localtime() etc functions.
 */

#if defined(PJ_HAS_TIME_H) && PJ_HAS_TIME_H != 0
#  include <time.h>
#endif

#if defined(PJ_HAS_SYS_TIME_H) && PJ_HAS_SYS_TIME_H != 0
#  include <sys/time.h>
#endif

#if defined(PJ_HAS_SYS_TIMEB_H) && PJ_HAS_SYS_TIMEB_H != 0
#  include <sys/timeb.h>
#endif


#endif	/* __PJ_COMPAT_TIME_H__ */

