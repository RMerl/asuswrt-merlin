/* camellia.h	ver 1.2.0
 *
 * Copyright (C) 2006,2007
 * NTT (Nippon Telegraph and Telephone Corporation).
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 */

#ifndef HEADER_CAMELLIA_H
#define HEADER_CAMELLIA_H

#ifdef  __cplusplus
extern "C" {
#endif

#define CAMELLIA_BLOCK_SIZE 16
#define CAMELLIA_TABLE_BYTE_LEN 272
#define CAMELLIA_TABLE_WORD_LEN (CAMELLIA_TABLE_BYTE_LEN / 4)

/* u32 must be 32bit word */
typedef uint32_t u32;
typedef unsigned char u8;

typedef u32 KEY_TABLE_TYPE[CAMELLIA_TABLE_WORD_LEN];


void Camellia_Ekeygen(const int keyBitLength,
		      const unsigned char *rawKey,
		      KEY_TABLE_TYPE keyTable);

void Camellia_EncryptBlock(const int keyBitLength,
			   const unsigned char *plaintext,
			   const KEY_TABLE_TYPE keyTable,
			   unsigned char *cipherText);

void Camellia_DecryptBlock(const int keyBitLength,
			   const unsigned char *cipherText,
			   const KEY_TABLE_TYPE keyTable,
			   unsigned char *plaintext);


#ifdef  __cplusplus
}
#endif

#endif /* HEADER_CAMELLIA_H */
