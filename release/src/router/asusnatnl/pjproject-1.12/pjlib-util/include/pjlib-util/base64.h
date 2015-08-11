/* $Id: base64.h 3553 2011-05-05 06:14:19Z nanang $ */
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
#ifndef __PJLIB_UTIL_BASE64_H__
#define __PJLIB_UTIL_BASE64_H__

/**
 * @file base64.h
 * @brief Base64 encoding and decoding
 */

#include <pjlib-util/types.h>

PJ_BEGIN_DECL

/**
 * @defgroup PJLIB_UTIL_BASE64 Base64 Encoding/Decoding
 * @ingroup PJLIB_UTIL_ENCRYPTION
 * @{
 * This module implements base64 encoding and decoding.
 */

/**
 * Helper macro to calculate the approximate length required for base256 to
 * base64 conversion.
 */
#define PJ_BASE256_TO_BASE64_LEN(len)	(len * 4 / 3 + 3)

/**
 * Helper macro to calculate the approximage length required for base64 to
 * base256 conversion.
 */
#define PJ_BASE64_TO_BASE256_LEN(len)	(len * 3 / 4)


/**
 * Encode a buffer into base64 encoding.
 *
 * @param input	    The input buffer.
 * @param in_len    Size of the input buffer.
 * @param output    Output buffer. Caller must allocate this buffer with
 *		    the appropriate size.
 * @param out_len   On entry, it specifies the length of the output buffer. 
 *		    Upon return, this will be filled with the actual
 *		    length of the output buffer.
 *
 * @return	    PJ_SUCCESS on success.
 */
PJ_DECL(pj_status_t) pj_base64_encode(const pj_uint8_t *input, int in_len,
				     char *output, int *out_len);


/**
 * Decode base64 string.
 *
 * @param input	    Input string.
 * @param out	    Buffer to store the output. Caller must allocate
 *		    this buffer with the appropriate size.
 * @param out_len   On entry, it specifies the length of the output buffer. 
 *		    Upon return, this will be filled with the actual
 *		    length of the output.
 */
PJ_DECL(pj_status_t) pj_base64_decode(const pj_str_t *input, 
				      pj_uint8_t *out, int *out_len);



/**
 * @}
 */

PJ_END_DECL


#endif	/* __PJLIB_UTIL_BASE64_H__ */

