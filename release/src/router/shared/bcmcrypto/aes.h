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
 * aes.h
 * AES encrypt/decrypt wrapper functions used around Rijndael reference 
 * implementation
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

#ifndef _AES_H_
#define _AES_H_

#include <typedefs.h>
/* For size_t */
#include <stddef.h>

#define AES_BLOCK_SZ    	16
#define AES_BLOCK_BITLEN	(AES_BLOCK_SZ * 8)
#define AES_KEY_BITLEN(kl)  	(kl * 8)
#define AES_ROUNDS(kl)		((AES_KEY_BITLEN(kl) / 32) + 6)
#define AES_MAXROUNDS		14

void aes_encrypt(const size_t KL, const uint8 *K, const uint8 *ptxt, uint8 *ctxt);
void aes_decrypt(const size_t KL, const uint8 *K, const uint8 *ctxt, uint8 *ptxt);

#define aes_block_encrypt(nr, rk, ptxt, ctxt) rijndaelEncrypt(rk, nr, ptxt, ctxt)
#define aes_block_decrypt(nr, rk, ctxt, ptxt) rijndaelDecrypt(rk, nr, ctxt, ptxt)

#endif /* _AES_H_ */

