/*
 * dh.h: Diffie-Hellman header file.
 *
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
 * $Id: dh.h 241182 2011-02-17 21:50:03Z $
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

#ifndef HEADER_DH_H
#define HEADER_DH_H

#include <bcmcrypto/bn.h>

#define DH_FLAG_CACHE_MONT_P	0x01

typedef struct dh_st
	{
	BIGNUM *p;
	BIGNUM *g;
	long length; /* optional */
	BIGNUM *pub_key;	/* g^x */
	BIGNUM *priv_key;	/* x */

	int flags;
	BN_MONT_CTX *method_mont_p;
	/* Place holders if we want to do X9.42 DH */
	BIGNUM *q;
	BIGNUM *j;
	unsigned char *seed;
	int seedlen;
	BIGNUM *counter;
	} DH;

#define DH_GENERATOR_2		2
/* #define DH_GENERATOR_3	3 */
#define DH_GENERATOR_5		5

/* DH_check error codes */
#define DH_CHECK_P_NOT_PRIME		0x01
#define DH_CHECK_P_NOT_SAFE_PRIME	0x02
#define DH_UNABLE_TO_CHECK_GENERATOR	0x04
#define DH_NOT_SUITABLE_GENERATOR	0x08

/* primes p where (p-1)/2 is prime too are called "safe"; we define
 * this for backward compatibility:
 */
#define DH_CHECK_P_NOT_STRONG_PRIME	DH_CHECK_P_NOT_SAFE_PRIME


DH *	DH_new(void);
void	DH_free(DH *dh);
int	DH_generate_key(unsigned char *key, DH *dh);
int	DH_compute_key(unsigned char *key, unsigned char *pubbuf, int buflen, DH *dh);
int     DH_compute_key_bn(unsigned char *key, BIGNUM *peer_key, DH *dh);
DH 	*DH_init(unsigned char *pbuf, int plen, int g);

/* for standalone test purposes */
int	DH_size(const DH *dh);
DH *	DH_generate_parameters(DH *dh, int prime_len, int generator,
                               void (*callback)(int, int, void *), void *cb_arg);
int	DH_check(const DH *dh, int *codes);

#endif	/* HEADER_DH_H */
