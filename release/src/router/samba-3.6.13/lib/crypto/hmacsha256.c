/* 
   Unix SMB/CIFS implementation.

   Interface header:    HMAC SHA-256 code

   Copyright (C) Andrew Tridgell 2008

   based in hmacsha1.c which is:
     Copyright (C) Stefan Metzmacher
   
   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 3 of the License, or
   (at your option) any later version.
   
   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.
   
   You should have received a copy of the GNU General Public License
   along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

/*
 taken direct from rfc2202 implementation and modified for suitable use
 */

#include "replace.h"
#include "../lib/crypto/crypto.h"

/***********************************************************************
 the rfc 2104/2202 version of hmac_sha256 initialisation.
***********************************************************************/
_PUBLIC_ void hmac_sha256_init(const uint8_t *key, size_t key_len, struct HMACSHA256Context *ctx)
{
        int i;
	uint8_t tk[SHA256_DIGEST_LENGTH];

        /* if key is longer than 64 bytes reset it to key=HASH(key) */
        if (key_len > 64)
	{
                SHA256_CTX tctx;

                SHA256_Init(&tctx);
                SHA256_Update(&tctx, key, key_len);
                SHA256_Final(tk, &tctx);

                key = tk;
                key_len = SHA256_DIGEST_LENGTH;
        }

        /* start out by storing key in pads */
        ZERO_STRUCT(ctx->k_ipad);
        ZERO_STRUCT(ctx->k_opad);
        memcpy( ctx->k_ipad, key, key_len);
        memcpy( ctx->k_opad, key, key_len);

        /* XOR key with ipad and opad values */
        for (i=0; i<64; i++)
	{
                ctx->k_ipad[i] ^= 0x36;
                ctx->k_opad[i] ^= 0x5c;
        }

        SHA256_Init(&ctx->ctx);
        SHA256_Update(&ctx->ctx, ctx->k_ipad, 64);  
}

/***********************************************************************
 update hmac_sha256 "inner" buffer
***********************************************************************/
_PUBLIC_ void hmac_sha256_update(const uint8_t *data, size_t data_len, struct HMACSHA256Context *ctx)
{
        SHA256_Update(&ctx->ctx, data, data_len); /* then text of datagram */
}

/***********************************************************************
 finish off hmac_sha256 "inner" buffer and generate outer one.
***********************************************************************/
_PUBLIC_ void hmac_sha256_final(uint8_t digest[SHA256_DIGEST_LENGTH], struct HMACSHA256Context *ctx)
{
        SHA256_CTX ctx_o;

        SHA256_Final(digest, &ctx->ctx);

        SHA256_Init(&ctx_o);
        SHA256_Update(&ctx_o, ctx->k_opad, 64);
        SHA256_Update(&ctx_o, digest, SHA256_DIGEST_LENGTH);
        SHA256_Final(digest, &ctx_o);
}
