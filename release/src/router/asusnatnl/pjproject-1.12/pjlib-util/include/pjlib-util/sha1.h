/* $Id: sha1.h 3553 2011-05-05 06:14:19Z nanang $ */
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
#ifndef __PJLIB_UTIL_SHA1_H__
#define __PJLIB_UTIL_SHA1_H__

/**
 * @file sha1.h
 * @brief SHA1 encryption implementation
 */

#include <pj/types.h>

PJ_BEGIN_DECL

/**
 * @defgroup PJLIB_UTIL_SHA1 SHA1
 * @ingroup PJLIB_UTIL_ENCRYPTION
 * @{
 */

/** SHA1 context */
typedef struct pj_sha1_context
{
    pj_uint32_t state[5];	/**< State  */
    pj_uint32_t count[2];	/**< Count  */
    pj_uint8_t	buffer[64];	/**< Buffer */
} pj_sha1_context;

/** SHA1 digest size is 20 bytes */
#define PJ_SHA1_DIGEST_SIZE	20


/** Initialize the algorithm. 
 *  @param ctx		SHA1 context.
 */
PJ_DECL(void) pj_sha1_init(pj_sha1_context *ctx);

/** Append a stream to the message. 
 *  @param ctx		SHA1 context.
 *  @param data		Data.
 *  @param nbytes	Length of data.
 */
PJ_DECL(void) pj_sha1_update(pj_sha1_context *ctx, 
			     const pj_uint8_t *data, 
			     const pj_size_t nbytes);

/** Finish the message and return the digest. 
 *  @param ctx		SHA1 context.
 *  @param digest	16 byte digest.
 */
PJ_DECL(void) pj_sha1_final(pj_sha1_context *ctx, 
			    pj_uint8_t digest[PJ_SHA1_DIGEST_SIZE]);


/**
 * @}
 */

PJ_END_DECL


#endif	/* __PJLIB_UTIL_SHA1_H__ */

