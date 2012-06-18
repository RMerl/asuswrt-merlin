/*
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of
 * the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston,
 * MA 02111-1307 USA
 */
/*
 * aeskeywrap.h
 * Perform RFC3394 AES-based key wrap and unwrap functions.  
 *
 * Copyright 2003, ASUSTeK Inc.
 * All Rights Reserved.
 *     
 * This is UNPUBLISHED PROPRIETARY SOURCE CODE of ASUSTeK Inc.; the
 * contents of this file may not be disclosed to third parties, copied or
 * duplicated in any form, in whole or in part, without the prior written
 * permission of ASUSTeK Inc..
 *
 */

#ifndef _AESWRAP_H_
#define _AESWRAP_H_

#include <typedefs.h>
/* For size_t */
#include <stddef.h>
#include <bcmcrypto/aes.h>

#define AKW_BLOCK_LEN	8
/* Max size of wrapped data, not including overhead
   The key wrap algorithm doesn't impose any upper bound the wrap length,
   but we need something big enought to handle all users.  802.11i and
   probably most other users will be limited by MTU size, so using a 2K
   buffer limit seems relatively safe. */
#define AKW_MAX_WRAP_LEN	2048

/* aes_wrap: perform AES-based keywrap function defined in RFC3394
	return 0 on success, 1 on error
	input is il bytes
	output is (il+8) bytes */
int aes_wrap(size_t kl, uint8 *key, size_t il, uint8 *input, uint8 *output);

/* aes_unwrap: perform AES-based key unwrap function defined in RFC3394,
	return 0 on success, 1 on error
	input is il bytes
	output is (il-8) bytes */
int aes_unwrap(size_t kl, uint8 *key, size_t il, uint8 *input, uint8 *output);

#endif /* _AESWRAP_H_ */

