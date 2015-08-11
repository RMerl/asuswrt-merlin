/* $Id: scanner_cis_bitwise.h 3553 2011-05-05 06:14:19Z nanang $ */
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
#ifndef __PJLIB_UTIL_SCANNER_CIS_BIT_H__
#define __PJLIB_UTIL_SCANNER_CIS_BIT_H__

#include <pj/types.h>

PJ_BEGIN_DECL

/**
 * This describes the type of individual character specification in
 * #pj_cis_buf_t. Basicly the number of bits here
 */
#ifndef PJ_CIS_ELEM_TYPE
#   define PJ_CIS_ELEM_TYPE pj_uint32_t
#endif

/**
 * This describes the type of individual character specification in
 * #pj_cis_buf_t.
 */
typedef PJ_CIS_ELEM_TYPE pj_cis_elem_t;

/**
 * Maximum number of input specification in a buffer.
 * Effectively this means the number of bits in pj_cis_elem_t.
 */
#define PJ_CIS_MAX_INDEX   (sizeof(pj_cis_elem_t) << 3)

/**
 * The scanner input specification buffer.
 */
typedef struct pj_cis_buf_t
{
    pj_cis_elem_t    cis_buf[256];  /**< Must be 256 (not 128)! */
    pj_cis_elem_t    use_mask;      /**< To keep used indexes.  */
} pj_cis_buf_t;

/**
 * Character input specification.
 */
typedef struct pj_cis_t
{
    pj_cis_elem_t   *cis_buf;       /**< Pointer to buffer.     */
    int              cis_id;        /**< Id.                    */
} pj_cis_t;


/**
 * Set the membership of the specified character.
 * Note that this is a macro, and arguments may be evaluated more than once.
 *
 * @param cis       Pointer to character input specification.
 * @param c         The character.
 */
#define PJ_CIS_SET(cis,c)   ((cis)->cis_buf[(int)(c)] |= (1 << (cis)->cis_id))

/**
 * Remove the membership of the specified character.
 * Note that this is a macro, and arguments may be evaluated more than once.
 *
 * @param cis       Pointer to character input specification.
 * @param c         The character to be removed from the membership.
 */
#define PJ_CIS_CLR(cis,c)   ((cis)->cis_buf[(int)c] &= ~(1 << (cis)->cis_id))

/**
 * Check the membership of the specified character.
 * Note that this is a macro, and arguments may be evaluated more than once.
 *
 * @param cis       Pointer to character input specification.
 * @param c         The character.
 */
#define PJ_CIS_ISSET(cis,c) ((cis)->cis_buf[(int)c] & (1 << (cis)->cis_id))



PJ_END_DECL

#endif	/* __PJLIB_UTIL_SCANNER_CIS_BIT_H__ */
