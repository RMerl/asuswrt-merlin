/* $Id: guid_win32.c 3553 2011-05-05 06:14:19Z nanang $ */
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
#include <pj/guid.h>
#include <pj/string.h>
#include <pj/sock.h>
#include <windows.h>
#include <objbase.h>
#include <pj/os.h>


PJ_DEF_DATA(const unsigned) PJ_GUID_STRING_LENGTH=32;

PJ_DEF(unsigned) pj_GUID_STRING_LENGTH()
{
    return PJ_GUID_STRING_LENGTH;
}

PJ_INLINE(void) hex2digit(unsigned value, char *p)
{
    static char hex[] = {'0', '1', '2', '3', '4', '5', '6', '7',
			 '8', '9', 'a', 'b', 'c', 'd', 'e', 'f' };
    *p++ = hex[ (value & 0xF0) >> 4 ];
    *p++ = hex[ (value & 0x0F) ];
}

static void guid_to_str( GUID *guid, pj_str_t *str )
{
    unsigned i;
    const unsigned char *src = (const unsigned char*)guid;
    char *dst = str->ptr;

    guid->Data1 = pj_ntohl(guid->Data1);
    guid->Data2 = pj_ntohs(guid->Data2);
    guid->Data3 = pj_ntohs(guid->Data3);

    for (i=0; i<16; ++i) {
	hex2digit( *src, dst );
	dst += 2;
	++src;
    }
    str->slen = 32;
}


PJ_DEF(pj_str_t*) pj_generate_unique_string(int inst_id, pj_str_t *str)
{
    GUID guid;

	PJ_UNUSED_ARG(inst_id);

    PJ_CHECK_STACK();

    CoCreateGuid(&guid);
    guid_to_str( &guid, str );
    return str;
}

