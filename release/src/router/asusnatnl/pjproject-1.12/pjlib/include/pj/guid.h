/* $Id: guid.h 4385 2013-02-27 10:11:59Z nanang $ */
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
#ifndef __PJ_GUID_H__
#define __PJ_GUID_H__


/**
 * @file guid.h
 * @brief GUID Globally Unique Identifier.
 */
#include <pj/types.h>


PJ_BEGIN_DECL


/**
 * @defgroup PJ_DS Data Structure.
 */
/**
 * @defgroup PJ_GUID Globally Unique Identifier
 * @ingroup PJ_DS
 * @{
 *
 * This module provides API to create string that is globally unique.
 * If application doesn't require that strong requirement, it can just
 * use #pj_create_random_string() instead.
 */


/**
 * PJ_GUID_STRING_LENGTH specifies length of GUID string. The value is
 * dependent on the algorithm used internally to generate the GUID string.
 * If real GUID generator is used, then the length will be between 32 and
 * 36 bytes. Application should not assume which algorithm will
 * be used by GUID generator.
 *
 * Regardless of the actual length of the GUID, it will not exceed
 * PJ_GUID_MAX_LENGTH characters.
 *
 * @see pj_GUID_STRING_LENGTH()
 * @see PJ_GUID_MAX_LENGTH
 */
PJ_DECL_DATA(const unsigned) PJ_GUID_STRING_LENGTH;

/**
 * Get #PJ_GUID_STRING_LENGTH constant.
 */
PJ_DECL(unsigned) pj_GUID_STRING_LENGTH(void);

/**
 * PJ_GUID_MAX_LENGTH specifies the maximum length of GUID string,
 * regardless of which algorithm to use.
 */
#define PJ_GUID_MAX_LENGTH  36

/**
 * Create a globally unique string, which length is PJ_GUID_STRING_LENGTH
 * characters. Caller is responsible for preallocating the storage used
 * in the string.
 *
 * @param inst_id   The instance id of pjsua.
 * @param str       The string to store the result.
 *
 * @return          The string.
 */
PJ_DECL(pj_str_t*) pj_generate_unique_string(int inst_id, pj_str_t *str);

/**
 * Create a globally unique string in lowercase, which length is
 * PJ_GUID_STRING_LENGTH characters. Caller is responsible for preallocating
 * the storage used in the string.
 *
 * @param inst_id   The instance id of pjsua.
 * @param str       The string to store the result.
 *
 * @return          The string.
 */
PJ_DECL(pj_str_t*) pj_generate_unique_string_lower(int inst_id, pj_str_t *str);

/**
 * Generate a unique string.
 *
 * @param pool	    Pool to allocate memory from.
 * @param str	    The string.
 */
PJ_DECL(void) pj_create_unique_string(pj_pool_t *pool, pj_str_t *str);

/**
 * Generate a unique string in lowercase.
 *
 * @param pool	    Pool to allocate memory from.
 * @param str	    The string.
 */
PJ_DECL(void) pj_create_unique_string_lower(pj_pool_t *pool, pj_str_t *str);


/**
 * @}
 */

PJ_END_DECL

#endif/* __PJ_GUID_H__ */

