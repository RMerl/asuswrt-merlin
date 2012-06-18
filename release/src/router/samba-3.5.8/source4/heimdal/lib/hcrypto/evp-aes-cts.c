/*
 * Copyright (c) 2006 - 2007 Kungliga Tekniska Högskolan
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


#include <krb5-types.h>

#if defined(BUILD_KRB5_LIB) && defined(HAVE_OPENSSL)
#include <openssl/evp.h>
#include <openssl/aes.h>

#define _hc_EVP_hcrypto_aes_128_cts _krb5_EVP_hcrypto_aes_128_cts
#define _hc_EVP_hcrypto_aes_192_cts _krb5_EVP_hcrypto_aes_192_cts
#define _hc_EVP_hcrypto_aes_256_cts _krb5_EVP_hcrypto_aes_256_cts

const EVP_CIPHER * _krb5_EVP_hcrypto_aes_128_cts(void);
const EVP_CIPHER * _krb5_EVP_hcrypto_aes_192_cts(void);
const EVP_CIPHER * _krb5_EVP_hcrypto_aes_256_cts(void);

#else
#include <evp.h>
#include <aes.h>

#define _hc_EVP_hcrypto_aes_128_cts hc_EVP_hcrypto_aes_128_cts
#define _hc_EVP_hcrypto_aes_192_cts hc_EVP_hcrypto_aes_192_cts
#define _hc_EVP_hcrypto_aes_256_cts hc_EVP_hcrypto_aes_256_cts

#endif

/*
 *
 */

static int
aes_cts_init(EVP_CIPHER_CTX *ctx,
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

static void
_krb5_aes_cts_encrypt(const unsigned char *in, unsigned char *out,
		      size_t len, const AES_KEY *key,
		      unsigned char *ivec, const int encryptp)
{
    unsigned char tmp[AES_BLOCK_SIZE];
    int i;

    /*
     * In the framework of kerberos, the length can never be shorter
     * then at least one blocksize.
     */

    if (encryptp) {

	while(len > AES_BLOCK_SIZE) {
	    for (i = 0; i < AES_BLOCK_SIZE; i++)
		tmp[i] = in[i] ^ ivec[i];
	    AES_encrypt(tmp, out, key);
	    memcpy(ivec, out, AES_BLOCK_SIZE);
	    len -= AES_BLOCK_SIZE;
	    in += AES_BLOCK_SIZE;
	    out += AES_BLOCK_SIZE;
	}

	for (i = 0; i < len; i++)
	    tmp[i] = in[i] ^ ivec[i];
	for (; i < AES_BLOCK_SIZE; i++)
	    tmp[i] = 0 ^ ivec[i];

	AES_encrypt(tmp, out - AES_BLOCK_SIZE, key);

	memcpy(out, ivec, len);
	memcpy(ivec, out - AES_BLOCK_SIZE, AES_BLOCK_SIZE);

    } else {
	unsigned char tmp2[AES_BLOCK_SIZE];
	unsigned char tmp3[AES_BLOCK_SIZE];

	while(len > AES_BLOCK_SIZE * 2) {
	    memcpy(tmp, in, AES_BLOCK_SIZE);
	    AES_decrypt(in, out, key);
	    for (i = 0; i < AES_BLOCK_SIZE; i++)
		out[i] ^= ivec[i];
	    memcpy(ivec, tmp, AES_BLOCK_SIZE);
	    len -= AES_BLOCK_SIZE;
	    in += AES_BLOCK_SIZE;
	    out += AES_BLOCK_SIZE;
	}

	len -= AES_BLOCK_SIZE;

	memcpy(tmp, in, AES_BLOCK_SIZE); /* save last iv */
	AES_decrypt(in, tmp2, key);

	memcpy(tmp3, in + AES_BLOCK_SIZE, len);
	memcpy(tmp3 + len, tmp2 + len, AES_BLOCK_SIZE - len); /* xor 0 */

	for (i = 0; i < len; i++)
	    out[i + AES_BLOCK_SIZE] = tmp2[i] ^ tmp3[i];

	AES_decrypt(tmp3, out, key);
	for (i = 0; i < AES_BLOCK_SIZE; i++)
	    out[i] ^= ivec[i];
	memcpy(ivec, tmp, AES_BLOCK_SIZE);
    }
}

static int
aes_cts_do_cipher(EVP_CIPHER_CTX *ctx,
		  unsigned char *out,
		  const unsigned char *in,
		  unsigned int len)
{
    AES_KEY *k = ctx->cipher_data;

    if (len < AES_BLOCK_SIZE)
	abort();  /* krb5_abortx(context, "invalid use of AES_CTS_encrypt"); */
    if (len == AES_BLOCK_SIZE) {
	if (ctx->encrypt)
	    AES_encrypt(in, out, k);
	else
	    AES_decrypt(in, out, k);
    } else {
	_krb5_aes_cts_encrypt(in, out, len, k, ctx->iv, ctx->encrypt);
    }

    return 1;
}


static int
aes_cts_cleanup(EVP_CIPHER_CTX *ctx)
{
    memset(ctx->cipher_data, 0, sizeof(AES_KEY));
    return 1;
}

/**
 * The AES-128 cts cipher type (hcrypto)
 *
 * @return the AES-128 EVP_CIPHER pointer.
 *
 * @ingroup hcrypto_evp
 */

const EVP_CIPHER *
_hc_EVP_hcrypto_aes_128_cts(void)
{
    static const EVP_CIPHER aes_128_cts = {
	0,
	1,
	16,
	16,
	EVP_CIPH_CBC_MODE,
	aes_cts_init,
	aes_cts_do_cipher,
	aes_cts_cleanup,
	sizeof(AES_KEY),
	NULL,
	NULL,
	NULL,
	NULL
    };

    return &aes_128_cts;
}

/**
 * The AES-192 cts cipher type (hcrypto)
 *
 * @return the AES-192 EVP_CIPHER pointer.
 *
 * @ingroup hcrypto_evp
 */

const EVP_CIPHER *
_hc_EVP_hcrypto_aes_192_cts(void)
{
    static const EVP_CIPHER aes_192_cts = {
	0,
	1,
	24,
	16,
	EVP_CIPH_CBC_MODE,
	aes_cts_init,
	aes_cts_do_cipher,
	aes_cts_cleanup,
	sizeof(AES_KEY),
	NULL,
	NULL,
	NULL,
	NULL
    };

    return &aes_192_cts;
}

/**
 * The AES-256 cts cipher type (hcrypto)
 *
 * @return the AES-256 EVP_CIPHER pointer.
 *
 * @ingroup hcrypto_evp
 */

const EVP_CIPHER *
_hc_EVP_hcrypto_aes_256_cts(void)
{
    static const EVP_CIPHER aes_256_cts = {
	0,
	1,
	32,
	16,
	EVP_CIPH_CBC_MODE,
	aes_cts_init,
	aes_cts_do_cipher,
	aes_cts_cleanup,
	sizeof(AES_KEY),
	NULL,
	NULL,
	NULL,
	NULL
    };

    return &aes_256_cts;
}
