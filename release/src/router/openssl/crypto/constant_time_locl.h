/* crypto/constant_time_locl.h */
/*
 * Utilities for constant-time cryptography.
 *
 * Author: Emilia Kasper (emilia@openssl.org)
 * Based on previous work by Bodo Moeller, Emilia Kasper, Adam Langley
 * (Google).
 * ====================================================================
 * Copyright (c) 2014 The OpenSSL Project.  All rights reserved.
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

#ifndef HEADER_CONSTANT_TIME_LOCL_H
#define HEADER_CONSTANT_TIME_LOCL_H

#include "e_os.h"  /* For 'inline' */

#ifdef __cplusplus
extern "C" {
#endif

/*
 * The following methods return a bitmask of all ones (0xff...f) for true
 * and 0 for false. This is useful for choosing a value based on the result
 * of a conditional in constant time. For example,
 *
 * if (a < b) {
 *   c = a;
 * } else {
 *   c = b;
 * }
 *
 * can be written as
 *
 * unsigned int lt = constant_time_lt(a, b);
 * c = a & lt | b & ~lt;
 */

/*
 * Returns the given value with the MSB copied to all the other
 * bits. Uses the fact that arithmetic shift shifts-in the sign bit.
 * However, this is not ensured by the C standard so you may need to
 * replace this with something else on odd CPUs.
 */
static inline unsigned int constant_time_msb(unsigned int a);

/*
 * Returns 0xff..f if a < b and 0 otherwise.
 */
static inline unsigned int constant_time_lt(unsigned int a, unsigned int b);
/* Convenience method for getting an 8-bit mask. */
static inline unsigned char constant_time_lt_8(unsigned int a, unsigned int b);

/*
 * Returns 0xff..f if a >= b and 0 otherwise.
 */
static inline unsigned int constant_time_ge(unsigned int a, unsigned int b);
/* Convenience method for getting an 8-bit mask. */
static inline unsigned char constant_time_ge_8(unsigned int a, unsigned int b);

/*
 * Returns 0xff..f if a == 0 and 0 otherwise.
 */
static inline unsigned int constant_time_is_zero(unsigned int a);
/* Convenience method for getting an 8-bit mask. */
static inline unsigned char constant_time_is_zero_8(unsigned int a);


/*
 * Returns 0xff..f if a == b and 0 otherwise.
 */
static inline unsigned int constant_time_eq(unsigned int a, unsigned int b);
/* Convenience method for getting an 8-bit mask. */
static inline unsigned char constant_time_eq_8(unsigned int a, unsigned int b);

static inline unsigned int constant_time_msb(unsigned int a)
	{
	return (unsigned int)((int)(a) >> (sizeof(int) * 8 - 1));
	}

static inline unsigned int constant_time_lt(unsigned int a, unsigned int b)
	{
	unsigned int lt;
	/* Case 1: msb(a) == msb(b). a < b iff the MSB of a - b is set.*/
	lt = ~(a ^ b) & (a - b);
	/* Case 2: msb(a) != msb(b). a < b iff the MSB of b is set. */
	lt |= ~a & b;
	return constant_time_msb(lt);
	}

static inline unsigned char constant_time_lt_8(unsigned int a, unsigned int b)
	{
	return (unsigned char)(constant_time_lt(a, b));
	}

static inline unsigned int constant_time_ge(unsigned int a, unsigned int b)
	{
	unsigned int ge;
	/* Case 1: msb(a) == msb(b). a >= b iff the MSB of a - b is not set.*/
	ge = ~((a ^ b) | (a - b));
	/* Case 2: msb(a) != msb(b). a >= b iff the MSB of a is set. */
	ge |= a & ~b;
	return constant_time_msb(ge);
	}

static inline unsigned char constant_time_ge_8(unsigned int a, unsigned int b)
	{
	return (unsigned char)(constant_time_ge(a, b));
	}

static inline unsigned int constant_time_is_zero(unsigned int a)
	{
	return constant_time_msb(~a & (a - 1));
	}

static inline unsigned char constant_time_is_zero_8(unsigned int a)
	{
	return (unsigned char)(constant_time_is_zero(a));
	}

static inline unsigned int constant_time_eq(unsigned int a, unsigned int b)
	{
	return constant_time_is_zero(a ^ b);
	}

static inline unsigned char constant_time_eq_8(unsigned int a, unsigned int b)
	{
	return (unsigned char)(constant_time_eq(a, b));
	}

#ifdef __cplusplus
}
#endif

#endif  /* HEADER_CONSTANT_TIME_LOCL_H */
