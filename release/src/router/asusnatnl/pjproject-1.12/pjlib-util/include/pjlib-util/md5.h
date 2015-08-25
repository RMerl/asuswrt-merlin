/* $Id: md5.h 3553 2011-05-05 06:14:19Z nanang $ */
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
#ifndef __PJLIB_UTIL_MD5_H__
#define __PJLIB_UTIL_MD5_H__

/**
 * @file md5.h
 * @brief MD5 Functions
 */

#include <pj/types.h>

PJ_BEGIN_DECL

/**
 * @defgroup PJLIB_UTIL_MD5 MD5
 * @ingroup PJLIB_UTIL_ENCRYPTION
 * @{
 */


/** MD5 context. */
typedef struct pj_md5_context
{
	pj_uint32_t buf[4];	/**< buf    */
	pj_uint32_t bits[2];	/**< bits   */
	pj_uint8_t  in[64];	/**< in	    */
} pj_md5_context;

/** Initialize the algorithm. 
 *  @param pms		MD5 context.
 */
PJ_DECL(void) pj_md5_init(pj_md5_context *pms);

/** Append a string to the message. 
 *  @param pms		MD5 context.
 *  @param data		Data.
 *  @param nbytes	Length of data.
 */
PJ_DECL(void) pj_md5_update( pj_md5_context *pms, 
			     const pj_uint8_t *data, unsigned nbytes);

/** Finish the message and return the digest. 
 *  @param pms		MD5 context.
 *  @param digest	16 byte digest.
 */
PJ_DECL(void) pj_md5_final(pj_md5_context *pms, pj_uint8_t digest[16]);


/**
 * @}
 */

PJ_END_DECL


#endif	/* __PJLIB_UTIL_MD5_H__ */
