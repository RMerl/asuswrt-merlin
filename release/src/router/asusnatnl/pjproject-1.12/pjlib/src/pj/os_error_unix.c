/* $Id: os_error_unix.c 3553 2011-05-05 06:14:19Z nanang $ */
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
#include <pj/errno.h>
#include <pj/string.h>
#include <errno.h>

PJ_DEF(pj_status_t) pj_get_os_error(void)
{
    return PJ_STATUS_FROM_OS(errno);
}

PJ_DEF(void) pj_set_os_error(pj_status_t code)
{
    errno = PJ_STATUS_TO_OS(code);
}

PJ_DEF(pj_status_t) pj_get_netos_error(void)
{
    return PJ_STATUS_FROM_OS(errno);
}

PJ_DEF(void) pj_set_netos_error(pj_status_t code)
{
    errno = PJ_STATUS_TO_OS(code);
}

PJ_BEGIN_DECL

    PJ_DECL(int) platform_strerror(pj_os_err_type code, 
                              	   char *buf, pj_size_t bufsize );
PJ_END_DECL

/* 
 * platform_strerror()
 *
 * Platform specific error message. This file is called by pj_strerror() 
 * in errno.c 
 */
int platform_strerror( pj_os_err_type os_errcode, 
                       char *buf, pj_size_t bufsize)
{
    const char *syserr = strerror(os_errcode);
    pj_size_t len = syserr ? strlen(syserr) : 0;

    if (len >= bufsize) len = bufsize - 1;
    if (len > 0)
	pj_memcpy(buf, syserr, len);
    buf[len] = '\0';
    return len;
}


