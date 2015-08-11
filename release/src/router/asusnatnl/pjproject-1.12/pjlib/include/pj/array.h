/* $Id: array.h 3553 2011-05-05 06:14:19Z nanang $ */
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
#ifndef __PJ_ARRAY_H__
#define __PJ_ARRAY_H__

/**
 * @file array.h
 * @brief PJLIB Array helper.
 */
#include <pj/types.h>

PJ_BEGIN_DECL

/**
 * @defgroup PJ_ARRAY Array helper.
 * @ingroup PJ_DS
 * @{
 *
 * This module provides helper to manipulate array of elements of any size.
 * It provides most used array operations such as insert, erase, and search.
 */

/**
 * Insert value to the array at the given position, and rearrange the
 * remaining nodes after the position.
 *
 * @param array	    the array.
 * @param elem_size the size of the individual element.
 * @param count	    the CURRENT number of elements in the array.
 * @param pos	    the position where the new element is put.
 * @param value	    the value to copy to the new element.
 */
PJ_DECL(void) pj_array_insert( void *array,
			       unsigned elem_size,
			       unsigned count,
			       unsigned pos,
			       const void *value);

/**
 * Erase a value from the array at given position, and rearrange the remaining
 * elements post the erased element.
 *
 * @param array	    the array.
 * @param elem_size the size of the individual element.
 * @param count	    the current number of elements in the array.
 * @param pos	    the index/position to delete.
 */
PJ_DECL(void) pj_array_erase( void *array,
			      unsigned elem_size,
			      unsigned count,
			      unsigned pos);

/**
 * Search the first value in the array according to matching function.
 *
 * @param array	    the array.
 * @param elem_size the individual size of the element.
 * @param count	    the number of elements.
 * @param matching  the matching function, which MUST return PJ_SUCCESS if 
 *		    the specified element match.
 * @param result    the pointer to the value found.
 *
 * @return	    PJ_SUCCESS if value is found, otherwise the error code.
 */
PJ_DECL(pj_status_t) pj_array_find(   const void *array, 
				      unsigned elem_size, 
				      unsigned count, 
				      pj_status_t (*matching)(const void *value),
				      void **result);

/**
 * @}
 */

PJ_END_DECL


#endif	/* __PJ_ARRAY_H__ */

