/*	
 *	mpi.h
 *	Release $Name: MATRIXSSL_1_8_8_OPEN $
 *
 *	multiple-precision integer library
 */
/*
 *	Copyright (c) PeerSec Networks, 2002-2009. All Rights Reserved.
 *	The latest version of this code is available at http://www.matrixssl.org
 *
 *	This software is open source; you can redistribute it and/or modify
 *	it under the terms of the GNU General Public License as published by
 *	the Free Software Foundation; either version 2 of the License, or
 *	(at your option) any later version.
 *
 *	This General Public License does NOT permit incorporating this software 
 *	into proprietary programs.  If you are unable to comply with the GPL, a 
 *	commercial license for this software may be purchased from PeerSec Networks
 *	at http://www.peersec.com
 *	
 *	This program is distributed in WITHOUT ANY WARRANTY; without even the 
 *	implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. 
 *	See the GNU General Public License for more details.
 *	
 *	You should have received a copy of the GNU General Public License
 *	along with this program; if not, write to the Free Software
 *	Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *	http://www.gnu.org/copyleft/gpl.html
 */
/******************************************************************************/


#ifndef _h_MPI
#define _h_MPI

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include <limits.h>

#undef MIN
#define MIN(x,y) ((x)<(y)?(x):(y))
#undef MAX
#define MAX(x,y) ((x)>(y)?(x):(y))

#ifdef __cplusplus
extern "C" {


/*
	C++ compilers don't like assigning void * to mp_digit *
 */
#define  OPT_CAST(x)  (x *)

#else

/*
	C on the other hand doesn't care
 */
#define  OPT_CAST(x)

#endif /* __cplusplus */

/******************************************************************************/
/*
	some default configurations.

	A "mp_digit" must be able to hold DIGIT_BIT + 1 bits
	A "mp_word" must be able to hold 2*DIGIT_BIT + 1 bits

	At the very least a mp_digit must be able to hold 7 bits
	[any size beyond that is ok provided it doesn't overflow the data type]
 */
#ifdef MP_8BIT
	typedef unsigned char		mp_digit;
	typedef unsigned short		mp_word;
#elif defined(MP_16BIT)
	typedef unsigned short		mp_digit;
	typedef unsigned long		mp_word;
#elif defined(MP_64BIT)
/*
	for GCC only on supported platforms
 */
	#ifndef CRYPT
		typedef unsigned long long	ulong64;
		typedef signed long long	long64;
	#endif /* CRYPT */

	typedef ulong64				mp_digit;
	typedef unsigned long		mp_word __attribute__ ((mode(TI)));

	#define DIGIT_BIT			60
#else  /* MP_8BIT */
/*
	this is the default case, 28-bit digits
 */
	#ifndef CRYPT
		#if defined(_MSC_VER) || defined(__BORLANDC__) 
			typedef unsigned __int64	ulong64;
			typedef signed __int64		long64;
		#else
			typedef unsigned long long	ulong64;
			typedef signed long long	long64;
		#endif
	#endif /* CRYPT */

	typedef unsigned long		mp_digit;
	typedef ulong64				mp_word;

	#ifdef MP_31BIT
/*
		this is an extension that uses 31-bit digits
 */
		#define DIGIT_BIT		31
	#else /* MP_31BIT */
/*
		default case is 28-bit digits, defines MP_28BIT as a handy macro to test
 */
		#define DIGIT_BIT		28
		#define MP_28BIT
	#endif /* MP_31BIT */
#endif /* MP_8BIT */

/*
	otherwise the bits per digit is calculated automatically from the size of
	a mp_digit
 */
#ifndef DIGIT_BIT
	#define DIGIT_BIT	((int32)((CHAR_BIT * sizeof(mp_digit) - 1)))	/* bits per digit */
#endif /* DIGIT_BIT */

#define MP_DIGIT_BIT		DIGIT_BIT
#define MP_MASK				((((mp_digit)1)<<((mp_digit)DIGIT_BIT))-((mp_digit)1))
#define MP_DIGIT_MAX		MP_MASK

/******************************************************************************/
/*
	equalities
 */
#define MP_LT			-1		/* less than */
#define MP_EQ			0		/* equal to */
#define MP_GT			1		/* greater than */

#define MP_ZPOS			0		/* positive integer */
#define MP_NEG			1		/* negative */

#define MP_OKAY			0		/* ok result */
#define MP_MEM			-2		/* out of mem */
#define MP_VAL			-3		/* invalid input */
#define MP_RANGE		MP_VAL

#define MP_YES			1		/* yes response */
#define MP_NO			0		/* no response */

typedef int32				mp_err;

/******************************************************************************/
/*
	various build options
 */
#define MP_PREC			64		/* default digits of precision */

/*
	define this to use lower memory usage routines (exptmods mostly)
 */
#define MP_LOW_MEM

/*
	size of comba arrays, should be at least 
	2 * 2**(BITS_PER_WORD - BITS_PER_DIGIT*2)
 */
#define MP_WARRAY		(1 << (sizeof(mp_word) * CHAR_BIT - 2 * DIGIT_BIT + 1))

typedef struct  {
	int32 used, alloc, sign;
	mp_digit *dp;
} mp_int;

#define USED(m)		((m)->used)
#define DIGIT(m,k)	((m)->dp[(k)])
#define SIGN(m)		((m)->sign)

/******************************************************************************/
/*
	init and deinit bignum functions
 */

/*
	init a bignum
 */
extern int32 mp_init(psPool_t *pool, mp_int *a);

/*
	free a bignum
 */
extern void mp_clear(mp_int *a);

/*
	init a series of arguments
 */
extern int32 _mp_init_multi(psPool_t *pool, mp_int *mp0, mp_int *mp1, mp_int *mp2, 
							mp_int *mp3, mp_int *mp4, mp_int *mp5, mp_int *mp6, 
							mp_int *mp7);

/*
	clear a  series of arguments
 */
extern void _mp_clear_multi(mp_int *mp0, mp_int *mp1, mp_int *mp2, mp_int *mp3,
					mp_int *mp4, mp_int *mp5, mp_int *mp6, mp_int *mp7);

/*
	exchange two ints
 */
extern void mp_exch(mp_int *a, mp_int *b);

/*
	shrink ram required for a bignum
 */
extern int32 mp_shrink(mp_int *a);

/*
	grow an int32 to a given size
 */
extern int32 mp_grow(mp_int *a, int32 size);

/*
	init to a given number of digits
 */
extern int32 mp_init_size(psPool_t *pool, mp_int *a, int32 size);

/******************************************************************************/
/*
	Basic Manipulations
 */
#define mp_iszero(a) (((a)->used == 0) ? MP_YES : MP_NO)
#define mp_iseven(a) (((a)->used > 0 && (((a)->dp[0] & 1) == 0)) ? MP_YES : MP_NO)
#define mp_isodd(a)  (((a)->used > 0 && (((a)->dp[0] & 1) == 1)) ? MP_YES : MP_NO)

extern int32 mp_add_d (mp_int * a, mp_digit b, mp_int * c);
extern int32 mp_sub_d (mp_int * a, mp_digit b, mp_int * c);
/*
	set to zero
 */
extern void mp_zero(mp_int *a);

/*
	set to a digit
 */
extern void mp_set(mp_int *a, mp_digit b);

/*
	copy, b = a
 */
extern int32 mp_copy(mp_int *a, mp_int *b);

/*
	inits and copies, a = b
 */
extern int32 mp_init_copy(psPool_t *pool, mp_int *a, mp_int *b);

/*
	trim unused digits
 */
extern void mp_clamp(mp_int *a);

/******************************************************************************/
/*
	digit manipulation
*/

/*
	right shift by "b" digits
 */
extern void mp_rshd(mp_int *a, int32 b);

/*
	left shift by "b" digits
 */
extern int32 mp_lshd(mp_int *a, int32 b);

/*
	c = a / 2**b
 */
extern int32 mp_div_2d(psPool_t *pool, mp_int *a, int32 b, mp_int *c, mp_int *d);

/*
	b = a/2
 */
extern int32 mp_div_2(mp_int *a, mp_int *b);

/*
	c = a * 2**b
 */
extern int32 mp_mul_2d(mp_int *a, int32 b, mp_int *c);

/*
	c = a mod 2**d
 */
extern int32 mp_mod_2d(mp_int *a, int32 b, mp_int *c);

/*
	computes a = 2**b
 */
extern int32 mp_2expt(mp_int *a, int32 b);

/******************************************************************************/
/*
	Basic arithmetic
 */

/*
	b = |a|
 */
extern int32 mp_abs(mp_int *a, mp_int *b);

/*
	compare a to b
 */
extern int32 mp_cmp(mp_int *a, mp_int *b);

/*
	compare |a| to |b|
 */
extern int32 mp_cmp_mag(mp_int *a, mp_int *b);

/*
	c = a + b
 */
extern int32 mp_add(mp_int *a, mp_int *b, mp_int *c);

/*
	c = a - b
 */
extern int32 mp_sub(mp_int *a, mp_int *b, mp_int *c);

/*
	c = a * b
	b = a*a
 */
/* moved mp_mul out of SLOW case */
extern int32 mp_mul(psPool_t *pool, mp_int *a, mp_int *b, mp_int *c);
#ifdef USE_SMALL_WORD
extern int32 mp_sqr(psPool_t *pool, mp_int *a, mp_int *b);
#endif

/*
	a/b => cb + d == a
 */
extern int32 mp_div(psPool_t *pool, mp_int *a, mp_int *b, mp_int *c, mp_int *d);

/*
	c = a mod b, 0 <= c < b
 */
extern int32 mp_mod(psPool_t *pool, mp_int *a, mp_int *b, mp_int *c);

/******************************************************************************/
/*
	single digit functions
 */

/*
	compare against a single digit
 */
extern int32 mp_cmp_d(mp_int *a, mp_digit b);

/*
	c = a * b
 */
extern int32 mp_mul_d(mp_int *a, mp_digit b, mp_int *c);

/******************************************************************************/
/*
	number theory
 */

/*
	d = a + b (mod c)
 */
extern int32 mp_addmod(psPool_t *pool, mp_int *a, mp_int *b, mp_int *c, mp_int *d);

/*
	d = a * b (mod c)
 */
extern int32 mp_mulmod(psPool_t *pool, mp_int *a, mp_int *b, mp_int *c, mp_int *d);

/*
	c = 1/a (mod b)
 */
extern int32 mp_invmodSSH(psPool_t *pool, mp_int *a, mp_int *b, mp_int *c);

/*
	setups the montgomery reduction
 */
extern int32 mp_montgomery_setup(mp_int *a, mp_digit *mp);

/*
	computes a = B**n mod b without division or multiplication useful for
	normalizing numbers in a Montgomery system.
 */
extern int32 mp_montgomery_calc_normalization(mp_int *a, mp_int *b);

/*
	computes x/R == x (mod N) via Montgomery Reduction
 */
#ifdef USE_SMALL_WORD
extern int32 mp_montgomery_reduce(mp_int *a, mp_int *m, mp_digit mp);
#endif

/*
	d = a**b (mod c)
 */
extern int32 mp_exptmod(psPool_t *pool, mp_int *a, mp_int *b, mp_int *c, mp_int *d);

/******************************************************************************/
/*
	If we're using 1024 or 2048 bit keys and 28 bit digits, we only need the
	fast_ versions of these functions, removing the others to save space.
	Otherwise, we include the slow versions as well and which version to use
	is done at runtime.
*/
#ifdef USE_SMALL_WORD
extern int32 s_mp_mul_digs(psPool_t *pool, mp_int *a, mp_int *b, mp_int *c,
						   int32 digs);
extern int32 s_mp_sqr(psPool_t *pool, mp_int *a, mp_int *b);

#else
#define mp_montgomery_reduce fast_mp_montgomery_reduce
#define mp_sqr	fast_s_mp_sqr
#define s_mp_mul_digs	fast_s_mp_mul_digs
#define mp_invmod	fast_mp_invmod
#endif

/******************************************************************************/
/*
	radix conversion
 */
extern int32 mp_count_bits(mp_int *a);

extern int32 mp_unsigned_bin_size(mp_int *a);
extern int32 mp_read_unsigned_bin(mp_int *a, unsigned char *b, int32 c);
extern int32 mp_to_unsigned_bin(psPool_t *pool, mp_int *a, unsigned char *b);

extern int32 mp_signed_bin_size(mp_int *a);

/*
	lowlevel functions, do not call!
 */
/* define this in all cases for now FUTURE*/
#define s_mp_mul(P, A, B, C) s_mp_mul_digs(P, A, B, C, (A)->used + (B)->used + 1)


/*
	b = a*2
 */
extern int32 mp_mul_2(mp_int *a, mp_int *b);

extern int32 s_mp_add(mp_int *a, mp_int *b, mp_int *c);
extern int32 s_mp_sub(mp_int *a, mp_int *b, mp_int *c);

extern int32 fast_s_mp_mul_digs(psPool_t *pool, mp_int *a, mp_int *b, mp_int *c,
								int32 digs);
extern int32 fast_s_mp_sqr(psPool_t *pool, mp_int *a, mp_int *b);

extern int32 fast_mp_invmod(psPool_t *pool, mp_int *a, mp_int *b, mp_int *c);
extern int32 fast_mp_montgomery_reduce(mp_int *a, mp_int *m, mp_digit mp);

extern void bn_reverse(unsigned char *s, int32 len);


#ifdef __cplusplus
   }
#endif /* __cplusplus */

#endif /* _h_MPI */

