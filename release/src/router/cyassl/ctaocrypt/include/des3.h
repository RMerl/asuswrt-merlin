/* des3.h
 *
 * Copyright (C) 2006-2009 Sawtooth Consulting Ltd.
 *
 * This file is part of CyaSSL.
 *
 * CyaSSL is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * CyaSSL is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA
 */

#ifndef NO_DES3

#ifndef CTAO_CRYPT_DES3_H
#define CTAO_CRYPT_DES3_H


#include "types.h"


#ifdef __cplusplus
    extern "C" {
#endif

enum {
    DES_BLOCK_SIZE  = 8,
    DES_KS_SIZE     = 32,

    DES_ENCRYPTION  = 0,
    DES_DECRYPTION  = 1,
};


/* DES encryption and decryption */
typedef struct Des {
    word32 key[DES_KS_SIZE];
    word32 reg[DES_BLOCK_SIZE / sizeof(word32)];      /* for CBC mode */
    word32 tmp[DES_BLOCK_SIZE / sizeof(word32)];      /* same         */
} Des;


/* DES3 encryption and decryption */
typedef struct Des3 {
    word32 key[3][DES_KS_SIZE];
    word32 reg[DES_BLOCK_SIZE / sizeof(word32)];      /* for CBC mode */
    word32 tmp[DES_BLOCK_SIZE / sizeof(word32)];      /* same         */
} Des3;


void Des_SetKey(Des* des, const byte* key, const byte* iv, int dir);
void Des_CbcEncrypt(Des* des, byte* out, const byte* in, word32 sz);
void Des_CbcDecrypt(Des* des, byte* out, const byte* in, word32 sz);

void Des3_SetKey(Des3* des, const byte* key, const byte* iv, int dir);
void Des3_CbcEncrypt(Des3* des, byte* out, const byte* in, word32 sz);
void Des3_CbcDecrypt(Des3* des, byte* out, const byte* in, word32 sz);


#ifdef __cplusplus
    } /* extern "C" */
#endif

#endif /* NO_DES3 */
#endif /* CTAO_CRYPT_DES3_H */

