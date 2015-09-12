/* crypto/hmac/hmac.c
 * Code copied from openssl distribution and
 * Modified just enough so that compiles and runs standalone
 *
 * Copyright (C) 2015, Broadcom Corporation. All Rights Reserved.
 * 
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY
 * SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION
 * OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN
 * CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 *
 * $Id: hmac_sha256.c 506740 2014-10-07 09:03:08Z $
 */
/* Copyright (C) 1995-1998 Eric Young (eay@cryptsoft.com)
 * All rights reserved.
 *
 * This package is an SSL implementation written
 * by Eric Young (eay@cryptsoft.com).
 * The implementation was written so as to conform with Netscapes SSL.
 *
 * This library is free for commercial and non-commercial use as long as
 * the following conditions are aheared to.  The following conditions
 * apply to all code found in this distribution, be it the RC4, RSA,
 * lhash, DES, etc., code; not just the SSL code.  The SSL documentation
 * included with this distribution is covered by the same copyright terms
 * except that the holder is Tim Hudson (tjh@cryptsoft.com).
 *
 * Copyright remains Eric Young's, and as such any Copyright notices in
 * the code are not to be removed.
 * If this package is used in a product, Eric Young should be given attribution
 * as the author of the parts of the library used.
 * This can be in the form of a textual message at program startup or
 * in documentation (online or textual) provided with the package.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *    "This product includes cryptographic software written by
 *     Eric Young (eay@cryptsoft.com)"
 *    The word 'cryptographic' can be left out if the rouines from the library
 *    being used are not cryptographic related :-).
 * 4. If you include any Windows specific code (or a derivative thereof) from
 *    the apps directory (application code) you must include an acknowledgement:
 *    "This product includes software written by Tim Hudson (tjh@cryptsoft.com)"
 *
 * THIS SOFTWARE IS PROVIDED BY ERIC YOUNG ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * The licence and distribution terms for any publically available version or
 * derivative of this code cannot be changed.  i.e. this code cannot simply be
 * copied and put under another distribution licence
 * [including the GNU Public Licence.]
 */

#include <typedefs.h>

#ifdef BCMDRIVER
#include <osl.h>
#else /* BCMDRIVER */
#include <string.h>
#endif	/* BCMDRIVER */

#include <bcmcrypto/sha256.h>
#include <bcmcrypto/hmac_sha256.h>

void
hmac_sha256(const void *key, int key_len,
            const unsigned char *text, size_t text_len, unsigned char *digest,
            unsigned int *digest_len)
{
	SHA256_CTX ctx;

	int i;
	unsigned char sha_key[SHA256_CBLOCK];
	unsigned char k_ipad[SHA256_CBLOCK];    /* inner padding -
						 * key XORd with ipad
						 */
	unsigned char k_opad[SHA256_CBLOCK];    /* outer padding -
						 * key XORd with opad
						 */
	/* set the key  */
	/* block size smaller than key size : hash down */
	if (SHA256_CBLOCK < key_len) {
		SHA256_Init(&ctx);
		SHA256_Update(&ctx, key, key_len);
		SHA256_Final(sha_key, &ctx);
		key = sha_key;
		key_len = SHA256_DIGEST_LENGTH;
	}

	/*
	 * the HMAC_SHA256 transform looks like:
	 *
	 * SHA256(K XOR opad, SHA256(K XOR ipad, text))
	 *
	 * where K is an n byte key
	 * ipad is the byte 0x36 repeated 64 times
	 * opad is the byte 0x5c repeated 64 times
	 * and text is the data being protected
	 */

	/* compute inner and outer pads from key */
	memset(k_ipad, 0, sizeof(k_ipad));
	memset(k_opad, 0, sizeof(k_opad));
	memcpy(k_ipad, key, key_len);
	memcpy(k_opad, key, key_len);

	/* XOR key with ipad and opad values */
	for (i = 0; i < 64; i++) {
		k_ipad[i] ^= 0x36;
		k_opad[i] ^= 0x5c;
	}

	/*
	 * perform inner SHA256
	 */
	SHA256_Init(&ctx);                   /* init context for 1st pass */
	SHA256_Update(&ctx, k_ipad, SHA256_CBLOCK);     /* start with inner pad */
	SHA256_Update(&ctx, text, text_len); /* then text of datagram */
	SHA256_Final(digest, &ctx);          /* finish up 1st pass */
	/*
	 * perform outer SHA256
	 */
	SHA256_Init(&ctx);                   /* init context for 2nd pass */
	SHA256_Update(&ctx, k_opad, SHA256_CBLOCK); /* start with outer pad */
	SHA256_Update(&ctx, digest, SHA256_DIGEST_LENGTH); /* then results of 1st hash */
	SHA256_Final(digest, &ctx);          /* finish up 2nd pass */

	if (digest_len)
		*digest_len = SHA256_DIGEST_LENGTH;
}

void
hmac_sha256_n(const void *key, int key_len,
              const unsigned char *text, size_t text_len, unsigned char *digest,
              unsigned int digest_len)
{
	uchar data[128];
	uchar digest_tmp[SHA256_DIGEST_LENGTH];
	int i, data_len = 2, digest_bitlen = (digest_len*8);

	memcpy(&data[data_len], text, text_len);
	data_len += (int)text_len;
	data[data_len++] = digest_bitlen & 0xFF;
	data[data_len++] = (digest_bitlen >> 8) & 0xFF;

	for (i = 0; i < ((int)digest_len + SHA256_DIGEST_LENGTH - 1) / SHA256_DIGEST_LENGTH; i++) {
		data[0] = (i + 1) & 0xFF;
		data[1] = ((i + 1) >> 8) & 0xFF;
		hmac_sha256(key, key_len, data, data_len, digest_tmp, NULL);
		memcpy(&digest[(i*SHA256_DIGEST_LENGTH)], digest_tmp, SHA256_DIGEST_LENGTH);
	}
}

void
sha256(const unsigned char *text, size_t text_len, unsigned char *digest,
       unsigned int digest_len)
{
	SHA256_CTX ctx;

	SHA256_Init(&ctx);                   /* init context for 1st pass */
	SHA256_Update(&ctx, text, text_len);     /* start with inner pad */
	SHA256_Final(digest, &ctx);          /* finish up 1st pass */
}

/* KDF
 * Length of output is in octets rather than bits
 * since length is always a multiple of 8
 * output array is organized so first N octets starting from 0
 * contains PRF output
 *
 * supported inputs are 16, 32, 48, 64
 * output array must be 80 octets in size to allow for sha1 overflow
 */
#define KDF_MAX_I_D_LEN 128
int
KDF(unsigned char *key, int key_len, unsigned char *prefix,
              int prefix_len, unsigned char *data, int data_len,
              unsigned char *output, int len)
{
	unsigned char input[KDF_MAX_I_D_LEN]; /* concatenated input */
	int total_len;
	int data_offset = 0;

	if ((prefix_len + data_len + 1) > KDF_MAX_I_D_LEN)
		return (-1);

	if (prefix_len != 0) {
		memcpy(input, prefix, prefix_len);
		data_offset = prefix_len;
	}
	memcpy(&input[data_offset], data, data_len);
	total_len = data_offset + data_len;
	hmac_sha256_n(key, key_len, input, total_len, output, len);
	return (0);
}
