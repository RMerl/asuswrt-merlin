/* $Id: log_writer_symbian_console.cpp 3553 2011-05-05 06:14:19Z nanang $ */
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
#include <pj/log.h>
#include <pj/os.h>
#include <pj/unicode.h>

#include "os_symbian.h"
#include <e32cons.h>

PJ_DEF(void) pj_log_write(int level, const char *buffer, int len)
{
#if 0
    wchar_t wbuffer[PJ_LOG_MAX_SIZE];
    CConsoleBase *cons = PjSymbianOS::Instance->Console();

    pj_ansi_to_unicode(buffer, len, wbuffer, PJ_ARRAY_SIZE(wbuffer));

    
    TPtrC16 aPtr((TUint16*)wbuffer, len);
    console->Write(aPtr);
#else
    PJ_UNUSED_ARG(level);
    PJ_UNUSED_ARG(buffer);
    PJ_UNUSED_ARG(len);
#endif
}

