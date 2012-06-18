/*	$KAME: auth.c,v 1.4 2004/09/07 05:03:02 jinmei Exp $	*/

/*
 * Copyright (C) 2004 WIDE Project.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the project nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE PROJECT AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE PROJECT OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

/*
 * Copyright (C) 2004  Internet Systems Consortium, Inc. ("ISC")
 * Copyright (C) 2000, 2001  Internet Software Consortium.
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND ISC DISCLAIMS ALL WARRANTIES WITH
 * REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY
 * AND FITNESS.  IN NO EVENT SHALL ISC BE LIABLE FOR ANY SPECIAL, DIRECT,
 * INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM
 * LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE
 * OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR
 * PERFORMANCE OF THIS SOFTWARE.
 */
#include <sys/param.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/queue.h>
#if TIME_WITH_SYS_TIME
# include <sys/time.h>
# include <time.h>
#else
# if HAVE_SYS_TIME_H
#  include <sys/time.h>
# else
#  include <time.h>
# endif
#endif

#include <netinet/in.h>

#include <syslog.h>
#include <string.h>
#include <errno.h>

#include <dhcp6.h>
#include <config.h>
#include <common.h>
#include <auth.h>

#define PADLEN 64
#define IPAD 0x36
#define OPAD 0x5C

#define HMACMD5_KEYLENGTH 64

typedef struct {
	u_int32_t buf[4];
	u_int32_t bytes[2];
	u_int32_t in[16];
} md5_t;

typedef struct {
	md5_t md5ctx;
	unsigned char key[HMACMD5_KEYLENGTH];
} hmacmd5_t;

static void hmacmd5_init __P((hmacmd5_t *, const unsigned char *,
    unsigned int));
static void hmacmd5_invalidate __P((hmacmd5_t *));
static void hmacmd5_update __P((hmacmd5_t *, const unsigned char *,
    unsigned int));
static void hmacmd5_sign __P((hmacmd5_t *, unsigned char *));
static int hmacmd5_verify __P((hmacmd5_t *, unsigned char *));

static void md5_init __P((md5_t *));
static void md5_invalidate __P((md5_t *));
static void md5_final __P((md5_t *, unsigned char *));
static void md5_update __P((md5_t *, const unsigned char *, unsigned int));

int
dhcp6_validate_key(key)
	struct keyinfo *key;
{
	time_t now;

	if (key->expire == 0)	/* never expire */
		return (0);

	if (time(&now) == -1)
		return (-1);	/* treat it as expiration (XXX) */

	if (now > key->expire)
		return (-1);

	return (0);
}

int
dhcp6_calc_mac(buf, len, proto, alg, off, key)
	char *buf;
	size_t len, off;
	int proto, alg;
	struct keyinfo *key;
{
	hmacmd5_t ctx;
	unsigned char digest[MD5_DIGESTLENGTH];

	/* right now, we don't care about the protocol */

	if (alg != DHCP6_AUTHALG_HMACMD5)
		return (-1);

	if (off + MD5_DIGESTLENGTH > len) {
		/*
		 * this should be assured by the caller, but check it here
		 * for safety.
		 */
		return (-1);
	}

	hmacmd5_init(&ctx, key->secret, key->secretlen);
	hmacmd5_update(&ctx, buf, len);
	hmacmd5_sign(&ctx, digest);

	memcpy(buf + off, digest, MD5_DIGESTLENGTH);

	return (0);
}

int
dhcp6_verify_mac(buf, len, proto, alg, off, key)
	char *buf;
	ssize_t len;
	int proto, alg;
	size_t off;
	struct keyinfo *key;
{
	hmacmd5_t ctx;
	unsigned char digest[MD5_DIGESTLENGTH];
	int result;

	/* right now, we don't care about the protocol */

	if (alg != DHCP6_AUTHALG_HMACMD5)
		return (-1);

	if (off + MD5_DIGESTLENGTH > len)
		return (-1);

	/*
	 * Copy the MAC value and clear the field.
	 * XXX: should we make a local working copy?
	 */
	memcpy(digest, buf + off, sizeof(digest));
	memset(buf + off, 0, sizeof(digest));

	hmacmd5_init(&ctx, key->secret, key->secretlen);
	hmacmd5_update(&ctx, buf, len);
	result = hmacmd5_verify(&ctx, digest);

	/* copy back the digest value (XXX) */
	memcpy(buf + off, digest, sizeof(digest));

	return (result);
}

/*
 * This code implements the HMAC-MD5 keyed hash algorithm
 * described in RFC 2104.
 */
/*
 * Start HMAC-MD5 process.  Initialize an md5 context and digest the key.
 */
static void
hmacmd5_init(hmacmd5_t *ctx, const unsigned char *key, unsigned int len)
{
	unsigned char ipad[PADLEN];
	int i;

	memset(ctx->key, 0, sizeof(ctx->key));
	if (len > sizeof(ctx->key)) {
		md5_t md5ctx;
		md5_init(&md5ctx);
		md5_update(&md5ctx, key, len);
		md5_final(&md5ctx, ctx->key);
	} else
		memcpy(ctx->key, key, len);

	md5_init(&ctx->md5ctx);
	memset(ipad, IPAD, sizeof(ipad));
	for (i = 0; i < PADLEN; i++)
		ipad[i] ^= ctx->key[i];
	md5_update(&ctx->md5ctx, ipad, sizeof(ipad));
}

static void
hmacmd5_invalidate(hmacmd5_t *ctx)
{
	md5_invalidate(&ctx->md5ctx);
	memset(ctx->key, 0, sizeof(ctx->key));
	memset(ctx, 0, sizeof(ctx));
}

/*
 * Update context to reflect the concatenation of another buffer full
 * of bytes.
 */
static void
hmacmd5_update(hmacmd5_t *ctx, const unsigned char *buf, unsigned int len)
{
	md5_update(&ctx->md5ctx, buf, len);
}

/*
 * Compute signature - finalize MD5 operation and reapply MD5.
 */
static void
hmacmd5_sign(hmacmd5_t *ctx, unsigned char *digest)
{
	unsigned char opad[PADLEN];
	int i;

	md5_final(&ctx->md5ctx, digest);

	memset(opad, OPAD, sizeof(opad));
	for (i = 0; i < PADLEN; i++)
		opad[i] ^= ctx->key[i];

	md5_init(&ctx->md5ctx);
	md5_update(&ctx->md5ctx, opad, sizeof(opad));
	md5_update(&ctx->md5ctx, digest, MD5_DIGESTLENGTH);
	md5_final(&ctx->md5ctx, digest);
	hmacmd5_invalidate(ctx);
}

/*
 * Verify signature - finalize MD5 operation and reapply MD5, then
 * compare to the supplied digest.
 */
static int
hmacmd5_verify(hmacmd5_t *ctx, unsigned char *digest) {
	unsigned char newdigest[MD5_DIGESTLENGTH];

	hmacmd5_sign(ctx, newdigest);
	return (memcmp(digest, newdigest, MD5_DIGESTLENGTH));
}

/*
 * This code implements the MD5 message-digest algorithm.
 * The algorithm is due to Ron Rivest.  This code was
 * written by Colin Plumb in 1993, no copyright is claimed.
 * This code is in the public domain; do with it what you wish.
 *
 * Equivalent code is available from RSA Data Security, Inc.
 * This code has been tested against that, and is equivalent,
 * except that you don't need to include two pages of legalese
 * with every copy.
 *
 * To compute the message digest of a chunk of bytes, declare an
 * MD5Context structure, pass it to MD5Init, call MD5Update as
 * needed on buffers full of bytes, and then call MD5Final, which
 * will fill a supplied 16-byte array with the digest.
 */

static void
byteSwap(u_int32_t *buf, unsigned words)
{
	unsigned char *p = (unsigned char *)buf;

	do {
		*buf++ = (u_int32_t)((unsigned)p[3] << 8 | p[2]) << 16 |
			((unsigned)p[1] << 8 | p[0]);
		p += 4;
	} while (--words);
}

/*
 * Start MD5 accumulation.  Set bit count to 0 and buffer to mysterious
 * initialization constants.
 */
static void
md5_init(md5_t *ctx)
{
	ctx->buf[0] = 0x67452301;
	ctx->buf[1] = 0xefcdab89;
	ctx->buf[2] = 0x98badcfe;
	ctx->buf[3] = 0x10325476;

	ctx->bytes[0] = 0;
	ctx->bytes[1] = 0;
}

static void
md5_invalidate(md5_t *ctx)
{
	memset(ctx, 0, sizeof(md5_t));
}

/* The four core functions - F1 is optimized somewhat */

/* #define F1(x, y, z) (x & y | ~x & z) */
#define F1(x, y, z) (z ^ (x & (y ^ z)))
#define F2(x, y, z) F1(z, x, y)
#define F3(x, y, z) (x ^ y ^ z)
#define F4(x, y, z) (y ^ (x | ~z))

/* This is the central step in the MD5 algorithm. */
#define MD5STEP(f,w,x,y,z,in,s) \
	 (w += f(x,y,z) + in, w = (w<<s | w>>(32-s)) + x)

/*
 * The core of the MD5 algorithm, this alters an existing MD5 hash to
 * reflect the addition of 16 longwords of new data.  MD5Update blocks
 * the data and converts bytes into longwords for this routine.
 */
static void
transform(u_int32_t buf[4], u_int32_t const in[16]) {
	register u_int32_t a, b, c, d;

	a = buf[0];
	b = buf[1];
	c = buf[2];
	d = buf[3];

	MD5STEP(F1, a, b, c, d, in[0] + 0xd76aa478, 7);
	MD5STEP(F1, d, a, b, c, in[1] + 0xe8c7b756, 12);
	MD5STEP(F1, c, d, a, b, in[2] + 0x242070db, 17);
	MD5STEP(F1, b, c, d, a, in[3] + 0xc1bdceee, 22);
	MD5STEP(F1, a, b, c, d, in[4] + 0xf57c0faf, 7);
	MD5STEP(F1, d, a, b, c, in[5] + 0x4787c62a, 12);
	MD5STEP(F1, c, d, a, b, in[6] + 0xa8304613, 17);
	MD5STEP(F1, b, c, d, a, in[7] + 0xfd469501, 22);
	MD5STEP(F1, a, b, c, d, in[8] + 0x698098d8, 7);
	MD5STEP(F1, d, a, b, c, in[9] + 0x8b44f7af, 12);
	MD5STEP(F1, c, d, a, b, in[10] + 0xffff5bb1, 17);
	MD5STEP(F1, b, c, d, a, in[11] + 0x895cd7be, 22);
	MD5STEP(F1, a, b, c, d, in[12] + 0x6b901122, 7);
	MD5STEP(F1, d, a, b, c, in[13] + 0xfd987193, 12);
	MD5STEP(F1, c, d, a, b, in[14] + 0xa679438e, 17);
	MD5STEP(F1, b, c, d, a, in[15] + 0x49b40821, 22);

	MD5STEP(F2, a, b, c, d, in[1] + 0xf61e2562, 5);
	MD5STEP(F2, d, a, b, c, in[6] + 0xc040b340, 9);
	MD5STEP(F2, c, d, a, b, in[11] + 0x265e5a51, 14);
	MD5STEP(F2, b, c, d, a, in[0] + 0xe9b6c7aa, 20);
	MD5STEP(F2, a, b, c, d, in[5] + 0xd62f105d, 5);
	MD5STEP(F2, d, a, b, c, in[10] + 0x02441453, 9);
	MD5STEP(F2, c, d, a, b, in[15] + 0xd8a1e681, 14);
	MD5STEP(F2, b, c, d, a, in[4] + 0xe7d3fbc8, 20);
	MD5STEP(F2, a, b, c, d, in[9] + 0x21e1cde6, 5);
	MD5STEP(F2, d, a, b, c, in[14] + 0xc33707d6, 9);
	MD5STEP(F2, c, d, a, b, in[3] + 0xf4d50d87, 14);
	MD5STEP(F2, b, c, d, a, in[8] + 0x455a14ed, 20);
	MD5STEP(F2, a, b, c, d, in[13] + 0xa9e3e905, 5);
	MD5STEP(F2, d, a, b, c, in[2] + 0xfcefa3f8, 9);
	MD5STEP(F2, c, d, a, b, in[7] + 0x676f02d9, 14);
	MD5STEP(F2, b, c, d, a, in[12] + 0x8d2a4c8a, 20);

	MD5STEP(F3, a, b, c, d, in[5] + 0xfffa3942, 4);
	MD5STEP(F3, d, a, b, c, in[8] + 0x8771f681, 11);
	MD5STEP(F3, c, d, a, b, in[11] + 0x6d9d6122, 16);
	MD5STEP(F3, b, c, d, a, in[14] + 0xfde5380c, 23);
	MD5STEP(F3, a, b, c, d, in[1] + 0xa4beea44, 4);
	MD5STEP(F3, d, a, b, c, in[4] + 0x4bdecfa9, 11);
	MD5STEP(F3, c, d, a, b, in[7] + 0xf6bb4b60, 16);
	MD5STEP(F3, b, c, d, a, in[10] + 0xbebfbc70, 23);
	MD5STEP(F3, a, b, c, d, in[13] + 0x289b7ec6, 4);
	MD5STEP(F3, d, a, b, c, in[0] + 0xeaa127fa, 11);
	MD5STEP(F3, c, d, a, b, in[3] + 0xd4ef3085, 16);
	MD5STEP(F3, b, c, d, a, in[6] + 0x04881d05, 23);
	MD5STEP(F3, a, b, c, d, in[9] + 0xd9d4d039, 4);
	MD5STEP(F3, d, a, b, c, in[12] + 0xe6db99e5, 11);
	MD5STEP(F3, c, d, a, b, in[15] + 0x1fa27cf8, 16);
	MD5STEP(F3, b, c, d, a, in[2] + 0xc4ac5665, 23);

	MD5STEP(F4, a, b, c, d, in[0] + 0xf4292244, 6);
	MD5STEP(F4, d, a, b, c, in[7] + 0x432aff97, 10);
	MD5STEP(F4, c, d, a, b, in[14] + 0xab9423a7, 15);
	MD5STEP(F4, b, c, d, a, in[5] + 0xfc93a039, 21);
	MD5STEP(F4, a, b, c, d, in[12] + 0x655b59c3, 6);
	MD5STEP(F4, d, a, b, c, in[3] + 0x8f0ccc92, 10);
	MD5STEP(F4, c, d, a, b, in[10] + 0xffeff47d, 15);
	MD5STEP(F4, b, c, d, a, in[1] + 0x85845dd1, 21);
	MD5STEP(F4, a, b, c, d, in[8] + 0x6fa87e4f, 6);
	MD5STEP(F4, d, a, b, c, in[15] + 0xfe2ce6e0, 10);
	MD5STEP(F4, c, d, a, b, in[6] + 0xa3014314, 15);
	MD5STEP(F4, b, c, d, a, in[13] + 0x4e0811a1, 21);
	MD5STEP(F4, a, b, c, d, in[4] + 0xf7537e82, 6);
	MD5STEP(F4, d, a, b, c, in[11] + 0xbd3af235, 10);
	MD5STEP(F4, c, d, a, b, in[2] + 0x2ad7d2bb, 15);
	MD5STEP(F4, b, c, d, a, in[9] + 0xeb86d391, 21);

	buf[0] += a;
	buf[1] += b;
	buf[2] += c;
	buf[3] += d;
}

/*
 * Update context to reflect the concatenation of another buffer full
 * of bytes.
 */
static void
md5_update(md5_t *ctx, const unsigned char *buf, unsigned int len)
{
	u_int32_t t;

	/* Update byte count */

	t = ctx->bytes[0];
	if ((ctx->bytes[0] = t + len) < t)
		ctx->bytes[1]++;	/* Carry from low to high */

	t = 64 - (t & 0x3f);	/* Space available in ctx->in (at least 1) */
	if (t > len) {
		memcpy((unsigned char *)ctx->in + 64 - t, buf, len);
		return;
	}
	/* First chunk is an odd size */
	memcpy((unsigned char *)ctx->in + 64 - t, buf, t);
	byteSwap(ctx->in, 16);
	transform(ctx->buf, ctx->in);
	buf += t;
	len -= t;

	/* Process data in 64-byte chunks */
	while (len >= 64) {
		memcpy(ctx->in, buf, 64);
		byteSwap(ctx->in, 16);
		transform(ctx->buf, ctx->in);
		buf += 64;
		len -= 64;
	}

	/* Handle any remaining bytes of data. */
	memcpy(ctx->in, buf, len);
}

/*
 * Final wrapup - pad to 64-byte boundary with the bit pattern
 * 1 0* (64-bit count of bits processed, MSB-first)
 */
static void
md5_final(md5_t *ctx, unsigned char *digest)
{
	int count = ctx->bytes[0] & 0x3f;    /* Number of bytes in ctx->in */
	unsigned char *p = (unsigned char *)ctx->in + count;

	/* Set the first char of padding to 0x80.  There is always room. */
	*p++ = 0x80;

	/* Bytes of padding needed to make 56 bytes (-8..55) */
	count = 56 - 1 - count;

	if (count < 0) {	/* Padding forces an extra block */
		memset(p, 0, count + 8);
		byteSwap(ctx->in, 16);
		transform(ctx->buf, ctx->in);
		p = (unsigned char *)ctx->in;
		count = 56;
	}
	memset(p, 0, count);
	byteSwap(ctx->in, 14);

	/* Append length in bits and transform */
	ctx->in[14] = ctx->bytes[0] << 3;
	ctx->in[15] = ctx->bytes[1] << 3 | ctx->bytes[0] >> 29;
	transform(ctx->buf, ctx->in);

	byteSwap(ctx->buf, 4);
	memcpy(digest, ctx->buf, 16);
	memset(ctx, 0, sizeof(md5_t));	/* In case it's sensitive */
}
