/* $Id: array.c 3553 2011-05-05 06:14:19Z nanang $ */
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
#include <pj/array.h>
#include <pj/string.h>
#include <pj/assert.h>
#include <pj/errno.h>

PJ_DEF(void) pj_array_insert( void *array,
			      unsigned elem_size,
			      unsigned count,
			      unsigned pos,
			      const void *value)
{
    if (count && pos < count) {
	pj_memmove( (char*)array + (pos+1)*elem_size,
		    (char*)array + pos*elem_size,
		    (count-pos)*elem_size);
    }
    pj_memmove((char*)array + pos*elem_size, value, elem_size);
}

PJ_DEF(void) pj_array_erase( void *array,
			     unsigned elem_size,
			     unsigned count,
			     unsigned pos)
{
    pj_assert(count != 0);
    if (pos < count-1) {
	pj_memmove( (char*)array + pos*elem_size,
		    (char*)array + (pos+1)*elem_size,
		    (count-pos-1)*elem_size);
    }
}

PJ_DEF(pj_status_t) pj_array_find( const void *array, 
				   unsigned elem_size, 
				   unsigned count, 
				   pj_status_t (*matching)(const void *value),
				   void **result)
{
    unsigned i;
    const char *char_array = (const char*)array;
    for (i=0; i<count; ++i) {
	if ( (*matching)(char_array) == PJ_SUCCESS) {
	    if (result) {
		*result = (void*)char_array;
	    }
	    return PJ_SUCCESS;
	}
	char_array += elem_size;
	}
	printf("pj_array_find() array not found.\n");
    return PJ_ENOTFOUND;
}

