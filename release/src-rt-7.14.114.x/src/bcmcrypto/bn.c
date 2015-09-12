/*
 * bn.c: Big Number routines. Needed by Diffie Hellman implementation.
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
 * $Id: bn.c 545409 2015-03-31 18:49:59Z $
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
/*
 * Copyright (c) 1998-2000 The OpenSSL Project.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the
 *    distribution.
 *
 * 3. All advertising materials mentioning features or use of this
 *    software must display the following acknowledgment:
 *    "This product includes software developed by the OpenSSL Project
 *    for use in the OpenSSL Toolkit. (http://www.openssl.org/)"
 *
 * 4. The names "OpenSSL Toolkit" and "OpenSSL Project" must not be used to
 *    endorse or promote products derived from this software without
 *    prior written permission. For written permission, please contact
 *    openssl-core@openssl.org.
 *
 * 5. Products derived from this software may not be called "OpenSSL"
 *    nor may "OpenSSL" appear in their names without prior written
 *    permission of the OpenSSL Project.
 *
 * 6. Redistributions of any form whatsoever must retain the following
 *    acknowledgment:
 *    "This product includes software developed by the OpenSSL Project
 *    for use in the OpenSSL Toolkit (http://www.openssl.org/)"
 *
 * THIS SOFTWARE IS PROVIDED BY THE OpenSSL PROJECT ``AS IS'' AND ANY
 * EXPRESSED OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE OpenSSL PROJECT OR
 * ITS CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED
 * OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * This product includes cryptographic software written by Eric Young
 * (eay@cryptsoft.com).  This product includes software written by Tim
 * Hudson (tjh@cryptsoft.com).
 *
 */
/* Includes code written by Lenka Fibikova <fibikova@exp-math.uni-essen.de>
 * for the OpenSSL project.
 */

#include <typedefs.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <bcmcrypto/bn.h>
#include "bn_lcl.h"

#define OPENSSL_malloc malloc
#define OPENSSL_free free
#define OPENSSL_cleanse(buf, size) memset(buf, 0, size)
#define BNerr(a, b)

static bn_rand_fn_t bn_rand_fn = NULL;

void
BN_register_RAND(bn_rand_fn_t fn)
{
	bn_rand_fn = fn;
}

/* r can == a or b */
int
BN_add(BIGNUM *r, const BIGNUM *a, const BIGNUM *b)
{
	const BIGNUM *tmp;
	int a_neg = a->neg;

	bn_check_top(a);
	bn_check_top(b);

	/*  a +  b	a+b
	 *  a + -b	a-b
	 * -a +  b	b-a
	 * -a + -b	-(a+b)
	 */
	if (a_neg ^ b->neg) {
		/* only one is negative */
		if (a_neg) {
			tmp = a; a = b; b = tmp;
		}

		/* we are now a - b */

		if (BN_ucmp(a, b) < 0) {
			if (!BN_usub(r, b, a)) return (0);
			r->neg = 1;
		} else {
			if (!BN_usub(r, a, b)) return (0);
			r->neg = 0;
		}
		return (1);
	}

	if (!BN_uadd(r, a, b)) return (0);
	if (a_neg) /* both are neg */
		r->neg = 1;
	else
		r->neg = 0;
	return (1);
}

/* unsigned add of b to a, r must be large enough */
int
BN_uadd(BIGNUM *r, const BIGNUM *a, const BIGNUM *b)
{
	register int i;
	int max, min;
	BN_ULONG *ap, *bp, *rp, carry, t1;
	const BIGNUM *tmp;

	bn_check_top(a);
	bn_check_top(b);

	if (a->top < b->top) {
		tmp = a; a = b; b = tmp;
	}
	max = a->top;
	min = b->top;

	if (bn_wexpand(r, max+1) == NULL)
		return (0);

	r->top = max;


	ap = a->d;
	bp = b->d;
	rp = r->d;
	carry = 0;

	carry = bn_add_words(rp, ap, bp, min);
	rp += min;
	ap += min;
	bp += min;
	i = min;

	if (carry) {
		while (i < max) {
			i++;
			t1 = *(ap++);
			if ((*(rp++) = (t1+1)&BN_MASK2) >= t1) {
				carry = 0;
				break;
				}
			}
		if ((i >= max) && carry) {
			*(rp++) = 1;
			r->top++;
			}
		}
	if (rp != ap) {
		for (; i < max; i++)
			*(rp++) = *(ap++);
		}
	/* memcpy(rp, ap, sizeof(*ap)*(max-i)); */
	r->neg = 0;
	return (1);
}

/* unsigned subtraction of b from a, a must be larger than b. */
int
BN_usub(BIGNUM *r, const BIGNUM *a, const BIGNUM *b)
{
	int max, min;
	register BN_ULONG t1, t2, *ap, *bp, *rp;
	int i, carry;
#if defined(IRIX_CC_BUG) && !defined(LINT)
	int dummy;
#endif

	bn_check_top(a);
	bn_check_top(b);

	if (a->top < b->top) /* hmm... should not be happening */ {
		BNerr(BN_F_BN_USUB, BN_R_ARG2_LT_ARG3);
		return (0);
		}

	max = a->top;
	min = b->top;
	if (bn_wexpand(r, max) == NULL) return (0);

	ap = a->d;
	bp = b->d;
	rp = r->d;

	carry = 0;
	for (i = 0; i < min; i++) {
		t1 = *(ap++);
		t2 = *(bp++);
		if (carry) {
			carry = (t1 <= t2);
			t1 = (t1-t2-1)&BN_MASK2;
			}
		else {
			carry = (t1 < t2);
			t1 = (t1-t2)&BN_MASK2;
			}
#if defined(IRIX_CC_BUG) && !defined(LINT)
		dummy = t1;
#endif
		*(rp++) = t1&BN_MASK2;
		}
	if (carry) /* subtracted */ {
		while (i < max) {
			i++;
			t1 = *(ap++);
			t2 = (t1-1)&BN_MASK2;
			*(rp++) = t2;
			if (t1 > t2) break;
			}
		}
	if (rp != ap) {
		for (;;) {
			if (i++ >= max) break;
			rp[0] = ap[0];
			if (i++ >= max) break;
			rp[1] = ap[1];
			if (i++ >= max) break;
			rp[2] = ap[2];
			if (i++ >= max) break;
			rp[3] = ap[3];
			rp += 4;
			ap += 4;
			}
		}

	r->top = max;
	r->neg = 0;
	bn_fix_top(r);
	return (1);
}

int
BN_sub(BIGNUM *r, const BIGNUM *a, const BIGNUM *b)
{
	int max;
	int add = 0, neg = 0;
	const BIGNUM *tmp;

	bn_check_top(a);
	bn_check_top(b);

	/*  a -  b	a-b
	 *  a - -b	a+b
	 * -a -  b	-(a+b)
	 * -a - -b	b-a
	 */
	if (a->neg) {
		if (b->neg) { tmp = a; a = b; b = tmp; }
		else { add = 1; neg = 1; }
		}
	else {
		if (b->neg) { add = 1; neg = 0; }
		}

	if (add) {
		if (!BN_uadd(r, a, b)) return (0);
		r->neg = neg;
		return (1);
		}

	/* We are actually doing a - b :-) */

	max = (a->top > b->top)?a->top:b->top;
	if (bn_wexpand(r, max) == NULL) return (0);
	if (BN_ucmp(a, b) < 0) {
		if (!BN_usub(r, b, a)) return (0);
		r->neg = 1;
		}
	else {
		if (!BN_usub(r, a, b)) return (0);
		r->neg = 0;
		}
	return (1);
}


#if defined(BN_LLONG) || defined(BN_UMULT_HIGH)

BN_ULONG
bn_mul_add_words(BN_ULONG *rp, const BN_ULONG *ap, int num, BN_ULONG w)
{
	BN_ULONG c1 = 0;

	assert(num >= 0);
	if (num <= 0) return (c1);

	while (num&~3) {
		mul_add(rp[0], ap[0], w, c1);
		mul_add(rp[1], ap[1], w, c1);
		mul_add(rp[2], ap[2], w, c1);
		mul_add(rp[3], ap[3], w, c1);
		ap += 4; rp += 4; num -= 4;
		}
	if (num) {
		mul_add(rp[0], ap[0], w, c1); if (--num == 0) return c1;
		mul_add(rp[1], ap[1], w, c1); if (--num == 0) return c1;
		mul_add(rp[2], ap[2], w, c1); return c1;
		}

	return (c1);
}

BN_ULONG
bn_mul_words(BN_ULONG *rp, const BN_ULONG *ap, int num, BN_ULONG w)
{
	BN_ULONG c1 = 0;

	assert(num >= 0);
	if (num <= 0) return (c1);

	while (num&~3) {
		mul(rp[0], ap[0], w, c1);
		mul(rp[1], ap[1], w, c1);
		mul(rp[2], ap[2], w, c1);
		mul(rp[3], ap[3], w, c1);
		ap += 4; rp += 4; num -= 4;
		}
	if (num) {
		mul(rp[0], ap[0], w, c1); if (--num == 0) return c1;
		mul(rp[1], ap[1], w, c1); if (--num == 0) return c1;
		mul(rp[2], ap[2], w, c1);
		}
	return (c1);
}

void
bn_sqr_words(BN_ULONG *r, const BN_ULONG *a, int n)
{
	assert(n >= 0);
	if (n <= 0) return;
	while (n & ~3) {
		sqr(r[0], r[1], a[0]);
		sqr(r[2], r[3], a[1]);
		sqr(r[4], r[5], a[2]);
		sqr(r[6], r[7], a[3]);
		a += 4; r += 8; n -= 4;
	}
	if (n) {
		sqr(r[0], r[1], a[0]); if (--n == 0) return;
		sqr(r[2], r[3], a[1]); if (--n == 0) return;
		sqr(r[4], r[5], a[2]);
	}
}

#else /* !(defined(BN_LLONG) || defined(BN_UMULT_HIGH)) */

BN_ULONG
bn_mul_add_words(BN_ULONG *rp, const BN_ULONG *ap, int num, BN_ULONG w)
{
	BN_ULONG c = 0;
	BN_ULONG bl, bh;

	assert(num >= 0);
	if (num <= 0) return ((BN_ULONG)0);

	bl = LBITS(w);
	bh = HBITS(w);

	for (;;) {
		mul_add(rp[0], ap[0], bl, bh, c);
		if (--num == 0) break;
		mul_add(rp[1], ap[1], bl, bh, c);
		if (--num == 0) break;
		mul_add(rp[2], ap[2], bl, bh, c);
		if (--num == 0) break;
		mul_add(rp[3], ap[3], bl, bh, c);
		if (--num == 0) break;
		ap += 4;
		rp += 4;
	}
	return (c);
}

BN_ULONG
bn_mul_words(BN_ULONG *rp, const BN_ULONG *ap, int num, BN_ULONG w)
{
	BN_ULONG carry = 0;
	BN_ULONG bl, bh;

	assert(num >= 0);
	if (num <= 0) return ((BN_ULONG)0);

	bl = LBITS(w);
	bh = HBITS(w);

	for (;;) {
		mul(rp[0], ap[0], bl, bh, carry);
		if (--num == 0) break;
		mul(rp[1], ap[1], bl, bh, carry);
		if (--num == 0) break;
		mul(rp[2], ap[2], bl, bh, carry);
		if (--num == 0) break;
		mul(rp[3], ap[3], bl, bh, carry);
		if (--num == 0) break;
		ap += 4;
		rp += 4;
	}
	return (carry);
}

void
bn_sqr_words(BN_ULONG *r, const BN_ULONG *a, int n)
{
	assert(n >= 0);
	if (n <= 0) return;
	for (;;) {
		sqr64(r[0], r[1], a[0]);
		if (--n == 0) break;

		sqr64(r[2], r[3], a[1]);
		if (--n == 0) break;

		sqr64(r[4], r[5], a[2]);
		if (--n == 0) break;

		sqr64(r[6], r[7], a[3]);
		if (--n == 0) break;

		a += 4;
		r += 8;
	}
}

#endif /* !(defined(BN_LLONG) || defined(BN_UMULT_HIGH)) */

#if defined(BN_LLONG) && defined(BN_DIV2W)

BN_ULONG
bn_div_words(BN_ULONG h, BN_ULONG l, BN_ULONG d)
{
	return ((BN_ULONG)(((((BN_ULLONG)h) << BN_BITS2)|l) / (BN_ULLONG)d));
}

#else

/* Divide h, l by d and return the result. */
/* I need to test this some more :-( */
BN_ULONG bn_div_words(BN_ULONG h, BN_ULONG l, BN_ULONG d)
{
	BN_ULONG dh, dl, q, ret = 0, th, tl, t;
	int i, count = 2;

	if (d == 0) return (BN_MASK2);

	i = BN_num_bits_word(d);
	assert((i == BN_BITS2) || (h > (BN_ULONG)1 << i));

	i = BN_BITS2-i;
	if (h >= d) h -= d;

	if (i) {
		d <<= i;
		h = (h << i) | (l >> (BN_BITS2 - i));
		l <<= i;
	}
	dh = (d&BN_MASK2h) >> BN_BITS4;
	dl = (d&BN_MASK2l);
	for (;;) {
		if ((h>>BN_BITS4) == dh)
			q = BN_MASK2l;
		else
			q = h/dh;

		th = q*dh;
		tl = dl*q;
		for (;;) {
			t = h-th;
			if ((t&BN_MASK2h) ||
			    ((tl) <= ((t << BN_BITS4) |
			              ((l & BN_MASK2h) >> BN_BITS4))))
				break;
			q--;
			th -= dh;
			tl -= dl;
		}
		t = (tl >> BN_BITS4);
		tl = (tl << BN_BITS4) & BN_MASK2h;
		th += t;

		if (l < tl) th++;
		l -= tl;
		if (h < th) {
			h += d;
			q--;
		}
		h -= th;

		if (--count == 0) break;

		ret = q << BN_BITS4;
		h = ((h << BN_BITS4) | (l >> BN_BITS4)) & BN_MASK2;
		l = (l & BN_MASK2l) << BN_BITS4;
	}
	ret |= q;
	return (ret);
}
#endif /* !defined(BN_LLONG) && defined(BN_DIV2W) */

#ifdef BN_LLONG
BN_ULONG
bn_add_words(BN_ULONG *r, const BN_ULONG *a, const BN_ULONG *b, int n)
{
	BN_ULLONG ll = 0;

	assert(n >= 0);
	if (n <= 0) return ((BN_ULONG)0);

	for (;;) {
		ll += (BN_ULLONG)a[0]+b[0];
		r[0] = (BN_ULONG)ll&BN_MASK2;
		ll >>= BN_BITS2;
		if (--n <= 0) break;

		ll += (BN_ULLONG)a[1]+b[1];
		r[1] = (BN_ULONG)ll&BN_MASK2;
		ll >>= BN_BITS2;
		if (--n <= 0) break;

		ll += (BN_ULLONG)a[2]+b[2];
		r[2] = (BN_ULONG)ll&BN_MASK2;
		ll >>= BN_BITS2;
		if (--n <= 0) break;

		ll += (BN_ULLONG)a[3]+b[3];
		r[3] = (BN_ULONG)ll&BN_MASK2;
		ll >>= BN_BITS2;
		if (--n <= 0) break;

		a += 4;
		b += 4;
		r += 4;
	}
	return ((BN_ULONG)ll);
}

#else /* !BN_LLONG */

BN_ULONG
bn_add_words(BN_ULONG *r, const BN_ULONG *a, const BN_ULONG *b, int n)
{
	BN_ULONG c, l, t;

	assert(n >= 0);
	if (n <= 0) return ((BN_ULONG)0);

	c = 0;
	for (;;) {
		t = a[0];
		t = (t+c)&BN_MASK2;
		c = (t < c);
		l = (t+b[0])&BN_MASK2;
		c += (l < t);
		r[0] = l;
		if (--n <= 0) break;

		t = a[1];
		t = (t+c)&BN_MASK2;
		c = (t < c);
		l = (t+b[1])&BN_MASK2;
		c += (l < t);
		r[1] = l;
		if (--n <= 0) break;

		t = a[2];
		t = (t+c)&BN_MASK2;
		c = (t < c);
		l = (t+b[2])&BN_MASK2;
		c += (l < t);
		r[2] = l;
		if (--n <= 0) break;

		t = a[3];
		t = (t+c)&BN_MASK2;
		c = (t < c);
		l = (t+b[3])&BN_MASK2;
		c += (l < t);
		r[3] = l;
		if (--n <= 0) break;

		a += 4;
		b += 4;
		r += 4;
	}
	return ((BN_ULONG)c);
}
#endif /* !BN_LLONG */

BN_ULONG
bn_sub_words(BN_ULONG *r, const BN_ULONG *a, const BN_ULONG *b, int n)
{
	BN_ULONG t1, t2;
	int c = 0;

	assert(n >= 0);
	if (n <= 0) return ((BN_ULONG)0);

	for (;;) {
		t1 = a[0]; t2 = b[0];
		r[0] = (t1-t2-c)&BN_MASK2;
		if (t1 != t2) c = (t1 < t2);
		if (--n <= 0) break;

		t1 = a[1]; t2 = b[1];
		r[1] = (t1-t2-c)&BN_MASK2;
		if (t1 != t2) c = (t1 < t2);
		if (--n <= 0) break;

		t1 = a[2]; t2 = b[2];
		r[2] = (t1-t2-c)&BN_MASK2;
		if (t1 != t2) c = (t1 < t2);
		if (--n <= 0) break;

		t1 = a[3]; t2 = b[3];
		r[3] = (t1-t2-c)&BN_MASK2;
		if (t1 != t2) c = (t1 < t2);
		if (--n <= 0) break;

		a += 4;
		b += 4;
		r += 4;
	}
	return (c);
}

#ifdef BN_MUL_COMBA

#undef bn_mul_comba8
#undef bn_mul_comba4
#undef bn_sqr_comba8
#undef bn_sqr_comba4

/* mul_add_c(a, b, c0, c1, c2)  -- c += a*b for three word number c = (c2, c1, c0) */
/* mul_add_c2(a, b, c0, c1, c2) -- c += 2*a*b for three word number c = (c2, c1, c0) */
/* sqr_add_c(a, i, c0, c1, c2)  -- c += a[i]^2 for three word number c = (c2, c1, c0) */
/* sqr_add_c2(a, i, c0, c1, c2) -- c += 2*a[i]*a[j] for three word number c = (c2, c1, c0) */

#ifdef BN_LLONG
#define mul_add_c(a, b, c0, c1, c2) \
	t = (BN_ULLONG)a * b; \
	t1 = (BN_ULONG)Lw(t); \
	t2 = (BN_ULONG)Hw(t); \
	c0 = (c0+t1)&BN_MASK2; if ((c0) < t1) t2++; \
	c1 = (c1+t2)&BN_MASK2; if ((c1) < t2) c2++;

#define mul_add_c2(a, b, c0, c1, c2) \
	t = (BN_ULLONG)a * b; \
	tt = (t + t) & BN_MASK; \
	if (tt < t) c2++; \
	t1 = (BN_ULONG)Lw(tt); \
	t2 = (BN_ULONG)Hw(tt); \
	c0 = (c0+t1)&BN_MASK2;  \
	if ((c0 < t1) && (((++t2) & BN_MASK2) == 0)) c2++; \
	c1 = (c1+t2)&BN_MASK2; if ((c1) < t2) c2++;

#define sqr_add_c(a, i, c0, c1, c2) \
	t = (BN_ULLONG)a[i]*a[i]; \
	t1 = (BN_ULONG)Lw(t); \
	t2 = (BN_ULONG)Hw(t); \
	c0 = (c0+t1)&BN_MASK2; if ((c0) < t1) t2++; \
	c1 = (c1+t2)&BN_MASK2; if ((c1) < t2) c2++;

#define sqr_add_c2(a, i, j, c0, c1, c2) \
	mul_add_c2((a)[i], (a)[j], c0, c1, c2)

#elif defined(BN_UMULT_HIGH)

#define mul_add_c(a, b, c0, c1, c2) {		\
	BN_ULONG ta = (a), tb = (b);		\
	t1 = ta * tb;				\
	t2 = BN_UMULT_HIGH(ta, tb);		\
	c0 += t1; t2 += (c0 < t1) ? 1 : 0;	\
	c1 += t2; c2 += (c1 < t2) ? 1 : 0;	\
}

#define mul_add_c2(a, b, c0, c1, c2) {		\
	BN_ULONG ta = (a), tb = (b), t0;	\
	t1 = BN_UMULT_HIGH(ta, tb);		\
	t0 = ta * tb;				\
	t2 = t1 + t1; c2 += (t2 < t1) ? 1 : 0;	\
	t1 = t0 + t0; t2 += (t1 < t0) ? 1 : 0;	\
	c0 += t1; t2 += (c0 < t1) ? 1 : 0;	\
	c1 += t2; c2 += (c1 < t2) ? 1 : 0;	\
}

#define sqr_add_c(a, i, c0, c1, c2) {		\
	BN_ULONG ta = (a)[i];			\
	t1 = ta * ta;				\
	t2 = BN_UMULT_HIGH(ta, ta);		\
	c0 += t1; t2 += (c0 < t1) ? 1 : 0;	\
	c1 += t2; c2 += (c1 < t2) ? 1 : 0;	\
}

#define sqr_add_c2(a, i, j, c0, c1, c2)	\
	mul_add_c2((a)[i], (a)[j], c0, c1, c2)

#else /* !BN_LLONG */

#define mul_add_c(a, b, c0, c1, c2) \
	t1 = LBITS(a); t2 = HBITS(a); \
	bl = LBITS(b); bh = HBITS(b); \
	mul64(t1, t2, bl, bh); \
	c0 = (c0+t1)&BN_MASK2; if ((c0) < t1) t2++; \
	c1 = (c1+t2)&BN_MASK2; if ((c1) < t2) c2++;

#define mul_add_c2(a, b, c0, c1, c2) \
	t1 = LBITS(a); t2 = HBITS(a); \
	bl = LBITS(b); bh = HBITS(b); \
	mul64(t1, t2, bl, bh); \
	if (t2 & BN_TBIT) c2++; \
	t2 = (t2+t2) & BN_MASK2; \
	if (t1 & BN_TBIT) t2++; \
	t1 = (t1+t1) & BN_MASK2; \
	c0 = (c0+t1) & BN_MASK2;  \
	if ((c0 < t1) && (((++t2) & BN_MASK2) == 0)) c2++; \
	c1 = (c1+t2) & BN_MASK2; if ((c1) < t2) c2++;

#define sqr_add_c(a, i, c0, c1, c2) \
	sqr64(t1, t2, (a)[i]); \
	c0 = (c0+t1) & BN_MASK2; if ((c0) < t1) t2++; \
	c1 = (c1+t2) & BN_MASK2; if ((c1) < t2) c2++;

#define sqr_add_c2(a, i, j, c0, c1, c2) \
	mul_add_c2((a)[i], (a)[j], c0, c1, c2)
#endif /* !BN_LLONG */

void
bn_mul_comba8(BN_ULONG *r, BN_ULONG *a, BN_ULONG *b)
{
#ifdef BN_LLONG
	BN_ULLONG t;
#else
	BN_ULONG bl, bh;
#endif
	BN_ULONG t1, t2;
	BN_ULONG c1, c2, c3;

	c1 = 0;
	c2 = 0;
	c3 = 0;
	mul_add_c(a[0], b[0], c1, c2, c3);
	r[0] = c1;
	c1 = 0;
	mul_add_c(a[0], b[1], c2, c3, c1);
	mul_add_c(a[1], b[0], c2, c3, c1);
	r[1] = c2;
	c2 = 0;
	mul_add_c(a[2], b[0], c3, c1, c2);
	mul_add_c(a[1], b[1], c3, c1, c2);
	mul_add_c(a[0], b[2], c3, c1, c2);
	r[2] = c3;
	c3 = 0;
	mul_add_c(a[0], b[3], c1, c2, c3);
	mul_add_c(a[1], b[2], c1, c2, c3);
	mul_add_c(a[2], b[1], c1, c2, c3);
	mul_add_c(a[3], b[0], c1, c2, c3);
	r[3] = c1;
	c1 = 0;
	mul_add_c(a[4], b[0], c2, c3, c1);
	mul_add_c(a[3], b[1], c2, c3, c1);
	mul_add_c(a[2], b[2], c2, c3, c1);
	mul_add_c(a[1], b[3], c2, c3, c1);
	mul_add_c(a[0], b[4], c2, c3, c1);
	r[4] = c2;
	c2 = 0;
	mul_add_c(a[0], b[5], c3, c1, c2);
	mul_add_c(a[1], b[4], c3, c1, c2);
	mul_add_c(a[2], b[3], c3, c1, c2);
	mul_add_c(a[3], b[2], c3, c1, c2);
	mul_add_c(a[4], b[1], c3, c1, c2);
	mul_add_c(a[5], b[0], c3, c1, c2);
	r[5] = c3;
	c3 = 0;
	mul_add_c(a[6], b[0], c1, c2, c3);
	mul_add_c(a[5], b[1], c1, c2, c3);
	mul_add_c(a[4], b[2], c1, c2, c3);
	mul_add_c(a[3], b[3], c1, c2, c3);
	mul_add_c(a[2], b[4], c1, c2, c3);
	mul_add_c(a[1], b[5], c1, c2, c3);
	mul_add_c(a[0], b[6], c1, c2, c3);
	r[6] = c1;
	c1 = 0;
	mul_add_c(a[0], b[7], c2, c3, c1);
	mul_add_c(a[1], b[6], c2, c3, c1);
	mul_add_c(a[2], b[5], c2, c3, c1);
	mul_add_c(a[3], b[4], c2, c3, c1);
	mul_add_c(a[4], b[3], c2, c3, c1);
	mul_add_c(a[5], b[2], c2, c3, c1);
	mul_add_c(a[6], b[1], c2, c3, c1);
	mul_add_c(a[7], b[0], c2, c3, c1);
	r[7] = c2;
	c2 = 0;
	mul_add_c(a[7], b[1], c3, c1, c2);
	mul_add_c(a[6], b[2], c3, c1, c2);
	mul_add_c(a[5], b[3], c3, c1, c2);
	mul_add_c(a[4], b[4], c3, c1, c2);
	mul_add_c(a[3], b[5], c3, c1, c2);
	mul_add_c(a[2], b[6], c3, c1, c2);
	mul_add_c(a[1], b[7], c3, c1, c2);
	r[8] = c3;
	c3 = 0;
	mul_add_c(a[2], b[7], c1, c2, c3);
	mul_add_c(a[3], b[6], c1, c2, c3);
	mul_add_c(a[4], b[5], c1, c2, c3);
	mul_add_c(a[5], b[4], c1, c2, c3);
	mul_add_c(a[6], b[3], c1, c2, c3);
	mul_add_c(a[7], b[2], c1, c2, c3);
	r[9] = c1;
	c1 = 0;
	mul_add_c(a[7], b[3], c2, c3, c1);
	mul_add_c(a[6], b[4], c2, c3, c1);
	mul_add_c(a[5], b[5], c2, c3, c1);
	mul_add_c(a[4], b[6], c2, c3, c1);
	mul_add_c(a[3], b[7], c2, c3, c1);
	r[10] = c2;
	c2 = 0;
	mul_add_c(a[4], b[7], c3, c1, c2);
	mul_add_c(a[5], b[6], c3, c1, c2);
	mul_add_c(a[6], b[5], c3, c1, c2);
	mul_add_c(a[7], b[4], c3, c1, c2);
	r[11] = c3;
	c3 = 0;
	mul_add_c(a[7], b[5], c1, c2, c3);
	mul_add_c(a[6], b[6], c1, c2, c3);
	mul_add_c(a[5], b[7], c1, c2, c3);
	r[12] = c1;
	c1 = 0;
	mul_add_c(a[6], b[7], c2, c3, c1);
	mul_add_c(a[7], b[6], c2, c3, c1);
	r[13] = c2;
	c2 = 0;
	mul_add_c(a[7], b[7], c3, c1, c2);
	r[14] = c3;
	r[15] = c1;
}

void bn_mul_comba4(BN_ULONG *r, BN_ULONG *a, BN_ULONG *b)
{
#ifdef BN_LLONG
	BN_ULLONG t;
#else
	BN_ULONG bl, bh;
#endif
	BN_ULONG t1, t2;
	BN_ULONG c1, c2, c3;

	c1 = 0;
	c2 = 0;
	c3 = 0;
	mul_add_c(a[0], b[0], c1, c2, c3);
	r[0] = c1;
	c1 = 0;
	mul_add_c(a[0], b[1], c2, c3, c1);
	mul_add_c(a[1], b[0], c2, c3, c1);
	r[1] = c2;
	c2 = 0;
	mul_add_c(a[2], b[0], c3, c1, c2);
	mul_add_c(a[1], b[1], c3, c1, c2);
	mul_add_c(a[0], b[2], c3, c1, c2);
	r[2] = c3;
	c3 = 0;
	mul_add_c(a[0], b[3], c1, c2, c3);
	mul_add_c(a[1], b[2], c1, c2, c3);
	mul_add_c(a[2], b[1], c1, c2, c3);
	mul_add_c(a[3], b[0], c1, c2, c3);
	r[3] = c1;
	c1 = 0;
	mul_add_c(a[3], b[1], c2, c3, c1);
	mul_add_c(a[2], b[2], c2, c3, c1);
	mul_add_c(a[1], b[3], c2, c3, c1);
	r[4] = c2;
	c2 = 0;
	mul_add_c(a[2], b[3], c3, c1, c2);
	mul_add_c(a[3], b[2], c3, c1, c2);
	r[5] = c3;
	c3 = 0;
	mul_add_c(a[3], b[3], c1, c2, c3);
	r[6] = c1;
	r[7] = c2;
}

void
bn_sqr_comba8(BN_ULONG *r, const BN_ULONG *a)
{
#ifdef BN_LLONG
	BN_ULLONG t, tt;
#else
	BN_ULONG bl, bh;
#endif
	BN_ULONG t1, t2;
	BN_ULONG c1, c2, c3;

	c1 = 0;
	c2 = 0;
	c3 = 0;
	sqr_add_c(a, 0, c1, c2, c3);
	r[0] = c1;
	c1 = 0;
	sqr_add_c2(a, 1, 0, c2, c3, c1);
	r[1] = c2;
	c2 = 0;
	sqr_add_c(a, 1, c3, c1, c2);
	sqr_add_c2(a, 2, 0, c3, c1, c2);
	r[2] = c3;
	c3 = 0;
	sqr_add_c2(a, 3, 0, c1, c2, c3);
	sqr_add_c2(a, 2, 1, c1, c2, c3);
	r[3] = c1;
	c1 = 0;
	sqr_add_c(a, 2, c2, c3, c1);
	sqr_add_c2(a, 3, 1, c2, c3, c1);
	sqr_add_c2(a, 4, 0, c2, c3, c1);
	r[4] = c2;
	c2 = 0;
	sqr_add_c2(a, 5, 0, c3, c1, c2);
	sqr_add_c2(a, 4, 1, c3, c1, c2);
	sqr_add_c2(a, 3, 2, c3, c1, c2);
	r[5] = c3;
	c3 = 0;
	sqr_add_c(a, 3, c1, c2, c3);
	sqr_add_c2(a, 4, 2, c1, c2, c3);
	sqr_add_c2(a, 5, 1, c1, c2, c3);
	sqr_add_c2(a, 6, 0, c1, c2, c3);
	r[6] = c1;
	c1 = 0;
	sqr_add_c2(a, 7, 0, c2, c3, c1);
	sqr_add_c2(a, 6, 1, c2, c3, c1);
	sqr_add_c2(a, 5, 2, c2, c3, c1);
	sqr_add_c2(a, 4, 3, c2, c3, c1);
	r[7] = c2;
	c2 = 0;
	sqr_add_c(a, 4, c3, c1, c2);
	sqr_add_c2(a, 5, 3, c3, c1, c2);
	sqr_add_c2(a, 6, 2, c3, c1, c2);
	sqr_add_c2(a, 7, 1, c3, c1, c2);
	r[8] = c3;
	c3 = 0;
	sqr_add_c2(a, 7, 2, c1, c2, c3);
	sqr_add_c2(a, 6, 3, c1, c2, c3);
	sqr_add_c2(a, 5, 4, c1, c2, c3);
	r[9] = c1;
	c1 = 0;
	sqr_add_c(a, 5, c2, c3, c1);
	sqr_add_c2(a, 6, 4, c2, c3, c1);
	sqr_add_c2(a, 7, 3, c2, c3, c1);
	r[10] = c2;
	c2 = 0;
	sqr_add_c2(a, 7, 4, c3, c1, c2);
	sqr_add_c2(a, 6, 5, c3, c1, c2);
	r[11] = c3;
	c3 = 0;
	sqr_add_c(a, 6, c1, c2, c3);
	sqr_add_c2(a, 7, 5, c1, c2, c3);
	r[12] = c1;
	c1 = 0;
	sqr_add_c2(a, 7, 6, c2, c3, c1);
	r[13] = c2;
	c2 = 0;
	sqr_add_c(a, 7, c3, c1, c2);
	r[14] = c3;
	r[15] = c1;
}

void
bn_sqr_comba4(BN_ULONG *r, const BN_ULONG *a)
{
#ifdef BN_LLONG
	BN_ULLONG t, tt;
#else
	BN_ULONG bl, bh;
#endif
	BN_ULONG t1, t2;
	BN_ULONG c1, c2, c3;

	c1 = 0;
	c2 = 0;
	c3 = 0;
	sqr_add_c(a, 0, c1, c2, c3);
	r[0] = c1;
	c1 = 0;
	sqr_add_c2(a, 1, 0, c2, c3, c1);
	r[1] = c2;
	c2 = 0;
	sqr_add_c(a, 1, c3, c1, c2);
	sqr_add_c2(a, 2, 0, c3, c1, c2);
	r[2] = c3;
	c3 = 0;
	sqr_add_c2(a, 3, 0, c1, c2, c3);
	sqr_add_c2(a, 2, 1, c1, c2, c3);
	r[3] = c1;
	c1 = 0;
	sqr_add_c(a, 2, c2, c3, c1);
	sqr_add_c2(a, 3, 1, c2, c3, c1);
	r[4] = c2;
	c2 = 0;
	sqr_add_c2(a, 3, 2, c3, c1, c2);
	r[5] = c3;
	c3 = 0;
	sqr_add_c(a, 3, c1, c2, c3);
	r[6] = c1;
	r[7] = c2;
}
#else /* !BN_MUL_COMBA */

/* hmm... is it faster just to do a multiply? */
#undef bn_sqr_comba4
void
bn_sqr_comba4(BN_ULONG *r, BN_ULONG *a)
{
	BN_ULONG t[8];
	bn_sqr_normal(r, a, 4, t);
}

#undef bn_sqr_comba8
void
bn_sqr_comba8(BN_ULONG *r, BN_ULONG *a)
{
	BN_ULONG t[16];
	bn_sqr_normal(r, a, 8, t);
}

void
bn_mul_comba4(BN_ULONG *r, BN_ULONG *a, BN_ULONG *b)
{
	r[4] = bn_mul_words(&(r[0]), a, 4, b[0]);
	r[5] = bn_mul_add_words(&(r[1]), a, 4, b[1]);
	r[6] = bn_mul_add_words(&(r[2]), a, 4, b[2]);
	r[7] = bn_mul_add_words(&(r[3]), a, 4, b[3]);
}

void
bn_mul_comba8(BN_ULONG *r, BN_ULONG *a, BN_ULONG *b)
{
	r[ 8] = bn_mul_words(&(r[0]), a, 8, b[0]);
	r[ 9] = bn_mul_add_words(&(r[1]), a, 8, b[1]);
	r[10] = bn_mul_add_words(&(r[2]), a, 8, b[2]);
	r[11] = bn_mul_add_words(&(r[3]), a, 8, b[3]);
	r[12] = bn_mul_add_words(&(r[4]), a, 8, b[4]);
	r[13] = bn_mul_add_words(&(r[5]), a, 8, b[5]);
	r[14] = bn_mul_add_words(&(r[6]), a, 8, b[6]);
	r[15] = bn_mul_add_words(&(r[7]), a, 8, b[7]);
}

#endif /* !BN_MUL_COMBA */


BN_CTX *
BN_CTX_new(void)
{
	BN_CTX *ret;

	ret = (BN_CTX *)OPENSSL_malloc(sizeof(BN_CTX));
	if (ret == NULL) {
		BNerr(BN_F_BN_CTX_NEW, ERR_R_MALLOC_FAILURE);
		return (NULL);
	}

	BN_CTX_init(ret);
	ret->flags = BN_FLG_MALLOCED;
	return (ret);
}

void
BN_CTX_init(BN_CTX *ctx)
{
	memset(ctx, 0, sizeof(*ctx));
}

void
BN_CTX_free(BN_CTX *ctx)
{
	int i;

	if (ctx == NULL) return;
	assert(ctx->depth == 0);

	for (i = 0; i < BN_CTX_NUM; i++)
		BN_clear_free(&(ctx->bn[i]));
	if (ctx->flags & BN_FLG_MALLOCED)
		OPENSSL_free(ctx);
}

void
BN_CTX_start(BN_CTX *ctx)
{
	if (ctx->depth < BN_CTX_NUM_POS)
		ctx->pos[ctx->depth] = ctx->tos;
	ctx->depth++;
}


BIGNUM *
BN_CTX_get(BN_CTX *ctx)
{
	/* Note: If BN_CTX_get is ever changed to allocate BIGNUMs dynamically,
	 * make sure that if BN_CTX_get fails once it will return NULL again
	 * until BN_CTX_end is called.  (This is so that callers have to check
	 * only the last return value.)
	 */
	if (ctx->depth > BN_CTX_NUM_POS || ctx->tos >= BN_CTX_NUM) {
		if (!ctx->too_many) {
			BNerr(BN_F_BN_CTX_GET, BN_R_TOO_MANY_TEMPORARY_VARIABLES);
			/* disable error code until BN_CTX_end is called: */
			ctx->too_many = 1;
		}
		return NULL;
	}
	return (&(ctx->bn[ctx->tos++]));
}

void
BN_CTX_end(BN_CTX *ctx)
{
	if (ctx == NULL) return;
	assert(ctx->depth > 0);
	if (ctx->depth == 0) {
		/* should never happen, but we can tolerate it if not in
		 * debug mode (could be a 'goto err' in the calling function
		 * before BN_CTX_start was reached)
		 */
		BN_CTX_start(ctx);
	}
	ctx->too_many = 0;
	ctx->depth--;
	if (ctx->depth < BN_CTX_NUM_POS)
		ctx->tos = ctx->pos[ctx->depth];
}

/* The old slow way */

#if !defined(OPENSSL_NO_ASM) && !defined(OPENSSL_NO_INLINE_ASM) && !defined(PEDANTIC) \
	&& !defined(BN_DIV3W)
#if defined(__GNUC__) && __GNUC__ >= 2
#if defined(__i386) || defined(__i386__)
/*
 * There were two reasons for implementing this template:
 * - GNU C generates a call to a function (__udivdi3 to be exact)
 *   in reply to ((((BN_ULLONG)n0) << BN_BITS2) | n1) / d0 (I fail to
 *   understand why...);
 * - divl doesn't only calculate quotient, but also leaves
 *   remainder in %edx which we can definitely use here:-)
 *
 *					<appro@fy.chalmers.se>
 */
#define bn_div_words(n0, n1, d0)		\
	({  asm volatile("divl	%4"		\
		: " = a"(q), " = d"(rem)	\
		: "a"(n1), "d"(n0), "g"(d0)	\
		: "cc");			\
	    q;					\
	})
#define REMAINDER_IS_ALREADY_CALCULATED
#elif defined(__x86_64) && defined(SIXTY_FOUR_BIT_LONG)
/*
 * Same story here, but it's 128-bit by 64-bit division. Wow!
 *					<appro@fy.chalmers.se>
 */
#define bn_div_words(n0, n1, d0)		\
	({  asm volatile("divq	%4"		\
		: " = a"(q), " = d"(rem)	\
		: "a"(n1), "d"(n0), "g"(d0)	\
		: "cc");			\
	    q;					\
	})
#define REMAINDER_IS_ALREADY_CALCULATED
#endif /* __<cpu> */
#endif /* __GNUC__ */
#endif /* OPENSSL_NO_ASM */


/* BN_div computes  dv : = num / divisor, rounding towards zero, and sets up
 * rm  such that  dv*divisor + rm = num  holds.
 * Thus:
 *     dv->neg == num->neg ^ divisor->neg  (unless the result is zero)
 *     rm->neg == num->neg                 (unless the remainder is zero)
 * If 'dv' or 'rm' is NULL, the respective value is not returned.
 */
int
BN_div(BIGNUM *dv, BIGNUM *rm, const BIGNUM *num, const BIGNUM *divisor, BN_CTX *ctx)
{
	int norm_shift, i, j, loop;
	BIGNUM *tmp, wnum, *snum, *sdiv, *res;
	BN_ULONG *resp, *wnump;
	BN_ULONG d0, d1;
	int num_n, div_n;

	bn_check_top(num);
	bn_check_top(divisor);

	if (BN_is_zero(divisor)) {
		BNerr(BN_F_BN_DIV, BN_R_DIV_BY_ZERO);
		return (0);
	}

	if (BN_ucmp(num, divisor) < 0) {
		if (rm != NULL) { if (BN_copy(rm, num) == NULL) return (0); }
		if (dv != NULL) BN_zero(dv);
		return (1);
	}

	BN_CTX_start(ctx);
	tmp = BN_CTX_get(ctx);
	snum = BN_CTX_get(ctx);
	sdiv = BN_CTX_get(ctx);
	if (dv == NULL)
		res = BN_CTX_get(ctx);
	else	res = dv;
	if (sdiv == NULL || res == NULL) goto err;
	tmp->neg = 0;

	/* First we normalise the numbers */
	norm_shift = BN_BITS2-((BN_num_bits(divisor))%BN_BITS2);
	if (!(BN_lshift(sdiv, divisor, norm_shift))) goto err;
	sdiv->neg = 0;
	norm_shift += BN_BITS2;
	if (!(BN_lshift(snum, num, norm_shift))) goto err;
	snum->neg = 0;
	div_n = sdiv->top;
	num_n = snum->top;
	loop = num_n-div_n;

	/* Lets setup a 'window' into snum
	 * This is the part that corresponds to the current
	 * 'area' being divided
	 */
	BN_init(&wnum);
	wnum.d = &(snum->d[loop]);
	wnum.top = div_n;
	wnum.dmax = snum->dmax+1; /* a bit of a lie */

	/* Get the top 2 words of sdiv */
	/* i = sdiv->top; */
	d0 = sdiv->d[div_n-1];
	d1 = (div_n == 1)?0:sdiv->d[div_n-2];

	/* pointer to the 'top' of snum */
	wnump = &(snum->d[num_n-1]);

	/* Setup to 'res' */
	res->neg = (num->neg^divisor->neg);
	if (!bn_wexpand(res, (loop+1))) goto err;
	res->top = loop;
	resp = &(res->d[loop-1]);

	/* space for temp */
	if (!bn_wexpand(tmp, (div_n+1))) goto err;

	if (BN_ucmp(&wnum, sdiv) >= 0) {
		if (!BN_usub(&wnum, &wnum, sdiv)) goto err;
		*resp = 1;
		res->d[res->top-1] = 1;
	} else
		res->top--;
	if (res->top == 0)
		res->neg = 0;
	resp--;

	for (i = 0; i < loop - 1; i++) {
		BN_ULONG q, l0;
#if defined(BN_DIV3W) && !defined(OPENSSL_NO_ASM)
		BN_ULONG bn_div_3_words(BN_ULONG*, BN_ULONG, BN_ULONG);
		q = bn_div_3_words(wnump, d1, d0);
#else
		BN_ULONG n0, n1, rem = 0;

		n0 = wnump[0];
		n1 = wnump[-1];
		if (n0 == d0)
			q = BN_MASK2;
		else /* n0 < d0 */ {
#ifdef BN_LLONG
			BN_ULLONG t2;

#if defined(BN_LLONG) && defined(BN_DIV2W) && !defined(bn_div_words)
			q = (BN_ULONG)(((((BN_ULLONG)n0) << BN_BITS2) | n1) / d0);
#else
			q = bn_div_words(n0, n1, d0);
#ifdef BN_DEBUG_LEVITTE
			fprintf(stderr, "DEBUG: bn_div_words(0x%08X, 0x%08X, 0x%08X) -> 0x%08X\n",
			        n0, n1, d0, q);
#endif	/* BN_DEBUG_LEVITTE */
#endif	/* BN_LLONG && BN_DIV2W && !bn_div_words */

#ifndef REMAINDER_IS_ALREADY_CALCULATED
			/*
			 * rem doesn't have to be BN_ULLONG. The least we
			 * know it's less that d0, isn't it?
			 */
			rem = (n1 - q * d0) & BN_MASK2;
#endif	/* !REMAINDER_IS_ALREADY_CALCULATED */
			t2 = (BN_ULLONG)d1 * q;

			for (;;) {
				if (t2 <= ((((BN_ULLONG)rem) << BN_BITS2) | wnump[-2]))
					break;
				q--;
				rem += d0;
				if (rem < d0) break; /* don't let rem overflow */
				t2 -= d1;
			}
#else /* !BN_LLONG */
			BN_ULONG t2l, t2h, ql, qh;

			q = bn_div_words(n0, n1, d0);
#ifdef BN_DEBUG_LEVITTE
			fprintf(stderr, "DEBUG: bn_div_words(0x%08X, 0x%08X, 0x%08X) -> 0x%08X\n",
				n0, n1, d0, q);
#endif
#ifndef REMAINDER_IS_ALREADY_CALCULATED
			rem = (n1 - q * d0) & BN_MASK2;
#endif

#if defined(BN_UMULT_LOHI)
			BN_UMULT_LOHI(t2l, t2h, d1, q);
#elif defined(BN_UMULT_HIGH)
			t2l = d1 * q;
			t2h = BN_UMULT_HIGH(d1, q);
#else
			t2l = LBITS(d1); t2h = HBITS(d1);
			ql = LBITS(q);  qh = HBITS(q);
			mul64(t2l, t2h, ql, qh); /* t2 = (BN_ULLONG)d1 * q; */
#endif

			for (;;) {
				if ((t2h < rem) || ((t2h == rem) && (t2l <= wnump[-2])))
					break;
				q--;
				rem += d0;
				if (rem < d0) break; /* don't let rem overflow */
				if (t2l < d1)
					t2h--;
				t2l -= d1;
			}
#endif /* !BN_LLONG */
		}
#endif /* !BN_DIV3W */

		l0 = bn_mul_words(tmp->d, sdiv->d, div_n, q);
		wnum.d--; wnum.top++;
		tmp->d[div_n] = l0;
		for (j = div_n + 1; j > 0; j--)
			if (tmp->d[j - 1]) break;
		tmp->top = j;

		j = wnum.top;
		if (!BN_sub(&wnum, &wnum, tmp)) goto err;

		snum->top = snum->top + wnum.top - j;

		if (wnum.neg) {
			q--;
			j = wnum.top;
			if (!BN_add(&wnum, &wnum, sdiv)) goto err;
			snum->top += wnum.top - j;
		}
		*(resp--) = q;
		wnump--;
	}
	if (rm != NULL) {
		/* Keep a copy of the neg flag in num because if rm == num
		 * BN_rshift() will overwrite it.
		 */
		int neg = num->neg;
		BN_rshift(rm, snum, norm_shift);
		if (!BN_is_zero(rm))
			rm->neg = neg;
	}
	BN_CTX_end(ctx);
	return (1);
err:
	BN_CTX_end(ctx);
	return (0);
}


#define TABLE_SIZE	32

#ifdef NOT_NEEDED_FOR_DH
/* this one works - simple but works */
int
BN_exp(BIGNUM *r, const BIGNUM *a, const BIGNUM *p, BN_CTX *ctx)
{
	int i, bits, ret = 0;
	BIGNUM *v, *rr;

	BN_CTX_start(ctx);
	if ((r == a) || (r == p))
		rr = BN_CTX_get(ctx);
	else
		rr = r;
	if ((v = BN_CTX_get(ctx)) == NULL) goto err;

	if (BN_copy(v, a) == NULL) goto err;
	bits = BN_num_bits(p);

	if (BN_is_odd(p)) {
		if (BN_copy(rr, a) == NULL)
			goto err;
	} else {
		if (!BN_one(rr))
			goto err;
	}

	for (i = 1; i < bits; i++) {
		if (!BN_sqr(v, v, ctx)) goto err;
		if (BN_is_bit_set(p, i)) {
			if (!BN_mul(rr, rr, v, ctx)) goto err;
		}
	}
	ret = 1;
err:
	if (r != rr) BN_copy(r, rr);
	BN_CTX_end(ctx);
	return (ret);
}

int
BN_mod_exp(BIGNUM *r, const BIGNUM *a, const BIGNUM *p, const BIGNUM *m, BN_CTX *ctx)
{
	int ret;

	bn_check_top(a);
	bn_check_top(p);
	bn_check_top(m);

	/* For even modulus  m = 2^k*m_odd, it might make sense to compute
	 * a^p mod m_odd  and  a^p mod 2^k  separately (with Montgomery
	 * exponentiation for the odd part), using appropriate exponent
	 * reductions, and combine the results using the CRT.
	 *
	 * For now, we use Montgomery only if the modulus is odd; otherwise,
	 * exponentiation using the reciprocal-based quick remaindering
	 * algorithm is used.
	 *
	 * (Timing obtained with expspeed.c [computations  a^p mod m
	 * where  a, p, m  are of the same length: 256, 512, 1024, 2048,
	 * 4096, 8192 bits], compared to the running time of the
	 * standard algorithm:
	 *
	 *   BN_mod_exp_mont   33 .. 40 %  [AMD K6-2, Linux, debug configuration]
	 *                     55 .. 77 %  [UltraSparc processor, but
	 *                                  debug-solaris-sparcv8-gcc conf.]
	 *
	 *   BN_mod_exp_recp   50 .. 70 %  [AMD K6-2, Linux, debug configuration]
	 *                     62 .. 118 % [UltraSparc, debug-solaris-sparcv8-gcc]
	 *
	 * On the Sparc, BN_mod_exp_recp was faster than BN_mod_exp_mont
	 * at 2048 and more bits, but at 512 and 1024 bits, it was
	 * slower even than the standard algorithm!
	 *
	 * "Real" timings [linux-elf, solaris-sparcv9-gcc configurations]
	 * should be obtained when the new Montgomery reduction code
	 * has been integrated into OpenSSL.)
	 */

#define MONT_MUL_MOD
#define MONT_EXP_WORD
#define RECP_MUL_MOD

#ifdef MONT_MUL_MOD
	/* I have finally been able to take out this pre-condition of
	 * the top bit being set.  It was caused by an error in BN_div
	 * with negatives.  There was also another problem when for a^b%m
	 * a >= m.  eay 07-May-97
	 */
/*	if ((m->d[m->top-1]&BN_TBIT) && BN_is_odd(m)) */

	if (BN_is_odd(m)) {
#ifdef MONT_EXP_WORD
		if (a->top == 1 && !a->neg) {
			BN_ULONG A = a->d[0];
			ret = BN_mod_exp_mont_word(r, A, p, m, ctx, NULL);
		} else
#endif
			ret = BN_mod_exp_mont(r, a, p, m, ctx, NULL);
	} else
#endif	/* MONT_MUL_MOD */
#ifdef RECP_MUL_MOD
	{
		ret = BN_mod_exp_recp(r, a, p, m, ctx);
	}
#else
	{
		ret = BN_mod_exp_simple(r, a, p, m, ctx);
	}
#endif

	return (ret);
}


int
BN_mod_exp_recp(BIGNUM *r, const BIGNUM *a, const BIGNUM *p, const BIGNUM *m, BN_CTX *ctx)
{
	int i, j, bits, ret = 0, wstart, wend, window, wvalue;
	int start = 1, ts = 0;
	BIGNUM *aa;
	BIGNUM val[TABLE_SIZE];
	BN_RECP_CTX recp;

	bits = BN_num_bits(p);

	if (bits == 0) {
		ret = BN_one(r);
		return ret;
	}

	BN_CTX_start(ctx);
	if ((aa = BN_CTX_get(ctx)) == NULL) goto err;

	BN_RECP_CTX_init(&recp);
	if (m->neg) {
		/* ignore sign of 'm' */
		if (!BN_copy(aa, m)) goto err;
		aa->neg = 0;
		if (BN_RECP_CTX_set(&recp, aa, ctx) <= 0) goto err;
	} else {
		if (BN_RECP_CTX_set(&recp, m, ctx) <= 0) goto err;
	}

	BN_init(&(val[0]));
	ts = 1;

	if (!BN_nnmod(&(val[0]), a, m, ctx)) goto err;		/* 1 */
	if (BN_is_zero(&(val[0]))) {
		ret = BN_zero(r);
		goto err;
	}

	window = BN_window_bits_for_exponent_size(bits);
	if (window > 1) {
		if (!BN_mod_mul_reciprocal(aa, &(val[0]), &(val[0]), &recp, ctx))
			goto err;				/* 2 */
		j = 1 << (window - 1);
		for (i = 1; i < j; i++) {
			BN_init(&val[i]);
			if (!BN_mod_mul_reciprocal(&(val[i]), &(val[i - 1]), aa, &recp, ctx))
				goto err;
		}
		ts = i;
	}

	start = 1;	/* This is used to avoid multiplication etc
			 * when there is only the value '1' in the
			 * buffer.
			 */
	wvalue = 0;		/* The 'value' of the window */
	wstart = bits - 1;	/* The top bit of the window */
	wend = 0;		/* The bottom bit of the window */

	if (!BN_one(r)) goto err;

	for (;;) {
		if (BN_is_bit_set(p, wstart) == 0) {
			if (!start)
				if (!BN_mod_mul_reciprocal(r, r, r, &recp, ctx))
				goto err;
			if (wstart == 0) break;
			wstart--;
			continue;
		}
		/* We now have wstart on a 'set' bit, we now need to work out
		 * how bit a window to do.  To do this we need to scan
		 * forward until the last set bit before the end of the
		 * window
		 */
		j = wstart;
		wvalue = 1;
		wend = 0;
		for (i = 1; i < window; i++) {
			if (wstart - i < 0) break;
			if (BN_is_bit_set(p, wstart-i)) {
				wvalue <<= (i - wend);
				wvalue |= 1;
				wend = i;
			}
		}

		/* wend is the size of the current window */
		j = wend+1;
		/* add the 'bytes above' */
		if (!start)
			for (i = 0; i < j; i++) {
				if (!BN_mod_mul_reciprocal(r, r, r, &recp, ctx))
					goto err;
			}

		/* wvalue will be an odd number < 2^window */
		if (!BN_mod_mul_reciprocal(r, r, &(val[wvalue >> 1]), &recp, ctx))
			goto err;

		/* move the 'window' down further */
		wstart -= wend+1;
		wvalue = 0;
		start = 0;
		if (wstart < 0) break;
	}
	ret = 1;
err:
	BN_CTX_end(ctx);
	for (i = 0; i < ts; i++)
		BN_clear_free(&(val[i]));
	BN_RECP_CTX_free(&recp);
	return (ret);
}
#endif /* NOT_NEEDED_FOR_DH */


int
BN_mod_exp_mont(BIGNUM *rr, const BIGNUM *a, const BIGNUM *p,
                const BIGNUM *m, BN_CTX *ctx, BN_MONT_CTX *in_mont)
{
	int i, j, bits, ret = 0, wstart, wend, window, wvalue;
	int start = 1, ts = 0;
	BIGNUM *d, *r;
	const BIGNUM *aa;
	BIGNUM val[TABLE_SIZE];
	BN_MONT_CTX *mont = NULL;

	bn_check_top(a);
	bn_check_top(p);
	bn_check_top(m);

	if (!(m->d[0] & 1)) {
		BNerr(BN_F_BN_MOD_EXP_MONT, BN_R_CALLED_WITH_EVEN_MODULUS);
		return (0);
	}
	bits = BN_num_bits(p);
	if (bits == 0) {
		ret = BN_one(rr);
		return ret;
	}

	BN_CTX_start(ctx);
	d = BN_CTX_get(ctx);
	r = BN_CTX_get(ctx);
	if (d == NULL || r == NULL) goto err;

	/* If this is not done, things will break in the montgomery part */

	if (in_mont != NULL)
		mont = in_mont;
	else {
		if ((mont = BN_MONT_CTX_new()) == NULL) goto err;
		if (!BN_MONT_CTX_set(mont, m, ctx)) goto err;
	}

	BN_init(&val[0]);
	ts = 1;
	if (a->neg || BN_ucmp(a, m) >= 0) {
		if (!BN_nnmod(&(val[0]), a, m, ctx))
			goto err;
		aa = &(val[0]);
	} else
		aa = a;
	if (BN_is_zero(aa)) {
		ret = BN_zero(rr);
		goto err;
	}
	if (!BN_to_montgomery(&(val[0]), aa, mont, ctx)) goto err; /* 1 */

	window = BN_window_bits_for_exponent_size(bits);
	if (window > 1) {
		if (!BN_mod_mul_montgomery(d, &(val[0]), &(val[0]), mont, ctx)) goto err; /* 2 */
		j = 1 << (window - 1);
		for (i = 1; i < j; i++) {
			BN_init(&(val[i]));
			if (!BN_mod_mul_montgomery(&(val[i]), &(val[i - 1]), d, mont, ctx))
				goto err;
		}
		ts = i;
	}

	start = 1;	/* This is used to avoid multiplication etc
			 * when there is only the value '1' in the
			 * buffer.
			 */
	wvalue = 0;		/* The 'value' of the window */
	wstart = bits-1;	/* The top bit of the window */
	wend = 0;		/* The bottom bit of the window */

	if (!BN_to_montgomery(r, BN_value_one(), mont, ctx)) goto err;
	for (;;) {
		if (BN_is_bit_set(p, wstart) == 0) {
			if (!start) {
				if (!BN_mod_mul_montgomery(r, r, r, mont, ctx))
				goto err;
			}
			if (wstart == 0) break;
			wstart--;
			continue;
		}
		/* We now have wstart on a 'set' bit, we now need to work out
		 * how bit a window to do.  To do this we need to scan
		 * forward until the last set bit before the end of the
		 * window
		 */
		j = wstart;
		wvalue = 1;
		wend = 0;
		for (i = 1; i < window; i++) {
			if (wstart - i < 0) break;
			if (BN_is_bit_set(p, wstart - i)) {
				wvalue <<= (i - wend);
				wvalue |= 1;
				wend = i;
			}
		}

		/* wend is the size of the current window */
		j = wend + 1;
		/* add the 'bytes above' */
		if (!start)
			for (i = 0; i < j; i++) {
				if (!BN_mod_mul_montgomery(r, r, r, mont, ctx))
					goto err;
			}

		/* wvalue will be an odd number < 2^window */
		if (!BN_mod_mul_montgomery(r, r, &(val[wvalue >> 1]), mont, ctx))
			goto err;

		/* move the 'window' down further */
		wstart -= wend+1;
		wvalue = 0;
		start = 0;
		if (wstart < 0) break;
	}
	if (!BN_from_montgomery(rr, r, mont, ctx)) goto err;
	ret = 1;
err:
	if ((in_mont == NULL) && (mont != NULL)) BN_MONT_CTX_free(mont);
	BN_CTX_end(ctx);
	for (i = 0; i < ts; i++)
		BN_clear_free(&(val[i]));
	return (ret);
}

int
BN_mod_exp_mont_word(BIGNUM *rr, BN_ULONG a, const BIGNUM *p,
                     const BIGNUM *m, BN_CTX *ctx, BN_MONT_CTX *in_mont)
{
	BN_MONT_CTX *mont = NULL;
	int b, bits, ret = 0;
	int r_is_one;
	BN_ULONG w, next_w;
	BIGNUM *d, *r, *t;
	BIGNUM *swap_tmp;
#define BN_MOD_MUL_WORD(r, w, m) \
		(BN_mul_word(r, (w)) && \
		(/* BN_ucmp(r, (m)) < 0 ? 1 : */  \
			(BN_mod(t, r, m, ctx) && (swap_tmp = r, r = t, t = swap_tmp, 1))))
		/* BN_MOD_MUL_WORD is only used with 'w' large,
		 * so the BN_ucmp test is probably more overhead
		 * than always using BN_mod (which uses BN_copy if
		 * a similar test returns true).
		 */
		/* We can use BN_mod and do not need BN_nnmod because our
		 * accumulator is never negative (the result of BN_mod does
		 * not depend on the sign of the modulus).
		 */
#define BN_TO_MONTGOMERY_WORD(r, w, mont) \
		(BN_set_word(r, (w)) && BN_to_montgomery(r, r, (mont), ctx))

	bn_check_top(p);
	bn_check_top(m);

	if (m->top == 0 || !(m->d[0] & 1)) {
		BNerr(BN_F_BN_MOD_EXP_MONT_WORD, BN_R_CALLED_WITH_EVEN_MODULUS);
		return (0);
	}
	if (m->top == 1)
		a %= m->d[0]; /* make sure that 'a' is reduced */

	bits = BN_num_bits(p);
	if (bits == 0) {
		ret = BN_one(rr);
		return ret;
	}
	if (a == 0) {
		ret = BN_zero(rr);
		return ret;
	}

	BN_CTX_start(ctx);
	d = BN_CTX_get(ctx);
	r = BN_CTX_get(ctx);
	t = BN_CTX_get(ctx);
	if (d == NULL || r == NULL || t == NULL) goto err;

	if (in_mont != NULL)
		mont = in_mont;
	else {
		if ((mont = BN_MONT_CTX_new()) == NULL) goto err;
		if (!BN_MONT_CTX_set(mont, m, ctx)) goto err;
	}

	r_is_one = 1; /* except for Montgomery factor */

	/* bits-1 >= 0 */

	/* The result is accumulated in the product r*w. */
	w = a; /* bit 'bits-1' of 'p' is always set */
	for (b = bits-2; b >= 0; b--) {
		/* First, square r*w. */
		next_w = w*w;
		if ((next_w/w) != w) /* overflow */ {
			if (r_is_one) {
				if (!BN_TO_MONTGOMERY_WORD(r, w, mont)) goto err;
				r_is_one = 0;
			} else {
				if (!BN_MOD_MUL_WORD(r, w, m)) goto err;
			}
			next_w = 1;
		}
		w = next_w;
		if (!r_is_one) {
			if (!BN_mod_mul_montgomery(r, r, r, mont, ctx)) goto err;
		}

		/* Second, multiply r*w by 'a' if exponent bit is set. */
		if (BN_is_bit_set(p, b)) {
			next_w = w*a;
			if ((next_w/a) != w) /* overflow */ {
				if (r_is_one) {
					if (!BN_TO_MONTGOMERY_WORD(r, w, mont)) goto err;
					r_is_one = 0;
				} else {
					if (!BN_MOD_MUL_WORD(r, w, m)) goto err;
				}
				next_w = a;
			}
			w = next_w;
		}
	}

	/* Finally, set r: = r*w. */
	if (w != 1) {
		if (r_is_one) {
			if (!BN_TO_MONTGOMERY_WORD(r, w, mont)) goto err;
			r_is_one = 0;
		} else {
			if (!BN_MOD_MUL_WORD(r, w, m)) goto err;
		}
	}

	if (r_is_one) /* can happen only if a == 1 */ {
		if (!BN_one(rr)) goto err;
	} else {
		if (!BN_from_montgomery(rr, r, mont, ctx)) goto err;
	}
	ret = 1;
err:
	if ((in_mont == NULL) && (mont != NULL)) BN_MONT_CTX_free(mont);
	BN_CTX_end(ctx);
	return (ret);
}


#ifdef NOT_NEEDED_FOR_DH
/* The old fallback, simple version :-) */
int
BN_mod_exp_simple(BIGNUM *r, const BIGNUM *a, const BIGNUM *p, const BIGNUM *m, BN_CTX *ctx)
{
	int i, j, bits, ret = 0, wstart, wend, window, wvalue, ts = 0;
	int start = 1;
	BIGNUM *d;
	BIGNUM val[TABLE_SIZE];

	bits = BN_num_bits(p);

	if (bits == 0) {
		ret = BN_one(r);
		return ret;
	}

	BN_CTX_start(ctx);
	if ((d = BN_CTX_get(ctx)) == NULL) goto err;

	BN_init(&(val[0]));
	ts = 1;
	if (!BN_nnmod(&(val[0]), a, m, ctx)) goto err;		/* 1 */
	if (BN_is_zero(&(val[0]))) {
		ret = BN_zero(r);
		goto err;
	}

	window = BN_window_bits_for_exponent_size(bits);
	if (window > 1) {
		if (!BN_mod_mul(d, &(val[0]), &(val[0]), m, ctx))
			goto err;				/* 2 */
		j = 1 << (window - 1);
		for (i = 1; i < j; i++) {
			BN_init(&(val[i]));
			if (!BN_mod_mul(&(val[i]), &(val[i - 1]), d, m, ctx))
				goto err;
		}
		ts = i;
	}

	start = 1;	/* This is used to avoid multiplication etc
			 * when there is only the value '1' in the
			 * buffer.
			 */
	wvalue = 0;		/* The 'value' of the window */
	wstart = bits-1;	/* The top bit of the window */
	wend = 0;		/* The bottom bit of the window */

	if (!BN_one(r)) goto err;

	for (;;) {
		if (BN_is_bit_set(p, wstart) == 0) {
			if (!start)
				if (!BN_mod_mul(r, r, r, m, ctx))
				goto err;
			if (wstart == 0) break;
			wstart--;
			continue;
		}
		/* We now have wstart on a 'set' bit, we now need to work out
		 * how bit a window to do.  To do this we need to scan
		 * forward until the last set bit before the end of the
		 * window
		 */
		j = wstart;
		wvalue = 1;
		wend = 0;
		for (i = 1; i < window; i++) {
			if (wstart - i < 0) break;
			if (BN_is_bit_set(p, wstart - i)) {
				wvalue <<= (i - wend);
				wvalue |= 1;
				wend = i;
			}
		}

		/* wend is the size of the current window */
		j = wend+1;
		/* add the 'bytes above' */
		if (!start)
			for (i = 0; i < j; i++) {
				if (!BN_mod_mul(r, r, r, m, ctx))
					goto err;
			}

		/* wvalue will be an odd number < 2^window */
		if (!BN_mod_mul(r, r, &(val[wvalue >> 1]), m, ctx))
			goto err;

		/* move the 'window' down further */
		wstart -= wend+1;
		wvalue = 0;
		start = 0;
		if (wstart < 0) break;
	}
	ret = 1;
err:
	BN_CTX_end(ctx);
	for (i = 0; i < ts; i++)
		BN_clear_free(&(val[i]));
	return (ret);
}

static BIGNUM *euclid(BIGNUM *a, BIGNUM *b);

int
BN_gcd(BIGNUM *r, const BIGNUM *in_a, const BIGNUM *in_b, BN_CTX *ctx)
{
	BIGNUM *a, *b, *t;
	int ret = 0;

	bn_check_top(in_a);
	bn_check_top(in_b);

	BN_CTX_start(ctx);
	a = BN_CTX_get(ctx);
	b = BN_CTX_get(ctx);
	if (a == NULL || b == NULL) goto err;

	if (BN_copy(a, in_a) == NULL) goto err;
	if (BN_copy(b, in_b) == NULL) goto err;
	a->neg = 0;
	b->neg = 0;

	if (BN_cmp(a, b) < 0) { t = a; a = b; b = t; }
	t = euclid(a, b);
	if (t == NULL) goto err;

	if (BN_copy(r, t) == NULL) goto err;
	ret = 1;
err:
	BN_CTX_end(ctx);
	return (ret);
}

static BIGNUM *
euclid(BIGNUM *a, BIGNUM *b)
{
	BIGNUM *t;
	int shifts = 0;

	bn_check_top(a);
	bn_check_top(b);

	/* 0 <= b <= a */
	while (!BN_is_zero(b)) {
		/* 0 < b <= a */

		if (BN_is_odd(a)) {
			if (BN_is_odd(b)) {
				if (!BN_sub(a, a, b)) goto err;
				if (!BN_rshift1(a, a)) goto err;
				if (BN_cmp(a, b) < 0) { t = a; a = b; b = t; }
			} else /* a odd - b even */ {
				if (!BN_rshift1(b, b)) goto err;
				if (BN_cmp(a, b) < 0) { t = a; a = b; b = t; }
			}
		} else /* a is even */ {
			if (BN_is_odd(b)) {
				if (!BN_rshift1(a, a)) goto err;
				if (BN_cmp(a, b) < 0) { t = a; a = b; b = t; }
			} else /* a even - b even */ {
				if (!BN_rshift1(a, a)) goto err;
				if (!BN_rshift1(b, b)) goto err;
				shifts++;
			}
		}
		/* 0 <= b <= a */
	}

	if (shifts) {
		if (!BN_lshift(a, a, shifts)) goto err;
	}
	return (a);
err:
	return (NULL);
}
#endif /* NOT_NEEDED_FOR_DH */


/* solves ax == 1 (mod n) */
BIGNUM *
BN_mod_inverse(BIGNUM *in, const BIGNUM *a, const BIGNUM *n, BN_CTX *ctx)
{
	BIGNUM *A, *B, *X, *Y, *M, *D, *T, *R = NULL;
	BIGNUM *ret = NULL;
	int sign;

	bn_check_top(a);
	bn_check_top(n);

	BN_CTX_start(ctx);
	A = BN_CTX_get(ctx);
	B = BN_CTX_get(ctx);
	X = BN_CTX_get(ctx);
	D = BN_CTX_get(ctx);
	M = BN_CTX_get(ctx);
	Y = BN_CTX_get(ctx);
	T = BN_CTX_get(ctx);
	if (T == NULL) goto err;

	if (in == NULL)
		R = BN_new();
	else
		R = in;
	if (R == NULL) goto err;

	BN_one(X);
	BN_zero(Y);
	if (BN_copy(B, a) == NULL) goto err;
	if (BN_copy(A, n) == NULL) goto err;
	A->neg = 0;
	if (B->neg || (BN_ucmp(B, A) >= 0)) {
		if (!BN_nnmod(B, B, A, ctx)) goto err;
	}
	sign = -1;
	/* From  B = a mod |n|, A = |n|  it follows that
	 *
	 *      0 <= B < A,
	 *     -sign*X*a  ==  B   (mod |n|),
	 *      sign*Y*a  ==  A   (mod |n|).
	 */

	if (BN_is_odd(n) && (BN_num_bits(n) <= (BN_BITS <= 32 ? 450 : 2048))) {
		/* Binary inversion algorithm; requires odd modulus.
		 * This is faster than the general algorithm if the modulus
		 * is sufficiently small (about 400 .. 500 bits on 32-bit
		 * sytems, but much more on 64-bit systems)
		 */
		int shift;

		while (!BN_is_zero(B)) {
			/*
			 *      0 < B < |n|,
			 *      0 < A <= |n|,
			 * (1) -sign*X*a  ==  B   (mod |n|),
			 * (2)  sign*Y*a  ==  A   (mod |n|)
			 */

			/* Now divide  B  by the maximum possible power of two in the integers,
			 * and divide  X  by the same value mod |n|.
			 * When we're done, (1) still holds.
			 */
			shift = 0;
			while (!BN_is_bit_set(B, shift)) /* note that 0 < B */ {
				shift++;

				if (BN_is_odd(X)) {
					if (!BN_uadd(X, X, n)) goto err;
				}
				/* now X is even, so we can easily divide it by two */
				if (!BN_rshift1(X, X)) goto err;
			}
			if (shift > 0) {
				if (!BN_rshift(B, B, shift)) goto err;
			}


			/* Same for  A  and  Y.  Afterwards, (2) still holds. */
			shift = 0;
			while (!BN_is_bit_set(A, shift)) /* note that 0 < A */ {
				shift++;

				if (BN_is_odd(Y)) {
					if (!BN_uadd(Y, Y, n)) goto err;
					}
				/* now Y is even */
				if (!BN_rshift1(Y, Y)) goto err;
			}
			if (shift > 0) {
				if (!BN_rshift(A, A, shift)) goto err;
			}

			/* We still have (1) and (2).
			 * Both  A  and  B  are odd.
			 * The following computations ensure that
			 *
			 *     0 <= B < |n|,
			 *      0 < A < |n|,
			 * (1) -sign*X*a  ==  B   (mod |n|),
			 * (2)  sign*Y*a  ==  A   (mod |n|),
			 *
			 * and that either  A  or  B  is even in the next iteration.
			 */
			if (BN_ucmp(B, A) >= 0) {
				/* -sign*(X + Y)*a == B - A  (mod |n|) */
				if (!BN_uadd(X, X, Y)) goto err;
				/* NB: we could use BN_mod_add_quick(X, X, Y, n), but that
				 * actually makes the algorithm slower
				 */
				if (!BN_usub(B, B, A)) goto err;
			} else {
				/*  sign*(X + Y)*a == A - B  (mod |n|) */
				if (!BN_uadd(Y, Y, X)) goto err;
				/* as above, BN_mod_add_quick(Y, Y, X, n) would slow things down */
				if (!BN_usub(A, A, B)) goto err;
			}
		}
	} else {
		/* general inversion algorithm */

		while (!BN_is_zero(B)) {
			BIGNUM *tmp;

			/*
			 *      0 < B < A,
			 * (*) -sign*X*a  ==  B   (mod |n|),
			 *      sign*Y*a  ==  A   (mod |n|)
			 */

			/* (D, M) : = (A/B, A%B) ... */
			if (BN_num_bits(A) == BN_num_bits(B)) {
				if (!BN_one(D)) goto err;
				if (!BN_sub(M, A, B)) goto err;
			} else if (BN_num_bits(A) == BN_num_bits(B) + 1) {
				/* A/B is 1, 2, or 3 */
				if (!BN_lshift1(T, B)) goto err;
				if (BN_ucmp(A, T) < 0) {
					/* A < 2*B, so D = 1 */
					if (!BN_one(D)) goto err;
					if (!BN_sub(M, A, B)) goto err;
				} else {
					/* A >= 2*B, so D = 2 or D = 3 */
					if (!BN_sub(M, A, T)) goto err;
					if (!BN_add(D, T, B)) goto err;
					/* use D ( := 3 * B) as temp */
					if (BN_ucmp(A, D) < 0) {
						/* A < 3*B, so D = 2 */
						if (!BN_set_word(D, 2)) goto err;
						/* M ( = A - 2*B) already has the correct value */
					} else {
						/* only D = 3 remains */
						if (!BN_set_word(D, 3)) goto err;
						/* currently  M = A - 2 * B,
						 * but we need  M = A - 3 * B
						 */
						if (!BN_sub(M, M, B)) goto err;
					}
				}
			} else {
				if (!BN_div(D, M, A, B, ctx)) goto err;
			}

			/* Now
			 *      A = D*B + M;
			 * thus we have
			 * (**)  sign*Y*a  ==  D*B + M   (mod |n|).
			 */

			tmp = A; /* keep the BIGNUM object, the value does not matter */

			/* (A, B) : = (B, A mod B) ... */
			A = B;
			B = M;
			/* ... so we have  0 <= B < A  again */

			/* Since the former  M  is now  B  and the former  B  is now  A,
			 * (**) translates into
			 *       sign*Y*a  ==  D*A + B    (mod |n|),
			 * i.e.
			 *       sign*Y*a - D*A  ==  B    (mod |n|).
			 * Similarly, (*) translates into
			 *      -sign*X*a  ==  A          (mod |n|).
			 *
			 * Thus,
			 *   sign*Y*a + D*sign*X*a  ==  B  (mod |n|),
			 * i.e.
			 *        sign*(Y + D*X)*a  ==  B  (mod |n|).
			 *
			 * So if we set  (X, Y, sign) : = (Y + D*X, X, -sign), we arrive back at
			 *      -sign*X*a  ==  B   (mod |n|),
			 *       sign*Y*a  ==  A   (mod |n|).
			 * Note that  X  and  Y  stay non-negative all the time.
			 */

			/* most of the time D is very small, so we can optimize tmp : = D*X+Y */
			if (BN_is_one(D)) {
				if (!BN_add(tmp, X, Y)) goto err;
			} else {
				if (BN_is_word(D, 2)) {
					if (!BN_lshift1(tmp, X)) goto err;
				} else if (BN_is_word(D, 4)) {
					if (!BN_lshift(tmp, X, 2)) goto err;
				} else if (D->top == 1) {
					if (!BN_copy(tmp, X)) goto err;
					if (!BN_mul_word(tmp, D->d[0])) goto err;
				} else {
					if (!BN_mul(tmp, D, X, ctx)) goto err;
				}
				if (!BN_add(tmp, tmp, Y)) goto err;
			}

			M = Y; /* keep the BIGNUM object, the value does not matter */
			Y = X;
			X = tmp;
			sign = -sign;
		}
	}

	/*
	 * The while loop (Euclid's algorithm) ends when
	 *      A == gcd(a, n);
	 * we have
	 *       sign*Y*a  ==  A  (mod |n|),
	 * where  Y  is non-negative.
	 */

	if (sign < 0) {
		if (!BN_sub(Y, n, Y)) goto err;
	}
	/* Now  Y*a  ==  A  (mod |n|).  */


	if (BN_is_one(A)) {
		/* Y*a == 1  (mod |n|) */
		if (!Y->neg && BN_ucmp(Y, n) < 0) {
			if (!BN_copy(R, Y)) goto err;
		} else {
			if (!BN_nnmod(R, Y, n, ctx)) goto err;
		}
	} else {
		BNerr(BN_F_BN_MOD_INVERSE, BN_R_NO_INVERSE);
		goto err;
	}
	ret = R;
err:
	if ((ret == NULL) && (in == NULL)) BN_free(R);
	BN_CTX_end(ctx);
	return (ret);
}

#include <limits.h>

#ifdef NOT_NEEDED_FOR_DH
/* For a 32 bit machine
 * 2 -   4 ==  128
 * 3 -   8 ==  256
 * 4 -  16 ==  512
 * 5 -  32 == 1024
 * 6 -  64 == 2048
 * 7 - 128 == 4096
 * 8 - 256 == 8192
 */
static int bn_limit_bits = 0;
static int bn_limit_num = 8;        /* (1 << bn_limit_bits) */
static int bn_limit_bits_low = 0;
static int bn_limit_num_low = 8;    /* (1 << bn_limit_bits_low) */
static int bn_limit_bits_high = 0;
static int bn_limit_num_high = 8;   /* (1 << bn_limit_bits_high) */
static int bn_limit_bits_mont = 0;
static int bn_limit_num_mont = 8;   /* (1 << bn_limit_bits_mont) */

void
BN_set_params(int mult, int high, int low, int mont)
{
	if (mult >= 0) {
		if (mult > (sizeof(int)*8)-1)
			mult = sizeof(int)*8-1;
		bn_limit_bits = mult;
		bn_limit_num = 1 << mult;
	}
	if (high >= 0) {
		if (high > (sizeof(int)*8)-1)
			high = sizeof(int)*8-1;
		bn_limit_bits_high = high;
		bn_limit_num_high = 1 << high;
	}
	if (low >= 0) {
		if (low > (sizeof(int)*8)-1)
			low = sizeof(int)*8-1;
		bn_limit_bits_low = low;
		bn_limit_num_low = 1 << low;
	}
	if (mont >= 0) {
		if (mont > (sizeof(int)*8)-1)
			mont = sizeof(int)*8-1;
		bn_limit_bits_mont = mont;
		bn_limit_num_mont = 1 << mont;
	}
}

int
BN_get_params(int which)
{
	if      (which == 0) return (bn_limit_bits);
	else if (which == 1) return (bn_limit_bits_high);
	else if (which == 2) return (bn_limit_bits_low);
	else if (which == 3) return (bn_limit_bits_mont);
	else return (0);
}

char *
BN_options(void)
{
	static int init = 0;
	static char data[16];

	if (!init) {
		init++;
#ifdef BN_LLONG
		sprintf(data, "bn(%d, %d)", (int)sizeof(BN_ULLONG)*8,
			(int)sizeof(BN_ULONG)*8);
#else
		sprintf(data, "bn(%d, %d)", (int)sizeof(BN_ULONG)*8,
			(int)sizeof(BN_ULONG)*8);
#endif
	}
	return (data);
}
#endif /* NOT_NEEDED_FOR_DH */

const BIGNUM *
BN_value_one(void)
{
	static BN_ULONG data_one = 1L;
	static BIGNUM const_one = {&data_one, 1, 1, 0};

	return (&const_one);
}

int
BN_num_bits_word(BN_ULONG l)
{
	static const char bits[256] = {
		0, 1, 2, 2, 3, 3, 3, 3, 4, 4, 4, 4, 4, 4, 4, 4,
		5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5,
		6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6,
		6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6,
		7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7,
		7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7,
		7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7,
		7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7,
		8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8,
		8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8,
		8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8,
		8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8,
		8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8,
		8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8,
		8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8,
		8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8
		};

#if defined(SIXTY_FOUR_BIT_LONG)
	if (l & 0xffffffff00000000L) {
		if (l & 0xffff000000000000L) {
			if (l & 0xff00000000000000L) {
				return (bits[(int)(l >> 56)] + 56);
			} else
				return (bits[(int)(l >> 48)] + 48);
		} else {
			if (l & 0x0000ff0000000000L) {
				return (bits[(int)(l >> 40)] + 40);
			} else
				return (bits[(int)(l >> 32)] + 32);
		}
	} else
#else
#ifdef SIXTY_FOUR_BIT
		if (l & 0xffffffff00000000LL) {
			if (l & 0xffff000000000000LL) {
				if (l & 0xff00000000000000LL) {
					return (bits[(int)(l >> 56)] + 56);
				} else
					return (bits[(int)(l >> 48)] + 48);
			} else {
				if (l & 0x0000ff0000000000LL) {
					return (bits[(int)(l >> 40)] + 40);
				} else
					return (bits[(int)(l >> 32)] + 32);
			}
		} else
#endif
#endif	/* SIXTY_FOUR_BIT_LONG */
		{
#if defined(THIRTY_TWO_BIT) || defined(SIXTY_FOUR_BIT) || defined(SIXTY_FOUR_BIT_LONG)
			if (l & 0xffff0000L) {
				if (l & 0xff000000L)
					return (bits[(int)(l >> 24L)] + 24);
				else
					return (bits[(int)(l >> 16L)] + 16);
			} else
#endif
			{
#if defined(SIXTEEN_BIT) || defined(THIRTY_TWO_BIT) || defined(SIXTY_FOUR_BIT) || \
	defined(SIXTY_FOUR_BIT_LONG)
				if (l & 0xff00L)
					return (bits[(int)(l >> 8)] + 8);
				else
#endif
					return (bits[(int)(l)]);
			}
		}
}

int
BN_num_bits(const BIGNUM *a)
{
	BN_ULONG l;
	int i;

	bn_check_top(a);

	if (a->top == 0) return (0);
	l = a->d[a->top-1];
	assert(l != 0);
	i = (a->top-1)*BN_BITS2;
	return (i+BN_num_bits_word(l));
}

void
BN_clear_free(BIGNUM *a)
{
	int i;

	if (a == NULL) return;
	if (a->d != NULL) {
		OPENSSL_cleanse(a->d, a->dmax*sizeof(a->d[0]));
		if (!(BN_get_flags(a, BN_FLG_STATIC_DATA)))
			OPENSSL_free(a->d);
	}
	i = BN_get_flags(a, BN_FLG_MALLOCED);
	OPENSSL_cleanse(a, sizeof(BIGNUM));
	if (i)
		OPENSSL_free(a);
}

void
BN_free(BIGNUM *a)
{
	if (a == NULL) return;
	if ((a->d != NULL) && !(BN_get_flags(a, BN_FLG_STATIC_DATA)))
		OPENSSL_free(a->d);
	a->flags |= BN_FLG_FREE; /* REMOVE? */
	if (a->flags & BN_FLG_MALLOCED)
		OPENSSL_free(a);
}

void
BN_init(BIGNUM *a)
{
	memset(a, 0, sizeof(BIGNUM));
}

BIGNUM *
BN_new(void)
{
	BIGNUM *ret;

	if ((ret = (BIGNUM *)OPENSSL_malloc(sizeof(BIGNUM))) == NULL) {
		BNerr(BN_F_BN_NEW, ERR_R_MALLOC_FAILURE);
		return (NULL);
	}
	ret->flags = BN_FLG_MALLOCED;
	ret->top = 0;
	ret->neg = 0;
	ret->dmax = 0;
	ret->d = NULL;
	return (ret);
}

/* This is used both by bn_expand2() and bn_dup_expand() */
/* The caller MUST check that words > b->dmax before calling this */
static BN_ULONG *
bn_expand_internal(const BIGNUM *b, int words)
{
	BN_ULONG *A, *a = NULL;
	const BN_ULONG *B;
	int i;

	if (words > (INT_MAX/(4*BN_BITS2))) {
		BNerr(BN_F_BN_EXPAND_INTERNAL, BN_R_BIGNUM_TOO_LONG);
		return NULL;
	}

	bn_check_top(b);
	if (BN_get_flags(b, BN_FLG_STATIC_DATA)) {
		BNerr(BN_F_BN_EXPAND_INTERNAL, BN_R_EXPAND_ON_STATIC_BIGNUM_DATA);
		return (NULL);
	}
	a = A = (BN_ULONG *)OPENSSL_malloc(sizeof(BN_ULONG)*(words+1));
	if (A == NULL) {
		BNerr(BN_F_BN_EXPAND_INTERNAL, ERR_R_MALLOC_FAILURE);
		return (NULL);
	}
	B = b->d;
	/* Check if the previous number needs to be copied */
	if (B != NULL) {
		for (i = b->top >> 2; i > 0; i--, A += 4, B += 4) {
			/*
			 * The fact that the loop is unrolled
			 * 4-wise is a tribute to Intel. It's
			 * the one that doesn't have enough
			 * registers to accomodate more data.
			 * I'd unroll it 8-wise otherwise:-)
			 *
			 *		<appro@fy.chalmers.se>
			 */
			BN_ULONG a0, a1, a2, a3;
			a0 = B[0]; a1 = B[1]; a2 = B[2]; a3 = B[3];
			A[0] = a0; A[1] = a1; A[2] = a2; A[3] = a3;
		}
		switch (b->top&3) {
		case 3:	A[2] = B[2];
		case 2:	A[1] = B[1];
		case 1:	A[0] = B[0];
		case 0:
			;
		}
	}

	/* Now need to zero any data between b->top and b->max */

	A = &(a[b->top]);
	for (i = (words - b->top) >> 3; i > 0; i--, A += 8) {
		A[0] = 0; A[1] = 0; A[2] = 0; A[3] = 0;
		A[4] = 0; A[5] = 0; A[6] = 0; A[7] = 0;
		}
	for (i = (words - b->top) & 7; i > 0; i--, A++)
		A[0] = 0;

	return (a);
}

#ifdef NOT_NEEDED_FOR_DH
/* This is an internal function that can be used instead of bn_expand2()
 * when there is a need to copy BIGNUMs instead of only expanding the
 * data part, while still expanding them.
 * Especially useful when needing to expand BIGNUMs that are declared
 * 'const' and should therefore not be changed.
 * The reason to use this instead of a BN_dup() followed by a bn_expand2()
 * is memory allocation overhead.  A BN_dup() followed by a bn_expand2()
 * will allocate new memory for the BIGNUM data twice, and free it once,
 * while bn_dup_expand() makes sure allocation is made only once.
 */

BIGNUM *
bn_dup_expand(const BIGNUM *b, int words)
{
	BIGNUM *r = NULL;

	/* This function does not work if
	 *      words <= b->dmax && top < words
	 * because BN_dup() does not preserve 'dmax'!
	 * (But bn_dup_expand() is not used anywhere yet.)
	 */

	if (words > b->dmax) {
		BN_ULONG *a = bn_expand_internal(b, words);

		if (a) {
			r = BN_new();
			if (r) {
				r->top = b->top;
				r->dmax = words;
				r->neg = b->neg;
				r->d = a;
			} else {
				/* r == NULL, BN_new failure */
				OPENSSL_free(a);
			}
		}
		/* If a == NULL, there was an error in allocation in
		 * bn_expand_internal(), and NULL should be returned
		 */
	} else {
		r = BN_dup(b);
	}

	return r;
}
#endif /* NOT_NEEDED_FOR_DH */

/* This is an internal function that should not be used in applications.
 * It ensures that 'b' has enough room for a 'words' word number number.
 * It is mostly used by the various BIGNUM routines. If there is an error,
 * NULL is returned. If not, 'b' is returned.
 */

BIGNUM *
bn_expand2(BIGNUM *b, int words)
{
	if (words > b->dmax) {
		BN_ULONG *a = bn_expand_internal(b, words);

		if (a) {
			if (b->d)
				OPENSSL_free(b->d);
			b->d = a;
			b->dmax = words;
		} else
			b = NULL;
	}
	return b;
}

#ifdef BCMDH_TEST
BIGNUM *
BN_dup(const BIGNUM *a)
{
	BIGNUM *r, *t;

	if (a == NULL) return NULL;

	bn_check_top(a);

	t = BN_new();
	if (t == NULL) return (NULL);
	r = BN_copy(t, a);
	/* now  r == t || r == NULL */
	if (r == NULL)
		BN_free(t);
	return r;
}
#endif /* BCMDH_TEST */

BIGNUM *
BN_copy(BIGNUM *a, const BIGNUM *b)
{
	int i;
	BN_ULONG *A;
	const BN_ULONG *B;

	bn_check_top(b);

	if (a == b) return (a);
	if (bn_wexpand(a, b->top) == NULL) return (NULL);

	A = a->d;
	B = b->d;
	for (i = b->top >> 2; i > 0; i--, A += 4, B += 4) {
		BN_ULONG a0, a1, a2, a3;
		a0 = B[0]; a1 = B[1]; a2 = B[2]; a3 = B[3];
		A[0] = a0; A[1] = a1; A[2] = a2; A[3] = a3;
	}
	switch (b->top & 3) {
		case 3: A[2] = B[2];
		case 2: A[1] = B[1];
		case 1: A[0] = B[0];
		case 0: ;
	}

/*	memset(&(a->d[b->top]), 0, sizeof(a->d[0])*(a->max-b->top)); */
	a->top = b->top;
	if ((a->top == 0) && (a->d != NULL))
		a->d[0] = 0;
	a->neg = b->neg;
	return (a);
}

#ifdef NOT_NEEDED_FOR_DH
void
BN_swap(BIGNUM *a, BIGNUM *b)
{
	int flags_old_a, flags_old_b;
	BN_ULONG *tmp_d;
	int tmp_top, tmp_dmax, tmp_neg;

	flags_old_a = a->flags;
	flags_old_b = b->flags;

	tmp_d = a->d;
	tmp_top = a->top;
	tmp_dmax = a->dmax;
	tmp_neg = a->neg;

	a->d = b->d;
	a->top = b->top;
	a->dmax = b->dmax;
	a->neg = b->neg;

	b->d = tmp_d;
	b->top = tmp_top;
	b->dmax = tmp_dmax;
	b->neg = tmp_neg;

	a->flags = (flags_old_a & BN_FLG_MALLOCED) | (flags_old_b & BN_FLG_STATIC_DATA);
	b->flags = (flags_old_b & BN_FLG_MALLOCED) | (flags_old_a & BN_FLG_STATIC_DATA);
}


void
BN_clear(BIGNUM *a)
{
	if (a->d != NULL)
		memset(a->d, 0, a->dmax*sizeof(a->d[0]));
	a->top = 0;
	a->neg = 0;
}

BN_ULONG
BN_get_word(const BIGNUM *a)
{
	int i, n;
	BN_ULONG ret = 0;

	n = BN_num_bytes(a);
	if (n > sizeof(BN_ULONG))
		return (BN_MASK2);
	for (i = a->top-1; i >= 0; i--) {
#ifndef SIXTY_FOUR_BIT /* the data item > unsigned long */
		ret <<= BN_BITS4; /* stops the compiler complaining */
		ret <<= BN_BITS4;
#else
		ret = 0;
#endif
		ret |= a->d[i];
	}
	return (ret);
}
#endif /* NOT_NEEDED_FOR_DH */

int
BN_set_word(BIGNUM *a, BN_ULONG w)
{
	int i, n;
	if (bn_expand(a, (int)sizeof(BN_ULONG)*8) == NULL) return (0);

	n = sizeof(BN_ULONG)/BN_BYTES;
	a->neg = 0;
	a->top = 0;
	a->d[0] = (BN_ULONG)w&BN_MASK2;
	if (a->d[0] != 0) a->top = 1;
	for (i = 1; i < n; i++) {
		/* the following is done instead of
		 * w >>= BN_BITS2 so compilers don't complain
		 * on builds where sizeof(long) == BN_TYPES
		 */
#ifndef SIXTY_FOUR_BIT /* the data item > unsigned long */
		w >>= BN_BITS4;
		w >>= BN_BITS4;
#else
		w = 0;
#endif
		a->d[i] = (BN_ULONG)w&BN_MASK2;
		if (a->d[i] != 0) a->top = i+1;
	}
	return (1);
}

BIGNUM *
BN_bin2bn(const unsigned char *s, int len, BIGNUM *ret)
{
	unsigned int i, m;
	unsigned int n;
	BN_ULONG l;

	if (ret == NULL) ret = BN_new();
	if (ret == NULL) return (NULL);
	l = 0;
	n = len;
	if (n == 0) {
		ret->top = 0;
		return (ret);
	}
	if (bn_expand(ret, (int)(n+2)*8) == NULL)
		return (NULL);
	i = ((n-1)/BN_BYTES)+1;
	m = ((n-1)%(BN_BYTES));
	ret->top = i;
	ret->neg = 0;
	while (n-- > 0) {
		l = (l << 8L)| *(s++);
		if (m-- == 0) {
			ret->d[--i] = l;
			l = 0;
			m = BN_BYTES-1;
		}
	}
	/* need to call this due to clear byte at top if avoiding
	 * having the top bit set (-ve number)
	 */
	bn_fix_top(ret);
	return (ret);
}

/* ignore negative */
int
BN_bn2bin(const BIGNUM *a, unsigned char *to)
{
	int n, i;
	BN_ULONG l;

	n = i = BN_num_bytes(a);
	while (i-- > 0) {
		l = a->d[i/BN_BYTES];
		*(to++) = (unsigned char)(l>>(8*(i%BN_BYTES)))&0xff;
		}
	return (n);
}

int
BN_ucmp(const BIGNUM *a, const BIGNUM *b)
{
	int i;
	BN_ULONG t1, t2, *ap, *bp;

	bn_check_top(a);
	bn_check_top(b);

	i = a->top-b->top;
	if (i != 0) return (i);
	ap = a->d;
	bp = b->d;
	for (i = a->top-1; i >= 0; i--) {
		t1 = ap[i];
		t2 = bp[i];
		if (t1 != t2)
			return (t1 > t2?1:-1);
	}
	return (0);
}

#ifdef BCMDH_TEST
int
BN_cmp(const BIGNUM *a, const BIGNUM *b)
{
	int i;
	int gt, lt;
	BN_ULONG t1, t2;

	if ((a == NULL) || (b == NULL)) {
		if (a != NULL)
			return (-1);
		else if (b != NULL)
			return (1);
		else
			return (0);
	}

	bn_check_top(a);
	bn_check_top(b);

	if (a->neg != b->neg) {
		if (a->neg)
			return (-1);
		else	return (1);
	}
	if (a->neg == 0) {
		gt = 1; lt = -1;
	} else {
		gt = -1; lt = 1;
	}

	if (a->top > b->top) return (gt);
	if (a->top < b->top) return (lt);
	for (i = a->top-1; i >= 0; i--) {
		t1 = a->d[i];
		t2 = b->d[i];
		if (t1 > t2) return (gt);
		if (t1 < t2) return (lt);
	}
	return (0);
}
#endif /* BCMDH_TEST */

int
BN_set_bit(BIGNUM *a, int n)
{
	int i, j, k;

	i = n/BN_BITS2;
	j = n%BN_BITS2;
	if (a->top <= i) {
		if (bn_wexpand(a, i + 1) == NULL) return (0);
		for (k = a->top; k < i + 1; k++)
			a->d[k] = 0;
		a->top = i+1;
	}

	a->d[i] |= (((BN_ULONG)1) << j);
	return (1);
}

#ifdef NOT_NEEDED_FOR_DH
int
BN_clear_bit(BIGNUM *a, int n)
{
	int i, j;

	i = n/BN_BITS2;
	j = n%BN_BITS2;
	if (a->top <= i) return (0);

	a->d[i] &= (~(((BN_ULONG)1) << j));
	bn_fix_top(a);
	return (1);
}
#endif /* NOT_NEEDED_FOR_DH */

int
BN_is_bit_set(const BIGNUM *a, int n)
{
	int i, j;

	if (n < 0) return (0);
	i = n/BN_BITS2;
	j = n%BN_BITS2;
	if (a->top <= i) return (0);
	return ((a->d[i]&(((BN_ULONG)1) << j)) ? 1 : 0);
}

#ifdef NOT_NEEDED_FOR_DH
int
BN_mask_bits(BIGNUM *a, int n)
{
	int b, w;

	w = n/BN_BITS2;
	b = n%BN_BITS2;
	if (w >= a->top) return (0);
	if (b == 0)
		a->top = w;
	else {
		a->top = w+1;
		a->d[w] &= ~(BN_MASK2 << b);
	}
	bn_fix_top(a);
	return (1);
}
#endif /* NOT_NEEDED_FOR_DH */

int
bn_cmp_words(const BN_ULONG *a, const BN_ULONG *b, int n)
{
	int i;
	BN_ULONG aa, bb;

	aa = a[n-1];
	bb = b[n-1];
	if (aa != bb) return ((aa > bb)?1:-1);
	for (i = n-2; i >= 0; i--) {
		aa = a[i];
		bb = b[i];
		if (aa != bb) return ((aa > bb)?1:-1);
	}
	return (0);
}

#ifdef NOT_NEEDED_FOR_DH
/* Here follows a specialised variants of bn_cmp_words().  It has the
 * property of performing the operation on arrays of different sizes.
 * The sizes of those arrays is expressed through cl, which is the
 * common length ( basicall, min(len(a), len(b)) ), and dl, which is the
 * delta between the two lengths, calculated as len(a)-len(b).
 * All lengths are the number of BN_ULONGs...
 */

int
bn_cmp_part_words(const BN_ULONG *a, const BN_ULONG *b, int cl, int dl)
{
	int n, i;
	n = cl-1;

	if (dl < 0) {
		for (i = dl; i < 0; i++) {
			if (b[n - i] != 0)
				return -1; /* a < b */
		}
	}
	if (dl > 0) {
		for (i = dl; i > 0; i--) {
			if (a[n + i] != 0)
				return 1; /* a > b */
		}
	}
	return bn_cmp_words(a, b, cl);
}
#endif /* NOT_NEEDED_FOR_DH */



int
BN_nnmod(BIGNUM *r, const BIGNUM *m, const BIGNUM *d, BN_CTX *ctx)
{
	/* like BN_mod, but returns non-negative remainder
	 * (i.e., 0 <= r < |d|  always holds)
	 */

	if (!(BN_mod(r, m, d, ctx)))
		return 0;
	if (!r->neg)
		return 1;
	/* now   -|d| < r < 0, so we have to set  r : = r + |d| */
	return (d->neg ? BN_sub : BN_add)(r, r, d);
}


#ifdef NOT_NEEDED_FOR_DH
int
BN_mod_add(BIGNUM *r, const BIGNUM *a, const BIGNUM *b, const BIGNUM *m, BN_CTX *ctx)
{
	if (!BN_add(r, a, b))
		return 0;
	return BN_nnmod(r, r, m, ctx);
}


/* BN_mod_add variant that may be used if both  a  and  b  are non-negative
 * and less than  m
 */
int
BN_mod_add_quick(BIGNUM *r, const BIGNUM *a, const BIGNUM *b, const BIGNUM *m)
{
	if (!BN_add(r, a, b)) return 0;
	if (BN_ucmp(r, m) >= 0)
		return BN_usub(r, r, m);
	return 1;
}

int
BN_mod_sub(BIGNUM *r, const BIGNUM *a, const BIGNUM *b, const BIGNUM *m, BN_CTX *ctx)
{
	if (!BN_sub(r, a, b)) return 0;
	return BN_nnmod(r, r, m, ctx);
}

/* BN_mod_sub variant that may be used if both  a  and  b  are non-negative
 * and less than  m
 */
int
BN_mod_sub_quick(BIGNUM *r, const BIGNUM *a, const BIGNUM *b, const BIGNUM *m)
{
	if (!BN_sub(r, a, b))
		return 0;
	if (r->neg)
		return BN_add(r, r, m);
	return 1;
}
#endif /* NOT_NEEDED_FOR_DH */

#ifdef BCMDH_TEST
/* slow but works */
int
BN_mod_mul(BIGNUM *r, const BIGNUM *a, const BIGNUM *b, const BIGNUM *m, BN_CTX *ctx)
{
	BIGNUM *t;
	int ret = 0;

	bn_check_top(a);
	bn_check_top(b);
	bn_check_top(m);

	BN_CTX_start(ctx);
	if ((t = BN_CTX_get(ctx)) == NULL) goto err;
	if (a == b) { if (!BN_sqr(t, a, ctx)) goto err; }
	else { if (!BN_mul(t, a, b, ctx)) goto err; }
	if (!BN_nnmod(r, t, m, ctx)) goto err;
	ret = 1;
err:
	BN_CTX_end(ctx);
	return (ret);
}
#endif /* BCMDH_TEST */


#ifdef NOT_NEEDED_FOR_DH
int
BN_mod_sqr(BIGNUM *r, const BIGNUM *a, const BIGNUM *m, BN_CTX *ctx)
{
	if (!BN_sqr(r, a, ctx)) return 0;
	/* r->neg == 0, thus we don't need BN_nnmod */
	return BN_mod(r, r, m, ctx);
}


int
BN_mod_lshift1(BIGNUM *r, const BIGNUM *a, const BIGNUM *m, BN_CTX *ctx)
{
	if (!BN_lshift1(r, a))
		return 0;
	return BN_nnmod(r, r, m, ctx);
}


/* BN_mod_lshift1 variant that may be used if  a  is non-negative
 * and less than  m
 */
int
BN_mod_lshift1_quick(BIGNUM *r, const BIGNUM *a, const BIGNUM *m)
{
	if (!BN_lshift1(r, a)) return 0;
	if (BN_cmp(r, m) >= 0)
		return BN_sub(r, r, m);
	return 1;
}


int
BN_mod_lshift(BIGNUM *r, const BIGNUM *a, int n, const BIGNUM *m, BN_CTX *ctx)
{
	BIGNUM *abs_m = NULL;
	int ret;

	if (!BN_nnmod(r, a, m, ctx))
		return 0;

	if (m->neg) {
		abs_m = BN_dup(m);
		if (abs_m == NULL) return 0;
		abs_m->neg = 0;
	}

	ret = BN_mod_lshift_quick(r, r, n, (abs_m ? abs_m : m));

	if (abs_m)
		BN_free(abs_m);
	return ret;
}


/* BN_mod_lshift variant that may be used if  a  is non-negative
 * and less than  m
 */
int
BN_mod_lshift_quick(BIGNUM *r, const BIGNUM *a, int n, const BIGNUM *m)
{
	if (r != a) {
		if (BN_copy(r, a) == NULL) return 0;
	}

	while (n > 0) {
		int max_shift;

		/* 0 < r < m */
		max_shift = BN_num_bits(m) - BN_num_bits(r);
		/* max_shift >= 0 */

		if (max_shift < 0) {
			BNerr(BN_F_BN_MOD_LSHIFT_QUICK, BN_R_INPUT_NOT_REDUCED);
			return 0;
		}

		if (max_shift > n)
			max_shift = n;

		if (max_shift) {
			if (!BN_lshift(r, r, max_shift)) return 0;
			n -= max_shift;
		} else {
			if (!BN_lshift1(r, r)) return 0;
			--n;
		}

		/* BN_num_bits(r) <= BN_num_bits(m) */

		if (BN_cmp(r, m) >= 0) {
			if (!BN_sub(r, r, m)) return 0;
		}
	}

	return 1;
}
#endif /* NOT_NEEDED_FOR_DH */

/*
 * Details about Montgomery multiplication algorithms can be found at
 * http://security.ece.orst.edu/publications.html, e.g.
 * http://security.ece.orst.edu/koc/papers/j37acmon.pdf and
 * sections 3.8 and 4.2 in http://security.ece.orst.edu/koc/papers/r01rsasw.pdf
 */


#define MONT_WORD /* use the faster word-based algorithm */

int
BN_mod_mul_montgomery(BIGNUM *r, const BIGNUM *a, const BIGNUM *b, BN_MONT_CTX *mont, BN_CTX *ctx)
{
	BIGNUM *tmp;
	int ret = 0;

	BN_CTX_start(ctx);
	tmp = BN_CTX_get(ctx);
	if (tmp == NULL) goto err;

	bn_check_top(tmp);
	if (a == b) {
		if (!BN_sqr(tmp, a, ctx)) goto err;
	} else {
		if (!BN_mul(tmp, a, b, ctx)) goto err;
	}
	/* reduce from aRR to aR */
	if (!BN_from_montgomery(r, tmp, mont, ctx)) goto err;
	ret = 1;
err:
	BN_CTX_end(ctx);
	return (ret);
}

int
BN_from_montgomery(BIGNUM *ret, const BIGNUM *a, BN_MONT_CTX *mont, BN_CTX *ctx)
{
	int retn = 0;

#ifdef MONT_WORD
	BIGNUM *n, *r;
	BN_ULONG *ap, *np, *rp, n0, v, *nrp;
	int al, nl, max, i, x, ri;

	BN_CTX_start(ctx);
	if ((r = BN_CTX_get(ctx)) == NULL) goto err;

	if (!BN_copy(r, a)) goto err;
	n = &(mont->N);

	ap = a->d;
	/* mont->ri is the size of mont->N in bits (rounded up
	 * to the word size)
	 */
	al = ri = mont->ri/BN_BITS2;

	nl = n->top;
	if ((al == 0) || (nl == 0)) {
		r->top = 0;
		return (1);
	}

	max = (nl + al + 1);
	if (bn_wexpand(r, max) == NULL)
		goto err;
	if (bn_wexpand(ret, max) == NULL)
		goto err;

	r->neg = a->neg^n->neg;
	np = n->d;
	rp = r->d;
	nrp = &(r->d[nl]);

	/* clear the top words of T */
	for (i = r->top; i < max; i++)
		r->d[i] = 0;

	r->top = max;
	n0 = mont->n0;

#ifdef BN_COUNT
	fprintf(stderr, "word BN_from_montgomery %d * %d\n", nl, nl);
#endif
	for (i = 0; i < nl; i++) {
#ifdef __TANDEM
		{
			long long t1;
			long long t2;
			long long t3;
			t1 = rp[0] * (n0 & 0177777);
			t2 = 037777600000l;
			t2 = n0 & t2;
			t3 = rp[0] & 0177777;
			t2 = (t3 * t2) & BN_MASK2;
			t1 = t1 + t2;
			v = bn_mul_add_words(rp, np, nl, (BN_ULONG) t1);
		}
#else
		v = bn_mul_add_words(rp, np, nl, (rp[0]*n0)&BN_MASK2);
#endif
		nrp++;
		rp++;
		if (((nrp[-1] += v)&BN_MASK2) >= v)
			continue;
		else {
			if (((++nrp[0])&BN_MASK2) != 0) continue;
			if (((++nrp[1])&BN_MASK2) != 0) continue;
			for (x = 2; (((++nrp[x]) & BN_MASK2) == 0); x++)
				;
		}
	}
	bn_fix_top(r);

	/* mont->ri will be a multiple of the word size */
	ret->neg = r->neg;
	x = ri;
	rp = ret->d;
	ap = &(r->d[x]);
	if (r->top < x)
		al = 0;
	else
		al = r->top-x;
	ret->top = al;
	al -= 4;
	for (i = 0; i < al; i += 4) {
		BN_ULONG t1, t2, t3, t4;

		t1 = ap[i+0];
		t2 = ap[i+1];
		t3 = ap[i+2];
		t4 = ap[i+3];
		rp[i+0] = t1;
		rp[i+1] = t2;
		rp[i+2] = t3;
		rp[i+3] = t4;
	}
	al += 4;
	for (; i < al; i++)
		rp[i] = ap[i];
#else /* !MONT_WORD */
	BIGNUM *t1, *t2;

	BN_CTX_start(ctx);
	t1 = BN_CTX_get(ctx);
	t2 = BN_CTX_get(ctx);
	if (t1 == NULL || t2 == NULL) goto err;

	if (!BN_copy(t1, a)) goto err;
	BN_mask_bits(t1, mont->ri);

	if (!BN_mul(t2, t1, &mont->Ni, ctx)) goto err;
	BN_mask_bits(t2, mont->ri);

	if (!BN_mul(t1, t2, &mont->N, ctx)) goto err;
	if (!BN_add(t2, a, t1)) goto err;
	if (!BN_rshift(ret, t2, mont->ri)) goto err;
#endif /* MONT_WORD */

	if (BN_ucmp(ret, &(mont->N)) >= 0) {
		if (!BN_usub(ret, ret, &(mont->N))) goto err;
	}
	retn = 1;
err:
	BN_CTX_end(ctx);
	return (retn);
}

BN_MONT_CTX *
BN_MONT_CTX_new(void)
{
	BN_MONT_CTX *ret;

	if ((ret = (BN_MONT_CTX *)OPENSSL_malloc(sizeof(BN_MONT_CTX))) == NULL)
		return (NULL);

	BN_MONT_CTX_init(ret);
	ret->flags = BN_FLG_MALLOCED;
	return (ret);
}

void
BN_MONT_CTX_init(BN_MONT_CTX *ctx)
{
	ctx->ri = 0;
	BN_init(&(ctx->RR));
	BN_init(&(ctx->N));
	BN_init(&(ctx->Ni));
	ctx->flags = 0;
}

void
BN_MONT_CTX_free(BN_MONT_CTX *mont)
{
	if (mont == NULL)
	    return;

	BN_free(&(mont->RR));
	BN_free(&(mont->N));
	BN_free(&(mont->Ni));
	if (mont->flags & BN_FLG_MALLOCED)
		OPENSSL_free(mont);
}

int
BN_MONT_CTX_set(BN_MONT_CTX *mont, const BIGNUM *mod, BN_CTX *ctx)
{
	BIGNUM Ri, *R;

	BN_init(&Ri);
	R = &(mont->RR);					/* grab RR as a temp */
	BN_copy(&(mont->N), mod);			/* Set N */
	mont->N.neg = 0;

#ifdef MONT_WORD
	{
		BIGNUM tmod;
		BN_ULONG buf[2];

		mont->ri = (BN_num_bits(mod)+(BN_BITS2-1))/BN_BITS2*BN_BITS2;
		if (!(BN_zero(R))) goto err;
		if (!(BN_set_bit(R, BN_BITS2))) goto err;	/* R */

		buf[0] = mod->d[0]; /* tmod = N mod word size */
		buf[1] = 0;
		tmod.d = buf;
		tmod.top = 1;
		tmod.dmax = 2;
		tmod.neg = 0;
							/* Ri = R^-1 mod N */
		if ((BN_mod_inverse(&Ri, R, &tmod, ctx)) == NULL)
			goto err;
		if (!BN_lshift(&Ri, &Ri, BN_BITS2)) goto err; /* R*Ri */
		if (!BN_is_zero(&Ri)) {
			if (!BN_sub_word(&Ri, 1)) goto err;
		} else {
			/* if N mod word size == 1 */
			if (!BN_set_word(&Ri, BN_MASK2)) goto err;  /* Ri-- (mod word size) */
		}
		if (!BN_div(&Ri, NULL, &Ri, &tmod, ctx)) goto err;
		/* Ni = (R*Ri-1)/N,
		 * keep only least significant word:
		 */
		mont->n0 = (Ri.top > 0) ? Ri.d[0] : 0;
		BN_free(&Ri);
	}
#else /* !MONT_WORD */
	{
		/* bignum version */
		mont->ri = BN_num_bits(&mont->N);
		if (!BN_zero(R)) goto err;
		if (!BN_set_bit(R, mont->ri)) goto err;  /* R = 2^ri */
		                                        /* Ri = R^-1 mod N */
		if ((BN_mod_inverse(&Ri, R, &mont->N, ctx)) == NULL)
			goto err;
		if (!BN_lshift(&Ri, &Ri, mont->ri)) goto err; /* R*Ri */
		if (!BN_sub_word(&Ri, 1)) goto err;
							/* Ni = (R*Ri-1) / N */
		if (!BN_div(&(mont->Ni), NULL, &Ri, &mont->N, ctx)) goto err;
		BN_free(&Ri);
	}
#endif	/* MONT_WORD */

	/* setup RR for conversions */
	if (!BN_zero(&(mont->RR))) goto err;
	if (!BN_set_bit(&(mont->RR), mont->ri*2)) goto err;
	if (!BN_mod(&(mont->RR), &(mont->RR), &(mont->N), ctx)) goto err;

	return (1);
err:
	return (0);
}

#ifdef NOT_NEEDED_FOR_DH
BN_MONT_CTX *
BN_MONT_CTX_copy(BN_MONT_CTX *to, BN_MONT_CTX *from)
{
	if (to == from) return (to);

	if (!BN_copy(&(to->RR), &(from->RR))) return NULL;
	if (!BN_copy(&(to->N), &(from->N))) return NULL;
	if (!BN_copy(&(to->Ni), &(from->Ni))) return NULL;
	to->ri = from->ri;
	to->n0 = from->n0;
	return (to);
}
#endif /* NOT_NEEDED_FOR_DH */


#ifdef BN_RECURSION
/* Karatsuba recursive multiplication algorithm
 * (cf. Knuth, The Art of Computer Programming, Vol. 2)
 */

/* r is 2*n2 words in size,
 * a and b are both n2 words in size.
 * n2 must be a power of 2.
 * We multiply and return the result.
 * t must be 2*n2 words in size
 * We calculate
 * a[0]*b[0]
 * a[0]*b[0]+a[1]*b[1]+(a[0]-a[1])*(b[1]-b[0])
 * a[1]*b[1]
 */
void
bn_mul_recursive(BN_ULONG *r, BN_ULONG *a, BN_ULONG *b, int n2, BN_ULONG *t)
{
	int n = n2/2, c1, c2;
	unsigned int neg, zero;
	BN_ULONG ln, lo, *p;

#ifdef BN_COUNT
	printf(" bn_mul_recursive %d * %d\n", n2, n2);
#endif
#ifdef BN_MUL_COMBA
	if (n2 == 8) {
		bn_mul_comba8(r, a, b);
		return;
	}
#endif /* BN_MUL_COMBA */
	if (n2 < BN_MUL_RECURSIVE_SIZE_NORMAL) {
		/* This should not happen */
		bn_mul_normal(r, a, n2, b, n2);
		return;
	}
	/* r = (a[0]-a[1])*(b[1]-b[0]) */
	c1 = bn_cmp_words(a, &(a[n]), n);
	c2 = bn_cmp_words(&(b[n]), b, n);
	zero = neg = 0;
	switch (c1*3+c2) {
	case -4:
		bn_sub_words(t,     &(a[n]), a,     n); /* - */
		bn_sub_words(&(t[n]), b,     &(b[n]), n); /* - */
		break;
	case -3:
		zero = 1;
		break;
	case -2:
		bn_sub_words(t,     &(a[n]), a,     n); /* - */
		bn_sub_words(&(t[n]), &(b[n]), b,     n); /* + */
		neg = 1;
		break;
	case -1:
	case 0:
	case 1:
		zero = 1;
		break;
	case 2:
		bn_sub_words(t,     a,     &(a[n]), n); /* + */
		bn_sub_words(&(t[n]), b,     &(b[n]), n); /* - */
		neg = 1;
		break;
	case 3:
		zero = 1;
		break;
	case 4:
		bn_sub_words(t,     a,     &(a[n]), n);
		bn_sub_words(&(t[n]), &(b[n]), b,     n);
		break;
	}

# ifdef BN_MUL_COMBA
	if (n == 4) {
		if (!zero)
			bn_mul_comba4(&(t[n2]), t, &(t[n]));
		else
			memset(&(t[n2]), 0, 8*sizeof(BN_ULONG));

		bn_mul_comba4(r, a, b);
		bn_mul_comba4(&(r[n2]), &(a[n]), &(b[n]));
	} else if (n == 8) {
		if (!zero)
			bn_mul_comba8(&(t[n2]), t, &(t[n]));
		else
			memset(&(t[n2]), 0, 16*sizeof(BN_ULONG));

		bn_mul_comba8(r, a, b);
		bn_mul_comba8(&(r[n2]), &(a[n]), &(b[n]));
	} else
# endif /* BN_MUL_COMBA */
	{
		p = &(t[n2*2]);
		if (!zero)
			bn_mul_recursive(&(t[n2]), t, &(t[n]), n, p);
		else
			memset(&(t[n2]), 0, n2*sizeof(BN_ULONG));
		bn_mul_recursive(r, a, b, n, p);
		bn_mul_recursive(&(r[n2]), &(a[n]), &(b[n]), n, p);
	}

	/* t[32] holds (a[0]-a[1])*(b[1]-b[0]), c1 is the sign
	 * r[10] holds (a[0]*b[0])
	 * r[32] holds (b[1]*b[1])
	 */

	c1 = (int)(bn_add_words(t, r, &(r[n2]), n2));

	if (neg) {
		/* if t[32] is negative */
		c1 -= (int)(bn_sub_words(&(t[n2]), t, &(t[n2]), n2));
	} else {
		/* Might have a carry */
		c1 += (int)(bn_add_words(&(t[n2]), &(t[n2]), t, n2));
	}

	/* t[32] holds (a[0]-a[1])*(b[1]-b[0])+(a[0]*b[0])+(a[1]*b[1])
	 * r[10] holds (a[0]*b[0])
	 * r[32] holds (b[1]*b[1])
	 * c1 holds the carry bits
	 */
	c1 += (int)(bn_add_words(&(r[n]), &(r[n]), &(t[n2]), n2));
	if (c1) {
		p = &(r[n+n2]);
		lo = *p;
		ln = (lo+c1)&BN_MASK2;
		*p = ln;

		/* The overflow will stop before we over write
		 * words we should not overwrite
		 */
		if (ln < (BN_ULONG)c1) {
			do {
				p++;
				lo = *p;
				ln = (lo+1)&BN_MASK2;
				*p = ln;
			} while (ln == 0);
		}
	}
}

/* n+tn is the word length
 * t needs to be n*4 is size, as does r
 */
void
bn_mul_part_recursive(BN_ULONG *r, BN_ULONG *a, BN_ULONG *b, int tn, int n, BN_ULONG *t)
{
	int i, j, n2 = n*2;
	int c1, c2, neg, zero;
	BN_ULONG ln, lo, *p;

	BCM_REFERENCE(zero);

# ifdef BN_COUNT
	printf(" bn_mul_part_recursive %d * %d\n", tn+n, tn+n);
# endif
	if (n < 8) {
		i = tn+n;
		bn_mul_normal(r, a, i, b, i);
		return;
	}

	/* r = (a[0]-a[1])*(b[1]-b[0]) */
	c1 = bn_cmp_words(a, &(a[n]), n);
	c2 = bn_cmp_words(&(b[n]), b, n);
	zero = neg = 0;
	switch (c1 * 3 + c2) {
	case -4:
		bn_sub_words(t,     &(a[n]), a,     n); /* - */
		bn_sub_words(&(t[n]), b,     &(b[n]), n); /* - */
		break;
	case -3:
		zero = 1;
		/* break; */
	case -2:
		bn_sub_words(t,     &(a[n]), a,     n); /* - */
		bn_sub_words(&(t[n]), &(b[n]), b,     n); /* + */
		neg = 1;
		break;
	case -1:
	case 0:
	case 1:
		zero = 1;
		/* break; */
	case 2:
		bn_sub_words(t,     a,     &(a[n]), n); /* + */
		bn_sub_words(&(t[n]), b,     &(b[n]), n); /* - */
		neg = 1;
		break;
	case 3:
		zero = 1;
		/* break; */
	case 4:
		bn_sub_words(t,     a,     &(a[n]), n);
		bn_sub_words(&(t[n]), &(b[n]), b,     n);
		break;
	}
	/* The zero case isn't yet implemented here. The speedup
	 * would probably be negligible.
	 */
		if (n == 8) {
			bn_mul_comba8(&(t[n2]), t, &(t[n]));
			bn_mul_comba8(r, a, b);
			bn_mul_normal(&(r[n2]), &(a[n]), tn, &(b[n]), tn);
			memset(&(r[n2+tn*2]), 0, sizeof(BN_ULONG)*(n2-tn*2));
		} else {
			p = &(t[n2*2]);
			bn_mul_recursive(&(t[n2]), t, &(t[n]), n, p);
			bn_mul_recursive(r, a, b, n, p);
			i = n/2;
			/* If there is only a bottom half to the number,
			 * just do it
			 */
			j = tn-i;
			if (j == 0) {
				bn_mul_recursive(&(r[n2]), &(a[n]), &(b[n]), i, p);
				memset(&(r[n2+i*2]), 0, sizeof(BN_ULONG)*(n2-i*2));
			} else if (j > 0) {
				/* eg, n == 16, i == 8 and tn == 11 */
				bn_mul_part_recursive(&(r[n2]), &(a[n]), &(b[n]), j, i, p);
				memset(&(r[n2+tn*2]), 0, sizeof(BN_ULONG)*(n2-tn*2));
			} else {
				/* (j < 0) eg, n == 16, i == 8 and tn == 5 */
				memset(&(r[n2]), 0, sizeof(BN_ULONG)*n2);
				if (tn < BN_MUL_RECURSIVE_SIZE_NORMAL) {
					bn_mul_normal(&(r[n2]), &(a[n]), tn, &(b[n]), tn);
				} else {
					for (;;) {
						i /= 2;
						if (i < tn) {
							bn_mul_part_recursive(&(r[n2]),
							                      &(a[n]), &(b[n]),
							                      tn-i, i, p);
							break;
						} else if (i == tn) {
							bn_mul_recursive(&(r[n2]),
							                 &(a[n]), &(b[n]),
							                 i, p);
							break;
						}
					}
				}
			}
		}

	/* t[32] holds (a[0]-a[1])*(b[1]-b[0]), c1 is the sign
	 * r[10] holds (a[0]*b[0])
	 * r[32] holds (b[1]*b[1])
	 */

	c1 = (int)(bn_add_words(t, r, &(r[n2]), n2));

	if (neg) /* if t[32] is negative */ {
		c1 -= (int)(bn_sub_words(&(t[n2]), t, &(t[n2]), n2));
	} else {
		/* Might have a carry */
		c1 += (int)(bn_add_words(&(t[n2]), &(t[n2]), t, n2));
	}

	/* t[32] holds (a[0]-a[1])*(b[1]-b[0])+(a[0]*b[0])+(a[1]*b[1])
	 * r[10] holds (a[0]*b[0])
	 * r[32] holds (b[1]*b[1])
	 * c1 holds the carry bits
	 */
	c1 += (int)(bn_add_words(&(r[n]), &(r[n]), &(t[n2]), n2));
	if (c1) {
		p = &(r[n+n2]);
		lo = *p;
		ln = (lo+c1)&BN_MASK2;
		*p = ln;

		/* The overflow will stop before we over write
		 * words we should not overwrite
		 */
		if (ln < (BN_ULONG)c1) {
			do {
				p++;
				lo = *p;
				ln = (lo+1)&BN_MASK2;
				*p = ln;
			} while (ln == 0);
		}
	}
}

#ifdef NOT_NEEDED_FOR_DH
/* a and b must be the same size, which is n2.
 * r needs to be n2 words and t needs to be n2*2
 */
void
bn_mul_low_recursive(BN_ULONG *r, BN_ULONG *a, BN_ULONG *b, int n2, BN_ULONG *t)
{
	int n = n2/2;

# ifdef BN_COUNT
	printf(" bn_mul_low_recursive %d * %d\n", n2, n2);
# endif

	bn_mul_recursive(r, a, b, n, &(t[0]));
	if (n >= BN_MUL_LOW_RECURSIVE_SIZE_NORMAL) {
		bn_mul_low_recursive(&(t[0]), &(a[0]), &(b[n]), n, &(t[n2]));
		bn_add_words(&(r[n]), &(r[n]), &(t[0]), n);
		bn_mul_low_recursive(&(t[0]), &(a[n]), &(b[0]), n, &(t[n2]));
		bn_add_words(&(r[n]), &(r[n]), &(t[0]), n);
	} else {
		bn_mul_low_normal(&(t[0]), &(a[0]), &(b[n]), n);
		bn_mul_low_normal(&(t[n]), &(a[n]), &(b[0]), n);
		bn_add_words(&(r[n]), &(r[n]), &(t[0]), n);
		bn_add_words(&(r[n]), &(r[n]), &(t[n]), n);
		}
}
#endif /* NOT_NEEDED_FOR_DH */

/* a and b must be the same size, which is n2.
 * r needs to be n2 words and t needs to be n2*2
 * l is the low words of the output.
 * t needs to be n2*3
 */
void
bn_mul_high(BN_ULONG *r, BN_ULONG *a, BN_ULONG *b, BN_ULONG *l, int n2, BN_ULONG *t)
{
	int i, n;
	int c1, c2;
	int neg, oneg, zero;
	BN_ULONG ll, lc, *lp, *mp;

# ifdef BN_COUNT
	printf(" bn_mul_high %d * %d\n", n2, n2);
# endif
	n = n2 / 2;

	/* Calculate (al - ah) * (bh - bl) */
	neg = zero = 0;
	c1 = bn_cmp_words(&(a[0]), &(a[n]), n);
	c2 = bn_cmp_words(&(b[n]), &(b[0]), n);
	switch (c1 * 3 + c2) {
	case -4:
		bn_sub_words(&(r[0]), &(a[n]), &(a[0]), n);
		bn_sub_words(&(r[n]), &(b[0]), &(b[n]), n);
		break;
	case -3:
		zero = 1;
		break;
	case -2:
		bn_sub_words(&(r[0]), &(a[n]), &(a[0]), n);
		bn_sub_words(&(r[n]), &(b[n]), &(b[0]), n);
		neg = 1;
		break;
	case -1:
	case 0:
	case 1:
		zero = 1;
		break;
	case 2:
		bn_sub_words(&(r[0]), &(a[0]), &(a[n]), n);
		bn_sub_words(&(r[n]), &(b[0]), &(b[n]), n);
		neg = 1;
		break;
	case 3:
		zero = 1;
		break;
	case 4:
		bn_sub_words(&(r[0]), &(a[0]), &(a[n]), n);
		bn_sub_words(&(r[n]), &(b[n]), &(b[0]), n);
		break;
	}

	oneg = neg;
	/* t[10] = (a[0]-a[1])*(b[1]-b[0]) */
	/* r[10] = (a[1]*b[1]) */
# ifdef BN_MUL_COMBA
	if (n == 8) {
		bn_mul_comba8(&(t[0]), &(r[0]), &(r[n]));
		bn_mul_comba8(r, &(a[n]), &(b[n]));
	} else
# endif
	{
		bn_mul_recursive(&(t[0]), &(r[0]), &(r[n]), n, &(t[n2]));
		bn_mul_recursive(r, &(a[n]), &(b[n]), n, &(t[n2]));
	}

	/* s0 == low(al*bl)
	 * s1 == low(ah*bh)+low((al-ah)*(bh-bl))+low(al*bl)+high(al*bl)
	 * We know s0 and s1 so the only unknown is high(al*bl)
	 * high(al*bl) == s1 - low(ah*bh+s0+(al-ah)*(bh-bl))
	 * high(al*bl) == s1 - (r[0]+l[0]+t[0])
	 */
	if (l != NULL) {
		lp = &(t[n2+n]);
		c1 = (int)(bn_add_words(lp, &(r[0]), &(l[0]), n));
	} else {
		c1 = 0;
		lp = &(r[0]);
	}

	if (neg)
		neg = (int)(bn_sub_words(&(t[n2]), lp, &(t[0]), n));
	else {
		bn_add_words(&(t[n2]), lp, &(t[0]), n);
		neg = 0;
	}

	if (l != NULL) {
		bn_sub_words(&(t[n2+n]), &(l[n]), &(t[n2]), n);
	} else {
		lp = &(t[n2+n]);
		mp = &(t[n2]);
		for (i = 0; i < n; i++)
			lp[i] = ((~mp[i])+1)&BN_MASK2;
	}

	/* s[0] = low(al*bl)
	 * t[3] = high(al*bl)
	 * t[10] = (a[0]-a[1])*(b[1]-b[0]) neg is the sign
	 * r[10] = (a[1]*b[1])
	 */
	/* R[10] = al*bl
	 * R[21] = al*bl + ah*bh + (a[0]-a[1])*(b[1]-b[0])
	 * R[32] = ah*bh
	 */
	/* R[1] = t[3]+l[0]+r[0](+-)t[0] (have carry/borrow)
	 * R[2] = r[0]+t[3]+r[1](+-)t[1] (have carry/borrow)
	 * R[3] = r[1]+(carry/borrow)
	 */
	if (l != NULL) {
		lp = &(t[n2]);
		c1 = (int)(bn_add_words(lp, &(t[n2+n]), &(l[0]), n));
	} else {
		lp = &(t[n2+n]);
		c1 = 0;
	}
	c1 += (int)(bn_add_words(&(t[n2]), lp, &(r[0]), n));
	if (oneg)
		c1 -= (int)(bn_sub_words(&(t[n2]), &(t[n2]), &(t[0]), n));
	else
		c1 += (int)(bn_add_words(&(t[n2]), &(t[n2]), &(t[0]), n));

	c2 = (int)(bn_add_words(&(r[0]), &(r[0]), &(t[n2+n]), n));
	c2 += (int)(bn_add_words(&(r[0]), &(r[0]), &(r[n]), n));
	if (oneg)
		c2 -= (int)(bn_sub_words(&(r[0]), &(r[0]), &(t[n]), n));
	else
		c2 += (int)(bn_add_words(&(r[0]), &(r[0]), &(t[n]), n));

	if (c1 != 0) {
		/* Add starting at r[0], could be +ve or -ve */
		i = 0;
		if (c1 > 0) {
			lc = c1;
			do {
				ll = (r[i]+lc)&BN_MASK2;
				r[i++] = ll;
				lc = (lc > ll);
			} while (lc);
		} else {
			lc = -c1;
			do {
				ll = r[i];
				r[i++] = (ll-lc)&BN_MASK2;
				lc = (lc > ll);
			} while (lc);
		}
	}
	if (c2 != 0) {
		/* Add starting at r[1] */
		i = n;
		if (c2 > 0) {
			lc = c2;
			do {
				ll = (r[i]+lc)&BN_MASK2;
				r[i++] = ll;
				lc = (lc > ll);
			} while (lc);
		} else {
			lc = -c2;
			do {
				ll = r[i];
				r[i++] = (ll-lc)&BN_MASK2;
				lc = (lc > ll);
			} while (lc);
		}
	}
}
#endif /* BN_RECURSION */

int
BN_mul(BIGNUM *r, const BIGNUM *a, const BIGNUM *b, BN_CTX *ctx)
{
	int top, al, bl;
	BIGNUM *rr;
	int ret = 0;
#if defined(BN_MUL_COMBA) || defined(BN_RECURSION)
	int i;
#endif
#ifdef BN_RECURSION
	BIGNUM *t;
	int j, k;
#endif

#ifdef BN_COUNT
	printf("BN_mul %d * %d\n", a->top, b->top);
#endif

	bn_check_top(a);
	bn_check_top(b);
	bn_check_top(r);

	al = a->top;
	bl = b->top;

	if ((al == 0) || (bl == 0)) {
		if (!BN_zero(r)) goto err;
		return (1);
	}
	top = al+bl;

	BN_CTX_start(ctx);
	if ((r == a) || (r == b)) {
		if ((rr = BN_CTX_get(ctx)) == NULL) goto err;
	} else
		rr = r;
	rr->neg = a->neg^b->neg;

#if defined(BN_MUL_COMBA) || defined(BN_RECURSION)
	i = al-bl;
#endif
#ifdef BN_MUL_COMBA
	if (i == 0) {
		if (al == 8) {
			if (bn_wexpand(rr, 16) == NULL) goto err;
			rr->top = 16;
			bn_mul_comba8(rr->d, a->d, b->d);
			goto end;
		}
	}
#endif /* BN_MUL_COMBA */
#ifdef BN_RECURSION
	if ((al >= BN_MULL_SIZE_NORMAL) && (bl >= BN_MULL_SIZE_NORMAL)) {
		if (i == 1 && !BN_get_flags(b, BN_FLG_STATIC_DATA) && bl < b->dmax) {
			b->d[bl] = 0;
			bl++;
			i--;
		} else if (i == -1 && !BN_get_flags(a, BN_FLG_STATIC_DATA) && al < a->dmax) {
			a->d[al] = 0;
			al++;
			i++;
		}
		if (i == 0) {
			/* symmetric and > 4 */
			/* 16 or larger */
			j = BN_num_bits_word((BN_ULONG)al);
			j = 1 << (j - 1);
			k = j + j;
			t = BN_CTX_get(ctx);
			if (al == j) {
				/* exact multiple */
				if (bn_wexpand(t, k*2) == NULL) goto err;
				if (bn_wexpand(rr, k*2) == NULL) goto err;
				bn_mul_recursive(rr->d, a->d, b->d, al, t->d);
				rr->top = top;
				goto end;
			}
		}
	}
#endif /* BN_RECURSION */
	if (bn_wexpand(rr, top) == NULL) goto err;
	rr->top = top;
	bn_mul_normal(rr->d, a->d, al, b->d, bl);

#if defined(BN_MUL_COMBA) || defined(BN_RECURSION)
end:
#endif
	bn_fix_top(rr);
	if (r != rr) BN_copy(r, rr);
	ret = 1;
err:
	BN_CTX_end(ctx);
	return (ret);
}

void
bn_mul_normal(BN_ULONG *r, BN_ULONG *a, int na, BN_ULONG *b, int nb)
{
	BN_ULONG *rr;

#ifdef BN_COUNT
	printf(" bn_mul_normal %d * %d\n", na, nb);
#endif

	if (na < nb) {
		int itmp;
		BN_ULONG *ltmp;

		itmp = na; na = nb; nb = itmp;
		ltmp = a;   a = b;   b = ltmp;
	}
	rr = &(r[na]);
	rr[0] = bn_mul_words(r, a, na, b[0]);

	for (;;) {
		if (--nb <= 0) return;
		rr[1] = bn_mul_add_words(&(r[1]), a, na, b[1]);
		if (--nb <= 0) return;
		rr[2] = bn_mul_add_words(&(r[2]), a, na, b[2]);
		if (--nb <= 0) return;
		rr[3] = bn_mul_add_words(&(r[3]), a, na, b[3]);
		if (--nb <= 0) return;
		rr[4] = bn_mul_add_words(&(r[4]), a, na, b[4]);
		rr += 4;
		r += 4;
		b += 4;
	}
}

#ifdef NOT_NEEDED_FOR_DH
void
bn_mul_low_normal(BN_ULONG *r, BN_ULONG *a, BN_ULONG *b, int n)
{
#ifdef BN_COUNT
	printf(" bn_mul_low_normal %d * %d\n", n, n);
#endif
	bn_mul_words(r, a, n, b[0]);

	for (;;) {
		if (--n <= 0) return;
		bn_mul_add_words(&(r[1]), a, n, b[1]);
		if (--n <= 0) return;
		bn_mul_add_words(&(r[2]), a, n, b[2]);
		if (--n <= 0) return;
		bn_mul_add_words(&(r[3]), a, n, b[3]);
		if (--n <= 0) return;
		bn_mul_add_words(&(r[4]), a, n, b[4]);
		r += 4;
		b += 4;
	}
}
#endif /* NOT_NEEDED_FOR_DH */

static int
bnrand(int pseudorand, BIGNUM *rnd, int bits, int top, int bottom)
{
	unsigned char *buf = NULL;
	int ret = 0, bit, bytes, mask;

	if (bits == 0) {
		BN_zero(rnd);
		return 1;
	}

	bytes = (bits + 7) / 8;
	bit = (bits - 1) % 8;
	mask = 0xff << (bit + 1);

	buf = (unsigned char *)OPENSSL_malloc(bytes);
	if (buf == NULL) {
		BNerr(BN_F_BN_RAND, ERR_R_MALLOC_FAILURE);
		goto err;
	}


	assert(bn_rand_fn);
	bn_rand_fn(buf, bytes);

	if (pseudorand == 2) {
		/* generate patterns that are more likely to trigger BN
		 * library bugs
		 */
		int i;
		unsigned char c;

		for (i = 0; i < bytes; i++) {
/*			RAND_pseudo_bytes(&c, 1); */
			bn_rand_fn(&c, 1);
			if (c >= 128 && i > 0)
				buf[i] = buf[i-1];
			else if (c < 42)
				buf[i] = 0;
			else if (c < 84)
				buf[i] = 255;
		}
	}

	if (top != -1) {
		if (top) {
			if (bit == 0) {
				buf[0] = 1;
				buf[1] |= 0x80;
			} else {
				buf[0] |= (3 << (bit - 1));
			}
		} else {
			buf[0] |= (1 << bit);
		}
	}
	buf[0] &= ~mask;
	if (bottom) /* set bottom bit if requested */
		buf[bytes-1] |= 1;
	if (!BN_bin2bn(buf, bytes, rnd)) goto err;
	ret = 1;
err:
	if (buf != NULL) {
		OPENSSL_cleanse(buf, bytes);
		OPENSSL_free(buf);
	}
	return (ret);
}

int
BN_rand(BIGNUM *rnd, int bits, int top, int bottom)
{
	return bnrand(0, rnd, bits, top, bottom);
}

#ifdef BCMDH_TEST
int
BN_pseudo_rand(BIGNUM *rnd, int bits, int top, int bottom)
{
	return bnrand(1, rnd, bits, top, bottom);
}
#endif /* BCMDH_TEST */

#ifdef NOT_NEEDED_FOR_DH
int
BN_bntest_rand(BIGNUM *rnd, int bits, int top, int bottom)
{
	return bnrand(2, rnd, bits, top, bottom);
}
#endif /* NOT_NEEDED_FOR_DH */


#ifdef BCMDH_TEST
/* random number r:  0 <= r < range */
static int
bn_rand_range(int pseudo, BIGNUM *r, BIGNUM *range)
{
	int (*bn_rand)(BIGNUM *, int, int, int) = pseudo ? BN_pseudo_rand : BN_rand;
	int n;

	if (range->neg || BN_is_zero(range)) {
		BNerr(BN_F_BN_RAND_RANGE, BN_R_INVALID_RANGE);
		return 0;
	}

	n = BN_num_bits(range); /* n > 0 */

	/* BN_is_bit_set(range, n - 1) always holds */

	if (n == 1) {
		if (!BN_zero(r)) return 0;
	} else if (!BN_is_bit_set(range, n - 2) && !BN_is_bit_set(range, n - 3)) {
		/* range = 100..._2,
		 * so  3*range ( = 11..._2)  is exactly one bit longer than  range
		 */
		do {
			if (!bn_rand(r, n + 1, -1, 0)) return 0;
			/* If  r < 3*range, use  r : = r MOD range
			 * (which is either  r, r - range, or  r - 2*range).
			 * Otherwise, iterate once more.
			 * Since  3*range = 11..._2, each iteration succeeds with
			 * probability >= .75.
			 */
			if (BN_cmp(r, range) >= 0) {
				if (!BN_sub(r, r, range)) return 0;
				if (BN_cmp(r, range) >= 0)
					if (!BN_sub(r, r, range)) return 0;
			}
		}
		while (BN_cmp(r, range) >= 0)
			;
	} else {
		do {
			/* range = 11..._2  or  range = 101..._2 */
			if (!bn_rand(r, n, -1, 0))
				return 0;
		} while (BN_cmp(r, range) >= 0);
	}

	return 1;
}
#endif /* BCMDH_TEST */

#ifdef NOT_NEEDED_FOR_DH
int
BN_rand_range(BIGNUM *r, BIGNUM *range)
{
	return bn_rand_range(0, r, range);
}
#endif /* NOT_NEEDED_FOR_DH */

#ifdef BCMDH_TEST
int
BN_pseudo_rand_range(BIGNUM *r, BIGNUM *range)
{
	return bn_rand_range(1, r, range);
}
#endif /* BCMDH_TEST */

#ifdef NOT_NEEDED_FOR_DH
void
BN_RECP_CTX_init(BN_RECP_CTX *recp)
{
	BN_init(&(recp->N));
	BN_init(&(recp->Nr));
	recp->num_bits = 0;
	recp->flags = 0;
}

BN_RECP_CTX *
BN_RECP_CTX_new(void)
{
	BN_RECP_CTX *ret;

	if ((ret = (BN_RECP_CTX *)OPENSSL_malloc(sizeof(BN_RECP_CTX))) == NULL)
		return (NULL);

	BN_RECP_CTX_init(ret);
	ret->flags = BN_FLG_MALLOCED;
	return (ret);
}

void
BN_RECP_CTX_free(BN_RECP_CTX *recp)
{
	if (recp == NULL)
	    return;

	BN_free(&(recp->N));
	BN_free(&(recp->Nr));
	if (recp->flags & BN_FLG_MALLOCED)
		OPENSSL_free(recp);
}

int
BN_RECP_CTX_set(BN_RECP_CTX *recp, const BIGNUM *d, BN_CTX *ctx)
{
	if (!BN_copy(&(recp->N), d)) return 0;
	if (!BN_zero(&(recp->Nr))) return 0;
	recp->num_bits = BN_num_bits(d);
	recp->shift = 0;
	return (1);
}

int
BN_mod_mul_reciprocal(BIGNUM *r, const BIGNUM *x, const BIGNUM *y,
                      BN_RECP_CTX *recp, BN_CTX *ctx)
{
	int ret = 0;
	BIGNUM *a;
	const BIGNUM *ca;

	BN_CTX_start(ctx);
	if ((a = BN_CTX_get(ctx)) == NULL)
		goto err;
	if (y != NULL) {
		if (x == y) {
			if (!BN_sqr(a, x, ctx))
				goto err;
		} else {
			if (!BN_mul(a, x, y, ctx))
				goto err;
		}
		ca = a;
	} else
		ca = x; /* Just do the mod */

	ret = BN_div_recp(NULL, r, ca, recp, ctx);
err:
	BN_CTX_end(ctx);
	return (ret);
}

int
BN_div_recp(BIGNUM *dv, BIGNUM *rem, const BIGNUM *m, BN_RECP_CTX *recp, BN_CTX *ctx)
{
	int i, j, ret = 0;
	BIGNUM *a, *b, *d, *r;

	BN_CTX_start(ctx);
	a = BN_CTX_get(ctx);
	b = BN_CTX_get(ctx);
	if (dv != NULL)
		d = dv;
	else
		d = BN_CTX_get(ctx);
	if (rem != NULL)
		r = rem;
	else
		r = BN_CTX_get(ctx);
	if (a == NULL || b == NULL || d == NULL || r == NULL)
		goto err;

	if (BN_ucmp(m, &(recp->N)) < 0) {
		if (!BN_zero(d)) return 0;
		if (!BN_copy(r, m)) return 0;
		BN_CTX_end(ctx);
		return (1);
	}

	/* We want the remainder
	 * Given input of ABCDEF / ab
	 * we need multiply ABCDEF by 3 digests of the reciprocal of ab
	 *
	 */

	/* i : = max(BN_num_bits(m), 2*BN_num_bits(N)) */
	i = BN_num_bits(m);
	j = recp->num_bits << 1;
	if (j > i) i = j;

	/* Nr : = round(2^i / N) */
	if (i != recp->shift)
		recp->shift = BN_reciprocal(&(recp->Nr), &(recp->N),
			i, ctx); /* BN_reciprocal returns i, or -1 for an error */
	if (recp->shift == -1) goto err;

	/* d : = |round(round(m / 2^BN_num_bits(N)) * recp->Nr / 2^(i - BN_num_bits(N)))|
	 *    = |round(round(m / 2^BN_num_bits(N)) * round(2^i / N) / 2^(i - BN_num_bits(N)))|
	 *   <= |(m / 2^BN_num_bits(N)) * (2^i / N) * (2^BN_num_bits(N) / 2^i)|
	 *    = |m/N|
	 */
	if (!BN_rshift(a, m, recp->num_bits)) goto err;
	if (!BN_mul(b, a, &(recp->Nr), ctx)) goto err;
	if (!BN_rshift(d, b, i-recp->num_bits)) goto err;
	d->neg = 0;

	if (!BN_mul(b, &(recp->N), d, ctx)) goto err;
	if (!BN_usub(r, m, b)) goto err;
	r->neg = 0;

	j = 0;
	while (BN_ucmp(r, &(recp->N)) >= 0) {
		if (j++ > 2) {
			BNerr(BN_F_BN_MOD_MUL_RECIPROCAL, BN_R_BAD_RECIPROCAL);
			goto err;
			}
		if (!BN_usub(r, r, &(recp->N))) goto err;
		if (!BN_add_word(d, 1)) goto err;
		}

	r->neg = BN_is_zero(r)?0:m->neg;
	d->neg = m->neg^recp->N.neg;
	ret = 1;
err:
	BN_CTX_end(ctx);
	return (ret);
}

/* len is the expected size of the result
 * We actually calculate with an extra word of precision, so
 * we can do faster division if the remainder is not required.
 */
/* r : = 2^len / m */
int
BN_reciprocal(BIGNUM *r, const BIGNUM *m, int len, BN_CTX *ctx)
{
	int ret = -1;
	BIGNUM t;

	BN_init(&t);

	if (!BN_zero(&t)) goto err;
	if (!BN_set_bit(&t, len)) goto err;

	if (!BN_div(r, NULL, &t, m, ctx)) goto err;

	ret = len;
err:
	BN_free(&t);
	return (ret);
}
#endif /* NOT_NEEDED_FOR_DH */

int
BN_lshift1(BIGNUM *r, const BIGNUM *a)
{
	register BN_ULONG *ap, *rp, t, c;
	int i;

	if (r != a) {
		r->neg = a->neg;
		if (bn_wexpand(r, a->top+1) == NULL) return (0);
		r->top = a->top;
	} else {
		if (bn_wexpand(r, a->top+1) == NULL) return (0);
	}
	ap = a->d;
	rp = r->d;
	c = 0;
	for (i = 0; i < a->top; i++) {
		t = *(ap++);
		*(rp++) = ((t << 1) | c) & BN_MASK2;
		c = (t & BN_TBIT)?1:0;
	}
	if (c) {
		*rp = 1;
		r->top++;
	}
	return (1);
}

int
BN_rshift1(BIGNUM *r, const BIGNUM *a)
{
	BN_ULONG *ap, *rp, t, c;
	int i;

	if (BN_is_zero(a)) {
		BN_zero(r);
		return (1);
	}
	if (a != r) {
		if (bn_wexpand(r, a->top) == NULL) return (0);
		r->top = a->top;
		r->neg = a->neg;
	}
	ap = a->d;
	rp = r->d;
	c = 0;
	for (i = a->top - 1; i >= 0; i--) {
		t = ap[i];
		rp[i] = ((t >> 1) & BN_MASK2) | c;
		c = (t & 1) ? BN_TBIT : 0;
	}
	bn_fix_top(r);
	return (1);
}

int
BN_lshift(BIGNUM *r, const BIGNUM *a, int n)
{
	int i, nw, lb, rb;
	BN_ULONG *t, *f;
	BN_ULONG l;

	r->neg = a->neg;
	nw = n / BN_BITS2;
	if (bn_wexpand(r, a->top + nw + 1) == NULL)
		return (0);
	lb = n % BN_BITS2;
	rb = BN_BITS2 - lb;
	f = a->d;
	t = r->d;
	t[a->top + nw] = 0;
	if (lb == 0)
		for (i = a->top - 1; i >= 0; i--)
			t[nw+i] = f[i];
	else
		for (i = a->top-1; i >= 0; i--) {
			l = f[i];
			t[nw + i + 1] |= (l >> rb) & BN_MASK2;
			t[nw + i] = (l << lb) & BN_MASK2;
		}
	memset(t, 0, nw * sizeof(t[0]));
/*	for (i = 0; i < nw; i++)
 *		t[i] = 0;
 */
	r->top = a->top + nw + 1;
	bn_fix_top(r);
	return (1);
}

int
BN_rshift(BIGNUM *r, const BIGNUM *a, int n)
{
	int i, j, nw, lb, rb;
	BN_ULONG *t, *f;
	BN_ULONG l, tmp;

	nw = n/BN_BITS2;
	rb = n%BN_BITS2;
	lb = BN_BITS2-rb;
	if (nw > a->top || a->top == 0) {
		BN_zero(r);
		return (1);
	}
	if (r != a) {
		r->neg = a->neg;
		if (bn_wexpand(r, a->top-nw+1) == NULL) return (0);
	} else {
		if (n == 0)
			return 1; /* or the copying loop will go berserk */
	}

	f = &(a->d[nw]);
	t = r->d;
	j = a->top-nw;
	r->top = j;

	if (rb == 0) {
		for (i = j+1; i > 0; i--)
			*(t++) = *(f++);
	} else {
		l = *(f++);
		for (i = 1; i < j; i++) {
			tmp = (l >> rb)&BN_MASK2;
			l = *(f++);
			*(t++) = (tmp | (l << lb)) & BN_MASK2;
		}
		*(t++) = (l>>rb)&BN_MASK2;
	}
	*t = 0;
	bn_fix_top(r);
	return (1);
}

/* r must not be a */
/* I've just gone over this and it is now %20 faster on x86 - eay - 27 Jun 96 */
int
BN_sqr(BIGNUM *r, const BIGNUM *a, BN_CTX *ctx)
{
	int max, al;
	int ret = 0;
	BIGNUM *tmp, *rr;

#ifdef BN_COUNT
	fprintf(stderr, "BN_sqr %d * %d\n", a->top, a->top);
#endif
	bn_check_top(a);

	al = a->top;
	if (al <= 0) {
		r->top = 0;
		return (1);
	}

	BN_CTX_start(ctx);
	rr = (a != r) ? r : BN_CTX_get(ctx);
	tmp = BN_CTX_get(ctx);
	if (tmp == NULL) goto err;

	max = (al + al);
	if (bn_wexpand(rr, max + 1) == NULL)
		goto err;

	if (al == 4) {
#ifndef BN_SQR_COMBA
		BN_ULONG t[8];
		bn_sqr_normal(rr->d, a->d, 4, t);
#else
		bn_sqr_comba4(rr->d, a->d);
#endif
	} else if (al == 8) {
#ifndef BN_SQR_COMBA
		BN_ULONG t[16];
		bn_sqr_normal(rr->d, a->d, 8, t);
#else
		bn_sqr_comba8(rr->d, a->d);
#endif
	} else {
#if defined(BN_RECURSION)
		if (al < BN_SQR_RECURSIVE_SIZE_NORMAL) {
			BN_ULONG t[BN_SQR_RECURSIVE_SIZE_NORMAL*2];
			bn_sqr_normal(rr->d, a->d, al, t);
		} else {
			int j, k;

			j = BN_num_bits_word((BN_ULONG)al);
			j = 1 << (j - 1);
			k = j + j;
			if (al == j) {
				if (bn_wexpand(tmp, k*2) == NULL) goto err;
				bn_sqr_recursive(rr->d, a->d, al, tmp->d);
			} else {
				if (bn_wexpand(tmp, max) == NULL) goto err;
				bn_sqr_normal(rr->d, a->d, al, tmp->d);
			}
		}
#else
		if (bn_wexpand(tmp, max) == NULL)
			goto err;
		bn_sqr_normal(rr->d, a->d, al, tmp->d);
#endif	/* BN_RECURSION */
	}

	rr->top = max;
	rr->neg = 0;
	if ((max > 0) && (rr->d[max-1] == 0)) rr->top--;
	if (rr != r) BN_copy(r, rr);
	ret = 1;
err:
	BN_CTX_end(ctx);
	return (ret);
}

/* tmp must have 2*n words */
void
bn_sqr_normal(BN_ULONG *r, const BN_ULONG *a, int n, BN_ULONG *tmp)
{
	int i, j, max;
	const BN_ULONG *ap;
	BN_ULONG *rp;

	max = n*2;
	ap = a;
	rp = r;
	rp[0] = rp[max-1] = 0;
	rp++;
	j = n;

	if (--j > 0) {
		ap++;
		rp[j] = bn_mul_words(rp, ap, j, ap[-1]);
		rp += 2;
	}

	for (i = n - 2; i > 0; i--) {
		j--;
		ap++;
		rp[j] = bn_mul_add_words(rp, ap, j, ap[-1]);
		rp += 2;
	}

	bn_add_words(r, r, r, max);

	/* There will not be a carry */

	bn_sqr_words(tmp, a, n);

	bn_add_words(r, r, tmp, max);
}

#ifdef BN_RECURSION
/* r is 2*n words in size,
 * a and b are both n words in size.    (There's not actually a 'b' here ...)
 * n must be a power of 2.
 * We multiply and return the result.
 * t must be 2*n words in size
 * We calculate
 * a[0]*b[0]
 * a[0]*b[0]+a[1]*b[1]+(a[0]-a[1])*(b[1]-b[0])
 * a[1]*b[1]
 */
void
bn_sqr_recursive(BN_ULONG *r, const BN_ULONG *a, int n2, BN_ULONG *t)
{
	int n = n2/2;
	int zero, c1;
	BN_ULONG ln, lo, *p;

#ifdef BN_COUNT
	fprintf(stderr, " bn_sqr_recursive %d * %d\n", n2, n2);
#endif
	if (n2 == 4) {
#ifndef BN_SQR_COMBA
		bn_sqr_normal(r, a, 4, t);
#else
		bn_sqr_comba4(r, a);
#endif
		return;
	} else if (n2 == 8) {
#ifndef BN_SQR_COMBA
		bn_sqr_normal(r, a, 8, t);
#else
		bn_sqr_comba8(r, a);
#endif
		return;
	}
	if (n2 < BN_SQR_RECURSIVE_SIZE_NORMAL) {
		bn_sqr_normal(r, a, n2, t);
		return;
		}
	/* r = (a[0]-a[1])*(a[1]-a[0]) */
	c1 = bn_cmp_words(a, &(a[n]), n);
	zero = 0;
	if (c1 > 0)
		bn_sub_words(t, a, &(a[n]), n);
	else if (c1 < 0)
		bn_sub_words(t, &(a[n]), a, n);
	else
		zero = 1;

	/* The result will always be negative unless it is zero */
	p = &(t[n2*2]);

	if (!zero)
		bn_sqr_recursive(&(t[n2]), t, n, p);
	else
		memset(&(t[n2]), 0, n2*sizeof(BN_ULONG));
	bn_sqr_recursive(r, a, n, p);
	bn_sqr_recursive(&(r[n2]), &(a[n]), n, p);

	/* t[32] holds (a[0]-a[1])*(a[1]-a[0]), it is negative or zero
	 * r[10] holds (a[0]*b[0])
	 * r[32] holds (b[1]*b[1])
	 */

	c1 = (int)(bn_add_words(t, r, &(r[n2]), n2));

	/* t[32] is negative */
	c1 -= (int)(bn_sub_words(&(t[n2]), t, &(t[n2]), n2));

	/* t[32] holds (a[0]-a[1])*(a[1]-a[0])+(a[0]*a[0])+(a[1]*a[1])
	 * r[10] holds (a[0]*a[0])
	 * r[32] holds (a[1]*a[1])
	 * c1 holds the carry bits
	 */
	c1 += (int)(bn_add_words(&(r[n]), &(r[n]), &(t[n2]), n2));
	if (c1) {
		p = &(r[n+n2]);
		lo = *p;
		ln = (lo+c1)&BN_MASK2;
		*p = ln;

		/* The overflow will stop before we over write
		 * words we should not overwrite
		 */
		if (ln < (BN_ULONG)c1) {
			do {
				p++;
				lo = *p;
				ln = (lo+1)&BN_MASK2;
				*p = ln;
			} while (ln == 0);
		}
	}
}
#endif	/* BN_RECURSION */

#ifdef BCMDH_TEST
BN_ULONG
BN_mod_word(const BIGNUM *a, BN_ULONG w)
{
#ifndef BN_LLONG
	BN_ULONG ret = 0;
#else
	BN_ULLONG ret = 0;
#endif
	int i;

	w &= BN_MASK2;
	for (i = a->top-1; i >= 0; i--) {
#ifndef BN_LLONG
		ret = ((ret << BN_BITS4) | ((a->d[i] >> BN_BITS4) & BN_MASK2l)) % w;
		ret = ((ret << BN_BITS4) | (a->d[i] & BN_MASK2l)) % w;
#else
		ret = (BN_ULLONG)(((ret << (BN_ULLONG)BN_BITS2) | a->d[i]) % (BN_ULLONG)w);
#endif
	}
	return ((BN_ULONG)ret);
}
#endif /* BCMDH_TEST */

#ifdef NOT_NEEDED_FOR_DH
BN_ULONG
BN_div_word(BIGNUM *a, BN_ULONG w)
{
	BN_ULONG ret;
	int i;

	if (a->top == 0) return (0);
	ret = 0;
	w &= BN_MASK2;
	for (i = a->top-1; i >= 0; i--) {
		BN_ULONG l, d;

		l = a->d[i];
		d = bn_div_words(ret, l, w);
		ret = (l-((d*w)&BN_MASK2))&BN_MASK2;
		a->d[i] = d;
	}
	if ((a->top > 0) && (a->d[a->top-1] == 0))
		a->top--;
	return (ret);
}
#endif /* NOT_NEEDED_FOR_DH */

int
BN_add_word(BIGNUM *a, BN_ULONG w)
{
	BN_ULONG l;
	int i;

	if (a->neg) {
		a->neg = 0;
		i = BN_sub_word(a, w);
		if (!BN_is_zero(a))
			a->neg = !(a->neg);
		return (i);
	}
	w &= BN_MASK2;
	if (bn_wexpand(a, a->top+1) == NULL) return (0);
	i = 0;
	for (;;) {
		if (i >= a->top)
			l = w;
		else
			l = (a->d[i]+(BN_ULONG)w)&BN_MASK2;
		a->d[i] = l;
		if (w > l)
			w = 1;
		else
			break;
		i++;
	}
	if (i >= a->top)
		a->top++;
	return (1);
}

int
BN_sub_word(BIGNUM *a, BN_ULONG w)
{
	int i;

	if (BN_is_zero(a) || a->neg) {
		a->neg = 0;
		i = BN_add_word(a, w);
		a->neg = 1;
		return (i);
		}

	w &= BN_MASK2;
	if ((a->top == 1) && (a->d[0] < w)) {
		a->d[0] = w-a->d[0];
		a->neg = 1;
		return (1);
	}
	i = 0;
	for (;;) {
		if (a->d[i] >= w) {
			a->d[i] -= w;
			break;
		} else {
			a->d[i] = (a->d[i]-w)&BN_MASK2;
			i++;
			w = 1;
		}
	}
	if ((a->d[i] == 0) && (i == (a->top-1)))
		a->top--;
	return (1);
}

int
BN_mul_word(BIGNUM *a, BN_ULONG w)
{
	BN_ULONG ll;

	w &= BN_MASK2;
	if (a->top) {
		if (w == 0)
			BN_zero(a);
		else {
			ll = bn_mul_words(a->d, a->d, a->top, w);
			if (ll) {
				if (bn_wexpand(a, a->top+1) == NULL) return (0);
				a->d[a->top++] = ll;
			}
		}
	}
	return (1);
}

#ifdef BCMDH_TEST
/* The quick sieve algorithm approach to weeding out primes is
 * Philip Zimmermann's, as implemented in PGP.  I have had a read of
 * his comments and implemented my own version.
 */

#ifndef EIGHT_BIT
#define NUMPRIMES 2048
#else
#define NUMPRIMES 54
#endif
static const unsigned int primes[NUMPRIMES] =
{
	2,   3,   5,  7, 11, 13, 17, 19,
	23,  29,  31, 37, 41, 43, 47, 53,
	59,  61,  67, 71, 73, 79, 83, 89,
	97,  101, 103, 107, 109, 113, 127, 131,
	137, 139, 149, 151, 157, 163, 167, 173,
	179, 181, 191, 193, 197, 199, 211, 223,
	227, 229, 233, 239, 241, 251,
#ifndef EIGHT_BIT
	257, 263,
	269, 271, 277, 281, 283, 293, 307, 311,
	313, 317, 331, 337, 347, 349, 353, 359,
	367, 373, 379, 383, 389, 397, 401, 409,
	419, 421, 431, 433, 439, 443, 449, 457,
	461, 463, 467, 479, 487, 491, 499, 503,
	509, 521, 523, 541, 547, 557, 563, 569,
	571, 577, 587, 593, 599, 601, 607, 613,
	617, 619, 631, 641, 643, 647, 653, 659,
	661, 673, 677, 683, 691, 701, 709, 719,
	727, 733, 739, 743, 751, 757, 761, 769,
	773, 787, 797, 809, 811, 821, 823, 827,
	829, 839, 853, 857, 859, 863, 877, 881,
	883, 887, 907, 911, 919, 929, 937, 941,
	947, 953, 967, 971, 977, 983, 991, 997,
	1009, 1013, 1019, 1021, 1031, 1033, 1039, 1049,
	1051, 1061, 1063, 1069, 1087, 1091, 1093, 1097,
	1103, 1109, 1117, 1123, 1129, 1151, 1153, 1163,
	1171, 1181, 1187, 1193, 1201, 1213, 1217, 1223,
	1229, 1231, 1237, 1249, 1259, 1277, 1279, 1283,
	1289, 1291, 1297, 1301, 1303, 1307, 1319, 1321,
	1327, 1361, 1367, 1373, 1381, 1399, 1409, 1423,
	1427, 1429, 1433, 1439, 1447, 1451, 1453, 1459,
	1471, 1481, 1483, 1487, 1489, 1493, 1499, 1511,
	1523, 1531, 1543, 1549, 1553, 1559, 1567, 1571,
	1579, 1583, 1597, 1601, 1607, 1609, 1613, 1619,
	1621, 1627, 1637, 1657, 1663, 1667, 1669, 1693,
	1697, 1699, 1709, 1721, 1723, 1733, 1741, 1747,
	1753, 1759, 1777, 1783, 1787, 1789, 1801, 1811,
	1823, 1831, 1847, 1861, 1867, 1871, 1873, 1877,
	1879, 1889, 1901, 1907, 1913, 1931, 1933, 1949,
	1951, 1973, 1979, 1987, 1993, 1997, 1999, 2003,
	2011, 2017, 2027, 2029, 2039, 2053, 2063, 2069,
	2081, 2083, 2087, 2089, 2099, 2111, 2113, 2129,
	2131, 2137, 2141, 2143, 2153, 2161, 2179, 2203,
	2207, 2213, 2221, 2237, 2239, 2243, 2251, 2267,
	2269, 2273, 2281, 2287, 2293, 2297, 2309, 2311,
	2333, 2339, 2341, 2347, 2351, 2357, 2371, 2377,
	2381, 2383, 2389, 2393, 2399, 2411, 2417, 2423,
	2437, 2441, 2447, 2459, 2467, 2473, 2477, 2503,
	2521, 2531, 2539, 2543, 2549, 2551, 2557, 2579,
	2591, 2593, 2609, 2617, 2621, 2633, 2647, 2657,
	2659, 2663, 2671, 2677, 2683, 2687, 2689, 2693,
	2699, 2707, 2711, 2713, 2719, 2729, 2731, 2741,
	2749, 2753, 2767, 2777, 2789, 2791, 2797, 2801,
	2803, 2819, 2833, 2837, 2843, 2851, 2857, 2861,
	2879, 2887, 2897, 2903, 2909, 2917, 2927, 2939,
	2953, 2957, 2963, 2969, 2971, 2999, 3001, 3011,
	3019, 3023, 3037, 3041, 3049, 3061, 3067, 3079,
	3083, 3089, 3109, 3119, 3121, 3137, 3163, 3167,
	3169, 3181, 3187, 3191, 3203, 3209, 3217, 3221,
	3229, 3251, 3253, 3257, 3259, 3271, 3299, 3301,
	3307, 3313, 3319, 3323, 3329, 3331, 3343, 3347,
	3359, 3361, 3371, 3373, 3389, 3391, 3407, 3413,
	3433, 3449, 3457, 3461, 3463, 3467, 3469, 3491,
	3499, 3511, 3517, 3527, 3529, 3533, 3539, 3541,
	3547, 3557, 3559, 3571, 3581, 3583, 3593, 3607,
	3613, 3617, 3623, 3631, 3637, 3643, 3659, 3671,
	3673, 3677, 3691, 3697, 3701, 3709, 3719, 3727,
	3733, 3739, 3761, 3767, 3769, 3779, 3793, 3797,
	3803, 3821, 3823, 3833, 3847, 3851, 3853, 3863,
	3877, 3881, 3889, 3907, 3911, 3917, 3919, 3923,
	3929, 3931, 3943, 3947, 3967, 3989, 4001, 4003,
	4007, 4013, 4019, 4021, 4027, 4049, 4051, 4057,
	4073, 4079, 4091, 4093, 4099, 4111, 4127, 4129,
	4133, 4139, 4153, 4157, 4159, 4177, 4201, 4211,
	4217, 4219, 4229, 4231, 4241, 4243, 4253, 4259,
	4261, 4271, 4273, 4283, 4289, 4297, 4327, 4337,
	4339, 4349, 4357, 4363, 4373, 4391, 4397, 4409,
	4421, 4423, 4441, 4447, 4451, 4457, 4463, 4481,
	4483, 4493, 4507, 4513, 4517, 4519, 4523, 4547,
	4549, 4561, 4567, 4583, 4591, 4597, 4603, 4621,
	4637, 4639, 4643, 4649, 4651, 4657, 4663, 4673,
	4679, 4691, 4703, 4721, 4723, 4729, 4733, 4751,
	4759, 4783, 4787, 4789, 4793, 4799, 4801, 4813,
	4817, 4831, 4861, 4871, 4877, 4889, 4903, 4909,
	4919, 4931, 4933, 4937, 4943, 4951, 4957, 4967,
	4969, 4973, 4987, 4993, 4999, 5003, 5009, 5011,
	5021, 5023, 5039, 5051, 5059, 5077, 5081, 5087,
	5099, 5101, 5107, 5113, 5119, 5147, 5153, 5167,
	5171, 5179, 5189, 5197, 5209, 5227, 5231, 5233,
	5237, 5261, 5273, 5279, 5281, 5297, 5303, 5309,
	5323, 5333, 5347, 5351, 5381, 5387, 5393, 5399,
	5407, 5413, 5417, 5419, 5431, 5437, 5441, 5443,
	5449, 5471, 5477, 5479, 5483, 5501, 5503, 5507,
	5519, 5521, 5527, 5531, 5557, 5563, 5569, 5573,
	5581, 5591, 5623, 5639, 5641, 5647, 5651, 5653,
	5657, 5659, 5669, 5683, 5689, 5693, 5701, 5711,
	5717, 5737, 5741, 5743, 5749, 5779, 5783, 5791,
	5801, 5807, 5813, 5821, 5827, 5839, 5843, 5849,
	5851, 5857, 5861, 5867, 5869, 5879, 5881, 5897,
	5903, 5923, 5927, 5939, 5953, 5981, 5987, 6007,
	6011, 6029, 6037, 6043, 6047, 6053, 6067, 6073,
	6079, 6089, 6091, 6101, 6113, 6121, 6131, 6133,
	6143, 6151, 6163, 6173, 6197, 6199, 6203, 6211,
	6217, 6221, 6229, 6247, 6257, 6263, 6269, 6271,
	6277, 6287, 6299, 6301, 6311, 6317, 6323, 6329,
	6337, 6343, 6353, 6359, 6361, 6367, 6373, 6379,
	6389, 6397, 6421, 6427, 6449, 6451, 6469, 6473,
	6481, 6491, 6521, 6529, 6547, 6551, 6553, 6563,
	6569, 6571, 6577, 6581, 6599, 6607, 6619, 6637,
	6653, 6659, 6661, 6673, 6679, 6689, 6691, 6701,
	6703, 6709, 6719, 6733, 6737, 6761, 6763, 6779,
	6781, 6791, 6793, 6803, 6823, 6827, 6829, 6833,
	6841, 6857, 6863, 6869, 6871, 6883, 6899, 6907,
	6911, 6917, 6947, 6949, 6959, 6961, 6967, 6971,
	6977, 6983, 6991, 6997, 7001, 7013, 7019, 7027,
	7039, 7043, 7057, 7069, 7079, 7103, 7109, 7121,
	7127, 7129, 7151, 7159, 7177, 7187, 7193, 7207,
	7211, 7213, 7219, 7229, 7237, 7243, 7247, 7253,
	7283, 7297, 7307, 7309, 7321, 7331, 7333, 7349,
	7351, 7369, 7393, 7411, 7417, 7433, 7451, 7457,
	7459, 7477, 7481, 7487, 7489, 7499, 7507, 7517,
	7523, 7529, 7537, 7541, 7547, 7549, 7559, 7561,
	7573, 7577, 7583, 7589, 7591, 7603, 7607, 7621,
	7639, 7643, 7649, 7669, 7673, 7681, 7687, 7691,
	7699, 7703, 7717, 7723, 7727, 7741, 7753, 7757,
	7759, 7789, 7793, 7817, 7823, 7829, 7841, 7853,
	7867, 7873, 7877, 7879, 7883, 7901, 7907, 7919,
	7927, 7933, 7937, 7949, 7951, 7963, 7993, 8009,
	8011, 8017, 8039, 8053, 8059, 8069, 8081, 8087,
	8089, 8093, 8101, 8111, 8117, 8123, 8147, 8161,
	8167, 8171, 8179, 8191, 8209, 8219, 8221, 8231,
	8233, 8237, 8243, 8263, 8269, 8273, 8287, 8291,
	8293, 8297, 8311, 8317, 8329, 8353, 8363, 8369,
	8377, 8387, 8389, 8419, 8423, 8429, 8431, 8443,
	8447, 8461, 8467, 8501, 8513, 8521, 8527, 8537,
	8539, 8543, 8563, 8573, 8581, 8597, 8599, 8609,
	8623, 8627, 8629, 8641, 8647, 8663, 8669, 8677,
	8681, 8689, 8693, 8699, 8707, 8713, 8719, 8731,
	8737, 8741, 8747, 8753, 8761, 8779, 8783, 8803,
	8807, 8819, 8821, 8831, 8837, 8839, 8849, 8861,
	8863, 8867, 8887, 8893, 8923, 8929, 8933, 8941,
	8951, 8963, 8969, 8971, 8999, 9001, 9007, 9011,
	9013, 9029, 9041, 9043, 9049, 9059, 9067, 9091,
	9103, 9109, 9127, 9133, 9137, 9151, 9157, 9161,
	9173, 9181, 9187, 9199, 9203, 9209, 9221, 9227,
	9239, 9241, 9257, 9277, 9281, 9283, 9293, 9311,
	9319, 9323, 9337, 9341, 9343, 9349, 9371, 9377,
	9391, 9397, 9403, 9413, 9419, 9421, 9431, 9433,
	9437, 9439, 9461, 9463, 9467, 9473, 9479, 9491,
	9497, 9511, 9521, 9533, 9539, 9547, 9551, 9587,
	9601, 9613, 9619, 9623, 9629, 9631, 9643, 9649,
	9661, 9677, 9679, 9689, 9697, 9719, 9721, 9733,
	9739, 9743, 9749, 9767, 9769, 9781, 9787, 9791,
	9803, 9811, 9817, 9829, 9833, 9839, 9851, 9857,
	9859, 9871, 9883, 9887, 9901, 9907, 9923, 9929,
	9931, 9941, 9949, 9967, 9973, 10007, 10009, 10037,
	10039, 10061, 10067, 10069, 10079, 10091, 10093, 10099,
	10103, 10111, 10133, 10139, 10141, 10151, 10159, 10163,
	10169, 10177, 10181, 10193, 10211, 10223, 10243, 10247,
	10253, 10259, 10267, 10271, 10273, 10289, 10301, 10303,
	10313, 10321, 10331, 10333, 10337, 10343, 10357, 10369,
	10391, 10399, 10427, 10429, 10433, 10453, 10457, 10459,
	10463, 10477, 10487, 10499, 10501, 10513, 10529, 10531,
	10559, 10567, 10589, 10597, 10601, 10607, 10613, 10627,
	10631, 10639, 10651, 10657, 10663, 10667, 10687, 10691,
	10709, 10711, 10723, 10729, 10733, 10739, 10753, 10771,
	10781, 10789, 10799, 10831, 10837, 10847, 10853, 10859,
	10861, 10867, 10883, 10889, 10891, 10903, 10909, 10937,
	10939, 10949, 10957, 10973, 10979, 10987, 10993, 11003,
	11027, 11047, 11057, 11059, 11069, 11071, 11083, 11087,
	11093, 11113, 11117, 11119, 11131, 11149, 11159, 11161,
	11171, 11173, 11177, 11197, 11213, 11239, 11243, 11251,
	11257, 11261, 11273, 11279, 11287, 11299, 11311, 11317,
	11321, 11329, 11351, 11353, 11369, 11383, 11393, 11399,
	11411, 11423, 11437, 11443, 11447, 11467, 11471, 11483,
	11489, 11491, 11497, 11503, 11519, 11527, 11549, 11551,
	11579, 11587, 11593, 11597, 11617, 11621, 11633, 11657,
	11677, 11681, 11689, 11699, 11701, 11717, 11719, 11731,
	11743, 11777, 11779, 11783, 11789, 11801, 11807, 11813,
	11821, 11827, 11831, 11833, 11839, 11863, 11867, 11887,
	11897, 11903, 11909, 11923, 11927, 11933, 11939, 11941,
	11953, 11959, 11969, 11971, 11981, 11987, 12007, 12011,
	12037, 12041, 12043, 12049, 12071, 12073, 12097, 12101,
	12107, 12109, 12113, 12119, 12143, 12149, 12157, 12161,
	12163, 12197, 12203, 12211, 12227, 12239, 12241, 12251,
	12253, 12263, 12269, 12277, 12281, 12289, 12301, 12323,
	12329, 12343, 12347, 12373, 12377, 12379, 12391, 12401,
	12409, 12413, 12421, 12433, 12437, 12451, 12457, 12473,
	12479, 12487, 12491, 12497, 12503, 12511, 12517, 12527,
	12539, 12541, 12547, 12553, 12569, 12577, 12583, 12589,
	12601, 12611, 12613, 12619, 12637, 12641, 12647, 12653,
	12659, 12671, 12689, 12697, 12703, 12713, 12721, 12739,
	12743, 12757, 12763, 12781, 12791, 12799, 12809, 12821,
	12823, 12829, 12841, 12853, 12889, 12893, 12899, 12907,
	12911, 12917, 12919, 12923, 12941, 12953, 12959, 12967,
	12973, 12979, 12983, 13001, 13003, 13007, 13009, 13033,
	13037, 13043, 13049, 13063, 13093, 13099, 13103, 13109,
	13121, 13127, 13147, 13151, 13159, 13163, 13171, 13177,
	13183, 13187, 13217, 13219, 13229, 13241, 13249, 13259,
	13267, 13291, 13297, 13309, 13313, 13327, 13331, 13337,
	13339, 13367, 13381, 13397, 13399, 13411, 13417, 13421,
	13441, 13451, 13457, 13463, 13469, 13477, 13487, 13499,
	13513, 13523, 13537, 13553, 13567, 13577, 13591, 13597,
	13613, 13619, 13627, 13633, 13649, 13669, 13679, 13681,
	13687, 13691, 13693, 13697, 13709, 13711, 13721, 13723,
	13729, 13751, 13757, 13759, 13763, 13781, 13789, 13799,
	13807, 13829, 13831, 13841, 13859, 13873, 13877, 13879,
	13883, 13901, 13903, 13907, 13913, 13921, 13931, 13933,
	13963, 13967, 13997, 13999, 14009, 14011, 14029, 14033,
	14051, 14057, 14071, 14081, 14083, 14087, 14107, 14143,
	14149, 14153, 14159, 14173, 14177, 14197, 14207, 14221,
	14243, 14249, 14251, 14281, 14293, 14303, 14321, 14323,
	14327, 14341, 14347, 14369, 14387, 14389, 14401, 14407,
	14411, 14419, 14423, 14431, 14437, 14447, 14449, 14461,
	14479, 14489, 14503, 14519, 14533, 14537, 14543, 14549,
	14551, 14557, 14561, 14563, 14591, 14593, 14621, 14627,
	14629, 14633, 14639, 14653, 14657, 14669, 14683, 14699,
	14713, 14717, 14723, 14731, 14737, 14741, 14747, 14753,
	14759, 14767, 14771, 14779, 14783, 14797, 14813, 14821,
	14827, 14831, 14843, 14851, 14867, 14869, 14879, 14887,
	14891, 14897, 14923, 14929, 14939, 14947, 14951, 14957,
	14969, 14983, 15013, 15017, 15031, 15053, 15061, 15073,
	15077, 15083, 15091, 15101, 15107, 15121, 15131, 15137,
	15139, 15149, 15161, 15173, 15187, 15193, 15199, 15217,
	15227, 15233, 15241, 15259, 15263, 15269, 15271, 15277,
	15287, 15289, 15299, 15307, 15313, 15319, 15329, 15331,
	15349, 15359, 15361, 15373, 15377, 15383, 15391, 15401,
	15413, 15427, 15439, 15443, 15451, 15461, 15467, 15473,
	15493, 15497, 15511, 15527, 15541, 15551, 15559, 15569,
	15581, 15583, 15601, 15607, 15619, 15629, 15641, 15643,
	15647, 15649, 15661, 15667, 15671, 15679, 15683, 15727,
	15731, 15733, 15737, 15739, 15749, 15761, 15767, 15773,
	15787, 15791, 15797, 15803, 15809, 15817, 15823, 15859,
	15877, 15881, 15887, 15889, 15901, 15907, 15913, 15919,
	15923, 15937, 15959, 15971, 15973, 15991, 16001, 16007,
	16033, 16057, 16061, 16063, 16067, 16069, 16073, 16087,
	16091, 16097, 16103, 16111, 16127, 16139, 16141, 16183,
	16187, 16189, 16193, 16217, 16223, 16229, 16231, 16249,
	16253, 16267, 16273, 16301, 16319, 16333, 16339, 16349,
	16361, 16363, 16369, 16381, 16411, 16417, 16421, 16427,
	16433, 16447, 16451, 16453, 16477, 16481, 16487, 16493,
	16519, 16529, 16547, 16553, 16561, 16567, 16573, 16603,
	16607, 16619, 16631, 16633, 16649, 16651, 16657, 16661,
	16673, 16691, 16693, 16699, 16703, 16729, 16741, 16747,
	16759, 16763, 16787, 16811, 16823, 16829, 16831, 16843,
	16871, 16879, 16883, 16889, 16901, 16903, 16921, 16927,
	16931, 16937, 16943, 16963, 16979, 16981, 16987, 16993,
	17011, 17021, 17027, 17029, 17033, 17041, 17047, 17053,
	17077, 17093, 17099, 17107, 17117, 17123, 17137, 17159,
	17167, 17183, 17189, 17191, 17203, 17207, 17209, 17231,
	17239, 17257, 17291, 17293, 17299, 17317, 17321, 17327,
	17333, 17341, 17351, 17359, 17377, 17383, 17387, 17389,
	17393, 17401, 17417, 17419, 17431, 17443, 17449, 17467,
	17471, 17477, 17483, 17489, 17491, 17497, 17509, 17519,
	17539, 17551, 17569, 17573, 17579, 17581, 17597, 17599,
	17609, 17623, 17627, 17657, 17659, 17669, 17681, 17683,
	17707, 17713, 17729, 17737, 17747, 17749, 17761, 17783,
	17789, 17791, 17807, 17827, 17837, 17839, 17851, 17863
#endif	/* !EIGHT_BIT */
};

static int witness(BIGNUM *w, const BIGNUM *a, const BIGNUM *a1,
                   const BIGNUM *a1_odd, int k, BN_CTX *ctx, BN_MONT_CTX *mont);
static int probable_prime(BIGNUM *rnd, int bits);
static int probable_prime_dh(BIGNUM *rnd, int bits,
                             const BIGNUM *add, const BIGNUM *rem, BN_CTX *ctx);
static int probable_prime_dh_safe(BIGNUM *rnd, int bits,
                                  const BIGNUM *add, const BIGNUM *rem, BN_CTX *ctx);

BIGNUM *
BN_generate_prime(BIGNUM *ret, int bits, int safe, const BIGNUM *add, const BIGNUM *rem,
                  void (*callback)(int, int, void *), void *cb_arg)
{
	BIGNUM *rnd = NULL;
	BIGNUM t;
	int found = 0;
	int i, j, c1 = 0;
	BN_CTX *ctx;
	int checks = BN_prime_checks_for_size(bits);

	BN_init(&t);
	ctx = BN_CTX_new();
	if (ctx == NULL) goto err;
	if (ret == NULL) {
		if ((rnd = BN_new()) == NULL) goto err;
	} else
		rnd = ret;
loop:
	/* make a random number and set the top and bottom bits */
	if (add == NULL) {
		if (!probable_prime(rnd, bits)) goto err;
	} else {
		if (safe) {
			if (!probable_prime_dh_safe(rnd, bits, add, rem, ctx))
				 goto err;
		} else {
			if (!probable_prime_dh(rnd, bits, add, rem, ctx))
				goto err;
		}
	}
	/* if (BN_mod_word(rnd, (BN_ULONG)3) == 1) goto loop; */
	if (callback != NULL)
		callback(0, c1++, cb_arg);

	if (!safe) {
		i = BN_is_prime_fasttest(rnd, checks, callback, ctx, cb_arg, 0);
		if (i == -1) goto err;
		if (i == 0) goto loop;
	} else {
		/* for "safe prime" generation,
		 * check that (p-1)/2 is prime.
		 * Since a prime is odd, We just
		 * need to divide by 2
		 */
		if (!BN_rshift1(&t, rnd)) goto err;

		for (i = 0; i < checks; i++) {
			j = BN_is_prime_fasttest(rnd, 1, callback, ctx, cb_arg, 0);
			if (j == -1) goto err;
			if (j == 0) goto loop;

			j = BN_is_prime_fasttest(&t, 1, callback, ctx, cb_arg, 0);
			if (j == -1) goto err;
			if (j == 0) goto loop;

			if (callback != NULL) callback(2, c1-1, cb_arg);
			/* We have a safe prime test pass */
		}
	}
	/* we have a prime :-) */
	found = 1;
err:
	if (!found && (ret == NULL) && (rnd != NULL)) BN_free(rnd);
	BN_free(&t);
	if (ctx != NULL) BN_CTX_free(ctx);
	return (found ? rnd : NULL);
}

int
BN_is_prime(const BIGNUM *a, int checks, void (*callback)(int, int, void *),
            BN_CTX *ctx_passed, void *cb_arg)
{
	return BN_is_prime_fasttest(a, checks, callback, ctx_passed, cb_arg, 0);
}

int
BN_is_prime_fasttest(const BIGNUM *a, int checks, void (*callback)(int, int, void *),
                     BN_CTX *ctx_passed, void *cb_arg, int do_trial_division)
{
	int i, j, ret = -1;
	int k;
	BN_CTX *ctx = NULL;
	BIGNUM *A1, *A1_odd, *check; /* taken from ctx */
	BN_MONT_CTX *mont = NULL;
	const BIGNUM *A = NULL;

	if (BN_cmp(a, BN_value_one()) <= 0)
		return 0;

	if (checks == BN_prime_checks)
		checks = BN_prime_checks_for_size(BN_num_bits(a));

	/* first look for small factors */
	if (!BN_is_odd(a))
		return 0;
	if (do_trial_division) {
		for (i = 1; i < NUMPRIMES; i++)
			if (BN_mod_word(a, primes[i]) == 0)
				return 0;
		if (callback != NULL)
			callback(1, -1, cb_arg);
	}

	if (ctx_passed != NULL)
		ctx = ctx_passed;
	else
		if ((ctx = BN_CTX_new()) == NULL)
			goto err;
	BN_CTX_start(ctx);

	/* A : = abs(a) */
	if (a->neg) {
		BIGNUM *t;
		if ((t = BN_CTX_get(ctx)) == NULL) goto err;
		BN_copy(t, a);
		t->neg = 0;
		A = t;
	} else
		A = a;
	A1 = BN_CTX_get(ctx);
	A1_odd = BN_CTX_get(ctx);
	check = BN_CTX_get(ctx);
	if (check == NULL) goto err;

	/* compute A1 : = A - 1 */
	if (!BN_copy(A1, A))
		goto err;
	if (!BN_sub_word(A1, 1))
		goto err;
	if (BN_is_zero(A1)) {
		ret = 0;
		goto err;
	}

	/* write  A1  as  A1_odd * 2^k */
	k = 1;
	while (!BN_is_bit_set(A1, k))
		k++;
	if (!BN_rshift(A1_odd, A1, k))
		goto err;

	/* Montgomery setup for computations mod A */
	mont = BN_MONT_CTX_new();
	if (mont == NULL)
		goto err;
	if (!BN_MONT_CTX_set(mont, A, ctx))
		goto err;

	for (i = 0; i < checks; i++) {
		if (!BN_pseudo_rand_range(check, A1))
			goto err;
		if (!BN_add_word(check, 1))
			goto err;
		/* now 1 <= check < A */

		j = witness(check, A, A1, A1_odd, k, ctx, mont);
		if (j == -1) goto err;
		if (j) {
			ret = 0;
			goto err;
		}
		if (callback != NULL)
			callback(1, i, cb_arg);
	}
	ret = 1;
err:
	if (ctx != NULL) {
		BN_CTX_end(ctx);
		if (ctx_passed == NULL)
			BN_CTX_free(ctx);
	}
	if (mont != NULL)
		BN_MONT_CTX_free(mont);

	return (ret);
}

static int
witness(BIGNUM *w, const BIGNUM *a, const BIGNUM *a1,
        const BIGNUM *a1_odd, int k, BN_CTX *ctx, BN_MONT_CTX *mont)
{
	if (!BN_mod_exp_mont(w, w, a1_odd, a, ctx, mont)) /* w : = w^a1_odd mod a */
		return -1;
	if (BN_is_one(w))
		return 0; /* probably prime */
	if (BN_cmp(w, a1) == 0)
		return 0; /* w == -1 (mod a), 'a' is probably prime */
	while (--k) {
		if (!BN_mod_mul(w, w, w, a, ctx)) /* w : = w^2 mod a */
			return -1;
		if (BN_is_one(w))
			return 1; /* 'a' is composite, otherwise a previous 'w' would
			           * have been == -1 (mod 'a')
				   */
		if (BN_cmp(w, a1) == 0)
			return 0; /* w == -1 (mod a), 'a' is probably prime */
	}
	/* If we get here, 'w' is the (a-1)/2-th power of the original 'w',
	 * and it is neither -1 nor +1 -- so 'a' cannot be prime
	 */
	return 1;
}

static int
probable_prime(BIGNUM *rnd, int bits)
{
	int i;
	BN_ULONG mods[NUMPRIMES];
	BN_ULONG delta, d;

again:
	if (!BN_rand(rnd, bits, 1, 1)) return (0);
	/* we now have a random number 'rand' to test. */
	for (i = 1; i < NUMPRIMES; i++)
		mods[i] = BN_mod_word(rnd, (BN_ULONG)primes[i]);
	delta = 0;
loop:
	for (i = 1; i < NUMPRIMES; i++) {
		/* check that rnd is not a prime and also
		 * that gcd(rnd-1, primes) == 1 (except for 2)
		 */
		if (((mods[i]+delta)%primes[i]) <= 1) {
			d = delta;
			delta += 2;
			/* perhaps need to check for overflow of
			 * delta (but delta can be up to 2^32)
			 * 21-May-98 eay - added overflow check
			 */
			if (delta < d) goto again;
			goto loop;
		}
	}
	if (!BN_add_word(rnd, delta)) return (0);
	return (1);
}

static int
probable_prime_dh(BIGNUM *rnd, int bits, const BIGNUM *add, const BIGNUM *rem, BN_CTX *ctx)
{
	int i, ret = 0;
	BIGNUM *t1;

	BN_CTX_start(ctx);
	if ((t1 = BN_CTX_get(ctx)) == NULL) goto err;

	if (!BN_rand(rnd, bits, 0, 1))
		goto err;

	/* we need ((rnd-rem) % add) == 0 */

	if (!BN_mod(t1, rnd, add, ctx))
		goto err;
	if (!BN_sub(rnd, rnd, t1))
		goto err;
	if (rem == NULL) {
		if (!BN_add_word(rnd, 1))
			goto err;
	} else {
		if (!BN_add(rnd, rnd, rem))
			goto err;
	}

	/* we now have a random number 'rand' to test. */

	loop: for (i = 1; i < NUMPRIMES; i++) {
		/* check that rnd is a prime */
		if (BN_mod_word(rnd, (BN_ULONG)primes[i]) <= 1) {
			if (!BN_add(rnd, rnd, add)) goto err;
			goto loop;
		}
	}
	ret = 1;
err:
	BN_CTX_end(ctx);
	return (ret);
}

static int
probable_prime_dh_safe(BIGNUM *p, int bits, const BIGNUM *padd, const BIGNUM *rem, BN_CTX *ctx)
{
	int i, ret = 0;
	BIGNUM *t1, *qadd, *q;

	bits--;
	BN_CTX_start(ctx);
	t1 = BN_CTX_get(ctx);
	q = BN_CTX_get(ctx);
	qadd = BN_CTX_get(ctx);
	if (qadd == NULL) goto err;

	if (!BN_rshift1(qadd, padd)) goto err;

	if (!BN_rand(q, bits, 0, 1)) goto err;

	/* we need ((rnd-rem) % add) == 0 */
	if (!BN_mod(t1, q, qadd, ctx)) goto err;
	if (!BN_sub(q, q, t1)) goto err;
	if (rem == NULL) {
		if (!BN_add_word(q, 1)) goto err;
	} else {
		if (!BN_rshift1(t1, rem)) goto err;
		if (!BN_add(q, q, t1)) goto err;
	}

	/* we now have a random number 'rand' to test. */
	if (!BN_lshift1(p, q)) goto err;
	if (!BN_add_word(p, 1)) goto err;

loop:
	for (i = 1; i < NUMPRIMES; i++) {
		/* check that p and q are prime */
		/* check that for p and q
		 * gcd(p-1, primes) == 1 (except for 2)
		 */
		if ((BN_mod_word(p, (BN_ULONG)primes[i]) == 0) ||
		    (BN_mod_word(q, (BN_ULONG)primes[i]) == 0)) {
			if (!BN_add(p, p, padd)) goto err;
			if (!BN_add(q, q, qadd)) goto err;
			goto loop;
		}
	}
	ret = 1;
err:
	BN_CTX_end(ctx);
	return (ret);
}
#endif /* BCMDH_TEST */
