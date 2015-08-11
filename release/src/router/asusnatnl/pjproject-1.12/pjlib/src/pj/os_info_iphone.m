/* $Id: os_info_iphone.m 3553 2011-05-05 06:14:19Z nanang $ */
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
#include "TargetConditionals.h"

#if !defined TARGET_IPHONE_SIMULATOR || TARGET_IPHONE_SIMULATOR == 0

#include <pj/os.h>
#include <pj/string.h>

#include <UIKit/UIDevice.h>

void pj_iphone_os_get_sys_info(pj_sys_info *si, pj_str_t *si_buffer)
{
    unsigned buf_len = si_buffer->slen, left = si_buffer->slen, len;
    UIDevice *device = [UIDevice currentDevice];
    
    if ([device respondsToSelector:@selector(isMultitaskingSupported)])
	si->flags |= PJ_SYS_HAS_IOS_BG;
    
#define ALLOC_CP_STR(str,field)	\
    do { \
	len = [str length]; \
	if (len && left >= len+1) { \
	    si->field.ptr = si_buffer->ptr + buf_len - left; \
	    si->field.slen = len; \
	    [str getCString:si->field.ptr maxLength:len+1 \
		 encoding:NSASCIIStringEncoding]; \
	    left -= (len+1); \
	} \
    } while (0)

    ALLOC_CP_STR([device systemName], os_name);
    ALLOC_CP_STR([device systemVersion], machine);
}

#endif
