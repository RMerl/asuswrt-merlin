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

#include <stdio.h>
#include <stdlib.h>
#include <krb5-types.h>
#include <assert.h>

#include <rsa.h>

#include <roken.h>

#include "imath/imath.h"
#include "imath/iprime.h"

static void
BN2mpz(mpz_t *s, const BIGNUM *bn)
{
    size_t len;
    void *p;

    mp_int_init(s);

    len = BN_num_bytes(bn);
    p = malloc(len);
    BN_bn2bin(bn, p);
    mp_int_read_unsigned(s, p, len);
    free(p);
}

static BIGNUM *
mpz2BN(mpz_t *s)
{
    size_t size;
    BIGNUM *bn;
    void *p;

    size = mp_int_unsigned_len(s);
    p = malloc(size);
    if (p == NULL && size != 0)
	return NULL;
    mp_int_to_unsigned(s, p, size);

    bn = BN_bin2bn(p, size, NULL);
    free(p);
    return bn;
}

static int random_num(mp_int, size_t);

static void
setup_blind(mp_int n, mp_int b, mp_int bi)
{
    mp_int_init(b);
    mp_int_init(bi);
    random_num(b, mp_int_count_bits(n));
    mp_int_mod(b, n, b);
    mp_int_invmod(b, n, bi);
}

static void
blind(mp_int in, mp_int b, mp_int e, mp_int n)
{
    mpz_t t1;
    mp_int_init(&t1);
    /* in' = (in * b^e) mod n */
    mp_int_exptmod(b, e, n, &t1);
    mp_int_mul(&t1, in, in);
    mp_int_mod(in, n, in);
    mp_int_clear(&t1);
}

static void
unblind(mp_int out, mp_int bi, mp_int n)
{
    /* out' = (out * 1/b) mod n */
    mp_int_mul(out, bi, out);
    mp_int_mod(out, n, out);
}

static mp_result
rsa_private_calculate(mp_int in, mp_int p,  mp_int q,
		      mp_int dmp1, mp_int dmq1, mp_int iqmp,
		      mp_int out)
{
    mpz_t vp, vq, u;
    mp_int_init(&vp); mp_int_init(&vq); mp_int_init(&u);

    /* vq = c ^ (d mod (q - 1)) mod q */
    /* vp = c ^ (d mod (p - 1)) mod p */
    mp_int_mod(in, p, &u);
    mp_int_exptmod(&u, dmp1, p, &vp);
    mp_int_mod(in, q, &u);
    mp_int_exptmod(&u, dmq1, q, &vq);

    /* C2 = 1/q mod p  (iqmp) */
    /* u = (vp - vq)C2 mod p. */
    mp_int_sub(&vp, &vq, &u);
    if (mp_int_compare_zero(&u) < 0)
	mp_int_add(&u, p, &u);
    mp_int_mul(&u, iqmp, &u);
    mp_int_mod(&u, p, &u);

    /* c ^ d mod n = vq + u q */
    mp_int_mul(&u, q, &u);
    mp_int_add(&u, &vq, out);

    mp_int_clear(&vp);
    mp_int_clear(&vq);
    mp_int_clear(&u);

    return MP_OK;
}

/*
 *
 */

static int
imath_rsa_public_encrypt(int flen, const unsigned char* from,
			unsigned char* to, RSA* rsa, int padding)
{
    unsigned char *p, *p0;
    mp_result res;
    size_t size, padlen;
    mpz_t enc, dec, n, e;

    if (padding != RSA_PKCS1_PADDING)
	return -1;

    size = RSA_size(rsa);

    if (size < RSA_PKCS1_PADDING_SIZE || size - RSA_PKCS1_PADDING_SIZE < flen)
	return -2;

    BN2mpz(&n, rsa->n);
    BN2mpz(&e, rsa->e);

    p = p0 = malloc(size - 1);
    if (p0 == NULL) {
	mp_int_clear(&e);
	mp_int_clear(&n);
	return -3;
    }

    padlen = size - flen - 3;

    *p++ = 2;
    if (RAND_bytes(p, padlen) != 1) {
	mp_int_clear(&e);
	mp_int_clear(&n);
	free(p0);
	return -4;
    }
    while(padlen) {
	if (*p == 0)
	    *p = 1;
	padlen--;
	p++;
    }
    *p++ = 0;
    memcpy(p, from, flen);
    p += flen;
    assert((p - p0) == size - 1);

    mp_int_init(&enc);
    mp_int_init(&dec);
    mp_int_read_unsigned(&dec, p0, size - 1);
    free(p0);

    res = mp_int_exptmod(&dec, &e, &n, &enc);

    mp_int_clear(&dec);
    mp_int_clear(&e);
    mp_int_clear(&n);
    {
	size_t ssize;
	ssize = mp_int_unsigned_len(&enc);
	assert(size >= ssize);
	mp_int_to_unsigned(&enc, to, ssize);
	size = ssize;
    }
    mp_int_clear(&enc);

    return size;
}

static int
imath_rsa_public_decrypt(int flen, const unsigned char* from,
			 unsigned char* to, RSA* rsa, int padding)
{
    unsigned char *p;
    mp_result res;
    size_t size;
    mpz_t s, us, n, e;

    if (padding != RSA_PKCS1_PADDING)
	return -1;

    if (flen > RSA_size(rsa))
	return -2;

    BN2mpz(&n, rsa->n);
    BN2mpz(&e, rsa->e);

#if 0
    /* Check that the exponent is larger then 3 */
    if (mp_int_compare_value(&e, 3) <= 0) {
	mp_int_clear(&n);
	mp_int_clear(&e);
	return -3;
    }
#endif

    mp_int_init(&s);
    mp_int_init(&us);
    mp_int_read_unsigned(&s, rk_UNCONST(from), flen);

    if (mp_int_compare(&s, &n) >= 0) {
	mp_int_clear(&n);
	mp_int_clear(&e);
	return -4;
    }

    res = mp_int_exptmod(&s, &e, &n, &us);

    mp_int_clear(&s);
    mp_int_clear(&n);
    mp_int_clear(&e);

    if (res != MP_OK)
	return -5;
    p = to;


    size = mp_int_unsigned_len(&us);
    assert(size <= RSA_size(rsa));
    mp_int_to_unsigned(&us, p, size);

    mp_int_clear(&us);

    /* head zero was skipped by mp_int_to_unsigned */
    if (*p == 0)
	return -6;
    if (*p != 1)
	return -7;
    size--; p++;
    while (size && *p == 0xff) {
	size--; p++;
    }
    if (size == 0 || *p != 0)
	return -8;
    size--; p++;

    memmove(to, p, size);

    return size;
}

static int
imath_rsa_private_encrypt(int flen, const unsigned char* from,
			  unsigned char* to, RSA* rsa, int padding)
{
    unsigned char *p, *p0;
    mp_result res;
    size_t size;
    mpz_t in, out, n, e, b, bi;
    int blinding = (rsa->flags & RSA_FLAG_NO_BLINDING) == 0;

    if (padding != RSA_PKCS1_PADDING)
	return -1;

    size = RSA_size(rsa);

    if (size < RSA_PKCS1_PADDING_SIZE || size - RSA_PKCS1_PADDING_SIZE < flen)
	return -2;

    p0 = p = malloc(size);
    *p++ = 0;
    *p++ = 1;
    memset(p, 0xff, size - flen - 3);
    p += size - flen - 3;
    *p++ = 0;
    memcpy(p, from, flen);
    p += flen;
    assert((p - p0) == size);

    BN2mpz(&n, rsa->n);
    BN2mpz(&e, rsa->e);

    mp_int_init(&in);
    mp_int_init(&out);
    mp_int_read_unsigned(&in, p0, size);
    free(p0);

    if(mp_int_compare_zero(&in) < 0 ||
       mp_int_compare(&in, &n) >= 0) {
	size = 0;
	goto out;
    }

    if (blinding) {
	setup_blind(&n, &b, &bi);
	blind(&in, &b, &e, &n);
    }

    if (rsa->p && rsa->q && rsa->dmp1 && rsa->dmq1 && rsa->iqmp) {
	mpz_t p, q, dmp1, dmq1, iqmp;

	BN2mpz(&p, rsa->p);
	BN2mpz(&q, rsa->q);
	BN2mpz(&dmp1, rsa->dmp1);
	BN2mpz(&dmq1, rsa->dmq1);
	BN2mpz(&iqmp, rsa->iqmp);

	res = rsa_private_calculate(&in, &p, &q, &dmp1, &dmq1, &iqmp, &out);

	mp_int_clear(&p);
	mp_int_clear(&q);
	mp_int_clear(&dmp1);
	mp_int_clear(&dmq1);
	mp_int_clear(&iqmp);
    } else {
	mpz_t d;

	BN2mpz(&d, rsa->d);
	res = mp_int_exptmod(&in, &d, &n, &out);
	mp_int_clear(&d);
	if (res != MP_OK) {
	    size = 0;
	    goto out;
	}
    }

    if (blinding) {
	unblind(&out, &bi, &n);
	mp_int_clear(&b);
	mp_int_clear(&bi);
    }

    {
	size_t ssize;
	ssize = mp_int_unsigned_len(&out);
	assert(size >= ssize);
	mp_int_to_unsigned(&out, to, size);
	size = ssize;
    }

out:
    mp_int_clear(&e);
    mp_int_clear(&n);
    mp_int_clear(&in);
    mp_int_clear(&out);

    return size;
}

static int
imath_rsa_private_decrypt(int flen, const unsigned char* from,
			  unsigned char* to, RSA* rsa, int padding)
{
    unsigned char *ptr;
    mp_result res;
    size_t size;
    mpz_t in, out, n, e, b, bi;
    int blinding = (rsa->flags & RSA_FLAG_NO_BLINDING) == 0;

    if (padding != RSA_PKCS1_PADDING)
	return -1;

    size = RSA_size(rsa);
    if (flen > size)
	return -2;

    mp_int_init(&in);
    mp_int_init(&out);

    BN2mpz(&n, rsa->n);
    BN2mpz(&e, rsa->e);

    res = mp_int_read_unsigned(&in, rk_UNCONST(from), flen);
    if (res != MP_OK) {
	size = -1;
	goto out;
    }

    if(mp_int_compare_zero(&in) < 0 ||
       mp_int_compare(&in, &n) >= 0) {
	size = 0;
	goto out;
    }

    if (blinding) {
	setup_blind(&n, &b, &bi);
	blind(&in, &b, &e, &n);
    }

    if (rsa->p && rsa->q && rsa->dmp1 && rsa->dmq1 && rsa->iqmp) {
	mpz_t p, q, dmp1, dmq1, iqmp;

	BN2mpz(&p, rsa->p);
	BN2mpz(&q, rsa->q);
	BN2mpz(&dmp1, rsa->dmp1);
	BN2mpz(&dmq1, rsa->dmq1);
	BN2mpz(&iqmp, rsa->iqmp);

	res = rsa_private_calculate(&in, &p, &q, &dmp1, &dmq1, &iqmp, &out);

	mp_int_clear(&p);
	mp_int_clear(&q);
	mp_int_clear(&dmp1);
	mp_int_clear(&dmq1);
	mp_int_clear(&iqmp);
    } else {
	mpz_t d;

	if(mp_int_compare_zero(&in) < 0 ||
	   mp_int_compare(&in, &n) >= 0)
	    return MP_RANGE;

	BN2mpz(&d, rsa->d);
	res = mp_int_exptmod(&in, &d, &n, &out);
	mp_int_clear(&d);
	if (res != MP_OK) {
	    size = 0;
	    goto out;
	}
    }

    if (blinding) {
	unblind(&out, &bi, &n);
	mp_int_clear(&b);
	mp_int_clear(&bi);
    }

    ptr = to;
    {
	size_t ssize;
	ssize = mp_int_unsigned_len(&out);
	assert(size >= ssize);
	mp_int_to_unsigned(&out, ptr, ssize);
	size = ssize;
    }

    /* head zero was skipped by mp_int_to_unsigned */
    if (*ptr != 2)
	return -3;
    size--; ptr++;
    while (size && *ptr != 0) {
	size--; ptr++;
    }
    if (size == 0)
	return -4;
    size--; ptr++;

    memmove(to, ptr, size);

out:
    mp_int_clear(&e);
    mp_int_clear(&n);
    mp_int_clear(&in);
    mp_int_clear(&out);

    return size;
}

static int
random_num(mp_int num, size_t len)
{
    unsigned char *p;
    mp_result res;

    len = (len + 7) / 8;
    p = malloc(len);
    if (p == NULL)
	return 1;
    if (RAND_bytes(p, len) != 1) {
	free(p);
	return 1;
    }
    res = mp_int_read_unsigned(num, p, len);
    free(p);
    if (res != MP_OK)
	return 1;
    return 0;
}

#define CHECK(f, v) if ((f) != (v)) { goto out; }

static int
imath_rsa_generate_key(RSA *rsa, int bits, BIGNUM *e, BN_GENCB *cb)
{
    mpz_t el, p, q, n, d, dmp1, dmq1, iqmp, t1, t2, t3;
    int counter, ret;

    if (bits < 789)
	return -1;

    ret = -1;

    mp_int_init(&el);
    mp_int_init(&p);
    mp_int_init(&q);
    mp_int_init(&n);
    mp_int_init(&d);
    mp_int_init(&dmp1);
    mp_int_init(&dmq1);
    mp_int_init(&iqmp);
    mp_int_init(&t1);
    mp_int_init(&t2);
    mp_int_init(&t3);

    BN2mpz(&el, e);

    /* generate p and q so that p != q and bits(pq) ~ bits */
    counter = 0;
    do {
	BN_GENCB_call(cb, 2, counter++);
	CHECK(random_num(&p, bits / 2 + 1), 0);
	CHECK(mp_int_find_prime(&p), MP_TRUE);

	CHECK(mp_int_sub_value(&p, 1, &t1), MP_OK);
	CHECK(mp_int_gcd(&t1, &el, &t2), MP_OK);
    } while(mp_int_compare_value(&t2, 1) != 0);

    BN_GENCB_call(cb, 3, 0);

    counter = 0;
    do {
	BN_GENCB_call(cb, 2, counter++);
	CHECK(random_num(&q, bits / 2 + 1), 0);
	CHECK(mp_int_find_prime(&q), MP_TRUE);

	if (mp_int_compare(&p, &q) == 0) /* don't let p and q be the same */
	    continue;

	CHECK(mp_int_sub_value(&q, 1, &t1), MP_OK);
	CHECK(mp_int_gcd(&t1, &el, &t2), MP_OK);
    } while(mp_int_compare_value(&t2, 1) != 0);

    /* make p > q */
    if (mp_int_compare(&p, &q) < 0)
	mp_int_swap(&p, &q);

    BN_GENCB_call(cb, 3, 1);

    /* calculate n,  		n = p * q */
    CHECK(mp_int_mul(&p, &q, &n), MP_OK);

    /* calculate d, 		d = 1/e mod (p - 1)(q - 1) */
    CHECK(mp_int_sub_value(&p, 1, &t1), MP_OK);
    CHECK(mp_int_sub_value(&q, 1, &t2), MP_OK);
    CHECK(mp_int_mul(&t1, &t2, &t3), MP_OK);
    CHECK(mp_int_invmod(&el, &t3, &d), MP_OK);

    /* calculate dmp1		dmp1 = d mod (p-1) */
    CHECK(mp_int_mod(&d, &t1, &dmp1), MP_OK);
    /* calculate dmq1		dmq1 = d mod (q-1) */
    CHECK(mp_int_mod(&d, &t2, &dmq1), MP_OK);
    /* calculate iqmp 		iqmp = 1/q mod p */
    CHECK(mp_int_invmod(&q, &p, &iqmp), MP_OK);

    /* fill in RSA key */

    rsa->e = mpz2BN(&el);
    rsa->p = mpz2BN(&p);
    rsa->q = mpz2BN(&q);
    rsa->n = mpz2BN(&n);
    rsa->d = mpz2BN(&d);
    rsa->dmp1 = mpz2BN(&dmp1);
    rsa->dmq1 = mpz2BN(&dmq1);
    rsa->iqmp = mpz2BN(&iqmp);

    ret = 1;
out:
    mp_int_clear(&el);
    mp_int_clear(&p);
    mp_int_clear(&q);
    mp_int_clear(&n);
    mp_int_clear(&d);
    mp_int_clear(&dmp1);
    mp_int_clear(&dmq1);
    mp_int_clear(&iqmp);
    mp_int_clear(&t1);
    mp_int_clear(&t2);
    mp_int_clear(&t3);

    return ret;
}

static int
imath_rsa_init(RSA *rsa)
{
    return 1;
}

static int
imath_rsa_finish(RSA *rsa)
{
    return 1;
}

const RSA_METHOD hc_rsa_imath_method = {
    "hcrypto imath RSA",
    imath_rsa_public_encrypt,
    imath_rsa_public_decrypt,
    imath_rsa_private_encrypt,
    imath_rsa_private_decrypt,
    NULL,
    NULL,
    imath_rsa_init,
    imath_rsa_finish,
    0,
    NULL,
    NULL,
    NULL,
    imath_rsa_generate_key
};

const RSA_METHOD *
RSA_imath_method(void)
{
    return &hc_rsa_imath_method;
}
