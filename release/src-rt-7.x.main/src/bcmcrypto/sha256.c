/* crypto/sha/sha256.c
 * Code copied from openssl distribution and
 * Modified just enough so that compiles and runs standalone
 *
 * Copyright (C) 2014, Broadcom Corporation. All Rights Reserved.
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
 * $Id: sha256.c 241182 2011-02-17 21:50:03Z $
 */
/* ====================================================================
 * Copyright (c) 2004 The OpenSSL Project.  All rights reserved
 * according to the OpenSSL license [found in ../../LICENSE].
 * ====================================================================
 */
#ifndef BCMDRIVER
#include <stdlib.h>
#include <string.h>
#endif

#include <typedefs.h>
#ifdef BCMDRIVER
#include <osl.h>
#else
#include <string.h>
#endif	/* BCMDRIVER */

#include <bcmcrypto/sha256.h>

const char *SHA256_version = "SHA-256";

int SHA224_Init(SHA256_CTX *c)
{
	c->h[0] = 0xc1059ed8UL;	c->h[1] = 0x367cd507UL;
	c->h[2] = 0x3070dd17UL;	c->h[3] = 0xf70e5939UL;
	c->h[4] = 0xffc00b31UL;	c->h[5] = 0x68581511UL;
	c->h[6] = 0x64f98fa7UL;	c->h[7] = 0xbefa4fa4UL;
	c->Nl = 0;
	c->Nh = 0;
	c->num = 0;
	c->md_len = SHA224_DIGEST_LENGTH;
	return 1;
}

int
SHA256_Init(SHA256_CTX *c)
{
	c->h[0] = 0x6a09e667UL;	c->h[1] = 0xbb67ae85UL;
	c->h[2] = 0x3c6ef372UL;	c->h[3] = 0xa54ff53aUL;
	c->h[4] = 0x510e527fUL;	c->h[5] = 0x9b05688cUL;
	c->h[6] = 0x1f83d9abUL;	c->h[7] = 0x5be0cd19UL;
	c->Nl = 0;
	c->Nh = 0;
	c->num = 0;
	c->md_len = SHA256_DIGEST_LENGTH;
	return 1;
}

unsigned char *SHA224(const unsigned char *d, size_t n, unsigned char *md)
{
	SHA256_CTX c;
	static unsigned char m[SHA224_DIGEST_LENGTH];

	if (md == NULL)
		md = m;

	SHA224_Init(&c);
	SHA256_Update(&c, d, n);
	SHA256_Final(md, &c);

	return (md);
}

unsigned char *SHA256(const unsigned char *key, size_t n, unsigned char *md)
{
	SHA256_CTX c;
	static unsigned char m[SHA256_DIGEST_LENGTH];

	if (md == NULL)
		md = m;

	SHA256_Init(&c);
	SHA256_Update(&c, key, n);
	SHA256_Final(md, &c);

	return (md);
}

#ifndef	SHA_LONG_LOG2
#define	SHA_LONG_LOG2	2	/* default to 32 bits */
#endif

#define	DATA_ORDER_IS_BIG_ENDIAN

#define	HASH_LONG		SHA_LONG
#define	HASH_LONG_LOG2		SHA_LONG_LOG2
#define	HASH_CTX		SHA256_CTX
#define	HASH_CBLOCK		SHA_CBLOCK
#define	HASH_LBLOCK		SHA_LBLOCK

/*
 * Note that FIPS180-2 discusses "Truncation of the Hash Function Output."
 * default: case below covers for it. It's not clear however if it's
 * permitted to truncate to amount of bytes not divisible by 4. I bet not,
 * but if it is, then default: case shall be extended. For reference.
 * Idea behind separate cases for pre-defined lenghts is to let the
 * compiler decide if it's appropriate to unroll small loops.
 */
#define	HASH_MAKE_STRING(c, s)	do {	\
	unsigned long ll;		\
	unsigned int  n;		\
	switch ((c)->md_len)		\
	{   case SHA224_DIGEST_LENGTH:	\
		for (n = 0; n < SHA224_DIGEST_LENGTH/4; n++)	\
		{   ll = (c)->h[n]; HOST_l2c(ll, (s));   }	\
		break;			\
	    case SHA256_DIGEST_LENGTH:	\
		for (n = 0; n < SHA256_DIGEST_LENGTH/4; n++)	\
		{   ll = (c)->h[n]; HOST_l2c(ll, (s));   }	\
		break;			\
	    default:			\
		if ((c)->md_len > SHA256_DIGEST_LENGTH)	\
		    return 0;				\
		for (n = 0; n < (c)->md_len/4; n++)		\
		{   ll = (c)->h[n]; HOST_l2c(ll, (s));   }	\
		break;			\
	}				\
	} while (0)

#define	HASH_UPDATE		SHA256_Update
#define	HASH_TRANSFORM		SHA256_Transform
#define	HASH_FINAL		SHA256_Final
#define	HASH_BLOCK_HOST_ORDER	sha256_block_host_order
#define	HASH_BLOCK_DATA_ORDER	sha256_block_data_order

void sha256_block_host_order(SHA256_CTX *ctx, const void *in, size_t num);
void sha256_block_data_order(SHA256_CTX *ctx, const void *in, size_t num);

#include <bcmcrypto/md32_common.h>

#ifdef SHA256_ASM
void sha256_block(SHA256_CTX *ctx, const void *in, size_t num, int host);
#else
static const SHA_LONG K256[64] = {
	0x428a2f98UL, 0x71374491UL, 0xb5c0fbcfUL, 0xe9b5dba5UL,
	0x3956c25bUL, 0x59f111f1UL, 0x923f82a4UL, 0xab1c5ed5UL,
	0xd807aa98UL, 0x12835b01UL, 0x243185beUL, 0x550c7dc3UL,
	0x72be5d74UL, 0x80deb1feUL, 0x9bdc06a7UL, 0xc19bf174UL,
	0xe49b69c1UL, 0xefbe4786UL, 0x0fc19dc6UL, 0x240ca1ccUL,
	0x2de92c6fUL, 0x4a7484aaUL, 0x5cb0a9dcUL, 0x76f988daUL,
	0x983e5152UL, 0xa831c66dUL, 0xb00327c8UL, 0xbf597fc7UL,
	0xc6e00bf3UL, 0xd5a79147UL, 0x06ca6351UL, 0x14292967UL,
	0x27b70a85UL, 0x2e1b2138UL, 0x4d2c6dfcUL, 0x53380d13UL,
	0x650a7354UL, 0x766a0abbUL, 0x81c2c92eUL, 0x92722c85UL,
	0xa2bfe8a1UL, 0xa81a664bUL, 0xc24b8b70UL, 0xc76c51a3UL,
	0xd192e819UL, 0xd6990624UL, 0xf40e3585UL, 0x106aa070UL,
	0x19a4c116UL, 0x1e376c08UL, 0x2748774cUL, 0x34b0bcb5UL,
	0x391c0cb3UL, 0x4ed8aa4aUL, 0x5b9cca4fUL, 0x682e6ff3UL,
	0x748f82eeUL, 0x78a5636fUL, 0x84c87814UL, 0x8cc70208UL,
	0x90befffaUL, 0xa4506cebUL, 0xbef9a3f7UL, 0xc67178f2UL };

/*
 * FIPS specification refers to right rotations, while our ROTATE macro
 * is left one. This is why you might notice that rotation coefficients
 * differ from those observed in FIPS document by 32-N...
 */
#define Sigma0(x)	(ROTATE((x), 30) ^ ROTATE((x), 19) ^ ROTATE((x), 10))
#define Sigma1(x)	(ROTATE((x), 26) ^ ROTATE((x), 21) ^ ROTATE((x), 7))
#define sigma0(x)	(ROTATE((x), 25) ^ ROTATE((x), 14) ^ ((x)>>3))
#define sigma1(x)	(ROTATE((x), 15) ^ ROTATE((x), 13) ^ ((x)>>10))

#define Ch(x, y, z)	(((x) & (y)) ^ ((~(x)) & (z)))
#define Maj(x, y, z)	(((x) & (y)) ^ ((x) & (z)) ^ ((y) & (z)))

#ifdef OPENSSL_SMALL_FOOTPRINT

static void
sha256_block(SHA256_CTX *ctx, const void *in, size_t num, int host)
{
	unsigned MD32_REG_T a, b, c, d, e, f, g, h, s0, s1, T1, T2;
	SHA_LONG	X[16];
	int i;
	const unsigned char *data = in;

	while (num--) {

		a = ctx->h[0];	b = ctx->h[1];	c = ctx->h[2];	d = ctx->h[3];
		e = ctx->h[4];	f = ctx->h[5];	g = ctx->h[6];	h = ctx->h[7];

		if (host) {
			const SHA_LONG *W = (const SHA_LONG *)data;

			for (i = 0; i < 16; i++)
			{
				T1 = X[i] = W[i];
				T1 += h + Sigma1(e) + Ch(e, f, g) + K256[i];
				T2 = Sigma0(a) + Maj(a, b, c);
				h = g;	g = f;	f = e;	e = d + T1;
				d = c;	c = b;	b = a;	a = T1 + T2;
			}

			data += SHA256_CBLOCK;
		} else {
			SHA_LONG l;

			for (i = 0; i < 16; i++) {
				HOST_c2l(data, l); T1 = X[i] = l;
				T1 += h + Sigma1(e) + Ch(e, f, g) + K256[i];
				T2 = Sigma0(a) + Maj(a, b, c);
				h = g;	g = f;	f = e;	e = d + T1;
				d = c;	c = b;	b = a;	a = T1 + T2;
			}
		}

		for (; i < 64; i++) {
			s0 = X[(i+1)&0x0f];	s0 = sigma0(s0);
			s1 = X[(i+14)&0x0f];	s1 = sigma1(s1);

			T1 = X[i&0xf] += s0 + s1 + X[(i+9)&0xf];
			T1 += h + Sigma1(e) + Ch(e, f, g) + K256[i];
			T2 = Sigma0(a) + Maj(a, b, c);
			h = g;	g = f;	f = e;	e = d + T1;
			d = c;	c = b;	b = a;	a = T1 + T2;
		}

		ctx->h[0] += a;	ctx->h[1] += b;	ctx->h[2] += c;	ctx->h[3] += d;
		ctx->h[4] += e;	ctx->h[5] += f;	ctx->h[6] += g;	ctx->h[7] += h;

	}
}

#else

#define	ROUND_00_15(i, a, b, c, d, e, f, g, h)		do {	\
	T1 += h + Sigma1(e) + Ch(e, f, g) + K256[i];	\
	h = Sigma0(a) + Maj(a, b, c);			\
	d += T1;	h += T1;		} while (0)

#define	ROUND_16_63(i, a, b, c, d, e, f, g, h, X)	do {	\
	s0 = X[(i+1)&0x0f];	s0 = sigma0(s0);	\
	s1 = X[(i+14)&0x0f];	s1 = sigma1(s1);	\
	T1 = X[(i)&0x0f] += s0 + s1 + X[(i+9)&0x0f];	\
	ROUND_00_15(i, a, b, c, d, e, f, g, h);		} while (0)

static void
sha256_block(SHA256_CTX *ctx, const void *in, size_t num, int host)
{
	unsigned MD32_REG_T a, b, c, d, e, f, g, h, s0, s1, T1;
	SHA_LONG	X[16];
	int i;
	const unsigned char *data = in;

	while (num--) {

		a = ctx->h[0];	b = ctx->h[1];	c = ctx->h[2];	d = ctx->h[3];
		e = ctx->h[4];	f = ctx->h[5];	g = ctx->h[6];	h = ctx->h[7];

		if (host) {
			const SHA_LONG *W = (const SHA_LONG *)data;

			T1 = X[0] = W[0];	ROUND_00_15(0, a, b, c, d, e, f, g, h);
			T1 = X[1] = W[1];	ROUND_00_15(1, h, a, b, c, d, e, f, g);
			T1 = X[2] = W[2];	ROUND_00_15(2, g, h, a, b, c, d, e, f);
			T1 = X[3] = W[3];	ROUND_00_15(3, f, g, h, a, b, c, d, e);
			T1 = X[4] = W[4];	ROUND_00_15(4, e, f, g, h, a, b, c, d);
			T1 = X[5] = W[5];	ROUND_00_15(5, d, e, f, g, h, a, b, c);
			T1 = X[6] = W[6];	ROUND_00_15(6, c, d, e, f, g, h, a, b);
			T1 = X[7] = W[7];	ROUND_00_15(7, b, c, d, e, f, g, h, a);
			T1 = X[8] = W[8];	ROUND_00_15(8, a, b, c, d, e, f, g, h);
			T1 = X[9] = W[9];	ROUND_00_15(9, h, a, b, c, d, e, f, g);
			T1 = X[10] = W[10];	ROUND_00_15(10, g, h, a, b, c, d, e, f);
			T1 = X[11] = W[11];	ROUND_00_15(11, f, g, h, a, b, c, d, e);
			T1 = X[12] = W[12];	ROUND_00_15(12, e, f, g, h, a, b, c, d);
			T1 = X[13] = W[13];	ROUND_00_15(13, d, e, f, g, h, a, b, c);
			T1 = X[14] = W[14];	ROUND_00_15(14, c, d, e, f, g, h, a, b);
			T1 = X[15] = W[15];	ROUND_00_15(15, b, c, d, e, f, g, h, a);

			data += SHA256_CBLOCK;
		} else {
			SHA_LONG l;

			HOST_c2l(data, l); T1 = X[0] = l;  ROUND_00_15(0, a, b, c, d, e, f, g, h);
			HOST_c2l(data, l); T1 = X[1] = l;  ROUND_00_15(1, h, a, b, c, d, e, f, g);
			HOST_c2l(data, l); T1 = X[2] = l;  ROUND_00_15(2, g, h, a, b, c, d, e, f);
			HOST_c2l(data, l); T1 = X[3] = l;  ROUND_00_15(3, f, g, h, a, b, c, d, e);
			HOST_c2l(data, l); T1 = X[4] = l;  ROUND_00_15(4, e, f, g, h, a, b, c, d);
			HOST_c2l(data, l); T1 = X[5] = l;  ROUND_00_15(5, d, e, f, g, h, a, b, c);
			HOST_c2l(data, l); T1 = X[6] = l;  ROUND_00_15(6, c, d, e, f, g, h, a, b);
			HOST_c2l(data, l); T1 = X[7] = l;  ROUND_00_15(7, b, c, d, e, f, g, h, a);
			HOST_c2l(data, l); T1 = X[8] = l;  ROUND_00_15(8, a, b, c, d, e, f, g, h);
			HOST_c2l(data, l); T1 = X[9] = l;  ROUND_00_15(9, h, a, b, c, d, e, f, g);
			HOST_c2l(data, l); T1 = X[10] = l; ROUND_00_15(10, g, h, a, b, c, d, e, f);
			HOST_c2l(data, l); T1 = X[11] = l; ROUND_00_15(11, f, g, h, a, b, c, d, e);
			HOST_c2l(data, l); T1 = X[12] = l; ROUND_00_15(12, e, f, g, h, a, b, c, d);
			HOST_c2l(data, l); T1 = X[13] = l; ROUND_00_15(13, d, e, f, g, h, a, b, c);
			HOST_c2l(data, l); T1 = X[14] = l; ROUND_00_15(14, c, d, e, f, g, h, a, b);
			HOST_c2l(data, l); T1 = X[15] = l; ROUND_00_15(15, b, c, d, e, f, g, h, a);
		}

		for (i = 16; i < 64; i += 8) {
			ROUND_16_63(i+0, a, b, c, d, e, f, g, h, X);
			ROUND_16_63(i+1, h, a, b, c, d, e, f, g, X);
			ROUND_16_63(i+2, g, h, a, b, c, d, e, f, X);
			ROUND_16_63(i+3, f, g, h, a, b, c, d, e, X);
			ROUND_16_63(i+4, e, f, g, h, a, b, c, d, X);
			ROUND_16_63(i+5, d, e, f, g, h, a, b, c, X);
			ROUND_16_63(i+6, c, d, e, f, g, h, a, b, X);
			ROUND_16_63(i+7, b, c, d, e, f, g, h, a, X);
		}

		ctx->h[0] += a;	ctx->h[1] += b;	ctx->h[2] += c;	ctx->h[3] += d;
		ctx->h[4] += e;	ctx->h[5] += f;	ctx->h[6] += g;	ctx->h[7] += h;
	}
}

#endif /* OPENSSL_SMALL_FOOTPRINT */
#endif /* SHA256_ASM */

/*
 * Idea is to trade couple of cycles for some space. On IA-32 we save
 * about 4K in "big footprint" case. In "small footprint" case any gain
 * is appreciated:-)
 */
void
HASH_BLOCK_HOST_ORDER(SHA256_CTX *ctx, const void *in, size_t num)
{
	sha256_block(ctx, in, num, 1);
}

void
HASH_BLOCK_DATA_ORDER(SHA256_CTX *ctx, const void *in, size_t num)
{
	sha256_block(ctx, in, num, 0);
}


#ifdef BCMSHA256_TEST
/*
 *  sha1test.c
 *
 *  Description:
 *      This file will exercise the SHA-256 code performing the three
 *      tests documented in FIPS PUB 180-2 plus one which calls
 *      SHA1Input with an exact multiple of 512 bits, plus a few
 *      error test checks.
 *
 *  Portability Issues:
 *      None.
 *
 */

#include <stdio.h>

/*
 *  Define patterns for testing
 */
#define TEST1   "abc"
#define TEST2a  "abcdbcdecdefdefgefghfghighijhi"
#define TEST2b  "jkijkljklmklmnlmnomnopnopq"
#define TEST2   TEST2a TEST2b
#define TEST3   "a"

char *testarray[3] = {
	TEST1,
	TEST2,
	TEST3
};


int repeatcount[3] = { 1, 1, 1000000};
unsigned char resultarray[3][32] =
{
	{0xba, 0x78, 0x16, 0xbf,  0x8f, 0x01, 0xcf, 0xea,
	 0x41, 0x41, 0x40, 0xde,  0x5d, 0xae, 0x22, 0x23,
	 0xb0, 0x03, 0x61, 0xa3,  0x96, 0x17, 0x7a, 0x9c,
	 0xb4, 0x10, 0xff, 0x61,  0xf2, 0x00, 0x15, 0xad},

	{0x24, 0x8d, 0x6a, 0x61,  0xd2, 0x06, 0x38, 0xb8,
	 0xe5, 0xc0, 0x26, 0x93,  0x0c, 0x3e, 0x60, 0x39,
	 0xa3, 0x3c, 0xe4, 0x59,  0x64, 0xff, 0x21, 0x67,
	 0xf6, 0xec, 0xed, 0xd4,  0x19, 0xdb, 0x06, 0xc1},

	{0xcd, 0xc7, 0x6e, 0x5c,  0x99, 0x14, 0xfb, 0x92,
	 0x81, 0xa1, 0xc7, 0xe2,  0x84, 0xd7, 0x3e, 0x67,
	 0xf1, 0x80, 0x9a, 0x48,  0xa4, 0x97, 0x20, 0x0e,
	 0x04, 0x6d, 0x39, 0xcc,  0xc7, 0x11, 0x2c, 0xd0}
};

int
main()
{
	SHA256_CTX sha;
	int i, j, err, fail = 0;
	unsigned char Message_Digest[32];

	/*
	 *  Perform SHA-1 tests
	 */
	for (j = 0; j < 3; ++j) {
		printf("\nTest %d: %d, '%s'\n", j + 1, repeatcount[j], testarray[j]);

		err = SHA256_Init(&sha);

		for (i = 0; i < repeatcount[j]; ++i) {
			err = SHA256_Update(&sha,
			                    (const unsigned char *) testarray[j],
			                    strlen(testarray[j]));
		}

		err = SHA256_Final(Message_Digest, &sha);
		printf("\t");
		for (i = 0; i < 32; ++i) {
			printf("%02X ", Message_Digest[i]);
		}
		printf("\n");
		printf("Should match:\n");
		printf("\t");
		for (i = 0; i < 32; ++i) {
			printf("%02X ", resultarray[j][i]);
		}
		printf("\n");
		if (memcmp(Message_Digest, resultarray[j], 32)) fail++;
	}
#ifdef EXTRA_SHA256_TEST
	/* Test some error returns */
	err = SHA1Input(&sha, (const unsigned char *) testarray[1], 1);
	printf("\nError %d. Should be %d.\n", err, shaStateError);
	if (err != shaStateError) fail++;

	err = SHA1Reset(0);
	printf("\nError %d. Should be %d.\n", err, shaNull);
	if (err != shaNull) fail++;
#endif
	printf("SHA1 test %s\n", fail? "FAILED" : "PASSED");
	return fail;
}
#endif /* BCMSHA1_TEST */
