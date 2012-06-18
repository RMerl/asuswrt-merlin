/* vi: set sw=4 ts=4: */
/*
 *  md5.c - Compute MD5 checksum of strings according to the
 *          definition of MD5 in RFC 1321 from April 1992.
 *
 *  Written by Ulrich Drepper <drepper@gnu.ai.mit.edu>, 1995.
 *
 *  Copyright (C) 1995-1999 Free Software Foundation, Inc.
 *  Copyright (C) 2001 Manuel Novoa III
 *  Copyright (C) 2003 Glenn L. McGrath
 *  Copyright (C) 2003 Erik Andersen
 *
 *  Licensed under the GPL v2 or later, see the file LICENSE in this tarball.
 */

#include "libbb.h"

/* 0: fastest, 3: smallest */
#if CONFIG_MD5_SIZE_VS_SPEED < 0
# define MD5_SIZE_VS_SPEED 0
#elif CONFIG_MD5_SIZE_VS_SPEED > 3
# define MD5_SIZE_VS_SPEED 3
#else
# define MD5_SIZE_VS_SPEED CONFIG_MD5_SIZE_VS_SPEED
#endif

/* Initialize structure containing state of computation.
 * (RFC 1321, 3.3: Step 3)
 */
void FAST_FUNC md5_begin(md5_ctx_t *ctx)
{
	ctx->A = 0x67452301;
	ctx->B = 0xefcdab89;
	ctx->C = 0x98badcfe;
	ctx->D = 0x10325476;
	ctx->total = 0;
	ctx->buflen = 0;
}

/* These are the four functions used in the four steps of the MD5 algorithm
 * and defined in the RFC 1321.  The first function is a little bit optimized
 * (as found in Colin Plumbs public domain implementation).
 * #define FF(b, c, d) ((b & c) | (~b & d))
 */
#define FF(b, c, d) (d ^ (b & (c ^ d)))
#define FG(b, c, d) FF(d, b, c)
#define FH(b, c, d) (b ^ c ^ d)
#define FI(b, c, d) (c ^ (b | ~d))

#define rotl32(w, s) (((w) << (s)) | ((w) >> (32 - (s))))

/* Hash a single block, 64 bytes long and 4-byte aligned. */
static void md5_hash_block(const void *buffer, md5_ctx_t *ctx)
{
	uint32_t correct_words[16];
	const uint32_t *words = buffer;

#if MD5_SIZE_VS_SPEED > 0
	static const uint32_t C_array[] = {
		/* round 1 */
		0xd76aa478, 0xe8c7b756, 0x242070db, 0xc1bdceee,
		0xf57c0faf, 0x4787c62a, 0xa8304613, 0xfd469501,
		0x698098d8, 0x8b44f7af, 0xffff5bb1, 0x895cd7be,
		0x6b901122, 0xfd987193, 0xa679438e, 0x49b40821,
		/* round 2 */
		0xf61e2562, 0xc040b340, 0x265e5a51, 0xe9b6c7aa,
		0xd62f105d, 0x2441453, 0xd8a1e681, 0xe7d3fbc8,
		0x21e1cde6, 0xc33707d6, 0xf4d50d87, 0x455a14ed,
		0xa9e3e905, 0xfcefa3f8, 0x676f02d9, 0x8d2a4c8a,
		/* round 3 */
		0xfffa3942, 0x8771f681, 0x6d9d6122, 0xfde5380c,
		0xa4beea44, 0x4bdecfa9, 0xf6bb4b60, 0xbebfbc70,
		0x289b7ec6, 0xeaa127fa, 0xd4ef3085, 0x4881d05,
		0xd9d4d039, 0xe6db99e5, 0x1fa27cf8, 0xc4ac5665,
		/* round 4 */
		0xf4292244, 0x432aff97, 0xab9423a7, 0xfc93a039,
		0x655b59c3, 0x8f0ccc92, 0xffeff47d, 0x85845dd1,
		0x6fa87e4f, 0xfe2ce6e0, 0xa3014314, 0x4e0811a1,
		0xf7537e82, 0xbd3af235, 0x2ad7d2bb, 0xeb86d391
	};
	static const char P_array[] ALIGN1 = {
# if MD5_SIZE_VS_SPEED > 1
		0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15,	/* 1 */
# endif
		1, 6, 11, 0, 5, 10, 15, 4, 9, 14, 3, 8, 13, 2, 7, 12,	/* 2 */
		5, 8, 11, 14, 1, 4, 7, 10, 13, 0, 3, 6, 9, 12, 15, 2,	/* 3 */
		0, 7, 14, 5, 12, 3, 10, 1, 8, 15, 6, 13, 4, 11, 2, 9	/* 4 */
	};
# if MD5_SIZE_VS_SPEED > 1
	static const char S_array[] ALIGN1 = {
		7, 12, 17, 22,
		5, 9, 14, 20,
		4, 11, 16, 23,
		6, 10, 15, 21
	};
# endif	/* MD5_SIZE_VS_SPEED > 1 */
#endif
	uint32_t A = ctx->A;
	uint32_t B = ctx->B;
	uint32_t C = ctx->C;
	uint32_t D = ctx->D;

	/* Process all bytes in the buffer with 64 bytes in each round of
	   the loop.  */
	uint32_t *cwp = correct_words;
	uint32_t A_save = A;
	uint32_t B_save = B;
	uint32_t C_save = C;
	uint32_t D_save = D;

#if MD5_SIZE_VS_SPEED > 1
	const uint32_t *pc;
	const char *pp;
	const char *ps;
	int i;
	uint32_t temp;

	for (i = 0; i < 16; i++)
		cwp[i] = SWAP_LE32(words[i]);
	words += 16;

# if MD5_SIZE_VS_SPEED > 2
	pc = C_array;
	pp = P_array;
	ps = S_array - 4;

	for (i = 0; i < 64; i++) {
		if ((i & 0x0f) == 0)
			ps += 4;
		temp = A;
		switch (i >> 4) {
		case 0:
			temp += FF(B, C, D);
			break;
		case 1:
			temp += FG(B, C, D);
			break;
		case 2:
			temp += FH(B, C, D);
			break;
		case 3:
			temp += FI(B, C, D);
		}
		temp += cwp[(int) (*pp++)] + *pc++;
		temp = rotl32(temp, ps[i & 3]);
		temp += B;
		A = D;
		D = C;
		C = B;
		B = temp;
	}
# else
	pc = C_array;
	pp = P_array;
	ps = S_array;

	for (i = 0; i < 16; i++) {
		temp = A + FF(B, C, D) + cwp[(int) (*pp++)] + *pc++;
		temp = rotl32(temp, ps[i & 3]);
		temp += B;
		A = D;
		D = C;
		C = B;
		B = temp;
	}
	ps += 4;
	for (i = 0; i < 16; i++) {
		temp = A + FG(B, C, D) + cwp[(int) (*pp++)] + *pc++;
		temp = rotl32(temp, ps[i & 3]);
		temp += B;
		A = D;
		D = C;
		C = B;
		B = temp;
	}
	ps += 4;
	for (i = 0; i < 16; i++) {
		temp = A + FH(B, C, D) + cwp[(int) (*pp++)] + *pc++;
		temp = rotl32(temp, ps[i & 3]);
		temp += B;
		A = D;
		D = C;
		C = B;
		B = temp;
	}
	ps += 4;
	for (i = 0; i < 16; i++) {
		temp = A + FI(B, C, D) + cwp[(int) (*pp++)] + *pc++;
		temp = rotl32(temp, ps[i & 3]);
		temp += B;
		A = D;
		D = C;
		C = B;
		B = temp;
	}

# endif /* MD5_SIZE_VS_SPEED > 2 */
#else
	/* First round: using the given function, the context and a constant
	   the next context is computed.  Because the algorithms processing
	   unit is a 32-bit word and it is determined to work on words in
	   little endian byte order we perhaps have to change the byte order
	   before the computation.  To reduce the work for the next steps
	   we store the swapped words in the array CORRECT_WORDS.  */
# define OP(a, b, c, d, s, T) \
	do { \
		a += FF(b, c, d) + (*cwp++ = SWAP_LE32(*words)) + T; \
		++words; \
		a = rotl32(a, s); \
		a += b; \
	} while (0)

	/* Before we start, one word to the strange constants.
	   They are defined in RFC 1321 as
	   T[i] = (int)(4294967296.0 * fabs(sin(i))), i=1..64
	 */

# if MD5_SIZE_VS_SPEED == 1
	const uint32_t *pc;
	const char *pp;
	int i;
# endif	/* MD5_SIZE_VS_SPEED */

	/* Round 1.  */
# if MD5_SIZE_VS_SPEED == 1
	pc = C_array;
	for (i = 0; i < 4; i++) {
		OP(A, B, C, D, 7, *pc++);
		OP(D, A, B, C, 12, *pc++);
		OP(C, D, A, B, 17, *pc++);
		OP(B, C, D, A, 22, *pc++);
	}
# else
	OP(A, B, C, D, 7, 0xd76aa478);
	OP(D, A, B, C, 12, 0xe8c7b756);
	OP(C, D, A, B, 17, 0x242070db);
	OP(B, C, D, A, 22, 0xc1bdceee);
	OP(A, B, C, D, 7, 0xf57c0faf);
	OP(D, A, B, C, 12, 0x4787c62a);
	OP(C, D, A, B, 17, 0xa8304613);
	OP(B, C, D, A, 22, 0xfd469501);
	OP(A, B, C, D, 7, 0x698098d8);
	OP(D, A, B, C, 12, 0x8b44f7af);
	OP(C, D, A, B, 17, 0xffff5bb1);
	OP(B, C, D, A, 22, 0x895cd7be);
	OP(A, B, C, D, 7, 0x6b901122);
	OP(D, A, B, C, 12, 0xfd987193);
	OP(C, D, A, B, 17, 0xa679438e);
	OP(B, C, D, A, 22, 0x49b40821);
# endif /* MD5_SIZE_VS_SPEED == 1 */

	/* For the second to fourth round we have the possibly swapped words
	   in CORRECT_WORDS.  Redefine the macro to take an additional first
	   argument specifying the function to use.  */
# undef OP
# define OP(f, a, b, c, d, k, s, T) \
	do { \
		a += f(b, c, d) + correct_words[k] + T; \
		a = rotl32(a, s); \
		a += b; \
	} while (0)

	/* Round 2.  */
# if MD5_SIZE_VS_SPEED == 1
	pp = P_array;
	for (i = 0; i < 4; i++) {
		OP(FG, A, B, C, D, (int) (*pp++), 5, *pc++);
		OP(FG, D, A, B, C, (int) (*pp++), 9, *pc++);
		OP(FG, C, D, A, B, (int) (*pp++), 14, *pc++);
		OP(FG, B, C, D, A, (int) (*pp++), 20, *pc++);
	}
# else
	OP(FG, A, B, C, D, 1, 5, 0xf61e2562);
	OP(FG, D, A, B, C, 6, 9, 0xc040b340);
	OP(FG, C, D, A, B, 11, 14, 0x265e5a51);
	OP(FG, B, C, D, A, 0, 20, 0xe9b6c7aa);
	OP(FG, A, B, C, D, 5, 5, 0xd62f105d);
	OP(FG, D, A, B, C, 10, 9, 0x02441453);
	OP(FG, C, D, A, B, 15, 14, 0xd8a1e681);
	OP(FG, B, C, D, A, 4, 20, 0xe7d3fbc8);
	OP(FG, A, B, C, D, 9, 5, 0x21e1cde6);
	OP(FG, D, A, B, C, 14, 9, 0xc33707d6);
	OP(FG, C, D, A, B, 3, 14, 0xf4d50d87);
	OP(FG, B, C, D, A, 8, 20, 0x455a14ed);
	OP(FG, A, B, C, D, 13, 5, 0xa9e3e905);
	OP(FG, D, A, B, C, 2, 9, 0xfcefa3f8);
	OP(FG, C, D, A, B, 7, 14, 0x676f02d9);
	OP(FG, B, C, D, A, 12, 20, 0x8d2a4c8a);
# endif /* MD5_SIZE_VS_SPEED == 1 */

	/* Round 3.  */
# if MD5_SIZE_VS_SPEED == 1
	for (i = 0; i < 4; i++) {
		OP(FH, A, B, C, D, (int) (*pp++), 4, *pc++);
		OP(FH, D, A, B, C, (int) (*pp++), 11, *pc++);
		OP(FH, C, D, A, B, (int) (*pp++), 16, *pc++);
		OP(FH, B, C, D, A, (int) (*pp++), 23, *pc++);
	}
# else
	OP(FH, A, B, C, D, 5, 4, 0xfffa3942);
	OP(FH, D, A, B, C, 8, 11, 0x8771f681);
	OP(FH, C, D, A, B, 11, 16, 0x6d9d6122);
	OP(FH, B, C, D, A, 14, 23, 0xfde5380c);
	OP(FH, A, B, C, D, 1, 4, 0xa4beea44);
	OP(FH, D, A, B, C, 4, 11, 0x4bdecfa9);
	OP(FH, C, D, A, B, 7, 16, 0xf6bb4b60);
	OP(FH, B, C, D, A, 10, 23, 0xbebfbc70);
	OP(FH, A, B, C, D, 13, 4, 0x289b7ec6);
	OP(FH, D, A, B, C, 0, 11, 0xeaa127fa);
	OP(FH, C, D, A, B, 3, 16, 0xd4ef3085);
	OP(FH, B, C, D, A, 6, 23, 0x04881d05);
	OP(FH, A, B, C, D, 9, 4, 0xd9d4d039);
	OP(FH, D, A, B, C, 12, 11, 0xe6db99e5);
	OP(FH, C, D, A, B, 15, 16, 0x1fa27cf8);
	OP(FH, B, C, D, A, 2, 23, 0xc4ac5665);
# endif /* MD5_SIZE_VS_SPEED == 1 */

	/* Round 4.  */
# if MD5_SIZE_VS_SPEED == 1
	for (i = 0; i < 4; i++) {
		OP(FI, A, B, C, D, (int) (*pp++), 6, *pc++);
		OP(FI, D, A, B, C, (int) (*pp++), 10, *pc++);
		OP(FI, C, D, A, B, (int) (*pp++), 15, *pc++);
		OP(FI, B, C, D, A, (int) (*pp++), 21, *pc++);
	}
# else
	OP(FI, A, B, C, D, 0, 6, 0xf4292244);
	OP(FI, D, A, B, C, 7, 10, 0x432aff97);
	OP(FI, C, D, A, B, 14, 15, 0xab9423a7);
	OP(FI, B, C, D, A, 5, 21, 0xfc93a039);
	OP(FI, A, B, C, D, 12, 6, 0x655b59c3);
	OP(FI, D, A, B, C, 3, 10, 0x8f0ccc92);
	OP(FI, C, D, A, B, 10, 15, 0xffeff47d);
	OP(FI, B, C, D, A, 1, 21, 0x85845dd1);
	OP(FI, A, B, C, D, 8, 6, 0x6fa87e4f);
	OP(FI, D, A, B, C, 15, 10, 0xfe2ce6e0);
	OP(FI, C, D, A, B, 6, 15, 0xa3014314);
	OP(FI, B, C, D, A, 13, 21, 0x4e0811a1);
	OP(FI, A, B, C, D, 4, 6, 0xf7537e82);
	OP(FI, D, A, B, C, 11, 10, 0xbd3af235);
	OP(FI, C, D, A, B, 2, 15, 0x2ad7d2bb);
	OP(FI, B, C, D, A, 9, 21, 0xeb86d391);
# endif	/* MD5_SIZE_VS_SPEED == 1 */
#endif	/* MD5_SIZE_VS_SPEED > 1 */

	/* Add the starting values of the context.  */
	A += A_save;
	B += B_save;
	C += C_save;
	D += D_save;

	/* Put checksum in context given as argument.  */
	ctx->A = A;
	ctx->B = B;
	ctx->C = C;
	ctx->D = D;
}

/* Feed data through a temporary buffer to call md5_hash_aligned_block()
 * with chunks of data that are 4-byte aligned and a multiple of 64 bytes.
 * This function's internal buffer remembers previous data until it has 64
 * bytes worth to pass on.  Call md5_end() to flush this buffer. */
void FAST_FUNC md5_hash(const void *buffer, size_t len, md5_ctx_t *ctx)
{
	char *buf = (char *)buffer;

	/* RFC 1321 specifies the possible length of the file up to 2^64 bits,
	 * Here we only track the number of bytes.  */
	ctx->total += len;

	/* Process all input. */
	while (len) {
		unsigned i = 64 - ctx->buflen;

		/* Copy data into aligned buffer. */
		if (i > len)
			i = len;
		memcpy(ctx->buffer + ctx->buflen, buf, i);
		len -= i;
		ctx->buflen += i;
		buf += i;

		/* When buffer fills up, process it. */
		if (ctx->buflen == 64) {
			md5_hash_block(ctx->buffer, ctx);
			ctx->buflen = 0;
		}
	}
}

/* Process the remaining bytes in the buffer and put result from CTX
 * in first 16 bytes following RESBUF.  The result is always in little
 * endian byte order, so that a byte-wise output yields to the wanted
 * ASCII representation of the message digest.
 */
void FAST_FUNC md5_end(void *resbuf, md5_ctx_t *ctx)
{
	char *buf = ctx->buffer;
	int i;

	/* Pad data to block size.  */
	buf[ctx->buflen++] = 0x80;
	memset(buf + ctx->buflen, 0, 128 - ctx->buflen);

	/* Put the 64-bit file length in *bits* at the end of the buffer.  */
	ctx->total <<= 3;
	if (ctx->buflen > 56)
		buf += 64;
	for (i = 0; i < 8; i++)
		buf[56 + i] = ctx->total >> (i*8);

	/* Process last bytes.  */
	if (buf != ctx->buffer)
		md5_hash_block(ctx->buffer, ctx);
	md5_hash_block(buf, ctx);

	/* The MD5 result is in little endian byte order.
	 * We (ab)use the fact that A-D are consecutive in memory.
	 */
#if BB_BIG_ENDIAN
	ctx->A = SWAP_LE32(ctx->A);
	ctx->B = SWAP_LE32(ctx->B);
	ctx->C = SWAP_LE32(ctx->C);
	ctx->D = SWAP_LE32(ctx->D);
#endif
	memcpy(resbuf, &ctx->A, sizeof(ctx->A) * 4);
}
