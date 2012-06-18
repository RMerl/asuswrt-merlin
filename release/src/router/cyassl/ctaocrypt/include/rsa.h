/* rsa.h
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


#ifndef CTAO_CRYPT_RSA_H
#define CTAO_CRYPT_RSA_H

#include "types.h"
#include "integer.h"
#include "random.h"

#ifdef __cplusplus
    extern "C" {
#endif


enum {
    RSA_PUBLIC   = 0,
    RSA_PRIVATE  = 1
};

/* RSA */
typedef struct RsaKey {
    mp_int n, e, d, p, q, dP, dQ, u;
    int   type;                               /* public or private */
    void* heap;                               /* for user memory overrides */
} RsaKey;


void InitRsaKey(RsaKey* key, void*);
void FreeRsaKey(RsaKey* key);

int  RsaPublicEncrypt(const byte* in, word32 inLen, byte* out, word32 outLen,
                      RsaKey* key, RNG* rng);
int  RsaPrivateDecryptInline(byte* in, word32 inLen, byte** out, RsaKey* key);
int  RsaPrivateDecrypt(const byte* in, word32 inLen, byte* out, word32 outLen,
                       RsaKey* key);
int  RsaSSL_Sign(const byte* in, word32 inLen, byte* out, word32 outLen,
                 RsaKey* key, RNG* rng);
int  RsaSSL_VerifyInline(byte* in, word32 inLen, byte** out, RsaKey* key);
int  RsaSSL_Verify(const byte* in, word32 inLen, byte* out, word32 outLen,
                   RsaKey* key);

int  RsaEncryptSize(RsaKey* key);

#ifdef CYASSL_KEY_GEN
    int MakeRsaKey(RsaKey* key, int size, long e, RNG* rng);
#endif


#ifdef __cplusplus
    } /* extern "C" */
#endif

#endif /* CTAO_CRYPT_RSA_H */

