/* $Id: crc32.h 3553 2011-05-05 06:14:19Z nanang $ */
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
#ifndef __PJLIB_UTIL_CRC32_H__
#define __PJLIB_UTIL_CRC32_H__

/**
 * @file crc32.h
 * @brief CRC32 implementation
 */

#include <pjlib-util/types.h>

PJ_BEGIN_DECL

/**
 * @defgroup PJLIB_UTIL_CRC32 CRC32 (Cyclic Redundancy Check)
 * @ingroup PJLIB_UTIL_ENCRYPTION
 * @{
 * This implements CRC32 algorithm. See ITU-T V.42 for the formal 
 * specification.
 */

/** CRC32 context. */
typedef struct pj_crc32_context
{
    pj_uint32_t	crc_state;	/**< Current state. */
} pj_crc32_context;


/**
 * Initialize CRC32 context.
 *
 * @param ctx	    CRC32 context.
 */
PJ_DECL(void) pj_crc32_init(pj_crc32_context *ctx);

/**
 * Feed data incrementally to the CRC32 algorithm.
 *
 * @param ctx	    CRC32 context.
 * @param data	    Input data.
 * @param nbytes    Length of the input data.
 *
 * @return	    The current CRC32 value.
 */
PJ_DECL(pj_uint32_t) pj_crc32_update(pj_crc32_context *ctx, 
				     const pj_uint8_t *data,
				     pj_size_t nbytes);

/**
 * Finalize CRC32 calculation and retrieve the CRC32 value.
 *
 * @param ctx	    CRC32 context.
 *
 * @return	    The current CRC value.
 */
PJ_DECL(pj_uint32_t) pj_crc32_final(pj_crc32_context *ctx);

/**
 * Perform one-off CRC32 calculation to the specified data.
 *
 * @param data	    Input data.
 * @param nbytes    Length of input data.
 *
 * @return	    CRC value of the data.
 */
PJ_DECL(pj_uint32_t) pj_crc32_calc(const pj_uint8_t *data,
				   pj_size_t nbytes);


/**
 * @}
 */

PJ_END_DECL


#endif	/* __PJLIB_UTIL_CRC32_H__ */

