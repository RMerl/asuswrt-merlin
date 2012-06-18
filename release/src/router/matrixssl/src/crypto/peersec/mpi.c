/*	
 *	mpi.c
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

#include "../cryptoLayer.h"
#include <stdarg.h>

#ifndef USE_MPI2

static int32 mp_exptmod_fast (psPool_t *pool, mp_int * G, mp_int * X,
				mp_int * P, mp_int * Y, int32 redmode);

/******************************************************************************/
/*
	FUTURE
	1. Convert the mp_init and mp_clear functions to not use malloc + free,
	but to use static storage within the bignum variable instead - but
	how to handle grow()?  Maybe use a simple memory allocator
	2. verify stack usage of all functions and use of MP_LOW_MEM:
		fast_mp_montgomery_reduce
		fast_s_mp_mul_digs
		fast_s_mp_sqr
		fast_s_mp_mul_high_digs
	3. HAC stands for Handbook of Applied Cryptography
		http://www.cacr.math.uwaterloo.ca/hac/
*/
/******************************************************************************/
/*
	Utility functions
*/
void psZeromem(void *dst, size_t len)
{
	unsigned char *mem = (unsigned char *)dst;
	
	if (dst == NULL) {
		return;
	}
	while (len-- > 0) {
		*mem++ = 0;
	}
}

void psBurnStack(unsigned long len)
{
	unsigned char buf[32];
	
	psZeromem(buf, sizeof(buf));
	if (len > (unsigned long)sizeof(buf)) {
		psBurnStack(len - sizeof(buf));
	}
}

/******************************************************************************/
/*
	Multiple precision integer functions
	Note: we don't use va_args here to prevent portability issues.
*/
int32 _mp_init_multi(psPool_t *pool, mp_int *mp0, mp_int *mp1, mp_int *mp2, 
					mp_int *mp3, mp_int *mp4, mp_int *mp5, 
					mp_int *mp6, mp_int *mp7)
{
	mp_err	res		= MP_OKAY;		/* Assume ok until proven otherwise */
	int32		n		= 0;			/* Number of ok inits */
	mp_int	*tempArray[9];
	
	tempArray[0] = mp0;
	tempArray[1] = mp1;
	tempArray[2] = mp2;
	tempArray[3] = mp3;
	tempArray[4] = mp4;
	tempArray[5] = mp5;
	tempArray[6] = mp6;
	tempArray[7] = mp7;
	tempArray[8] = NULL;

	while (tempArray[n] != NULL) {
		if (mp_init(pool, tempArray[n]) != MP_OKAY) {
			res = MP_MEM;
			break;
		}
		n++;
	}

	if (res == MP_MEM) {
		n = 0;
		while (tempArray[n] != NULL) {
			mp_clear(tempArray[n]);
			n++;
		}
	}
	return res;		/* Assumed ok, if error flagged above. */
}
/******************************************************************************/
/*
	Reads a unsigned char array, assumes the msb is stored first [big endian]
 */
int32 mp_read_unsigned_bin (mp_int * a, unsigned char *b, int32 c)
{
	int32		res;

/*
	Make sure there are at least two digits.
 */
	if (a->alloc < 2) {
		if ((res = mp_grow(a, 2)) != MP_OKAY) {
			return res;
		}
	}

/*
	Zero the int32.
 */
	mp_zero (a);

/*
	read the bytes in
 */
	while (c-- > 0) {
		if ((res = mp_mul_2d (a, 8, a)) != MP_OKAY) {
			return res;
		}

#ifndef MP_8BIT
		a->dp[0] |= *b++;
		a->used += 1;
#else
		a->dp[0] = (*b & MP_MASK);
		a->dp[1] |= ((*b++ >> 7U) & 1);
		a->used += 2;
#endif /* MP_8BIT */
	}
	mp_clamp (a);
	return MP_OKAY;
}

/******************************************************************************/
/* 
	Compare two ints (signed)
 */
int32 mp_cmp (mp_int * a, mp_int * b)
{
/*
	compare based on sign
 */
	if (a->sign != b->sign) {
		if (a->sign == MP_NEG) {
			return MP_LT;
		} else {
			return MP_GT;
		}
	}

/*
	compare digits
 */
	if (a->sign == MP_NEG) {
		/* if negative compare opposite direction */
		return mp_cmp_mag(b, a);
	} else {
		return mp_cmp_mag(a, b);
	}
}

/******************************************************************************/
/*
	Store in unsigned [big endian] format.
*/
int32 mp_to_unsigned_bin(psPool_t *pool, mp_int * a, unsigned char *b)
{
	int32			x, res;
	mp_int		t;

	if ((res = mp_init_copy(pool, &t, a)) != MP_OKAY) {
		return res;
	}

	x = 0;
	while (mp_iszero (&t) == 0) {
#ifndef MP_8BIT
		b[x++] = (unsigned char) (t.dp[0] & 255);
#else
		b[x++] = (unsigned char) (t.dp[0] | ((t.dp[1] & 0x01) << 7));
#endif /* MP_8BIT */
		if ((res = mp_div_2d (pool, &t, 8, &t, NULL)) != MP_OKAY) {
			mp_clear (&t);
			return res;
		}
	}
	bn_reverse (b, x);
	mp_clear (&t);
	return MP_OKAY;
}

void _mp_clear_multi(mp_int *mp0, mp_int *mp1, mp_int *mp2, mp_int *mp3,
				  mp_int *mp4, mp_int *mp5, mp_int *mp6, mp_int *mp7)
{
	int32		n		= 0;		/* Number of ok inits */

	mp_int	*tempArray[9];
	
	tempArray[0] = mp0;
	tempArray[1] = mp1;
	tempArray[2] = mp2;
	tempArray[3] = mp3;
	tempArray[4] = mp4;
	tempArray[5] = mp5;
	tempArray[6] = mp6;
	tempArray[7] = mp7;
	tempArray[8] = NULL;

	for (n = 0; tempArray[n] != NULL; n++) {
		mp_clear(tempArray[n]);
	}
}

/******************************************************************************/
/*
	Init a new mp_int.
*/
int32 mp_init (psPool_t *pool, mp_int * a)
{
	int32		i;
/*
	allocate memory required and clear it
 */
	a->dp = OPT_CAST(mp_digit) psMalloc(pool, sizeof (mp_digit) * MP_PREC);
	if (a->dp == NULL) {
		return MP_MEM;
	}

/*
	set the digits to zero
 */
	for (i = 0; i < MP_PREC; i++) {
		a->dp[i] = 0;
	}
/*
	set the used to zero, allocated digits to the default precision and sign
	to positive
 */
	a->used  = 0;
	a->alloc = MP_PREC;
	a->sign  = MP_ZPOS;

	return MP_OKAY;
}

/******************************************************************************/
/*
	clear one (frees).
 */
void mp_clear (mp_int * a)
{
	int32		i;
/*
	only do anything if a hasn't been freed previously
 */
	if (a->dp != NULL) {
/*
		first zero the digits
 */
		for (i = 0; i < a->used; i++) {
			a->dp[i] = 0;
		}

		/* free ram */
		psFree (a->dp);

/*
		reset members to make debugging easier
 */
		a->dp		= NULL;
		a->alloc	= a->used = 0;
		a->sign		= MP_ZPOS;
	}
}

/******************************************************************************/
/*
	Get the size for an unsigned equivalent.
 */
int32 mp_unsigned_bin_size (mp_int * a)
{
	int32		size = mp_count_bits (a);

	return	(size / 8 + ((size & 7) != 0 ? 1 : 0));
}

/******************************************************************************/
/*
	Trim unused digits 

	This is used to ensure that leading zero digits are trimed and the 
	leading "used" digit will be non-zero. Typically very fast.  Also fixes 
	the sign if there are no more leading digits
*/
void mp_clamp (mp_int * a)
{
/*
	decrease used while the most significant digit is zero.
 */
	while (a->used > 0 && a->dp[a->used - 1] == 0) {
		--(a->used);
	}

/*
	reset the sign flag if used == 0
 */
	if (a->used == 0) {
		a->sign = MP_ZPOS;
	}
}

/******************************************************************************/
/*
	Shift left by a certain bit count.
 */
int32 mp_mul_2d (mp_int * a, int32 b, mp_int * c)
{
	mp_digit	d;
	int32			res;

/*
	Copy
 */
	if (a != c) {
		if ((res = mp_copy (a, c)) != MP_OKAY) {
			return res;
		}
	}

	if (c->alloc < (int32)(c->used + b/DIGIT_BIT + 1)) {
		if ((res = mp_grow (c, c->used + b / DIGIT_BIT + 1)) != MP_OKAY) {
			return res;
		}
	}

/*
	Shift by as many digits in the bit count
 */
	if (b >= (int32)DIGIT_BIT) {
		if ((res = mp_lshd (c, b / DIGIT_BIT)) != MP_OKAY) {
			return res;
		}
	}

/*
	shift any bit count < DIGIT_BIT
 */
	d = (mp_digit) (b % DIGIT_BIT);
	if (d != 0) {
		register mp_digit *tmpc, shift, mask, r, rr;
		register int32 x;

/*
		bitmask for carries
 */
		mask = (((mp_digit)1) << d) - 1;

/*
		shift for msbs
 */
		shift = DIGIT_BIT - d;

		/* alias */
		tmpc = c->dp;

		/* carry */
		r = 0;
		for (x = 0; x < c->used; x++) {
/*
			get the higher bits of the current word
 */
			rr = (*tmpc >> shift) & mask;

/*
			shift the current word and OR in the carry
 */
			*tmpc = ((*tmpc << d) | r) & MP_MASK;
			++tmpc;

/*
			set the carry to the carry bits of the current word
 */
			r = rr;
		}

/*
		set final carry
 */
		if (r != 0) {
			c->dp[(c->used)++] = r;
		}
	}
	mp_clamp (c);
	return MP_OKAY;
}

/******************************************************************************/
/* 
	Set to zero.
 */
void mp_zero (mp_int * a)
{
	int			n;
	mp_digit	*tmp;

	a->sign = MP_ZPOS;
	a->used = 0;

	tmp = a->dp;
	for (n = 0; n < a->alloc; n++) {
		*tmp++ = 0;
	}
}

#ifdef MP_LOW_MEM
#define TAB_SIZE 32
#else
#define TAB_SIZE 256
#endif /* MP_LOW_MEM */

/******************************************************************************/
/*
	Compare maginitude of two ints (unsigned).
 */
int32 mp_cmp_mag (mp_int * a, mp_int * b)
{
	int32			n;
	mp_digit	*tmpa, *tmpb;

/*
	compare based on # of non-zero digits
 */
	if (a->used > b->used) {
		return MP_GT;
	}

	if (a->used < b->used) {
		return MP_LT;
	}

	/* alias for a */
	tmpa = a->dp + (a->used - 1);

	/* alias for b */
	tmpb = b->dp + (a->used - 1);

/*
	compare based on digits
 */
	for (n = 0; n < a->used; ++n, --tmpa, --tmpb) {
		if (*tmpa > *tmpb) {
			return MP_GT;
		}

		if (*tmpa < *tmpb) {
			return MP_LT;
		}
	}
	return MP_EQ;
}

/******************************************************************************/
/*
	computes Y == G**X mod P, HAC pp.616, Algorithm 14.85

	Uses a left-to-right k-ary sliding window to compute the modular 
	exponentiation. The value of k changes based on the size of the exponent.

	Uses Montgomery or Diminished Radix reduction [whichever appropriate]
*/
int32 mp_exptmod(psPool_t *pool, mp_int * G, mp_int * X, mp_int * P, mp_int * Y)
{

/*
	modulus P must be positive
 */
	if (P->sign == MP_NEG) {
		return MP_VAL;
	}

/*
	if exponent X is negative we have to recurse
 */
	if (X->sign == MP_NEG) {
		mp_int tmpG, tmpX;
		int32 err;

/*
		first compute 1/G mod P
 */
		if ((err = mp_init(pool, &tmpG)) != MP_OKAY) {
			return err;
		}
		if ((err = mp_invmod(pool, G, P, &tmpG)) != MP_OKAY) {
			mp_clear(&tmpG);
			return err;
		}

/*
		now get |X|
 */
		if ((err = mp_init(pool, &tmpX)) != MP_OKAY) {
			mp_clear(&tmpG);
			return err;
		}
		if ((err = mp_abs(X, &tmpX)) != MP_OKAY) {
			mp_clear(&tmpG);
			mp_clear(&tmpX);
			return err;
		}

/*
		and now compute (1/G)**|X| instead of G**X [X < 0]
 */
		err = mp_exptmod(pool, &tmpG, &tmpX, P, Y);
		mp_clear(&tmpG);
		mp_clear(&tmpX);
		return err;
	}

/*
	if the modulus is odd or dr != 0 use the fast method
 */
	if (mp_isodd (P) == 1) {
		return mp_exptmod_fast (pool, G, X, P, Y, 0);
	} else {
/*
		no exptmod for evens
 */
		return MP_VAL;
	}
}

/******************************************************************************/
/*
	Call only from mp_exptmod to make sure this fast version qualifies
*/
static int32 mp_exptmod_fast(psPool_t *pool, mp_int * G, mp_int * X,
				mp_int * P, mp_int * Y, int32 redmode)
{
	mp_int		M[TAB_SIZE], res;
	mp_digit	buf, mp;
	int32		err, bitbuf, bitcpy, bitcnt, mode, digidx, x, y, winsize;


/*
	use a pointer to the reduction algorithm.  This allows us to use
	one of many reduction algorithms without modding the guts of
	the code with if statements everywhere.
 */
	int32	(*redux)(mp_int*,mp_int*,mp_digit);

/*
	find window size
 */
	x = mp_count_bits (X);
	if (x <= 7) {
		winsize = 2;
	} else if (x <= 36) {
		winsize = 3;
	} else if (x <= 140) {
		winsize = 4;
	} else if (x <= 450) {
		winsize = 5;
	} else if (x <= 1303) {
		winsize = 6;
	} else if (x <= 3529) {
		winsize = 7;
	} else {
		winsize = 8;
	}

#ifdef MP_LOW_MEM
	if (winsize > 5) {
		winsize = 5;
	}
#endif

/*
	init M array
	init first cell
 */
	if ((err = mp_init(pool, &M[1])) != MP_OKAY) {
		return err;
	}

/*
	now init the second half of the array
 */
	for (x = 1<<(winsize-1); x < (1 << winsize); x++) {
		if ((err = mp_init(pool, &M[x])) != MP_OKAY) {
			for (y = 1<<(winsize-1); y < x; y++) {
				mp_clear(&M[y]);
			}
			mp_clear(&M[1]);
			return err;
		}
	}


/*
	now setup montgomery
 */
	if ((err = mp_montgomery_setup(P, &mp)) != MP_OKAY) {
		goto LBL_M;
	}

/*
	automatically pick the comba one if available
 */
	if (((P->used * 2 + 1) < MP_WARRAY) &&
			P->used < (1 << ((CHAR_BIT * sizeof (mp_word)) - (2 * DIGIT_BIT)))) {
		redux = fast_mp_montgomery_reduce;
	} else {
/*
		use slower baseline Montgomery method
 */
		redux = mp_montgomery_reduce;
	}

/*
	setup result
 */
	if ((err = mp_init(pool, &res)) != MP_OKAY) {
		goto LBL_M;
	}

/*
	create M table. The first half of the table is not computed
	though accept for M[0] and M[1]
*/

/*
	now we need R mod m
 */
	if ((err = mp_montgomery_calc_normalization(&res, P)) != MP_OKAY) {
		goto LBL_RES;
	}

/*
	now set M[1] to G * R mod m
 */
	if ((err = mp_mulmod(pool, G, &res, P, &M[1])) != MP_OKAY) {
		goto LBL_RES;
	}

/*
	compute the value at M[1<<(winsize-1)] by squaring
	M[1] (winsize-1) times
*/
	if ((err = mp_copy(&M[1], &M[1 << (winsize - 1)])) != MP_OKAY) {
		goto LBL_RES;
	}

	for (x = 0; x < (winsize - 1); x++) {
		if ((err = mp_sqr(pool, &M[1 << (winsize - 1)],
				&M[1 << (winsize - 1)])) != MP_OKAY) {
			goto LBL_RES;
		}
		if ((err = redux(&M[1 << (winsize - 1)], P, mp)) != MP_OKAY) {
			goto LBL_RES;
		}
	}

/*
	create upper table
 */
	for (x = (1 << (winsize - 1)) + 1; x < (1 << winsize); x++) {
		if ((err = mp_mul(pool, &M[x - 1], &M[1], &M[x])) != MP_OKAY) {
			goto LBL_RES;
		}
		if ((err = redux(&M[x], P, mp)) != MP_OKAY) {
			goto LBL_RES;
		}
	}

/*
	set initial mode and bit cnt
 */
	mode   = 0;
	bitcnt = 1;
	buf    = 0;
	digidx = X->used - 1;
	bitcpy = 0;
	bitbuf = 0;

	for (;;) {
/*
		grab next digit as required
 */
		if (--bitcnt == 0) {
			/* if digidx == -1 we are out of digits so break */
			if (digidx == -1) {
				break;
			}
			/* read next digit and reset bitcnt */
			buf    = X->dp[digidx--];
			bitcnt = (int)DIGIT_BIT;
		}

		/* grab the next msb from the exponent */
		y     = (mp_digit)(buf >> (DIGIT_BIT - 1)) & 1;
		buf <<= (mp_digit)1;

/*
		if the bit is zero and mode == 0 then we ignore it
		These represent the leading zero bits before the first 1 bit
		in the exponent.  Technically this opt is not required but it
		does lower the # of trivial squaring/reductions used
*/
		if (mode == 0 && y == 0) {
			continue;
		}

/*
		if the bit is zero and mode == 1 then we square
 */
		if (mode == 1 && y == 0) {
			if ((err = mp_sqr (pool, &res, &res)) != MP_OKAY) {
				goto LBL_RES;
			}
			if ((err = redux (&res, P, mp)) != MP_OKAY) {
				goto LBL_RES;
			}
			continue;
		}

/*
		else we add it to the window
 */
		bitbuf |= (y << (winsize - ++bitcpy));
		mode    = 2;

		if (bitcpy == winsize) {
/*
			ok window is filled so square as required and multiply
			square first
 */
			for (x = 0; x < winsize; x++) {
				if ((err = mp_sqr(pool, &res, &res)) != MP_OKAY) {
					goto LBL_RES;
				}
				if ((err = redux(&res, P, mp)) != MP_OKAY) {
					goto LBL_RES;
				}
			}

			/* then multiply */
			if ((err = mp_mul(pool, &res, &M[bitbuf], &res)) != MP_OKAY) {
				goto LBL_RES;
			}
			if ((err = redux(&res, P, mp)) != MP_OKAY) {
				goto LBL_RES;
			}

/*
			empty window and reset
 */
			bitcpy = 0;
			bitbuf = 0;
			mode   = 1;
		}
	}

/*
	if bits remain then square/multiply
 */
	if (mode == 2 && bitcpy > 0) {
		/* square then multiply if the bit is set */
		for (x = 0; x < bitcpy; x++) {
			if ((err = mp_sqr(pool, &res, &res)) != MP_OKAY) {
				goto LBL_RES;
			}
			if ((err = redux(&res, P, mp)) != MP_OKAY) {
				goto LBL_RES;
			}

/*
			get next bit of the window
 */
			bitbuf <<= 1;
			if ((bitbuf & (1 << winsize)) != 0) {
/*
				then multiply
 */
				if ((err = mp_mul(pool, &res, &M[1], &res)) != MP_OKAY) {
					goto LBL_RES;
				}
				if ((err = redux(&res, P, mp)) != MP_OKAY) {
					goto LBL_RES;
				}
			}
		}
	}

/*
	fixup result if Montgomery reduction is used
	recall that any value in a Montgomery system is
	actually multiplied by R mod n.  So we have
	to reduce one more time to cancel out the factor of R.
*/
	if ((err = redux(&res, P, mp)) != MP_OKAY) {
		goto LBL_RES;
	}

/*
	swap res with Y
 */
	mp_exch(&res, Y);
	err = MP_OKAY;
LBL_RES:mp_clear(&res);
LBL_M:
	mp_clear(&M[1]);
	for (x = 1<<(winsize-1); x < (1 << winsize); x++) {
		mp_clear(&M[x]);
	}
	return err;
}

/******************************************************************************/
/*
	Grow as required
 */
int32 mp_grow (mp_int * a, int32 size)
{
	int32			i;
	mp_digit	*tmp;

/* 
	If the alloc size is smaller alloc more ram.
 */
	if (a->alloc < size) {
/*
		ensure there are always at least MP_PREC digits extra on top
 */
		size += (MP_PREC * 2) - (size % MP_PREC);

/*
		Reallocate the array a->dp

		We store the return in a temporary variable in case the operation 
		failed we don't want to overwrite the dp member of a.
*/
		tmp = OPT_CAST(mp_digit) psRealloc(a->dp, sizeof (mp_digit) * size);
		if (tmp == NULL) {
/*
			reallocation failed but "a" is still valid [can be freed]
 */
			return MP_MEM;
		}

/*
		reallocation succeeded so set a->dp
 */
		a->dp = tmp;

/*
		zero excess digits
 */
		i			= a->alloc;
		a->alloc	= size;
		for (; i < a->alloc; i++) {
			a->dp[i] = 0;
		}
	}
	return MP_OKAY;
}

/******************************************************************************/
/*
	b = |a|

	Simple function copies the input and fixes the sign to positive
*/
int32 mp_abs (mp_int * a, mp_int * b)
{
	int32		res;

/*
	copy a to b
 */
	if (a != b) {
		if ((res = mp_copy (a, b)) != MP_OKAY) {
			return res;
		}
	}

/*
	Force the sign of b to positive
 */
	b->sign = MP_ZPOS;

	return MP_OKAY;
}

/******************************************************************************/
/*
	Creates "a" then copies b into it
 */
int32 mp_init_copy(psPool_t *pool, mp_int * a, mp_int * b)
{
	int32		res;

	if ((res = mp_init(pool, a)) != MP_OKAY) {
		return res;
	}
	return mp_copy (b, a);
}

/******************************************************************************/
/* 
	Reverse an array, used for radix code
 */
void bn_reverse (unsigned char *s, int32 len)
{
	int32				ix, iy;
	unsigned char	t;

	ix = 0;
	iy = len - 1;
	while (ix < iy) {
		t		= s[ix];
		s[ix]	= s[iy];
		s[iy]	= t;
		++ix;
		--iy;
	}
}

/******************************************************************************/
/*
	Shift right by a certain bit count (store quotient in c, optional 
	remainder in d)
 */
int32 mp_div_2d(psPool_t *pool, mp_int * a, int32 b, mp_int * c, mp_int * d)
{
	mp_digit	D, r, rr;
	int32			x, res;
	mp_int		t;

/*
	If the shift count is <= 0 then we do no work
 */
	if (b <= 0) {
		res = mp_copy (a, c);
		if (d != NULL) {
			mp_zero (d);
		}
		return res;
	}

	if ((res = mp_init(pool, &t)) != MP_OKAY) {
		return res;
	}

/*
	Get the remainder
 */
	if (d != NULL) {
		if ((res = mp_mod_2d (a, b, &t)) != MP_OKAY) {
			mp_clear (&t);
			return res;
		}
	}

	/* copy */
	if ((res = mp_copy (a, c)) != MP_OKAY) {
		mp_clear (&t);
		return res;
	}

/*
	Shift by as many digits in the bit count
 */
	if (b >= (int32)DIGIT_BIT) {
		mp_rshd (c, b / DIGIT_BIT);
	}

	/* shift any bit count < DIGIT_BIT */
	D = (mp_digit) (b % DIGIT_BIT);
	if (D != 0) {
		register mp_digit *tmpc, mask, shift;

		/* mask */
		mask = (((mp_digit)1) << D) - 1;

		/* shift for lsb */
		shift = DIGIT_BIT - D;

		/* alias */
		tmpc = c->dp + (c->used - 1);

		/* carry */
		r = 0;
		for (x = c->used - 1; x >= 0; x--) {
/*	
			Get the lower  bits of this word in a temp.
 */
			rr = *tmpc & mask;

/*
			shift the current word and mix in the carry bits from the previous word
 */
			*tmpc = (*tmpc >> D) | (r << shift);
			--tmpc;

/*
			set the carry to the carry bits of the current word found above
 */
			r = rr;
		}
	}
	mp_clamp (c);
	if (d != NULL) {
		mp_exch (&t, d);
	}
	mp_clear (&t);
	return MP_OKAY;
}

/******************************************************************************/
/*
	copy, b = a
 */
int32 mp_copy (mp_int * a, mp_int * b)
{
	int32		res, n;

/*
	If dst == src do nothing
 */
	if (a == b) {
		return MP_OKAY;
	}

/*
	Grow dest
 */
	if (b->alloc < a->used) {
		if ((res = mp_grow (b, a->used)) != MP_OKAY) {
			return res;
		}
	}

/*
	Zero b and copy the parameters over
 */
	{
		register mp_digit *tmpa, *tmpb;

		/* pointer aliases */
		/* source */
		tmpa = a->dp;

		/* destination */
		tmpb = b->dp;

		/* copy all the digits */
		for (n = 0; n < a->used; n++) {
			*tmpb++ = *tmpa++;
		}

		/* clear high digits */
		for (; n < b->used; n++) {
			*tmpb++ = 0;
		}
	}

/*
	copy used count and sign
 */
	b->used = a->used;
	b->sign = a->sign;
	return MP_OKAY;
}

/******************************************************************************/
/*
	Returns the number of bits in an int32
 */
int32 mp_count_bits (mp_int * a)
{
	int32			r;
	mp_digit	q;

/* 
	Shortcut
 */
	if (a->used == 0) {
		return 0;
	}

/*
	Get number of digits and add that.
 */
	r = (a->used - 1) * DIGIT_BIT;

/*
	Take the last digit and count the bits in it.
 */
	q = a->dp[a->used - 1];
	while (q > ((mp_digit) 0)) {
		++r;
		q >>= ((mp_digit) 1);
	}
	return r;
}

/******************************************************************************/
/*
	Shift left a certain amount of digits.
 */
int32 mp_lshd (mp_int * a, int32 b)
{
	int32		x, res;

/*
	If its less than zero return.
 */
	if (b <= 0) {
		return MP_OKAY;
	}

/*
	Grow to fit the new digits.
 */
	if (a->alloc < a->used + b) {
		if ((res = mp_grow (a, a->used + b)) != MP_OKAY) {
			return res;
		}
	}

	{
		register mp_digit *top, *bottom;

/*
		Increment the used by the shift amount then copy upwards.
 */
		a->used += b;

		/* top */
		top = a->dp + a->used - 1;

		/* base */
		bottom = a->dp + a->used - 1 - b;

/*
		Much like mp_rshd this is implemented using a sliding window
		except the window goes the otherway around.  Copying from
		the bottom to the top.  see bn_mp_rshd.c for more info.
 */
		for (x = a->used - 1; x >= b; x--) {
			*top-- = *bottom--;
		}

		/* zero the lower digits */
		top = a->dp;
		for (x = 0; x < b; x++) {
			*top++ = 0;
		}
	}
	return MP_OKAY;
}

/******************************************************************************/
/*
	Set to a digit.
 */
void mp_set (mp_int * a, mp_digit b)
{
	mp_zero (a);
	a->dp[0] = b & MP_MASK;
	a->used  = (a->dp[0] != 0) ? 1 : 0;
}

/******************************************************************************/
/*
	Swap the elements of two integers, for cases where you can't simply swap 
	the 	mp_int pointers around 
*/
void mp_exch (mp_int * a, mp_int * b)
{
	mp_int		t;

	t	= *a;
	*a	= *b;
	*b	= t;
}

/******************************************************************************/
/*
	High level multiplication (handles sign)
 */
int32 mp_mul(psPool_t *pool, mp_int * a, mp_int * b, mp_int * c)
{
	int32			res, neg;
	int32			digs = a->used + b->used + 1;
	
	neg = (a->sign == b->sign) ? MP_ZPOS : MP_NEG;

/*	Can we use the fast multiplier?
	
	The fast multiplier can be used if the output will have less than 
	MP_WARRAY digits and the number of digits won't affect carry propagation
*/
	if ((digs < MP_WARRAY) && MIN(a->used, b->used) <= 
			(1 << ((CHAR_BIT * sizeof (mp_word)) - (2 * DIGIT_BIT)))) {
		res = fast_s_mp_mul_digs(pool, a, b, c, digs);
	} else {
		res = s_mp_mul(pool, a, b, c);
	}
	c->sign = (c->used > 0) ? neg : MP_ZPOS;
	return res;
}

/******************************************************************************/
/* 
	c = a mod b, 0 <= c < b
 */
int32 mp_mod(psPool_t *pool, mp_int * a, mp_int * b, mp_int * c)
{
	mp_int		t;
	int32			res;

	if ((res = mp_init(pool, &t)) != MP_OKAY) {
		return res;
	}

	if ((res = mp_div (pool, a, b, NULL, &t)) != MP_OKAY) {
		mp_clear (&t);
		return res;
	}

	if (t.sign != b->sign) {
		res = mp_add (b, &t, c);
	} else {
		res = MP_OKAY;
		mp_exch (&t, c);
	}

	mp_clear (&t);
	return res;
}

/******************************************************************************/
/*
	shifts with subtractions when the result is greater than b.

	The method is slightly modified to shift B unconditionally upto just under
	the leading bit of b.  This saves alot of multiple precision shifting.
*/
int32 mp_montgomery_calc_normalization (mp_int * a, mp_int * b)
{
	int32		x, bits, res;

/*
	How many bits of last digit does b use
 */
	bits = mp_count_bits (b) % DIGIT_BIT;

	if (b->used > 1) {
		if ((res = mp_2expt(a, (b->used - 1) * DIGIT_BIT + bits - 1)) != MP_OKAY) {
			return res;
		}
	} else {
		mp_set(a, 1);
		bits = 1;
	}

/*
	Now compute C = A * B mod b
 */
	for (x = bits - 1; x < (int32)DIGIT_BIT; x++) {
		if ((res = mp_mul_2(a, a)) != MP_OKAY) {
			return res;
		}
		if (mp_cmp_mag(a, b) != MP_LT) {
			if ((res = s_mp_sub(a, b, a)) != MP_OKAY) {
				return res;
			}
		}
	}

	return MP_OKAY;
}

/******************************************************************************/
/*
	d = a * b (mod c)
 */
int32 mp_mulmod(psPool_t *pool, mp_int * a, mp_int * b, mp_int * c, mp_int * d)
{
	int32		res;
	mp_int		t;

	if ((res = mp_init(pool, &t)) != MP_OKAY) {
		return res;
	}

	if ((res = mp_mul (pool, a, b, &t)) != MP_OKAY) {
		mp_clear (&t);
		return res;
	}
	res = mp_mod (pool, &t, c, d);
	mp_clear (&t);
	return res;
}

/******************************************************************************/
/*
	Computes b = a*a
 */
#ifdef USE_SMALL_WORD
int32 mp_sqr (psPool_t *pool, mp_int * a, mp_int * b)
{
	int32		res;

/*
	Can we use the fast comba multiplier?
 */
	if ((a->used * 2 + 1) < MP_WARRAY && a->used < 
			(1 << (sizeof(mp_word) * CHAR_BIT - 2*DIGIT_BIT - 1))) {
		res = fast_s_mp_sqr (pool, a, b);
	} else {
		res = s_mp_sqr (pool, a, b);
	}
	b->sign = MP_ZPOS;
	return res;
}
#endif /* USE_SMALL_WORD */

/******************************************************************************/
/* 
	Computes xR**-1 == x (mod N) via Montgomery Reduction.

	This is an optimized implementation of montgomery_reduce 
	which uses the comba method to quickly calculate the columns of the
	reduction.

	Based on Algorithm 14.32 on pp.601 of HAC.
*/

int32 fast_mp_montgomery_reduce (mp_int * x, mp_int * n, mp_digit rho)
{
	int32		ix, res, olduse;
	mp_word		W[MP_WARRAY];

/*
	Get old used count
 */
	olduse = x->used;

/*
	Grow a as required
 */
	if (x->alloc < n->used + 1) {
		if ((res = mp_grow(x, n->used + 1)) != MP_OKAY) {
			return res;
		}
	}

/*
	First we have to get the digits of the input into
	an array of double precision words W[...]
 */
	{
		register mp_word		*_W;
		register mp_digit	*tmpx;

/*		
		Alias for the W[] array
 */
		_W   = W;

/*
		Alias for the digits of  x
 */
		tmpx = x->dp;

/*
		Copy the digits of a into W[0..a->used-1]
 */
		for (ix = 0; ix < x->used; ix++) {
			*_W++ = *tmpx++;
		}

/*
		Zero the high words of W[a->used..m->used*2]
 */
		for (; ix < n->used * 2 + 1; ix++) {
			*_W++ = 0;
		}
	}

/*
	Now we proceed to zero successive digits from the least
	significant upwards.
 */
	for (ix = 0; ix < n->used; ix++) {
/*
		mu = ai * m' mod b

		We avoid a double precision multiplication (which isn't required) by 
		casting the value down to a mp_digit.  Note this requires that
		W[ix-1] have  the carry cleared (see after the inner loop)
 */
		register mp_digit mu;
		mu = (mp_digit) (((W[ix] & MP_MASK) * rho) & MP_MASK);

/*		
		a = a + mu * m * b**i

		This is computed in place and on the fly.  The multiplication by b**i 
		is handled by offseting which columns the results are added to.
		
		Note the comba method normally doesn't handle carries in the inner loop
		In this case we fix the carry from the previous column since the
		Montgomery reduction requires digits of the result (so far) [see above]
		to work.  This is handled by fixing up one carry after the inner loop.
		The carry fixups are done in order so after these loops the first
		m->used words of W[] have the carries fixed
 */
		{
			register int32		iy;
			register mp_digit	*tmpn;
			register mp_word	*_W;

/*
			Alias for the digits of the modulus
 */
			tmpn = n->dp;

/*
			Alias for the columns set by an offset of ix
 */
			_W = W + ix;

/*
				inner loop
 */
				for (iy = 0; iy < n->used; iy++) {
					*_W++ += ((mp_word)mu) * ((mp_word)*tmpn++);
				}
			}

/*
		Now fix carry for next digit, W[ix+1]
 */
		W[ix + 1] += W[ix] >> ((mp_word) DIGIT_BIT);
	}

/*
		Now we have to propagate the carries and shift the words downward [all those 
		least significant digits we zeroed].
 */
	{
		register mp_digit	*tmpx;
		register mp_word	*_W, *_W1;

/*
		Now fix rest of carries 
 */

/*
		alias for current word
 */
		_W1 = W + ix;

/*
		alias for next word, where the carry goes
 */
		_W = W + ++ix;

		for (; ix <= n->used * 2 + 1; ix++) {
			*_W++ += *_W1++ >> ((mp_word) DIGIT_BIT);
		}

/*
		copy out, A = A/b**n

		The result is A/b**n but instead of converting from an
		array of mp_word to mp_digit than calling mp_rshd
		we just copy them in the right order
 */

/*
		alias for destination word
 */
		tmpx = x->dp;

/*
		alias for shifted double precision result
 */
		_W = W + n->used;

		for (ix = 0; ix < n->used + 1; ix++) {
		*tmpx++ = (mp_digit)(*_W++ & ((mp_word) MP_MASK));
		}

/*
		zero oldused digits, if the input a was larger than
		m->used+1 we'll have to clear the digits
 */
		for (; ix < olduse; ix++) {
			*tmpx++ = 0;
		}
	}

/*
	Set the max used and clamp
 */
	x->used = n->used + 1;
	mp_clamp(x);

/*
	if A >= m then A = A - m
 */
	if (mp_cmp_mag(x, n) != MP_LT) {
		return s_mp_sub(x, n, x);
	}
	return MP_OKAY;
}

/******************************************************************************/
/*
	High level addition (handles signs)
 */
int32 mp_add (mp_int * a, mp_int * b, mp_int * c)
{
	int32		sa, sb, res;

/*
	Get sign of both inputs
 */
	sa = a->sign;
	sb = b->sign;

/*
	Handle two cases, not four.
 */
	if (sa == sb) {
/*
		Both positive or both negative. Add their magnitudes, copy the sign.
 */
		c->sign = sa;
		res = s_mp_add (a, b, c);
	} else {
/*
		One positive, the other negative.  Subtract the one with the greater
		magnitude from the one of the lesser magnitude.  The result gets the sign of
		the one with the greater magnitude.
 */
		if (mp_cmp_mag (a, b) == MP_LT) {
			c->sign = sb;
			res = s_mp_sub (b, a, c);
		} else {
			c->sign = sa;
			res = s_mp_sub (a, b, c);
		}
	}
	return res;
}

/******************************************************************************/
/*
	Compare a digit.
 */
int32 mp_cmp_d (mp_int * a, mp_digit b)
{
/*
	Compare based on sign
 */
	if (a->sign == MP_NEG) {
		return MP_LT;
	}

/*
	Compare based on magnitude
 */
	if (a->used > 1) {
		return MP_GT;
	}

/*
	Compare the only digit of a to b
 */
	if (a->dp[0] > b) {
		return MP_GT;
	} else if (a->dp[0] < b) {
		return MP_LT;
	} else {
		return MP_EQ;
	}
}

/******************************************************************************/
/*
	b = a/2
 */
int32 mp_div_2 (mp_int * a, mp_int * b)
{
	int32		x, res, oldused;

/*
	Copy
 */
	if (b->alloc < a->used) {
		if ((res = mp_grow (b, a->used)) != MP_OKAY) {
			return res;
		}
	}

	oldused = b->used;
	b->used = a->used;
	{
		register mp_digit r, rr, *tmpa, *tmpb;

/*
		Source alias
 */
		tmpa = a->dp + b->used - 1;

/*
		dest alias
 */
		tmpb = b->dp + b->used - 1;

/*
		carry
 */
		r = 0;
		for (x = b->used - 1; x >= 0; x--) {
/*
			Get the carry for the next iteration
 */
			rr = *tmpa & 1;

/*			
			Shift the current digit, add in carry and store
 */
			*tmpb-- = (*tmpa-- >> 1) | (r << (DIGIT_BIT - 1));
/*
			Forward carry to next iteration
 */
			r = rr;
		}

/* 
		Zero excess digits
 */
		tmpb = b->dp + b->used;
		for (x = b->used; x < oldused; x++) {
			*tmpb++ = 0;
		}
	}
	b->sign = a->sign;
	mp_clamp (b);
	return MP_OKAY;
}

/******************************************************************************/
/*
	Computes xR**-1 == x (mod N) via Montgomery Reduction
 */
#ifdef USE_SMALL_WORD
int32 mp_montgomery_reduce (mp_int * x, mp_int * n, mp_digit rho)
{
	int32			ix, res, digs;
	mp_digit	mu;

/*	Can the fast reduction [comba] method be used?

	Note that unlike in mul you're safely allowed *less* than the available
	columns [255 per default] since carries are fixed up in the inner loop.
 */
	digs = n->used * 2 + 1;
	if ((digs < MP_WARRAY) && 
		n->used < 
		(1 << ((CHAR_BIT * sizeof (mp_word)) - (2 * DIGIT_BIT)))) {
			return fast_mp_montgomery_reduce (x, n, rho);
		}

/*
		Grow the input as required.
 */
		if (x->alloc < digs) {
			if ((res = mp_grow (x, digs)) != MP_OKAY) {
				return res;
			}
		}
		x->used = digs;

		for (ix = 0; ix < n->used; ix++) {
/*
			mu = ai * rho mod b

			The value of rho must be precalculated via mp_montgomery_setup()
			such that it equals -1/n0 mod b this allows the following inner
			loop to reduce the input one digit at a time
 */
			mu = (mp_digit)(((mp_word)x->dp[ix]) * ((mp_word)rho) & MP_MASK);

			/* a = a + mu * m * b**i */
			{
				register int32 iy;
				register mp_digit *tmpn, *tmpx, u;
				register mp_word r;

/*
				alias for digits of the modulus
 */
				tmpn = n->dp;

/*
				alias for the digits of x [the input]
 */
				tmpx = x->dp + ix;

/*
				set the carry to zero
 */
				u = 0;

/*
				Multiply and add in place
 */
				for (iy = 0; iy < n->used; iy++) {
					/* compute product and sum */
					r = ((mp_word)mu) * ((mp_word)*tmpn++) +
						((mp_word) u) + ((mp_word) * tmpx);

					/* get carry */
					u = (mp_digit)(r >> ((mp_word) DIGIT_BIT));

					/* fix digit */
					*tmpx++ = (mp_digit)(r & ((mp_word) MP_MASK));
				}
				/* At this point the ix'th digit of x should be zero */


/*
				propagate carries upwards as required
 */
				while (u) {
					*tmpx		+= u;
					u			= *tmpx >> DIGIT_BIT;
					*tmpx++ &= MP_MASK;
				}
			}
		}

/*
		At this point the n.used'th least significant digits of x are all zero
		which means we can shift x to the right by n.used digits and the 
		residue is unchanged.
*/
		/* x = x/b**n.used */
		mp_clamp(x);
		mp_rshd (x, n->used);

		/* if x >= n then x = x - n */
		if (mp_cmp_mag (x, n) != MP_LT) {
			return s_mp_sub (x, n, x);
		}

		return MP_OKAY;
}
#endif /* USE_SMALL_WORD */

/******************************************************************************/
/*
	Setups the montgomery reduction stuff.
 */
int32 mp_montgomery_setup (mp_int * n, mp_digit * rho)
{
	mp_digit x, b;

/*
	fast inversion mod 2**k
	
	Based on the fact that
	
	XA = 1 (mod 2**n)	=>  (X(2-XA)) A		= 1 (mod 2**2n)
						=>  2*X*A - X*X*A*A	= 1
						=>  2*(1) - (1)		= 1
*/
	b = n->dp[0];

	if ((b & 1) == 0) {
		return MP_VAL;
	}

	x = (((b + 2) & 4) << 1) + b;		/* here x*a==1 mod 2**4 */
	x = (x * (2 - b * x)) & MP_MASK;	/* here x*a==1 mod 2**8 */
#if !defined(MP_8BIT)
	x = (x * (2 - b * x)) & MP_MASK;	/* here x*a==1 mod 2**8 */
#endif /* MP_8BIT */
#if defined(MP_64BIT) || !(defined(MP_8BIT) || defined(MP_16BIT))
	x *= 2 - b * x;						/* here x*a==1 mod 2**32 */
#endif
#ifdef MP_64BIT
	x *= 2 - b * x;						/* here x*a==1 mod 2**64 */
#endif /* MP_64BIT */

	/* rho = -1/m mod b */
	*rho = (((mp_word) 1 << ((mp_word) DIGIT_BIT)) - x) & MP_MASK;

	return MP_OKAY;
}

/******************************************************************************/
/*
	High level subtraction (handles signs)
 */
int32 mp_sub (mp_int * a, mp_int * b, mp_int * c)
{
	int32		sa, sb, res;

	sa = a->sign;
	sb = b->sign;

	if (sa != sb) {
/*
		Subtract a negative from a positive, OR subtract a positive from a
		negative.  In either case, ADD their magnitudes, and use the sign of
		the first number.
 */
		c->sign = sa;
		res = s_mp_add (a, b, c);
	} else {
/*
		Subtract a positive from a positive, OR subtract a negative 
		from a negative. First, take the difference between their
		magnitudes, then...
 */
		if (mp_cmp_mag (a, b) != MP_LT) {
/*
			Copy the sign from the first
 */
			c->sign = sa;
			/* The first has a larger or equal magnitude */
			res = s_mp_sub (a, b, c);
		} else {
/*
			The result has the *opposite* sign from the first number.
 */
			c->sign = (sa == MP_ZPOS) ? MP_NEG : MP_ZPOS;
/*
			The second has a larger magnitude 
 */
			res = s_mp_sub (b, a, c);
		}
	}
	return res;
}

/******************************************************************************/
/*
	calc a value mod 2**b
 */
int32 mp_mod_2d (mp_int * a, int32 b, mp_int * c)
{
	int32		x, res;

/*
	if b is <= 0 then zero the int32
 */
	if (b <= 0) {
		mp_zero (c);
		return MP_OKAY;
	}

/*
	If the modulus is larger than the value than return
 */
	if (b >=(int32) (a->used * DIGIT_BIT)) {
		res = mp_copy (a, c);
		return res;
	}

	/* copy */
	if ((res = mp_copy (a, c)) != MP_OKAY) {
		return res;
	}

/*
	Zero digits above the last digit of the modulus
 */
	for (x = (b / DIGIT_BIT) + ((b % DIGIT_BIT) == 0 ? 0 : 1); x < c->used; x++) {
		c->dp[x] = 0;
	}
/*
	Clear the digit that is not completely outside/inside the modulus
 */
	c->dp[b / DIGIT_BIT] &=
		(mp_digit) ((((mp_digit) 1) << (((mp_digit) b) % DIGIT_BIT)) - ((mp_digit) 1));
	mp_clamp (c);
	return MP_OKAY;
}

/******************************************************************************/
/*
	Shift right a certain amount of digits.
 */
void mp_rshd (mp_int * a, int32 b)
{
	int32		x;

/*
	If b <= 0 then ignore it
 */
	if (b <= 0) {
		return;
	}

/*
	If b > used then simply zero it and return.
*/
	if (a->used <= b) {
		mp_zero (a);
		return;
	}

	{
		register mp_digit *bottom, *top;

/*
		Shift the digits down
 */
		/* bottom */
		bottom = a->dp;

		/* top [offset into digits] */
		top = a->dp + b;

/*
		This is implemented as a sliding window where the window is b-digits long
		and digits from the top of the window are copied to the bottom.
		
		 e.g.

		b-2 | b-1 | b0 | b1 | b2 | ... | bb |   ---->
		/\                   |      ---->
		\-------------------/      ---->
 */
		for (x = 0; x < (a->used - b); x++) {
			*bottom++ = *top++;
		}

/*
		Zero the top digits
 */
		for (; x < a->used; x++) {
			*bottom++ = 0;
		}
	}

/*
	Remove excess digits
 */
	a->used -= b;
}

/******************************************************************************/
/* 
	Low level subtraction (assumes |a| > |b|), HAC pp.595 Algorithm 14.9
 */
int32 s_mp_sub (mp_int * a, mp_int * b, mp_int * c)
{
	int32		olduse, res, min, max;

/*
	Find sizes
 */
	min = b->used;
	max = a->used;

/*
	init result
 */
	if (c->alloc < max) {
		if ((res = mp_grow (c, max)) != MP_OKAY) {
			return res;
		}
	}
	olduse = c->used;
	c->used = max;

	{
		register mp_digit u, *tmpa, *tmpb, *tmpc;
		register int32 i;

/*
		alias for digit pointers
 */
		tmpa = a->dp;
		tmpb = b->dp;
		tmpc = c->dp;

/*
		set carry to zero
 */
		u = 0;
		for (i = 0; i < min; i++) {
			/* T[i] = A[i] - B[i] - U */
			*tmpc = *tmpa++ - *tmpb++ - u;

/*
			U = carry bit of T[i]
			Note this saves performing an AND operation since if a carry does occur it
			will propagate all the way to the MSB.  As a result a single shift
			is enough to get the carry
 */
			u = *tmpc >> ((mp_digit)(CHAR_BIT * sizeof (mp_digit) - 1));

			/* Clear carry from T[i] */
			*tmpc++ &= MP_MASK;
		}

/*
		Now copy higher words if any, e.g. if A has more digits than B
 */
		for (; i < max; i++) {
			/* T[i] = A[i] - U */
			*tmpc = *tmpa++ - u;

			/* U = carry bit of T[i] */
			u = *tmpc >> ((mp_digit)(CHAR_BIT * sizeof (mp_digit) - 1));

			/* Clear carry from T[i] */
			*tmpc++ &= MP_MASK;
		}

/*
		Clear digits above used (since we may not have grown result above)
 */
		for (i = c->used; i < olduse; i++) {
			*tmpc++ = 0;
		}
	}

	mp_clamp (c);
	return MP_OKAY;
}
/******************************************************************************/
/*
	integer signed division. 

	c*b + d == a [e.g. a/b, c=quotient, d=remainder]
	HAC pp.598 Algorithm 14.20

	Note that the description in HAC is horribly incomplete.  For example,
	it doesn't consider the case where digits are removed from 'x' in the inner
	loop.  It also doesn't consider the case that y has fewer than three
	digits, etc..

	The overall algorithm is as described as 14.20 from HAC but fixed to
	treat these cases.
 */
#ifdef MP_DIV_SMALL
int32 mp_div(psPool_t *pool, mp_int * a, mp_int * b, mp_int * c, mp_int * d)
{
	mp_int	ta, tb, tq, q;
	int32		res, n, n2;

/*
	is divisor zero ?
 */
	if (mp_iszero (b) == 1) {
		return MP_VAL;
	}

/*
	if a < b then q=0, r = a
 */
	if (mp_cmp_mag (a, b) == MP_LT) {
		if (d != NULL) {
			res = mp_copy (a, d);
		} else {
			res = MP_OKAY;
		}
		if (c != NULL) {
			mp_zero (c);
		}
		return res;
	}
	
/*
	init our temps
 */
	if ((res = _mp_init_multi(pool, &ta, &tb, &tq, &q, NULL, NULL, NULL, NULL) != MP_OKAY)) {
		return res;
	}

/*
	tq = 2^n,  tb == b*2^n
 */
	mp_set(&tq, 1);
	n = mp_count_bits(a) - mp_count_bits(b);
	if (((res = mp_abs(a, &ta)) != MP_OKAY) ||
			((res = mp_abs(b, &tb)) != MP_OKAY) || 
			((res = mp_mul_2d(&tb, n, &tb)) != MP_OKAY) ||
			((res = mp_mul_2d(&tq, n, &tq)) != MP_OKAY)) {
      goto __ERR;
	}
/* old
	if (((res = mp_copy(a, &ta)) != MP_OKAY) ||
		((res = mp_copy(b, &tb)) != MP_OKAY) || 
		((res = mp_mul_2d(&tb, n, &tb)) != MP_OKAY) ||
		((res = mp_mul_2d(&tq, n, &tq)) != MP_OKAY)) {
			goto LBL_ERR;
	}
*/
	while (n-- >= 0) {
		if (mp_cmp(&tb, &ta) != MP_GT) {
			if (((res = mp_sub(&ta, &tb, &ta)) != MP_OKAY) ||
				((res = mp_add(&q, &tq, &q)) != MP_OKAY)) {
					goto LBL_ERR;
			}
		}
		if (((res = mp_div_2d(pool, &tb, 1, &tb, NULL)) != MP_OKAY) ||
			((res = mp_div_2d(pool, &tq, 1, &tq, NULL)) != MP_OKAY)) {
			goto LBL_ERR;
		}
	}

/*
	now q == quotient and ta == remainder
 */
	n  = a->sign;
	n2 = (a->sign == b->sign ? MP_ZPOS : MP_NEG);
	if (c != NULL) {
		mp_exch(c, &q);
		c->sign  = (mp_iszero(c) == MP_YES) ? MP_ZPOS : n2;
	}
	if (d != NULL) {
		mp_exch(d, &ta);
		d->sign = (mp_iszero(d) == MP_YES) ? MP_ZPOS : n;
	}
LBL_ERR:
	_mp_clear_multi(&ta, &tb, &tq, &q, NULL, NULL, NULL, NULL);
	return res;
}
#else /* MP_DIV_SMALL */

int32 mp_div(psPool_t *pool, mp_int * a, mp_int * b, mp_int * c, mp_int * d)
{
	mp_int		q, x, y, t1, t2;
	int32		res, n, t, i, norm, neg;

/*
	is divisor zero ?
 */
	if (mp_iszero(b) == 1) {
		return MP_VAL;
	}

/*
	if a < b then q=0, r = a
 */
	if (mp_cmp_mag(a, b) == MP_LT) {
		if (d != NULL) {
			res = mp_copy(a, d);
		} else {
			res = MP_OKAY;
		}
		if (c != NULL) {
		mp_zero(c);
		}
		return res;
	}

	if ((res = mp_init_size(pool, &q, a->used + 2)) != MP_OKAY) {
		return res;
	}
	q.used = a->used + 2;

	if ((res = mp_init(pool, &t1)) != MP_OKAY) {
		goto LBL_Q;
	}

	if ((res = mp_init(pool, &t2)) != MP_OKAY) {
		goto LBL_T1;
	}

	if ((res = mp_init_copy(pool, &x, a)) != MP_OKAY) {
		goto LBL_T2;
	}

	if ((res = mp_init_copy(pool, &y, b)) != MP_OKAY) {
		goto LBL_X;
	}

/*
	fix the sign
 */
	neg = (a->sign == b->sign) ? MP_ZPOS : MP_NEG;
	x.sign = y.sign = MP_ZPOS;

/*
	normalize both x and y, ensure that y >= b/2, [b == 2**DIGIT_BIT]
 */
	norm = mp_count_bits(&y) % DIGIT_BIT;
	if (norm < (int32)(DIGIT_BIT-1)) {
		norm = (DIGIT_BIT-1) - norm;
		if ((res = mp_mul_2d(&x, norm, &x)) != MP_OKAY) {
			goto LBL_Y;
		}
		if ((res = mp_mul_2d(&y, norm, &y)) != MP_OKAY) {
			goto LBL_Y;
		}
	} else {
		norm = 0;
	}

/*
	note hac does 0 based, so if used==5 then its 0,1,2,3,4, e.g. use 4
 */
	n = x.used - 1;
	t = y.used - 1;

/*
	while (x >= y*b**n-t) do { q[n-t] += 1; x -= y*b**{n-t} }
 */
	if ((res = mp_lshd(&y, n - t)) != MP_OKAY) { /* y = y*b**{n-t} */
		goto LBL_Y;
	}

	while (mp_cmp(&x, &y) != MP_LT) {
		++(q.dp[n - t]);
		if ((res = mp_sub(&x, &y, &x)) != MP_OKAY) {
		goto LBL_Y;
		}
	}

/*
	reset y by shifting it back down
 */
	mp_rshd(&y, n - t);

/*
	step 3. for i from n down to (t + 1)
 */
	for (i = n; i >= (t + 1); i--) {
		if (i > x.used) {
			continue;
		}

/*
		step 3.1 if xi == yt then set q{i-t-1} to b-1,
		otherwise set q{i-t-1} to (xi*b + x{i-1})/yt
 */
		if (x.dp[i] == y.dp[t]) {
			q.dp[i - t - 1] = ((((mp_digit)1) << DIGIT_BIT) - 1);
		} else {
			mp_word tmp;
			tmp = ((mp_word) x.dp[i]) << ((mp_word) DIGIT_BIT);
			tmp |= ((mp_word) x.dp[i - 1]);
			tmp /= ((mp_word) y.dp[t]);
			if (tmp > (mp_word) MP_MASK) {
				tmp = MP_MASK;
			}
			q.dp[i - t - 1] = (mp_digit) (tmp & (mp_word) (MP_MASK));
		}

/*
		while (q{i-t-1} * (yt * b + y{t-1})) > 
				xi * b**2 + xi-1 * b + xi-2 

			do q{i-t-1} -= 1; 
 */
		q.dp[i - t - 1] = (q.dp[i - t - 1] + 1) & MP_MASK;
		do {
			q.dp[i - t - 1] = (q.dp[i - t - 1] - 1) & MP_MASK;

/*
			find left hand
 */
			mp_zero (&t1);
			t1.dp[0] = (t - 1 < 0) ? 0 : y.dp[t - 1];
			t1.dp[1] = y.dp[t];
			t1.used = 2;
			if ((res = mp_mul_d (&t1, q.dp[i - t - 1], &t1)) != MP_OKAY) {
				goto LBL_Y;
			}

/*
			find right hand
 */
			t2.dp[0] = (i - 2 < 0) ? 0 : x.dp[i - 2];
			t2.dp[1] = (i - 1 < 0) ? 0 : x.dp[i - 1];
			t2.dp[2] = x.dp[i];
			t2.used = 3;
		} while (mp_cmp_mag(&t1, &t2) == MP_GT);

/*
		step 3.3 x = x - q{i-t-1} * y * b**{i-t-1}
 */
		if ((res = mp_mul_d(&y, q.dp[i - t - 1], &t1)) != MP_OKAY) {
			goto LBL_Y;
		}

		if ((res = mp_lshd(&t1, i - t - 1)) != MP_OKAY) {
			goto LBL_Y;
		}

		if ((res = mp_sub(&x, &t1, &x)) != MP_OKAY) {
			goto LBL_Y;
		}

/*
	if x < 0 then { x = x + y*b**{i-t-1}; q{i-t-1} -= 1; }
 */
		if (x.sign == MP_NEG) {
			if ((res = mp_copy(&y, &t1)) != MP_OKAY) {
				goto LBL_Y;
			}
		if ((res = mp_lshd (&t1, i - t - 1)) != MP_OKAY) {
			goto LBL_Y;
		}
		if ((res = mp_add (&x, &t1, &x)) != MP_OKAY) {
			goto LBL_Y;
		}

		q.dp[i - t - 1] = (q.dp[i - t - 1] - 1UL) & MP_MASK;
		}
	}

/*
	now q is the quotient and x is the remainder 
	[which we have to normalize] 
 */

/*
	get sign before writing to c
 */
	x.sign = x.used == 0 ? MP_ZPOS : a->sign;

	if (c != NULL) {
		mp_clamp(&q);
		mp_exch(&q, c);
		c->sign = neg;
	}

	if (d != NULL) {
		mp_div_2d(pool, &x, norm, &x, NULL);
		mp_exch(&x, d);
	}

	res = MP_OKAY;

LBL_Y:mp_clear (&y);
LBL_X:mp_clear (&x);
LBL_T2:mp_clear (&t2);
LBL_T1:mp_clear (&t1);
LBL_Q:mp_clear (&q);
	return res;
}
#endif /* MP_DIV_SMALL */

/******************************************************************************/
/*
	multiplies |a| * |b| and only computes upto digs digits of result 
	HAC pp. 595, Algorithm 14.12  Modified so you can control how many digits
	of output are created.
 */
#ifdef USE_SMALL_WORD
int32 s_mp_mul_digs(psPool_t *pool, mp_int * a, mp_int * b, mp_int * c, int32 digs)
{
	mp_int		t;
	int32			res, pa, pb, ix, iy;
	mp_digit	u;
	mp_word		r;
	mp_digit	tmpx, *tmpt, *tmpy;

/*
	Can we use the fast multiplier?
 */
	if (((digs) < MP_WARRAY) &&
		MIN (a->used, b->used) < 
		(1 << ((CHAR_BIT * sizeof (mp_word)) - (2 * DIGIT_BIT)))) {
			return fast_s_mp_mul_digs (pool, a, b, c, digs);
		}

		if ((res = mp_init_size(pool, &t, digs)) != MP_OKAY) {
			return res;
		}
		t.used = digs;

/*
		Compute the digits of the product directly
 */
		pa = a->used;
		for (ix = 0; ix < pa; ix++) {
			/* set the carry to zero */
			u = 0;

/*
			Limit ourselves to making digs digits of output.
*/
			pb = MIN (b->used, digs - ix);

/*
			Setup some aliases. Copy of the digit from a used
			within the nested loop
 */
			tmpx = a->dp[ix];

/*
			An alias for the destination shifted ix places
 */
			tmpt = t.dp + ix;

/*
			An alias for the digits of b
 */
			tmpy = b->dp;

/*
			Compute the columns of the output and propagate the carry
 */
			for (iy = 0; iy < pb; iy++) {
				/* compute the column as a mp_word */
				r       = ((mp_word)*tmpt) +
					((mp_word)tmpx) * ((mp_word)*tmpy++) +
					((mp_word) u);

				/* the new column is the lower part of the result */
				*tmpt++ = (mp_digit) (r & ((mp_word) MP_MASK));

				/* get the carry word from the result */
				u       = (mp_digit) (r >> ((mp_word) DIGIT_BIT));
			}
/*
			Set carry if it is placed below digs
 */
			if (ix + iy < digs) {
				*tmpt = u;
			}
		}

		mp_clamp (&t);
		mp_exch (&t, c);

		mp_clear (&t);
		return MP_OKAY;
}
#endif /* USE_SMALL_WORD */

/******************************************************************************/
/*
	Fast (comba) multiplier

	This is the fast column-array [comba] multiplier.  It is designed to
	compute the columns of the product first then handle the carries afterwards.
	This has the effect of making the nested loops that compute the columns
	very simple and schedulable on super-scalar processors.

	This has been modified to produce a variable number of digits of output so
	if say only a half-product is required you don't have to compute the upper
	half (a feature required for fast Barrett reduction).

	Based on Algorithm 14.12 on pp.595 of HAC.
*/

int32 fast_s_mp_mul_digs(psPool_t *pool, mp_int * a, mp_int * b, mp_int * c,
						  int32 digs)
{
	int32		olduse, res, pa, ix, iz, neg;
	mp_digit	W[MP_WARRAY];
	register	mp_word  _W;

	neg = (a->sign == b->sign) ? MP_ZPOS : MP_NEG;

/*
	grow the destination as required
 */
	if (c->alloc < digs) {
		if ((res = mp_grow(c, digs)) != MP_OKAY) {
			return res;
		}
	}

/*
	number of output digits to produce
 */
	pa = MIN(digs, a->used + b->used);

/*
	clear the carry
 */
	_W = 0;
	for (ix = 0; ix < pa; ix++) { 
		int32		tx, ty;
		int32		iy;
		mp_digit	*tmpx, *tmpy;

/*
		get offsets into the two bignums
 */
		ty = MIN(b->used-1, ix);
		tx = ix - ty;

/*
		setup temp aliases
 */
		tmpx = a->dp + tx;
		tmpy = b->dp + ty;

/*
		this is the number of times the loop will iterrate, essentially its
		while (tx++ < a->used && ty-- >= 0) { ... }
 */
		iy = MIN(a->used-tx, ty+1);

/*
		execute loop
 */
		for (iz = 0; iz < iy; ++iz) {
			_W += ((mp_word)*tmpx++)*((mp_word)*tmpy--);
		}

/*
		store term
 */
		W[ix] = (mp_digit)(_W & MP_MASK);

/*
		make next carry
 */
		_W = _W >> ((mp_word)DIGIT_BIT);
	}

/*
	store final carry
 */
	W[ix] = (mp_digit)(_W & MP_MASK);

/*
	setup dest
 */
	olduse  = c->used;
	c->used = pa;

	{
		register mp_digit *tmpc;
		tmpc = c->dp;
		for (ix = 0; ix < pa+1; ix++) {
/*
			now extract the previous digit [below the carry]
 */
			*tmpc++ = W[ix];
		}

/*
		clear unused digits [that existed in the old copy of c]
 */
		for (; ix < olduse; ix++) {
			*tmpc++ = 0;
		}
	}
	mp_clamp (c);
	c->sign = (c->used > 0) ? neg : MP_ZPOS;
	return MP_OKAY;
}

/******************************************************************************/
/*
	b = a*2
 */
int32 mp_mul_2 (mp_int * a, mp_int * b)
{
	int32		x, res, oldused;

/*
	grow to accomodate result
 */
	if (b->alloc < a->used + 1) {
		if ((res = mp_grow (b, a->used + 1)) != MP_OKAY) {
			return res;
		}
	}

	oldused = b->used;
	b->used = a->used;

	{
		register mp_digit r, rr, *tmpa, *tmpb;

		/* alias for source */
		tmpa = a->dp;

		/* alias for dest */
		tmpb = b->dp;

		/* carry */
		r = 0;
		for (x = 0; x < a->used; x++) {

/*
			get what will be the *next* carry bit from the MSB of the 
			current digit 
 */
			rr = *tmpa >> ((mp_digit)(DIGIT_BIT - 1));

/*
			now shift up this digit, add in the carry [from the previous]
 */
			*tmpb++ = ((*tmpa++ << ((mp_digit)1)) | r) & MP_MASK;

/*			copy the carry that would be from the source digit into the next
			iteration 
 */
			r = rr;
		}

/*
		new leading digit?
 */
		if (r != 0) {
/*
			add a MSB which is always 1 at this point
 */
			*tmpb = 1;
			++(b->used);
		}

/*
		now zero any excess digits on the destination that we didn't write to
 */
		tmpb = b->dp + b->used;
		for (x = b->used; x < oldused; x++) {
			*tmpb++ = 0;
		}
	}
	b->sign = a->sign;
	return MP_OKAY;
}

/******************************************************************************/
/*
	multiply by a digit
 */
int32 mp_mul_d(mp_int * a, mp_digit b, mp_int * c)
{
	mp_digit	u, *tmpa, *tmpc;
	mp_word		r;
	int32			ix, res, olduse;

/*
	make sure c is big enough to hold a*b
 */
	if (c->alloc < a->used + 1) {
		if ((res = mp_grow (c, a->used + 1)) != MP_OKAY) {
			return res;
		}
	}

/*
	get the original destinations used count
 */
	olduse = c->used;

/*
	set the sign
 */
	c->sign = a->sign;

/*
	alias for a->dp [source]
 */
	tmpa = a->dp;

/*
	alias for c->dp [dest]
 */
	tmpc = c->dp;

	/* zero carry */
	u = 0;

	/* compute columns */
	for (ix = 0; ix < a->used; ix++) {
/*
		compute product and carry sum for this term
 */
		r       = ((mp_word) u) + ((mp_word)*tmpa++) * ((mp_word)b);

/*
		mask off higher bits to get a single digit
 */
		*tmpc++ = (mp_digit) (r & ((mp_word) MP_MASK));

/*
		send carry into next iteration
 */
		u       = (mp_digit) (r >> ((mp_word) DIGIT_BIT));
	}

/*
	store final carry [if any] and increment ix offset
 */
	*tmpc++ = u;
	++ix;

/*
	now zero digits above the top
 */
	while (ix++ < olduse) {
		*tmpc++ = 0;
	}

	/* set used count */
	c->used = a->used + 1;
	mp_clamp(c);

	return MP_OKAY;
}

/******************************************************************************/
/*
	low level squaring, b = a*a, HAC pp.596-597, Algorithm 14.16
 */
#ifdef USE_SMALL_WORD
int32 s_mp_sqr (psPool_t *pool, mp_int * a, mp_int * b)
{
	mp_int		t;
	int32			res, ix, iy, pa;
	mp_word		r;
	mp_digit	u, tmpx, *tmpt;

	pa = a->used;
	if ((res = mp_init_size(pool, &t, 2*pa + 1)) != MP_OKAY) {
		return res;
	}
	
/*
	default used is maximum possible size
 */
	t.used = 2*pa + 1;

	for (ix = 0; ix < pa; ix++) {
/*
		first calculate the digit at 2*ix
		calculate double precision result
 */
		r = ((mp_word) t.dp[2*ix]) +
			((mp_word)a->dp[ix])*((mp_word)a->dp[ix]);

/*
		store lower part in result
 */
		t.dp[ix+ix] = (mp_digit) (r & ((mp_word) MP_MASK));

/*
		get the carry
 */
		u = (mp_digit)(r >> ((mp_word) DIGIT_BIT));

/*
		left hand side of A[ix] * A[iy]
 */
		tmpx = a->dp[ix];

/*
		alias for where to store the results
 */
		tmpt = t.dp + (2*ix + 1);

		for (iy = ix + 1; iy < pa; iy++) {
/*
			first calculate the product
 */
			r = ((mp_word)tmpx) * ((mp_word)a->dp[iy]);

/*
			now calculate the double precision result, note we use addition
			instead of *2 since it's easier to optimize
 */
			r       = ((mp_word) *tmpt) + r + r + ((mp_word) u);

/*
			store lower part
 */
			*tmpt++ = (mp_digit) (r & ((mp_word) MP_MASK));

			/* get carry */
			u       = (mp_digit)(r >> ((mp_word) DIGIT_BIT));
		}
		/* propagate upwards */
		while (u != ((mp_digit) 0)) {
			r       = ((mp_word) *tmpt) + ((mp_word) u);
			*tmpt++ = (mp_digit) (r & ((mp_word) MP_MASK));
			u       = (mp_digit)(r >> ((mp_word) DIGIT_BIT));
		}
	}

	mp_clamp (&t);
	mp_exch (&t, b);
	mp_clear (&t);
	return MP_OKAY;
}
#endif /* USE_SMALL_WORD */

/******************************************************************************/
/*
	fast squaring

	This is the comba method where the columns of the product are computed
	first then the carries are computed.  This has the effect of making a very
	simple inner loop that is executed the most

	W2 represents the outer products and W the inner.

	A further optimizations is made because the inner products are of the 
	form "A * B * 2".  The *2 part does not need to be computed until the end
	which is good because 64-bit shifts are slow!

	Based on Algorithm 14.16 on pp.597 of HAC.

	This is the 1.0 version, but no SSE stuff
*/
int32 fast_s_mp_sqr(psPool_t *pool, mp_int * a, mp_int * b)
{
	int32		olduse, res, pa, ix, iz;
	mp_digit	W[MP_WARRAY], *tmpx;
	mp_word		W1;

/* 
	grow the destination as required
 */
	pa = a->used + a->used;
	if (b->alloc < pa) {
		if ((res = mp_grow(b, pa)) != MP_OKAY) {
			return res;
		}
	}

/*
	number of output digits to produce
 */
	W1 = 0;
	for (ix = 0; ix < pa; ix++) { 
		int32		tx, ty, iy;
		mp_word		_W;
		mp_digit	*tmpy;

/*
		clear counter
 */
		_W = 0;

/*
		get offsets into the two bignums
 */
		ty = MIN(a->used-1, ix);
		tx = ix - ty;

/*
		setup temp aliases
 */
		tmpx = a->dp + tx;
		tmpy = a->dp + ty;

/*
		this is the number of times the loop will iterrate, essentially 
		while (tx++ < a->used && ty-- >= 0) { ... }
*/
		iy = MIN(a->used-tx, ty+1);

/*
		now for squaring tx can never equal ty 
		we halve the distance since they approach at a rate of 2x
		and we have to round because odd cases need to be executed
*/
		iy = MIN(iy, (ty-tx+1)>>1);

/*
		execute loop
 */
		for (iz = 0; iz < iy; iz++) {
			 _W += ((mp_word)*tmpx++)*((mp_word)*tmpy--);
		}

/*
		double the inner product and add carry
 */
		_W = _W + _W + W1;

/*
		even columns have the square term in them
 */
		if ((ix&1) == 0) {
			_W += ((mp_word)a->dp[ix>>1])*((mp_word)a->dp[ix>>1]);
		}

/*
		store it
 */
		W[ix] = (mp_digit)(_W & MP_MASK);

/*
		make next carry
 */
		W1 = _W >> ((mp_word)DIGIT_BIT);
	}

/*
	setup dest
 */
	olduse  = b->used;
	b->used = a->used+a->used;

	{
		mp_digit *tmpb;
		tmpb = b->dp;
		for (ix = 0; ix < pa; ix++) {
			*tmpb++ = W[ix] & MP_MASK;
		}

/*
		clear unused digits [that existed in the old copy of c]
 */
		for (; ix < olduse; ix++) {
			*tmpb++ = 0;
		}
	}
	mp_clamp(b);
	return MP_OKAY;
}

/******************************************************************************/
/*
	computes a = 2**b 

	Simple algorithm which zeroes the int32, grows it then just sets one bit
	as required.
 */
int32 mp_2expt (mp_int * a, int32 b)
{
	int32		res;

/*
	zero a as per default
 */
	mp_zero (a);

/*
	grow a to accomodate the single bit
 */
	if ((res = mp_grow (a, b / DIGIT_BIT + 1)) != MP_OKAY) {
		return res;
	}

/*
	set the used count of where the bit will go
 */
	a->used = b / DIGIT_BIT + 1;

/*
	put the single bit in its place
 */
	a->dp[b / DIGIT_BIT] = ((mp_digit)1) << (b % DIGIT_BIT);

	return MP_OKAY;
}

/******************************************************************************/
/*
	init an mp_init for a given size
 */
int32 mp_init_size(psPool_t *pool, mp_int * a, int32 size)
{
	int		x;
/*
	pad size so there are always extra digits
 */
	size += (MP_PREC * 2) - (size % MP_PREC);	

/*
	alloc mem
 */
	a->dp = OPT_CAST(mp_digit) psMalloc(pool, sizeof (mp_digit) * size);
	if (a->dp == NULL) {
		return MP_MEM;
	}
	a->used  = 0;
	a->alloc = size;
	a->sign  = MP_ZPOS;

/*
	zero the digits
 */
	for (x = 0; x < size; x++) {
		a->dp[x] = 0;
	}
	return MP_OKAY;
}

/******************************************************************************/
/*
	low level addition, based on HAC pp.594, Algorithm 14.7
 */
int32 s_mp_add (mp_int * a, mp_int * b, mp_int * c)
{
	mp_int		*x;
	int32			olduse, res, min, max;

/*
	find sizes, we let |a| <= |b| which means we have to sort them.  "x" will
	point to the input with the most digits
 */
	if (a->used > b->used) {
		min = b->used;
		max = a->used;
		x = a;
	} else {
		min = a->used;
		max = b->used;
		x = b;
	}

	/* init result */
	if (c->alloc < max + 1) {
		if ((res = mp_grow (c, max + 1)) != MP_OKAY) {
			return res;
		}
	}

/*
	get old used digit count and set new one
 */
	olduse = c->used;
	c->used = max + 1;

	{
		register mp_digit u, *tmpa, *tmpb, *tmpc;
		register int32 i;

		/* alias for digit pointers */

		/* first input */
		tmpa = a->dp;

		/* second input */
		tmpb = b->dp;

		/* destination */
		tmpc = c->dp;

		/* zero the carry */
		u = 0;
		for (i = 0; i < min; i++) {
/*
			Compute the sum at one digit, T[i] = A[i] + B[i] + U
 */
			*tmpc = *tmpa++ + *tmpb++ + u;

/*
			U = carry bit of T[i]
 */
			u = *tmpc >> ((mp_digit)DIGIT_BIT);

/*
			take away carry bit from T[i]
 */
			*tmpc++ &= MP_MASK;
		}

/*
		now copy higher words if any, that is in A+B if A or B has more digits add
		those in 
 */
		if (min != max) {
			for (; i < max; i++) {
				/* T[i] = X[i] + U */
				*tmpc = x->dp[i] + u;

				/* U = carry bit of T[i] */
				u = *tmpc >> ((mp_digit)DIGIT_BIT);

				/* take away carry bit from T[i] */
				*tmpc++ &= MP_MASK;
			}
		}

		/* add carry */
		*tmpc++ = u;

/*
		clear digits above oldused
 */
		for (i = c->used; i < olduse; i++) {
			*tmpc++ = 0;
		}
	}

	mp_clamp (c);
	return MP_OKAY;
}

/******************************************************************************/
/*
	FUTURE - this is only needed by the SSH code, SLOW or not, because RSA
	exponents are always odd.
*/
int32 mp_invmodSSH(psPool_t *pool, mp_int * a, mp_int * b, mp_int * c)
{
	mp_int		x, y, u, v, A, B, C, D;
	int32			res;

/*
	b cannot be negative
 */
	if (b->sign == MP_NEG || mp_iszero(b) == 1) {
		return MP_VAL;
	}

/*
	if the modulus is odd we can use a faster routine instead
 */
	if (mp_isodd (b) == 1) {
		return fast_mp_invmod(pool, a, b, c);
	}

/*
	init temps
 */
	if ((res = _mp_init_multi(pool, &x, &y, &u, &v,
			&A, &B, &C, &D)) != MP_OKAY) {
		return res;
	}

	/* x = a, y = b */
	if ((res = mp_copy(a, &x)) != MP_OKAY) {
		goto LBL_ERR;
	}
	if ((res = mp_copy(b, &y)) != MP_OKAY) {
		goto LBL_ERR;
	}

/*
	2. [modified] if x,y are both even then return an error!
 */
	if (mp_iseven(&x) == 1 && mp_iseven (&y) == 1) {
		res = MP_VAL;
		goto LBL_ERR;
	}

/*
	3. u=x, v=y, A=1, B=0, C=0,D=1
 */
	if ((res = mp_copy(&x, &u)) != MP_OKAY) {
		goto LBL_ERR;
	}
	if ((res = mp_copy(&y, &v)) != MP_OKAY) {
		goto LBL_ERR;
	}
	mp_set (&A, 1);
	mp_set (&D, 1);

top:
/*
	4.  while u is even do
 */
	while (mp_iseven(&u) == 1) {
		/* 4.1 u = u/2 */
		if ((res = mp_div_2(&u, &u)) != MP_OKAY) {
		goto LBL_ERR;
		}
		/* 4.2 if A or B is odd then */
		if (mp_isodd (&A) == 1 || mp_isodd (&B) == 1) {
			/* A = (A+y)/2, B = (B-x)/2 */
			if ((res = mp_add(&A, &y, &A)) != MP_OKAY) {
				goto LBL_ERR;
			}
			if ((res = mp_sub(&B, &x, &B)) != MP_OKAY) {
				goto LBL_ERR;
			}
		}
		/* A = A/2, B = B/2 */
		if ((res = mp_div_2(&A, &A)) != MP_OKAY) {
			goto LBL_ERR;
		}
		if ((res = mp_div_2(&B, &B)) != MP_OKAY) {
			goto LBL_ERR;
		}
	}

/*
	5.  while v is even do
 */
	while (mp_iseven(&v) == 1) {
		/* 5.1 v = v/2 */
		if ((res = mp_div_2(&v, &v)) != MP_OKAY) {
		goto LBL_ERR;
		}
		/* 5.2 if C or D is odd then */
		if (mp_isodd(&C) == 1 || mp_isodd (&D) == 1) {
			/* C = (C+y)/2, D = (D-x)/2 */
			if ((res = mp_add(&C, &y, &C)) != MP_OKAY) {
				goto LBL_ERR;
			}
			if ((res = mp_sub(&D, &x, &D)) != MP_OKAY) {
				goto LBL_ERR;
			}
		}
		/* C = C/2, D = D/2 */
		if ((res = mp_div_2(&C, &C)) != MP_OKAY) {
			goto LBL_ERR;
		}
		if ((res = mp_div_2(&D, &D)) != MP_OKAY) {
			goto LBL_ERR;
		}
	}

/*
	6.  if u >= v then
 */
	if (mp_cmp(&u, &v) != MP_LT) {
		/* u = u - v, A = A - C, B = B - D */
		if ((res = mp_sub(&u, &v, &u)) != MP_OKAY) {
		goto LBL_ERR;
		}

		if ((res = mp_sub(&A, &C, &A)) != MP_OKAY) {
		goto LBL_ERR;
		}

		if ((res = mp_sub(&B, &D, &B)) != MP_OKAY) {
		goto LBL_ERR;
		}
	} else {
		/* v - v - u, C = C - A, D = D - B */
		if ((res = mp_sub(&v, &u, &v)) != MP_OKAY) {
		goto LBL_ERR;
		}

		if ((res = mp_sub(&C, &A, &C)) != MP_OKAY) {
		goto LBL_ERR;
		}

		if ((res = mp_sub(&D, &B, &D)) != MP_OKAY) {
		goto LBL_ERR;
		}
	}

/*
	if not zero goto step 4
 */
	if (mp_iszero(&u) == 0)
		goto top;

/*
	now a = C, b = D, gcd == g*v
 */

/*
	if v != 1 then there is no inverse
 */
	if (mp_cmp_d(&v, 1) != MP_EQ) {
		res = MP_VAL;
		goto LBL_ERR;
	}

/*
	if its too low
 */
	while (mp_cmp_d(&C, 0) == MP_LT) {
		if ((res = mp_add(&C, b, &C)) != MP_OKAY) {
			goto LBL_ERR;
		}
	}

/*
	too big
 */
	while (mp_cmp_mag(&C, b) != MP_LT) {
		if ((res = mp_sub(&C, b, &C)) != MP_OKAY) {
			goto LBL_ERR;
		}
	}

/*
	C is now the inverse
 */
	mp_exch(&C, c);
	res = MP_OKAY;
LBL_ERR:_mp_clear_multi(&x, &y, &u, &v, &A, &B, &C, &D);
	return res;
}

/******************************************************************************/

/*
 *	Computes the modular inverse via binary extended euclidean algorithm,
 *	that is c = 1/a mod b 
 *
 * Based on slow invmod except this is optimized for the case where b is 
 * odd as per HAC Note 14.64 on pp. 610
 */
int32 fast_mp_invmod(psPool_t *pool, mp_int * a, mp_int * b, mp_int * c)
{
	mp_int		x, y, u, v, B, D;
	int32			res, neg;

/*
	2. [modified] b must be odd
 */
	if (mp_iseven (b) == 1) {
		return MP_VAL;
	}

/*
	init all our temps
 */
	if ((res = _mp_init_multi(pool, &x, &y, &u, &v, &B, &D, NULL, NULL)) != MP_OKAY) {
		return res;
	}

/*
	x == modulus, y == value to invert
 */
	if ((res = mp_copy(b, &x)) != MP_OKAY) {
		goto LBL_ERR;
	}

/*
	we need y = |a|
 */
	if ((res = mp_mod(pool, a, b, &y)) != MP_OKAY) {
		goto LBL_ERR;
	}

/*
	3. u=x, v=y, A=1, B=0, C=0,D=1
 */
	if ((res = mp_copy(&x, &u)) != MP_OKAY) {
		goto LBL_ERR;
	}
	if ((res = mp_copy(&y, &v)) != MP_OKAY) {
		goto LBL_ERR;
	}
	mp_set(&D, 1);

top:
/*
	4.  while u is even do
*/
	while (mp_iseven(&u) == 1) {
		/* 4.1 u = u/2 */
		if ((res = mp_div_2(&u, &u)) != MP_OKAY) {
			goto LBL_ERR;
		}
		/* 4.2 if B is odd then */
		if (mp_isodd(&B) == 1) {
			if ((res = mp_sub(&B, &x, &B)) != MP_OKAY) {
				goto LBL_ERR;
			}
		}
		/* B = B/2 */
		if ((res = mp_div_2(&B, &B)) != MP_OKAY) {
			goto LBL_ERR;
		}
	}

/*
	5.  while v is even do
 */
	while (mp_iseven(&v) == 1) {
		/* 5.1 v = v/2 */
		if ((res = mp_div_2(&v, &v)) != MP_OKAY) {
			goto LBL_ERR;
		}
		/* 5.2 if D is odd then */
		if (mp_isodd(&D) == 1) {
			/* D = (D-x)/2 */
			if ((res = mp_sub(&D, &x, &D)) != MP_OKAY) {
				goto LBL_ERR;
			}
		}
		/* D = D/2 */
		if ((res = mp_div_2(&D, &D)) != MP_OKAY) {
			goto LBL_ERR;
		}
	}

/*
	6.  if u >= v then
 */
	if (mp_cmp(&u, &v) != MP_LT) {
		/* u = u - v, B = B - D */
		if ((res = mp_sub(&u, &v, &u)) != MP_OKAY) {
			goto LBL_ERR;
		}

		if ((res = mp_sub(&B, &D, &B)) != MP_OKAY) {
			goto LBL_ERR;
		}
	} else {
		/* v - v - u, D = D - B */
		if ((res = mp_sub(&v, &u, &v)) != MP_OKAY) {
			goto LBL_ERR;
		}

		if ((res = mp_sub(&D, &B, &D)) != MP_OKAY) {
		goto LBL_ERR;
		}
	}

/*
	if not zero goto step 4
 */
	if (mp_iszero(&u) == 0) {
		goto top;
	}

/*
	now a = C, b = D, gcd == g*v
 */

/*
	if v != 1 then there is no inverse
 */
	if (mp_cmp_d(&v, 1) != MP_EQ) {
		res = MP_VAL;
		goto LBL_ERR;
	}

/*
	b is now the inverse
 */
	neg = a->sign;
	while (D.sign == MP_NEG) {
		if ((res = mp_add(&D, b, &D)) != MP_OKAY) {
		goto LBL_ERR;
		}
	}
	mp_exch(&D, c);
	c->sign = neg;
	res = MP_OKAY;

LBL_ERR:_mp_clear_multi(&x, &y, &u, &v, &B, &D, NULL, NULL);
	return res;
}

/******************************************************************************/
/*
	d = a + b (mod c)
 */
int32 mp_addmod (psPool_t *pool, mp_int * a, mp_int * b, mp_int * c, mp_int * d)
{
	int32			res;
	mp_int		t;

	if ((res = mp_init(pool, &t)) != MP_OKAY) {
		return res;
	}

	if ((res = mp_add (a, b, &t)) != MP_OKAY) {
		mp_clear (&t);
		return res;
	}
	res = mp_mod (pool, &t, c, d);
	mp_clear (&t);
	return res;
}

/******************************************************************************/
/*
	shrink a bignum
 */
int32 mp_shrink (mp_int * a)
{
	mp_digit *tmp;

	if (a->alloc != a->used && a->used > 0) {
		if ((tmp = psRealloc(a->dp, sizeof (mp_digit) * a->used)) == NULL) {
			return MP_MEM;
		}
		a->dp    = tmp;
		a->alloc = a->used;
	}
	return MP_OKAY;
}

/* single digit subtraction */
int32 mp_sub_d (mp_int * a, mp_digit b, mp_int * c)
{
	mp_digit *tmpa, *tmpc, mu;
	int32       res, ix, oldused;

	/* grow c as required */
	if (c->alloc < a->used + 1) {
		if ((res = mp_grow(c, a->used + 1)) != MP_OKAY) {
			return res;
		}
	}

	/* if a is negative just do an unsigned
	* addition [with fudged signs]
	*/
	if (a->sign == MP_NEG) {
		a->sign = MP_ZPOS;
		res     = mp_add_d(a, b, c);
		a->sign = c->sign = MP_NEG;
		return res;
	}

	/* setup regs */
	oldused = c->used;
	tmpa    = a->dp;
	tmpc    = c->dp;

	/* if a <= b simply fix the single digit */
	if ((a->used == 1 && a->dp[0] <= b) || a->used == 0) {
		if (a->used == 1) {
			*tmpc++ = b - *tmpa;
		} else {
			*tmpc++ = b;
		}
		ix      = 1;

		/* negative/1digit */
		c->sign = MP_NEG;
		c->used = 1;
	} else {
		/* positive/size */
		c->sign = MP_ZPOS;
		c->used = a->used;

		/* subtract first digit */
		*tmpc    = *tmpa++ - b;
		mu       = *tmpc >> (sizeof(mp_digit) * CHAR_BIT - 1);
		*tmpc++ &= MP_MASK;

		/* handle rest of the digits */
		for (ix = 1; ix < a->used; ix++) {
			*tmpc    = *tmpa++ - mu;
			mu       = *tmpc >> (sizeof(mp_digit) * CHAR_BIT - 1);
			*tmpc++ &= MP_MASK;
		}
	}

	/* zero excess digits */
	while (ix++ < oldused) {
		*tmpc++ = 0;
	}
	mp_clamp(c);
	return MP_OKAY;
}

/* single digit addition */
int32 mp_add_d (mp_int * a, mp_digit b, mp_int * c)
{
	int32     res, ix, oldused;
	mp_digit *tmpa, *tmpc, mu;

	/* grow c as required */
	if (c->alloc < a->used + 1) {
		if ((res = mp_grow(c, a->used + 1)) != MP_OKAY) {
			return res;
		}
	}

	/* if a is negative and |a| >= b, call c = |a| - b */
	if (a->sign == MP_NEG && (a->used > 1 || a->dp[0] >= b)) {
		/* temporarily fix sign of a */
		a->sign = MP_ZPOS;

		/* c = |a| - b */
		res = mp_sub_d(a, b, c);

		/* fix sign  */
		a->sign = c->sign = MP_NEG;
		return res;
	}

	/* old number of used digits in c */
	oldused = c->used;

	/* sign always positive */
	c->sign = MP_ZPOS;

	/* source alias */
	tmpa    = a->dp;

	/* destination alias */
	tmpc    = c->dp;

	/* if a is positive */
	if (a->sign == MP_ZPOS) {
		/* add digit, after this we're propagating the carry */
		*tmpc   = *tmpa++ + b;
		mu      = *tmpc >> DIGIT_BIT;
		*tmpc++ &= MP_MASK;

		/* now handle rest of the digits */
		for (ix = 1; ix < a->used; ix++) {
			*tmpc   = *tmpa++ + mu;
			mu      = *tmpc >> DIGIT_BIT;
			*tmpc++ &= MP_MASK;
		}
		/* set final carry */
		ix++;
		*tmpc++  = mu;

		/* setup size */
		c->used = a->used + 1;
	} else {
		/* a was negative and |a| < b */
		c->used  = 1;

		/* the result is a single digit */
		if (a->used == 1) {
			*tmpc++  =  b - a->dp[0];
		} else {
			*tmpc++  =  b;
		}

		/* setup count so the clearing of oldused
		* can fall through correctly
		*/
		ix       = 1;
	}

	/* now zero to oldused */
	while (ix++ < oldused) {
		*tmpc++ = 0;
	}
	mp_clamp(c);
	return MP_OKAY;
}


/******************************************************************************/

#endif /* USE_MPI2 */
