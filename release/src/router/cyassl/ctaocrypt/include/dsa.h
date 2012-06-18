/* dsa.h
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

#ifndef NO_DSA

#ifndef CTAO_CRYPT_DSA_H
#define CTAO_CRYPT_DSA_H

#include "types.h"
#include "integer.h"
#include "random.h"

#ifdef __cplusplus
    extern "C" {
#endif


enum {
    DSA_PUBLIC   = 0,
    DSA_PRIVATE  = 1
};

/* DSA */
typedef struct DsaKey {
    mp_int p, q, g, y, x;
    int type;                               /* public or private */
} DsaKey;


void InitDsaKey(DsaKey* key);
void FreeDsaKey(DsaKey* key);

int DsaSign(const byte* digest, byte* out, DsaKey* key, RNG* rng);
int DsaVerify(const byte* digest, const byte* sig, DsaKey* key, int* answer);


#ifdef __cplusplus
    } /* extern "C" */
#endif

#endif /* CTAO_CRYPT_DSA_H */
#endif /* NO_DSA */

