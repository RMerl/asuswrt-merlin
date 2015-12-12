/*
 * dh.c: Diffie Hellman implementation.
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
 * $Id: dh.c 241182 2011-02-17 21:50:03Z $
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
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* #include <shutils.h> */

#include <bcmcrypto/dh.h>

#define OPENSSL_malloc malloc
#define OPENSSL_free free


static int dh_bn_mod_exp(const DH *dh, BIGNUM *r,
                         const BIGNUM *a, const BIGNUM *p,
                         const BIGNUM *m, BN_CTX *ctx,
                         BN_MONT_CTX *m_ctx);

#ifdef BCMDH_TEST

#include <getopt.h>

static int bn_print(const BIGNUM *a);
static void cb(int p, int n, void *arg);

unsigned char data192[] = {
	0xD4, 0xA0, 0xBA, 0x02, 0x50, 0xB6, 0xFD, 0x2E,
	0xC6, 0x26, 0xE7, 0xEF, 0xD6, 0x37, 0xDF, 0x76,
	0xC7, 0x16, 0xE2, 0x2D, 0x09, 0x44, 0xB8, 0x8B
};

unsigned char data512[] = {
	0xDA, 0x58, 0x3C, 0x16, 0xD9, 0x85, 0x22, 0x89,
	0xD0, 0xE4, 0xAF, 0x75, 0x6F, 0x4C, 0xCA, 0x92,
	0xDD, 0x4B, 0xE5, 0x33, 0xB8, 0x04, 0xFB, 0x0F,
	0xED, 0x94, 0xEF, 0x9C, 0x8A, 0x44, 0x03, 0xED,
	0x57, 0x46, 0x50, 0xD3, 0x69, 0x99, 0xDB, 0x29,
	0xD7, 0x76, 0x27, 0x6B, 0xA2, 0xD3, 0xD4, 0x12,
	0xE2, 0x18, 0xF4, 0xDD, 0x1E, 0x08, 0x4C, 0xF6,
	0xD8, 0x00, 0x3E, 0x7C, 0x47, 0x74, 0xE8, 0x33
};

unsigned char data1024[] = {
	0x97, 0xF6, 0x42, 0x61, 0xCA, 0xB5, 0x05, 0xDD,
	0x28, 0x28, 0xE1, 0x3F, 0x1D, 0x68, 0xB6, 0xD3,
	0xDB, 0xD0, 0xF3, 0x13, 0x04, 0x7F, 0x40, 0xE8,
	0x56, 0xDA, 0x58, 0xCB, 0x13, 0xB8, 0xA1, 0xBF,
	0x2B, 0x78, 0x3A, 0x4C, 0x6D, 0x59, 0xD5, 0xF9,
	0x2A, 0xFC, 0x6C, 0xFF, 0x3D, 0x69, 0x3F, 0x78,
	0xB2, 0x3D, 0x4F, 0x31, 0x60, 0xA9, 0x50, 0x2E,
	0x3E, 0xFA, 0xF7, 0xAB, 0x5E, 0x1A, 0xD5, 0xA6,
	0x5E, 0x55, 0x43, 0x13, 0x82, 0x8D, 0xA8, 0x3B,
	0x9F, 0xF2, 0xD9, 0x41, 0xDE, 0xE9, 0x56, 0x89,
	0xFA, 0xDA, 0xEA, 0x09, 0x36, 0xAD, 0xDF, 0x19,
	0x71, 0xFE, 0x63, 0x5B, 0x20, 0xAF, 0x47, 0x03,
	0x64, 0x60, 0x3C, 0x2D, 0xE0, 0x59, 0xF5, 0x4B,
	0x65, 0x0A, 0xD8, 0xFA, 0x0C, 0xF7, 0x01, 0x21,
	0xC7, 0x47, 0x99, 0xD7, 0x58, 0x71, 0x32, 0xBE,
	0x9B, 0x99, 0x9B, 0xB9, 0xB7, 0x87, 0xE8, 0xAB
};

/* uncomment this and write a matching function when testing on another OS */
#ifndef WIN32
#define BCMDH_TEST_LINUX
#endif

#ifdef BCMDH_TEST_LINUX

#define dbg(fmt, arg...)       printf(fmt, ##arg)

#endif	/* BCMDH_TEST_LINUX */


int
main(int argc, char *argv[])
{
	DH *a;
	DH *b = NULL;
	unsigned char *abuf = NULL, *bbuf = NULL;
	unsigned char *apubbuf = NULL, *bpubbuf = NULL;
	int i, alen, blen, apublen, bpublen, aout, bout, ret = 1, size = 0;
	char opt;

	while ((opt = getopt(argc, argv, "s:")) != EOF) {
		switch (opt) {
		case 's':
			size = (int)strtoul(optarg, NULL, 0);
			break;
		default:
			dbg("invalid option");
			return ret;
		}
	}

#ifdef BCMDH_TEST_LINUX
	{
		extern void linux_random(uint8 *rand, int len);
		BN_register_RAND(linux_random);
	}
#endif

	if (size == 192) {
		a = DH_init(data192, sizeof(data192), 3);
	} else if (size == 512) {
		a = DH_init(data512, sizeof(data512), 2);
	} else if (size == 1024) {
		a = DH_init(data1024, sizeof(data1024), 2);
	} else {
		a = DH_generate_parameters(NULL, 64, DH_GENERATOR_5, cb, NULL);
		if (a == NULL) goto err;
	}

	if (!DH_check(a, &i)) goto err;
	if (i & DH_CHECK_P_NOT_PRIME)
		dbg("p value is not prime\n");
	if (i & DH_CHECK_P_NOT_SAFE_PRIME)
		dbg("p value is not a safe prime\n");
	if (i & DH_UNABLE_TO_CHECK_GENERATOR)
		dbg("unable to check the generator value\n");
	if (i & DH_NOT_SUITABLE_GENERATOR)
		dbg("the g value is not a generator\n");

	dbg("\np    =");
	bn_print(a->p);
	dbg("\ng    =");
	bn_print(a->g);
	dbg("\n");

	b = DH_new();
	if (b == NULL) goto err;

	b->p = BN_dup(a->p);
	b->g = BN_dup(a->g);
	if ((b->p == NULL) || (b->g == NULL)) goto err;

	apubbuf = (unsigned char *)OPENSSL_malloc(DH_size(a));
	/* dbg("%s\n", file2str("/proc/uptime")); */
	if (!(apublen = DH_generate_key(apubbuf, a))) goto err;
	/* dbg("%s\n", file2str("/proc/uptime")); */
	dbg("pri 1=");
	bn_print(a->priv_key);
	dbg("\npub 1=");
	for (i = 0; i < apublen; i++) dbg("%02X", apubbuf[i]);
	dbg("\n");

	bpubbuf = (unsigned char *)OPENSSL_malloc(DH_size(b));
	if (!(bpublen = DH_generate_key(bpubbuf, b))) goto err;
	dbg("pri 2=");
	bn_print(b->priv_key);
	dbg("\npub 2=");
	for (i = 0; i < bpublen; i++) dbg("%02X", bpubbuf[i]);
	dbg("\n");

	alen = DH_size(a);
	abuf = (unsigned char *)OPENSSL_malloc(alen);
	/* dbg("%s\n", file2str("/proc/uptime")); */
	aout = DH_compute_key(abuf, bpubbuf, bpublen, a);
	/* dbg("%s\n", file2str("/proc/uptime")); */

	dbg("key1 =");
	for (i = 0; i < aout; i++) dbg("%02X", abuf[i]);
	dbg("\n");

	blen = DH_size(b);
	bbuf = (unsigned char *)OPENSSL_malloc(blen);
	bout = DH_compute_key(bbuf, apubbuf, apublen, b);

	dbg("key2 =");
	for (i = 0; i < bout; i++) dbg("%02X", abuf[i]);
	dbg("\n");

	if ((aout < 4) || (bout != aout) || (memcmp(abuf, bbuf, aout) != 0))
		ret = 1;
	else
		ret = 0;
err:
	if (abuf != NULL) OPENSSL_free(abuf);
	if (bbuf != NULL) OPENSSL_free(bbuf);
	if (b != NULL) DH_free(b);
	if (a != NULL) DH_free(a);
	fprintf(stderr, "%s: %s\n", *argv, ret?"FAILED":"PASSED");
	return (ret);
}

static void
cb(int p, int n, void *arg)
{
	if (p == 0) dbg(".");
	if (p == 1) dbg("+");
	if (p == 2) dbg("*");
	if (p == 3) dbg("\n");
}

static const char *Hex = "0123456789ABCDEF";

static int
bn_print(const BIGNUM *a)
{
	int i, j, v, z = 0;
	int ret = 0;

	if ((a->neg) && (dbg("-") != 1)) goto end;
	if ((a->top == 0) && (dbg("0") != 1)) goto end;
	for (i = a->top - 1; i >= 0; i--) {
		for (j = BN_BITS2 - 4; j >= 0; j -= 4) {
			/* strip leading zeros */
			v = ((int)(a->d[i] >> (long)j)) & 0x0f;
			if (z || (v != 0)) {
				dbg("%c", Hex[v]);
				z = 1;
			}
		}
	}
	ret = 1;
end:
	return (ret);
}

int DH_size(const DH *dh)
{
	return (BN_num_bytes(dh->p));
}

/* We generate DH parameters as follows
 * find a prime q which is prime_len/2 bits long.
 * p=(2*q)+1 or (p-1)/2 = q
 * For this case, g is a generator if
 * g^((p-1)/q) mod p != 1 for values of q which are the factors of p-1.
 * Since the factors of p-1 are q and 2, we just need to check
 * g^2 mod p != 1 and g^q mod p != 1.
 *
 * Having said all that,
 * there is another special case method for the generators 2, 3 and 5.
 * for 2, p mod 24 == 11
 * for 3, p mod 12 == 5  <<<<< does not work for safe primes.
 * for 5, p mod 10 == 3 or 7
 *
 * Thanks to Phil Karn <karn@qualcomm.com> for the pointers about the
 * special generators and for answering some of my questions.
 *
 * I've implemented the second simple method :-).
 * Since DH should be using a safe prime (both p and q are prime),
 * this generator function can take a very very long time to run.
 */
/* Actually there is no reason to insist that 'generator' be a generator.
 * It's just as OK (and in some sense better) to use a generator of the
 * order-q subgroup.
 */
DH *
DH_generate_parameters(DH* dh, int prime_len, int generator,
                       void (*callback)(int, int, void *), void *cb_arg)
{
	BIGNUM *p = NULL, *t1, *t2;
	DH *ret = NULL;
	int g, ok = -1;
	BN_CTX *ctx = NULL;

	ret = dh ? dh : DH_new();
	if (ret == NULL) goto err;
	ctx = BN_CTX_new();
	if (ctx == NULL) goto err;
	BN_CTX_start(ctx);
	t1 = BN_CTX_get(ctx);
	t2 = BN_CTX_get(ctx);
	if (t1 == NULL || t2 == NULL) goto err;

	if (generator <= 1) {
		goto err;
	}
	if (generator == DH_GENERATOR_2) {
		if (!BN_set_word(t1, 24)) goto err;
		if (!BN_set_word(t2, 11)) goto err;
		g = 2;
	}

	else if (generator == DH_GENERATOR_5) {
		if (!BN_set_word(t1, 10)) goto err;
		if (!BN_set_word(t2, 3)) goto err;
		/* BN_set_word(t3, 7); just have to miss
		 * out on these ones :-(
		 */
		g = 5;
	} else {
		/* in the general case, don't worry if 'generator' is a
		 * generator or not: since we are using safe primes,
		 * it will generate either an order-q or an order-2q group,
		 * which both is OK
		 */
		if (!BN_set_word(t1, 2)) goto err;
		if (!BN_set_word(t2, 1)) goto err;
		g = generator;
	}

	p = BN_generate_prime(NULL, prime_len, 1, t1, t2, callback, cb_arg);
	if (p == NULL) goto err;
	if (callback != NULL) callback(3, 0, cb_arg);
	ret->p = p;
	ret->g = BN_new();
	if (!BN_set_word(ret->g, g)) goto err;
	ok = 1;
err:
	if (ok == -1) {
		ok = 0;
	}

	if (ctx != NULL) {
		BN_CTX_end(ctx);
		BN_CTX_free(ctx);
	}
	if (!ok && (ret != NULL)) {
		DH_free(ret);
		ret = NULL;
	}
	return (ret);
}

/* Check that p is a safe prime and
 * if g is 2, 3 or 5, check that is is a suitable generator
 * where
 * for 2, p mod 24 == 11
 * for 3, p mod 12 == 5
 * for 5, p mod 10 == 3 or 7
 * should hold.
 */

int
DH_check(const DH *dh, int *ret)
{
	int ok = 0;
	BN_CTX *ctx = NULL;
	BN_ULONG l;
	BIGNUM *q = NULL;

	*ret = 0;
	ctx = BN_CTX_new();
	if (ctx == NULL) goto err;
	q = BN_new();
	if (q == NULL) goto err;

	if (BN_is_word(dh->g, DH_GENERATOR_2)) {
		l = BN_mod_word(dh->p, 24);
		if (l != 11) *ret |= DH_NOT_SUITABLE_GENERATOR;
	}

	else if (BN_is_word(dh->g, DH_GENERATOR_5)) {
		l = BN_mod_word(dh->p, 10);
		if ((l != 3) && (l != 7))
			*ret |= DH_NOT_SUITABLE_GENERATOR;
	} else
		*ret |= DH_UNABLE_TO_CHECK_GENERATOR;

	if (!BN_is_prime(dh->p, BN_prime_checks, NULL, ctx, NULL))
		*ret |= DH_CHECK_P_NOT_PRIME;
	else {
		if (!BN_rshift1(q, dh->p)) goto err;
		if (!BN_is_prime(q, BN_prime_checks, NULL, ctx, NULL))
			*ret |= DH_CHECK_P_NOT_SAFE_PRIME;
	}
	ok = 1;
err:
	if (ctx != NULL) BN_CTX_free(ctx);
	if (q != NULL) BN_free(q);
	return (ok);
}

#endif /* BCMDH_TEST */

DH *
DH_new(void)
{
	DH *ret;

	ret = (DH *)OPENSSL_malloc(sizeof(DH));
	if (ret == NULL) {
		return (NULL);
	}

	ret->p = NULL;
	ret->g = NULL;
	ret->length = 0;
	ret->pub_key = NULL;
	ret->priv_key = NULL;
	ret->q = NULL;
	ret->j = NULL;
	ret->seed = NULL;
	ret->seedlen = 0;
	ret->counter = NULL;
	ret->method_mont_p = NULL;
	ret->flags = DH_FLAG_CACHE_MONT_P;
	return (ret);
}

void
DH_free(DH *r)
{
	if (r == NULL) return;
	if (r->p != NULL) BN_clear_free(r->p);
	if (r->g != NULL) BN_clear_free(r->g);
	if (r->q != NULL) BN_clear_free(r->q);
	if (r->j != NULL) BN_clear_free(r->j);
	if (r->seed) OPENSSL_free(r->seed);
	if (r->counter != NULL) BN_clear_free(r->counter);
	if (r->pub_key != NULL) BN_clear_free(r->pub_key);
	if (r->priv_key != NULL) BN_clear_free(r->priv_key);
	if (r->method_mont_p)
		BN_MONT_CTX_free((BN_MONT_CTX *)r->method_mont_p);
	OPENSSL_free(r);
}

int
DH_generate_key(unsigned char *pubbuf, DH *dh)
{
	int ret = 0;
	unsigned l;
	BN_CTX *ctx = NULL;
	BN_MONT_CTX *mont;
	BIGNUM *pub_key = NULL, *priv_key = NULL;


	if (dh->pub_key != NULL) {
		if (pubbuf)
			return BN_bn2bin(dh->pub_key, pubbuf);
		else
			return BN_num_bytes(dh->pub_key);
	}
	/* first time in here */
	priv_key = BN_new();
	if (priv_key == NULL) goto err;

	pub_key = BN_new();
	if (pub_key == NULL) goto err;

	ctx = BN_CTX_new();
	if (ctx == NULL) goto err;

	if (dh->flags & DH_FLAG_CACHE_MONT_P) {
		if ((dh->method_mont_p = BN_MONT_CTX_new()) != NULL)
			if (!BN_MONT_CTX_set((BN_MONT_CTX *)dh->method_mont_p,
			                     dh->p, ctx))
				goto err;
	}

	mont = (BN_MONT_CTX *)dh->method_mont_p;

	l = dh->length ? dh->length : BN_num_bits(dh->p)-1; /* secret exponent length */
	if (!BN_rand(priv_key, l, 0, 0))
		goto err;
	if (!dh_bn_mod_exp(dh, pub_key, dh->g, priv_key, dh->p, ctx, mont))
		goto err;

	dh->pub_key = pub_key;
	dh->priv_key = priv_key;
	if (pubbuf)
		ret = BN_bn2bin(pub_key, pubbuf);
	else
		ret = BN_num_bytes(dh->pub_key);
err:
	if ((pub_key != NULL) && (dh->pub_key == NULL))
		BN_free(pub_key);
	if ((priv_key != NULL) && (dh->priv_key == NULL))
		BN_free(priv_key);
	if (ctx)
		BN_CTX_free(ctx);
	return (ret);
}


int
DH_compute_key_bn(unsigned char *key, BIGNUM *peer_key, DH *dh)
{
	BN_CTX *ctx;
	BN_MONT_CTX *mont;
	BIGNUM *tmp;
	int ret = -1;

	ctx = BN_CTX_new();
	if (ctx == NULL) goto err;
	BN_CTX_start(ctx);
	tmp = BN_CTX_get(ctx);

	if (dh->priv_key == NULL) {
		goto err;
	}
	if ((dh->method_mont_p == NULL) && (dh->flags & DH_FLAG_CACHE_MONT_P)) {
		if ((dh->method_mont_p = BN_MONT_CTX_new()) != NULL)
			if (!BN_MONT_CTX_set((BN_MONT_CTX *)dh->method_mont_p,
			                     dh->p, ctx))
				goto err;
	}

	mont = (BN_MONT_CTX *)dh->method_mont_p;
	if (!dh_bn_mod_exp(dh, tmp, peer_key, dh->priv_key, dh->p, ctx, mont)) {
		goto err;
	}

	ret = BN_bn2bin(tmp, key);
err:
	if (peer_key)
		BN_clear_free(peer_key);
	BN_CTX_end(ctx);
	BN_CTX_free(ctx);
	return (ret);
}


int
DH_compute_key(unsigned char *key, unsigned char *pubbuf, int buflen, DH *dh)
{
	BIGNUM *peer_key = NULL;

	peer_key = BN_bin2bn(pubbuf, buflen, NULL);
	return DH_compute_key_bn(key, peer_key, dh);
}

DH *
DH_init(unsigned char *pbuf, int plen, int g)
{
	DH *dh;

	dh = DH_new();
	dh->p = BN_bin2bn(pbuf, plen, NULL);
	dh->g = BN_new();
	BN_set_word(dh->g, g);
	return dh;
}

static int
dh_bn_mod_exp(const DH *dh, BIGNUM *r,
              const BIGNUM *a, const BIGNUM *p,
              const BIGNUM *m, BN_CTX *ctx,
              BN_MONT_CTX *m_ctx)
{
	if (a->top == 1) {
		BN_ULONG A = a->d[0];
		return BN_mod_exp_mont_word(r, A, p, m, ctx, m_ctx);
	} else
		return BN_mod_exp_mont(r, a, p, m, ctx, m_ctx);
}
