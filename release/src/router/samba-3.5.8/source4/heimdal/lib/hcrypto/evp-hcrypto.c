/*
 * Copyright (c) 2006 - 2008 Kungliga Tekniska Högskolan
 * (Royal Institute of Technology, Stockholm, Sweden).
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * 3. Neither the name of the Institute nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE INSTITUTE AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE INSTITUTE OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <config.h>

#define HC_DEPRECATED

#include <sys/types.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include <evp.h>

#include <krb5-types.h>

#include <aes.h>

/*
 *
 */

static int
aes_init(EVP_CIPHER_CTX *ctx,
	 const unsigned char * key,
	 const unsigned char * iv,
	 int encp)
{
    AES_KEY *k = ctx->cipher_data;
    if (ctx->encrypt)
	AES_set_encrypt_key(key, ctx->cipher->key_len * 8, k);
    else
	AES_set_decrypt_key(key, ctx->cipher->key_len * 8, k);
    return 1;
}

static int
aes_do_cipher(EVP_CIPHER_CTX *ctx,
	      unsigned char *out,
	      const unsigned char *in,
	      unsigned int size)
{
    AES_KEY *k = ctx->cipher_data;
    AES_cbc_encrypt(in, out, size, k, ctx->iv, ctx->encrypt);
    return 1;
}

static int
aes_cleanup(EVP_CIPHER_CTX *ctx)
{
    memset(ctx->cipher_data, 0, sizeof(AES_KEY));
    return 1;
}

/**
 * The AES-128 cipher type (hcrypto)
 *
 * @return the AES-128 EVP_CIPHER pointer.
 *
 * @ingroup hcrypto_evp
 */

const EVP_CIPHER *
EVP_hcrypto_aes_128_cbc(void)
{
    static const EVP_CIPHER aes_128_cbc = {
	0,
	16,
	16,
	16,
	EVP_CIPH_CBC_MODE,
	aes_init,
	aes_do_cipher,
	aes_cleanup,
	sizeof(AES_KEY),
	NULL,
	NULL,
	NULL,
	NULL
    };

    return &aes_128_cbc;
}

/**
 * The AES-192 cipher type (hcrypto)
 *
 * @return the AES-192 EVP_CIPHER pointer.
 *
 * @ingroup hcrypto_evp
 */

const EVP_CIPHER *
EVP_hcrypto_aes_192_cbc(void)
{
    static const EVP_CIPHER aes_192_cbc = {
	0,
	16,
	24,
	16,
	EVP_CIPH_CBC_MODE,
	aes_init,
	aes_do_cipher,
	aes_cleanup,
	sizeof(AES_KEY),
	NULL,
	NULL,
	NULL,
	NULL
    };
    return &aes_192_cbc;
}

/**
 * The AES-256 cipher type (hcrypto)
 *
 * @return the AES-256 EVP_CIPHER pointer.
 *
 * @ingroup hcrypto_evp
 */

const EVP_CIPHER *
EVP_hcrypto_aes_256_cbc(void)
{
    static const EVP_CIPHER aes_256_cbc = {
	0,
	16,
	32,
	16,
	EVP_CIPH_CBC_MODE,
	aes_init,
	aes_do_cipher,
	aes_cleanup,
	sizeof(AES_KEY),
	NULL,
	NULL,
	NULL,
	NULL
    };
    return &aes_256_cbc;
}
