/* This is a software floating point library which can be used instead
   of the floating point routines in libgcc1.c for targets without
   hardware floating point.  */

/* Copyright 1994, 1997, 1998, 2003, 2007 Free Software Foundation, Inc.

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>.  */

/* As a special exception, if you link this library with other files,
   some of which are compiled with GCC, to produce an executable,
   this library does not by itself cause the resulting executable
   to be covered by the GNU General Public License.
   This exception does not however invalidate any other reasons why
   the executable file might be covered by the GNU General Public License.  */

/* This implements IEEE 754 format arithmetic, but does not provide a
   mechanism for setting the rounding mode, or for generating or handling
   exceptions.

   The original code by Steve Chamberlain, hacked by Mark Eichin and Jim
   Wilson, all of Cygnus Support.  */


#ifndef SIM_FPU_C
#define SIM_FPU_C

#include "sim-basics.h"
#include "sim-fpu.h"

#include "sim-io.h"
#include "sim-assert.h"


/* Debugging support. 
   If digits is -1, then print all digits.  */

static void
print_bits (unsigned64 x,
	    int msbit,
	    int digits,
	    sim_fpu_print_func print,
	    void *arg)
{
  unsigned64 bit = LSBIT64 (msbit);
  int i = 4;
  while (bit && digits)
    {
      if (i == 0)
	print (arg, ",");

      if ((x & bit))
	print (arg, "1");
      else
	print (arg, "0");
      bit >>= 1;

      if (digits > 0) digits--;
      i = (i + 1) % 4;
    }
}



/* Quick and dirty conversion between a host double and host 64bit int */

typedef union {
  double d;
  unsigned64 i;
} sim_fpu_map;  


/* A packed IEEE floating point number.

   Form is <SIGN:1><BIASEDEXP:NR_EXPBITS><FRAC:NR_FRACBITS> for both
   32 and 64 bit numbers.  This number is interpreted as:

   Normalized (0 < BIASEDEXP && BIASEDEXP < EXPMAX):
   (sign ? '-' : '+') 1.<FRAC> x 2 ^ (BIASEDEXP - EXPBIAS)

   Denormalized (0 == BIASEDEXP && FRAC != 0):
   (sign ? "-" : "+") 0.<FRAC> x 2 ^ (- EXPBIAS)

   Zero (0 == BIASEDEXP && FRAC == 0):
   (sign ? "-" : "+") 0.0
   
   Infinity (BIASEDEXP == EXPMAX && FRAC == 0):
   (sign ? "-" : "+") "infinity"

   SignalingNaN (BIASEDEXP == EXPMAX && FRAC > 0 && FRAC < QUIET_NAN):
   SNaN.FRAC

   QuietNaN (BIASEDEXP == EXPMAX && FRAC > 0 && FRAC > QUIET_NAN):
   QNaN.FRAC

   */

#define NR_EXPBITS  (is_double ?   11 :   8)
#define NR_FRACBITS (is_double ?   52 : 23)
#define SIGNBIT     (is_double ? MSBIT64 (0) : MSBIT64 (32))

#define EXPMAX32    (255)
#define EXMPAX64    (2047)
#define EXPMAX      ((unsigned) (is_double ? EXMPAX64 : EXPMAX32))

#define EXPBIAS32   (127)
#define EXPBIAS64   (1023)
#define EXPBIAS     (is_double ? EXPBIAS64 : EXPBIAS32)

#define QUIET_NAN   LSBIT64 (NR_FRACBITS - 1)



/* An unpacked floating point number.

   When unpacked, the fraction of both a 32 and 64 bit floating point
   number is stored using the same format:

   64 bit - <IMPLICIT_1:1><FRACBITS:52><GUARDS:8><PAD:00>
   32 bit - <IMPLICIT_1:1><FRACBITS:23><GUARDS:7><PAD:30> */

#define NR_PAD32    (30)
#define NR_PAD64    (0)
#define NR_PAD      (is_double ? NR_PAD64 : NR_PAD32)
#define PADMASK     (is_double ? 0 : LSMASK64 (NR_PAD32 - 1, 0))

#define NR_GUARDS32 (7 + NR_PAD32)
#define NR_GUARDS64 (8 + NR_PAD64)
#define NR_GUARDS  (is_double ? NR_GUARDS64 : NR_GUARDS32)
#define GUARDMASK  LSMASK64 (NR_GUARDS - 1, 0)

#define GUARDMSB   LSBIT64  (NR_GUARDS - 1)
#define GUARDLSB   LSBIT64  (NR_PAD)
#define GUARDROUND LSMASK64 (NR_GUARDS - 2, 0)

#define NR_FRAC_GUARD   (60)
#define IMPLICIT_1 LSBIT64 (NR_FRAC_GUARD)
#define IMPLICIT_2 LSBIT64 (NR_FRAC_GUARD + 1)
#define IMPLICIT_4 LSBIT64 (NR_FRAC_GUARD + 2)
#define NR_SPARE 2

#define FRAC32MASK LSMASK64 (63, NR_FRAC_GUARD - 32 + 1)

#define NORMAL_EXPMIN (-(EXPBIAS)+1)

#define NORMAL_EXPMAX32 (EXPBIAS32)
#define NORMAL_EXPMAX64 (EXPBIAS64)
#define NORMAL_EXPMAX (EXPBIAS)


/* Integer constants */

#define MAX_INT32  ((signed64) LSMASK64 (30, 0))
#define MAX_UINT32 LSMASK64 (31, 0)
#define MIN_INT32  ((signed64) LSMASK64 (63, 31))

#define MAX_INT64  ((signed64) LSMASK64 (62, 0))
#define MAX_UINT64 LSMASK64 (63, 0)
#define MIN_INT64  ((signed64) LSMASK64 (63, 63))

#define MAX_INT   (is_64bit ? MAX_INT64  : MAX_INT32)
#define MIN_INT   (is_64bit ? MIN_INT64  : MIN_INT32)
#define MAX_UINT  (is_64bit ? MAX_UINT64 : MAX_UINT32)
#define NR_INTBITS (is_64bit ? 64 : 32)

/* Squeese an unpacked sim_fpu struct into a 32/64 bit integer */
STATIC_INLINE_SIM_FPU (unsigned64)
pack_fpu (const sim_fpu *src,
	  int is_double)
{
  int sign;
  unsigned64 exp;
  unsigned64 fraction;
  unsigned64 packed;

  switch (src->class)
    {
      /* create a NaN */
    case sim_fpu_class_qnan:
      sign = src->sign;
      exp = EXPMAX;
      /* force fraction to correct class */
      fraction = src->fraction;
      fraction >>= NR_GUARDS;
#ifdef SIM_QUIET_NAN_NEGATED
      fraction |= QUIET_NAN - 1;
#else
      fraction |= QUIET_NAN;
#endif
      break;
    case sim_fpu_class_snan:
      sign = src->sign;
      exp = EXPMAX;
      /* force fraction to correct class */
      fraction = src->fraction;
      fraction >>= NR_GUARDS;
#ifdef SIM_QUIET_NAN_NEGATED
      fraction |= QUIET_NAN;
#else
      fraction &= ~QUIET_NAN;
#endif
      break;
    case sim_fpu_class_infinity:
      sign = src->sign;
      exp = EXPMAX;
      fraction = 0;
      break;
    case sim_fpu_class_zero:
      sign = src->sign;
      exp = 0;
      fraction = 0;
      break;
    case sim_fpu_class_number:
    case sim_fpu_class_denorm:
      ASSERT (src->fraction >= IMPLICIT_1);
      ASSERT (src->fraction < IMPLICIT_2);
      if (src->normal_exp < NORMAL_EXPMIN)
	{
	  /* This number's exponent is too low to fit into the bits
	     available in the number We'll denormalize the number by
	     storing zero in the exponent and shift the fraction to
	     the right to make up for it. */
	  int nr_shift = NORMAL_EXPMIN - src->normal_exp;
	  if (nr_shift > NR_FRACBITS)
	    {
	      /* underflow, just make the number zero */
	      sign = src->sign;
	      exp = 0;
	      fraction = 0;
	    }
	  else
	    {
	      sign = src->sign;
	      exp = 0;
	      /* Shift by the value */
	      fraction = src->fraction;
	      fraction >>= NR_GUARDS;
	      fraction >>= nr_shift;
	    }
	}
      else if (src->normal_exp > NORMAL_EXPMAX)
	{
	  /* Infinity */
	  sign = src->sign;
	  exp = EXPMAX;
	  fraction = 0; 
	}
      else
	{
	  exp = (src->normal_exp + EXPBIAS);
	  sign = src->sign;
	  fraction = src->fraction;
	  /* FIXME: Need to round according to WITH_SIM_FPU_ROUNDING
             or some such */
	  /* Round to nearest: If the guard bits are the all zero, but
	     the first, then we're half way between two numbers,
	     choose the one which makes the lsb of the answer 0.  */
	  if ((fraction & GUARDMASK) == GUARDMSB)
	    {
	      if ((fraction & (GUARDMSB << 1)))
		fraction += (GUARDMSB << 1);
	    }
	  else
	    {
	      /* Add a one to the guards to force round to nearest */
	      fraction += GUARDROUND;
	    }
	  if ((fraction & IMPLICIT_2)) /* rounding resulted in carry */
	    {
	      exp += 1;
	      fraction >>= 1;
	    }
	  fraction >>= NR_GUARDS;
	  /* When exp == EXPMAX (overflow from carry) fraction must
	     have been made zero */
	  ASSERT ((exp == EXPMAX) <= ((fraction & ~IMPLICIT_1) == 0));
	}
      break;
    default:
      abort ();
    }

  packed = ((sign ? SIGNBIT : 0)
	     | (exp << NR_FRACBITS)
	     | LSMASKED64 (fraction, NR_FRACBITS - 1, 0));

  /* trace operation */
#if 0
  if (is_double)
    {
    }
  else
    {
      printf ("pack_fpu: ");
      printf ("-> %c%0lX.%06lX\n",
	      LSMASKED32 (packed, 31, 31) ? '8' : '0',
	      (long) LSEXTRACTED32 (packed, 30, 23),
	      (long) LSEXTRACTED32 (packed, 23 - 1, 0));
    }
#endif
  
  return packed;
}


/* Unpack a 32/64 bit integer into a sim_fpu structure */
STATIC_INLINE_SIM_FPU (void)
unpack_fpu (sim_fpu *dst, unsigned64 packed, int is_double)
{
  unsigned64 fraction = LSMASKED64 (packed, NR_FRACBITS - 1, 0);
  unsigned exp = LSEXTRACTED64 (packed, NR_EXPBITS + NR_FRACBITS - 1, NR_FRACBITS);
  int sign = (packed & SIGNBIT) != 0;

  if (exp == 0)
    {
      /* Hmm.  Looks like 0 */
      if (fraction == 0)
	{
	  /* tastes like zero */
	  dst->class = sim_fpu_class_zero;
	  dst->sign = sign;
	  dst->normal_exp = 0;
	}
      else
	{
	  /* Zero exponent with non zero fraction - it's denormalized,
	     so there isn't a leading implicit one - we'll shift it so
	     it gets one.  */
	  dst->normal_exp = exp - EXPBIAS + 1;
	  dst->class = sim_fpu_class_denorm;
	  dst->sign = sign;
	  fraction <<= NR_GUARDS;
	  while (fraction < IMPLICIT_1)
	    {
	      fraction <<= 1;
	      dst->normal_exp--;
	    }
	  dst->fraction = fraction;
	}
    }
  else if (exp == EXPMAX)
    {
      /* Huge exponent*/
      if (fraction == 0)
	{
	  /* Attached to a zero fraction - means infinity */
	  dst->class = sim_fpu_class_infinity;
	  dst->sign = sign;
	  /* dst->normal_exp = EXPBIAS; */
	  /* dst->fraction = 0; */
	}
      else
	{
	  int qnan;

	  /* Non zero fraction, means NaN */
	  dst->sign = sign;
	  dst->fraction = (fraction << NR_GUARDS);
#ifdef SIM_QUIET_NAN_NEGATED
	  qnan = (fraction & QUIET_NAN) == 0;
#else
	  qnan = fraction >= QUIET_NAN;
#endif
	  if (qnan)
	    dst->class = sim_fpu_class_qnan;
	  else
	    dst->class = sim_fpu_class_snan;
	}
    }
  else
    {
      /* Nothing strange about this number */
      dst->class = sim_fpu_class_number;
      dst->sign = sign;
      dst->fraction = ((fraction << NR_GUARDS) | IMPLICIT_1);
      dst->normal_exp = exp - EXPBIAS;
    }

  /* trace operation */
#if 0
  if (is_double)
    {
    }
  else
    {
      printf ("unpack_fpu: %c%02lX.%06lX ->\n",
	      LSMASKED32 (packed, 31, 31) ? '8' : '0',
	      (long) LSEXTRACTED32 (packed, 30, 23),
	      (long) LSEXTRACTED32 (packed, 23 - 1, 0));
    }
#endif

  /* sanity checks */
  {
    sim_fpu_map val;
    val.i = pack_fpu (dst, 1);
    if (is_double)
      {
	ASSERT (val.i == packed);
      }
    else
      {
	unsigned32 val = pack_fpu (dst, 0);
	unsigned32 org = packed;
	ASSERT (val == org);
      }
  }
}


/* Convert a floating point into an integer */
STATIC_INLINE_SIM_FPU (int)
fpu2i (signed64 *i,
       const sim_fpu *s,
       int is_64bit,
       sim_fpu_round round)
{
  unsigned64 tmp;
  int shift;
  int status = 0;
  if (sim_fpu_is_zero (s))
    {
      *i = 0;
      return 0;
    }
  if (sim_fpu_is_snan (s))
    {
      *i = MIN_INT; /* FIXME */
      return sim_fpu_status_invalid_cvi;
    }
  if (sim_fpu_is_qnan (s))
    {
      *i = MIN_INT; /* FIXME */
      return sim_fpu_status_invalid_cvi;
    }
  /* map infinity onto MAX_INT... */
  if (sim_fpu_is_infinity (s))
    {
      *i = s->sign ? MIN_INT : MAX_INT;
      return sim_fpu_status_invalid_cvi;
    }
  /* it is a number, but a small one */
  if (s->normal_exp < 0)
    {
      *i = 0;
      return sim_fpu_status_inexact;
    }
  /* Is the floating point MIN_INT or just close? */
  if (s->sign && s->normal_exp == (NR_INTBITS - 1))
    {
      *i = MIN_INT;
      ASSERT (s->fraction >= IMPLICIT_1);
      if (s->fraction == IMPLICIT_1)
	return 0; /* exact */
      if (is_64bit) /* can't round */
	return sim_fpu_status_invalid_cvi; /* must be overflow */
      /* For a 32bit with MAX_INT, rounding is possible */
      switch (round)
	{
	case sim_fpu_round_default:
	  abort ();
	case sim_fpu_round_zero:
	  if ((s->fraction & FRAC32MASK) != IMPLICIT_1)
	    return sim_fpu_status_invalid_cvi;
	  else
	    return sim_fpu_status_inexact;
	  break;
	case sim_fpu_round_near:
	  {
	    if ((s->fraction & FRAC32MASK) != IMPLICIT_1)
	      return sim_fpu_status_invalid_cvi;
	    else if ((s->fraction & !FRAC32MASK) >= (~FRAC32MASK >> 1))
	      return sim_fpu_status_invalid_cvi;
	    else
	      return sim_fpu_status_inexact;
	  }
	case sim_fpu_round_up:
	  if ((s->fraction & FRAC32MASK) == IMPLICIT_1)
	    return sim_fpu_status_inexact;
	  else
	    return sim_fpu_status_invalid_cvi;
	case sim_fpu_round_down:
	  return sim_fpu_status_invalid_cvi;
	}
    }
  /* Would right shifting result in the FRAC being shifted into
     (through) the integer's sign bit? */
  if (s->normal_exp > (NR_INTBITS - 2))
    {
      *i = s->sign ? MIN_INT : MAX_INT;
      return sim_fpu_status_invalid_cvi;
    }
  /* normal number shift it into place */
  tmp = s->fraction;
  shift = (s->normal_exp - (NR_FRAC_GUARD));
  if (shift > 0)
    {
      tmp <<= shift;
    }
  else
    {
      shift = -shift;
      if (tmp & ((SIGNED64 (1) << shift) - 1))
	status |= sim_fpu_status_inexact;
      tmp >>= shift;
    }
  *i = s->sign ? (-tmp) : (tmp);
  return status;
}

/* convert an integer into a floating point */
STATIC_INLINE_SIM_FPU (int)
i2fpu (sim_fpu *f, signed64 i, int is_64bit)
{
  int status = 0;
  if (i == 0)
    {
      f->class = sim_fpu_class_zero;
      f->sign = 0;
      f->normal_exp = 0;
    }
  else
    {
      f->class = sim_fpu_class_number;
      f->sign = (i < 0);
      f->normal_exp = NR_FRAC_GUARD;

      if (f->sign) 
	{
	  /* Special case for minint, since there is no corresponding
	     +ve integer representation for it */
	  if (i == MIN_INT)
	    {
	      f->fraction = IMPLICIT_1;
	      f->normal_exp = NR_INTBITS - 1;
	    }
	  else
	    f->fraction = (-i);
	}
      else
	f->fraction = i;

      if (f->fraction >= IMPLICIT_2)
	{
	  do 
	    {
	      f->fraction = (f->fraction >> 1) | (f->fraction & 1);
	      f->normal_exp += 1;
	    }
	  while (f->fraction >= IMPLICIT_2);
	}
      else if (f->fraction < IMPLICIT_1)
	{
	  do
	    {
	      f->fraction <<= 1;
	      f->normal_exp -= 1;
	    }
	  while (f->fraction < IMPLICIT_1);
	}
    }

  /* trace operation */
#if 0
  {
    printf ("i2fpu: 0x%08lX ->\n", (long) i);
  }
#endif

  /* sanity check */
  {
    signed64 val;
    fpu2i (&val, f, is_64bit, sim_fpu_round_zero);
    if (i >= MIN_INT32 && i <= MAX_INT32)
      {
	ASSERT (val == i);
      }
  }

  return status;
}


/* Convert a floating point into an integer */
STATIC_INLINE_SIM_FPU (int)
fpu2u (unsigned64 *u, const sim_fpu *s, int is_64bit)
{
  const int is_double = 1;
  unsigned64 tmp;
  int shift;
  if (sim_fpu_is_zero (s))
    {
      *u = 0;
      return 0;
    }
  if (sim_fpu_is_nan (s))
    {
      *u = 0;
      return 0;
    }
  /* it is a negative number */
  if (s->sign)
    {
      *u = 0;
      return 0;
    }
  /* get reasonable MAX_USI_INT... */
  if (sim_fpu_is_infinity (s))
    {
      *u = MAX_UINT;
      return 0;
    }
  /* it is a number, but a small one */
  if (s->normal_exp < 0)
    {
      *u = 0;
      return 0;
    }
  /* overflow */
  if (s->normal_exp > (NR_INTBITS - 1))
    {
      *u = MAX_UINT;
      return 0;
    }
  /* normal number */
  tmp = (s->fraction & ~PADMASK);
  shift = (s->normal_exp - (NR_FRACBITS + NR_GUARDS));
  if (shift > 0)
    {
      tmp <<= shift;
    }
  else
    {
      shift = -shift;
      tmp >>= shift;
    }
  *u = tmp;
  return 0;
}

/* Convert an unsigned integer into a floating point */
STATIC_INLINE_SIM_FPU (int)
u2fpu (sim_fpu *f, unsigned64 u, int is_64bit)
{
  if (u == 0)
    {
      f->class = sim_fpu_class_zero;
      f->sign = 0;
      f->normal_exp = 0;
    }
  else
    {
      f->class = sim_fpu_class_number;
      f->sign = 0;
      f->normal_exp = NR_FRAC_GUARD;
      f->fraction = u;

      while (f->fraction < IMPLICIT_1)
	{
	  f->fraction <<= 1;
	  f->normal_exp -= 1;
	}
    }
  return 0;
}


/* register <-> sim_fpu */

INLINE_SIM_FPU (void)
sim_fpu_32to (sim_fpu *f, unsigned32 s)
{
  unpack_fpu (f, s, 0);
}


INLINE_SIM_FPU (void)
sim_fpu_232to (sim_fpu *f, unsigned32 h, unsigned32 l)
{
  unsigned64 s = h;
  s = (s << 32) | l;
  unpack_fpu (f, s, 1);
}


INLINE_SIM_FPU (void)
sim_fpu_64to (sim_fpu *f, unsigned64 s)
{
  unpack_fpu (f, s, 1);
}


INLINE_SIM_FPU (void)
sim_fpu_to32 (unsigned32 *s,
	      const sim_fpu *f)
{
  *s = pack_fpu (f, 0);
}


INLINE_SIM_FPU (void)
sim_fpu_to232 (unsigned32 *h, unsigned32 *l,
	       const sim_fpu *f)
{
  unsigned64 s = pack_fpu (f, 1);
  *l = s;
  *h = (s >> 32);
}


INLINE_SIM_FPU (void)
sim_fpu_to64 (unsigned64 *u,
	      const sim_fpu *f)
{
  *u = pack_fpu (f, 1);
}


INLINE_SIM_FPU (void)
sim_fpu_fractionto (sim_fpu *f,
		    int sign,
		    int normal_exp,
		    unsigned64 fraction,
		    int precision)
{
  int shift = (NR_FRAC_GUARD - precision);
  f->class = sim_fpu_class_number;
  f->sign = sign;
  f->normal_exp = normal_exp;
  /* shift the fraction to where sim-fpu expects it */
  if (shift >= 0)
    f->fraction = (fraction << shift);
  else
    f->fraction = (fraction >> -shift);
  f->fraction |= IMPLICIT_1;
}


INLINE_SIM_FPU (unsigned64)
sim_fpu_tofraction (const sim_fpu *d,
		    int precision)
{
  /* we have NR_FRAC_GUARD bits, we want only PRECISION bits */
  int shift = (NR_FRAC_GUARD - precision);
  unsigned64 fraction = (d->fraction & ~IMPLICIT_1);
  if (shift >= 0)
    return fraction >> shift;
  else
    return fraction << -shift;
}


/* Rounding */

STATIC_INLINE_SIM_FPU (int)
do_normal_overflow (sim_fpu *f,
		    int is_double,
		    sim_fpu_round round)
{
  switch (round)
    {
    case sim_fpu_round_default:
      return 0;
    case sim_fpu_round_near:
      f->class = sim_fpu_class_infinity;
      break;
    case sim_fpu_round_up:
      if (!f->sign)
	f->class = sim_fpu_class_infinity;
      break;
    case sim_fpu_round_down:
      if (f->sign)
	f->class = sim_fpu_class_infinity;
      break;
    case sim_fpu_round_zero:
      break;
    }
  f->normal_exp = NORMAL_EXPMAX;
  f->fraction = LSMASK64 (NR_FRAC_GUARD, NR_GUARDS);
  return (sim_fpu_status_overflow | sim_fpu_status_inexact);
}

STATIC_INLINE_SIM_FPU (int)
do_normal_underflow (sim_fpu *f,
		     int is_double,
		     sim_fpu_round round)
{
  switch (round)
    {
    case sim_fpu_round_default:
      return 0;
    case sim_fpu_round_near:
      f->class = sim_fpu_class_zero;
      break;
    case sim_fpu_round_up:
      if (f->sign)
	f->class = sim_fpu_class_zero;
      break;
    case sim_fpu_round_down:
      if (!f->sign)
	f->class = sim_fpu_class_zero;
      break;
    case sim_fpu_round_zero:
      f->class = sim_fpu_class_zero;
      break;
    }
  f->normal_exp = NORMAL_EXPMIN - NR_FRACBITS;
  f->fraction = IMPLICIT_1;
  return (sim_fpu_status_inexact | sim_fpu_status_underflow);
}



/* Round a number using NR_GUARDS.
   Will return the rounded number or F->FRACTION == 0 when underflow */

STATIC_INLINE_SIM_FPU (int)
do_normal_round (sim_fpu *f,
		 int nr_guards,
		 sim_fpu_round round)
{
  unsigned64 guardmask = LSMASK64 (nr_guards - 1, 0);
  unsigned64 guardmsb = LSBIT64 (nr_guards - 1);
  unsigned64 fraclsb = guardmsb << 1;
  if ((f->fraction & guardmask))
    {
      int status = sim_fpu_status_inexact;
      switch (round)
	{
	case sim_fpu_round_default:
	  return 0;
	case sim_fpu_round_near:
	  if ((f->fraction & guardmsb))
	    {
	      if ((f->fraction & fraclsb))
		{
		  status |= sim_fpu_status_rounded;
		}
	      else if ((f->fraction & (guardmask >> 1)))
		{
		  status |= sim_fpu_status_rounded;
		}
	    }
	  break;
	case sim_fpu_round_up:
	  if (!f->sign)
	    status |= sim_fpu_status_rounded;
	  break;
	case sim_fpu_round_down:
	  if (f->sign)
	    status |= sim_fpu_status_rounded;
	  break;
	case sim_fpu_round_zero:
	  break;
	}
      f->fraction &= ~guardmask;
      /* round if needed, handle resulting overflow */
      if ((status & sim_fpu_status_rounded))
	{
	  f->fraction += fraclsb;
	  if ((f->fraction & IMPLICIT_2))
	    {
	      f->fraction >>= 1;
	      f->normal_exp += 1;
	    }
	}
      return status;
    }
  else
    return 0;
}


STATIC_INLINE_SIM_FPU (int)
do_round (sim_fpu *f,
	  int is_double,
	  sim_fpu_round round,
	  sim_fpu_denorm denorm)
{
  switch (f->class)
    {
    case sim_fpu_class_qnan:
    case sim_fpu_class_zero:
    case sim_fpu_class_infinity:
      return 0;
      break;
    case sim_fpu_class_snan:
      /* Quieten a SignalingNaN */ 
      f->class = sim_fpu_class_qnan;
      return sim_fpu_status_invalid_snan;
      break;
    case sim_fpu_class_number:
    case sim_fpu_class_denorm:
      {
	int status;
	ASSERT (f->fraction < IMPLICIT_2);
	ASSERT (f->fraction >= IMPLICIT_1);
	if (f->normal_exp < NORMAL_EXPMIN)
	  {
	    /* This number's exponent is too low to fit into the bits
	       available in the number.  Round off any bits that will be
	       discarded as a result of denormalization.  Edge case is
	       the implicit bit shifted to GUARD0 and then rounded
	       up. */
	    int shift = NORMAL_EXPMIN - f->normal_exp;
	    if (shift + NR_GUARDS <= NR_FRAC_GUARD + 1
		&& !(denorm & sim_fpu_denorm_zero))
	      {
		status = do_normal_round (f, shift + NR_GUARDS, round);
		if (f->fraction == 0) /* rounding underflowed */
		  {
		    status |= do_normal_underflow (f, is_double, round);
		  }
		else if (f->normal_exp < NORMAL_EXPMIN) /* still underflow? */
		  {
		    status |= sim_fpu_status_denorm;
		    /* Any loss of precision when denormalizing is
		       underflow. Some processors check for underflow
		       before rounding, some after! */
		    if (status & sim_fpu_status_inexact)
		      status |= sim_fpu_status_underflow;
		    /* Flag that resultant value has been denormalized */
		    f->class = sim_fpu_class_denorm;
		  }
		else if ((denorm & sim_fpu_denorm_underflow_inexact))
		  {
		    if ((status & sim_fpu_status_inexact))
		      status |= sim_fpu_status_underflow;
		  }
	      }
	    else
	      {
		status = do_normal_underflow (f, is_double, round);
	      }
	  }
	else if (f->normal_exp > NORMAL_EXPMAX)
	  {
	    /* Infinity */
	    status = do_normal_overflow (f, is_double, round);
	  }
	else
	  {
	    status = do_normal_round (f, NR_GUARDS, round);
	    if (f->fraction == 0)
	      /* f->class = sim_fpu_class_zero; */
	      status |= do_normal_underflow (f, is_double, round);
	    else if (f->normal_exp > NORMAL_EXPMAX)
	      /* oops! rounding caused overflow */
	      status |= do_normal_overflow (f, is_double, round);
	  }
	ASSERT ((f->class == sim_fpu_class_number
		 || f->class == sim_fpu_class_denorm)
		<= (f->fraction < IMPLICIT_2 && f->fraction >= IMPLICIT_1));
	return status;
      }
    }
  return 0;
}

INLINE_SIM_FPU (int)
sim_fpu_round_32 (sim_fpu *f,
		  sim_fpu_round round,
		  sim_fpu_denorm denorm)
{
  return do_round (f, 0, round, denorm);
}

INLINE_SIM_FPU (int)
sim_fpu_round_64 (sim_fpu *f,
		  sim_fpu_round round,
		  sim_fpu_denorm denorm)
{
  return do_round (f, 1, round, denorm);
}



/* Arithmetic ops */

INLINE_SIM_FPU (int)
sim_fpu_add (sim_fpu *f,
	     const sim_fpu *l,
	     const sim_fpu *r)
{
  if (sim_fpu_is_snan (l))
    {
      *f = *l;
      f->class = sim_fpu_class_qnan;
      return sim_fpu_status_invalid_snan;
    }
  if (sim_fpu_is_snan (r))
    {
      *f = *r;
      f->class = sim_fpu_class_qnan;
      return sim_fpu_status_invalid_snan;
    }
  if (sim_fpu_is_qnan (l))
    {
      *f = *l;
      return 0;
    }
  if (sim_fpu_is_qnan (r))
    {
      *f = *r;
      return 0;
    }
  if (sim_fpu_is_infinity (l))
    {
      if (sim_fpu_is_infinity (r)
	  && l->sign != r->sign)
	{
	  *f = sim_fpu_qnan;
	  return sim_fpu_status_invalid_isi;
	}
      *f = *l;
      return 0;
    }
  if (sim_fpu_is_infinity (r))
    {
      *f = *r;
      return 0;
    }
  if (sim_fpu_is_zero (l))
    {
      if (sim_fpu_is_zero (r))
	{
	  *f = sim_fpu_zero;
	  f->sign = l->sign & r->sign;
	}
      else
	*f = *r;
      return 0;
    }
  if (sim_fpu_is_zero (r))
    {
      *f = *l;
      return 0;
    }
  {
    int status = 0;
    int shift = l->normal_exp - r->normal_exp;
    unsigned64 lfraction;
    unsigned64 rfraction;
    /* use exp of larger */
    if (shift >= NR_FRAC_GUARD)
      {
	/* left has much bigger magnitute */
	*f = *l;
	return sim_fpu_status_inexact;
      }
    if (shift <= - NR_FRAC_GUARD)
      {
	/* right has much bigger magnitute */
	*f = *r;
	return sim_fpu_status_inexact;
      }
    lfraction = l->fraction;
    rfraction = r->fraction;
    if (shift > 0)
      {
	f->normal_exp = l->normal_exp;
	if (rfraction & LSMASK64 (shift - 1, 0))
	  {
	    status |= sim_fpu_status_inexact;
	    rfraction |= LSBIT64 (shift); /* stick LSBit */
	  }
	rfraction >>= shift;
      }
    else if (shift < 0)
      {
	f->normal_exp = r->normal_exp;
	if (lfraction & LSMASK64 (- shift - 1, 0))
	  {
	    status |= sim_fpu_status_inexact;
	    lfraction |= LSBIT64 (- shift); /* stick LSBit */
	  }
	lfraction >>= -shift;
      }
    else
      {
	f->normal_exp = r->normal_exp;
      }

    /* perform the addition */
    if (l->sign)
      lfraction = - lfraction;
    if (r->sign)
      rfraction = - rfraction;
    f->fraction = lfraction + rfraction;

    /* zero? */
    if (f->fraction == 0)
      {
	*f = sim_fpu_zero;
	return 0;
      }

    /* sign? */
    f->class = sim_fpu_class_number;
    if ((signed64) f->fraction >= 0)
      f->sign = 0;
    else
      {
	f->sign = 1;
	f->fraction = - f->fraction;
      }

    /* normalize it */
    if ((f->fraction & IMPLICIT_2))
      {
	f->fraction = (f->fraction >> 1) | (f->fraction & 1);
	f->normal_exp ++;
      }
    else if (f->fraction < IMPLICIT_1)
      {
	do
	  {
	    f->fraction <<= 1;
	    f->normal_exp --;
	  }
	while (f->fraction < IMPLICIT_1);
      }
    ASSERT (f->fraction >= IMPLICIT_1 && f->fraction < IMPLICIT_2);
    return status;
  }
}


INLINE_SIM_FPU (int)
sim_fpu_sub (sim_fpu *f,
	     const sim_fpu *l,
	     const sim_fpu *r)
{
  if (sim_fpu_is_snan (l))
    {
      *f = *l;
      f->class = sim_fpu_class_qnan;
      return sim_fpu_status_invalid_snan;
    }
  if (sim_fpu_is_snan (r))
    {
      *f = *r;
      f->class = sim_fpu_class_qnan;
      return sim_fpu_status_invalid_snan;
    }
  if (sim_fpu_is_qnan (l))
    {
      *f = *l;
      return 0;
    }
  if (sim_fpu_is_qnan (r))
    {
      *f = *r;
      return 0;
    }
  if (sim_fpu_is_infinity (l))
    {
      if (sim_fpu_is_infinity (r)
	  && l->sign == r->sign)
	{
	  *f = sim_fpu_qnan;
	  return sim_fpu_status_invalid_isi;
	}
      *f = *l;
      return 0;
    }
  if (sim_fpu_is_infinity (r))
    {
      *f = *r;
      f->sign = !r->sign;
      return 0;
    }
  if (sim_fpu_is_zero (l))
    {
      if (sim_fpu_is_zero (r))
	{
	  *f = sim_fpu_zero;
	  f->sign = l->sign & !r->sign;
	}
      else
	{
	  *f = *r;
	  f->sign = !r->sign;
	}
      return 0;
    }
  if (sim_fpu_is_zero (r))
    {
      *f = *l;
      return 0;
    }
  {
    int status = 0;
    int shift = l->normal_exp - r->normal_exp;
    unsigned64 lfraction;
    unsigned64 rfraction;
    /* use exp of larger */
    if (shift >= NR_FRAC_GUARD)
      {
	/* left has much bigger magnitute */
	*f = *l;
	return sim_fpu_status_inexact;
      }
    if (shift <= - NR_FRAC_GUARD)
      {
	/* right has much bigger magnitute */
	*f = *r;
	f->sign = !r->sign;
	return sim_fpu_status_inexact;
      }
    lfraction = l->fraction;
    rfraction = r->fraction;
    if (shift > 0)
      {
	f->normal_exp = l->normal_exp;
	if (rfraction & LSMASK64 (shift - 1, 0))
	  {
	    status |= sim_fpu_status_inexact;
	    rfraction |= LSBIT64 (shift); /* stick LSBit */
	  }
	rfraction >>= shift;
      }
    else if (shift < 0)
      {
	f->normal_exp = r->normal_exp;
	if (lfraction & LSMASK64 (- shift - 1, 0))
	  {
	    status |= sim_fpu_status_inexact;
	    lfraction |= LSBIT64 (- shift); /* stick LSBit */
	  }
	lfraction >>= -shift;
      }
    else
      {
	f->normal_exp = r->normal_exp;
      }

    /* perform the subtraction */
    if (l->sign)
      lfraction = - lfraction;
    if (!r->sign)
      rfraction = - rfraction;
    f->fraction = lfraction + rfraction;

    /* zero? */
    if (f->fraction == 0)
      {
	*f = sim_fpu_zero;
	return 0;
      }

    /* sign? */
    f->class = sim_fpu_class_number;
    if ((signed64) f->fraction >= 0)
      f->sign = 0;
    else
      {
	f->sign = 1;
	f->fraction = - f->fraction;
      }

    /* normalize it */
    if ((f->fraction & IMPLICIT_2))
      {
	f->fraction = (f->fraction >> 1) | (f->fraction & 1);
	f->normal_exp ++;
      }
    else if (f->fraction < IMPLICIT_1)
      {
	do
	  {
	    f->fraction <<= 1;
	    f->normal_exp --;
	  }
	while (f->fraction < IMPLICIT_1);
      }
    ASSERT (f->fraction >= IMPLICIT_1 && f->fraction < IMPLICIT_2);
    return status;
  }
}


INLINE_SIM_FPU (int)
sim_fpu_mul (sim_fpu *f,
	     const sim_fpu *l,
	     const sim_fpu *r)
{
  if (sim_fpu_is_snan (l))
    {
      *f = *l;
      f->class = sim_fpu_class_qnan;
      return sim_fpu_status_invalid_snan;
    }
  if (sim_fpu_is_snan (r))
    {
      *f = *r;
      f->class = sim_fpu_class_qnan;
      return sim_fpu_status_invalid_snan;
    }
  if (sim_fpu_is_qnan (l))
    {
      *f = *l;
      return 0;
    }
  if (sim_fpu_is_qnan (r))
    {
      *f = *r;
      return 0;
    }
  if (sim_fpu_is_infinity (l))
    {
      if (sim_fpu_is_zero (r))
	{
	  *f = sim_fpu_qnan;
	  return sim_fpu_status_invalid_imz;
	}
      *f = *l;
      f->sign = l->sign ^ r->sign;
      return 0;
    }
  if (sim_fpu_is_infinity (r))
    {
      if (sim_fpu_is_zero (l))
	{
	  *f = sim_fpu_qnan;
	  return sim_fpu_status_invalid_imz;
	}
      *f = *r;
      f->sign = l->sign ^ r->sign;
      return 0;
    }
  if (sim_fpu_is_zero (l) || sim_fpu_is_zero (r))
    {
      *f = sim_fpu_zero;
      f->sign = l->sign ^ r->sign;
      return 0;
    }
  /* Calculate the mantissa by multiplying both 64bit numbers to get a
     128 bit number */
  {
    unsigned64 low;
    unsigned64 high;
    unsigned64 nl = l->fraction & 0xffffffff;
    unsigned64 nh = l->fraction >> 32;
    unsigned64 ml = r->fraction & 0xffffffff;
    unsigned64 mh = r->fraction >>32;
    unsigned64 pp_ll = ml * nl;
    unsigned64 pp_hl = mh * nl;
    unsigned64 pp_lh = ml * nh;
    unsigned64 pp_hh = mh * nh;
    unsigned64 res2 = 0;
    unsigned64 res0 = 0;
    unsigned64 ps_hh__ = pp_hl + pp_lh;
    if (ps_hh__ < pp_hl)
      res2 += UNSIGNED64 (0x100000000);
    pp_hl = (ps_hh__ << 32) & UNSIGNED64 (0xffffffff00000000);
    res0 = pp_ll + pp_hl;
    if (res0 < pp_ll)
      res2++;
    res2 += ((ps_hh__ >> 32) & 0xffffffff) + pp_hh;
    high = res2;
    low = res0;
    
    f->normal_exp = l->normal_exp + r->normal_exp;
    f->sign = l->sign ^ r->sign;
    f->class = sim_fpu_class_number;

    /* Input is bounded by [1,2)   ;   [2^60,2^61)
       Output is bounded by [1,4)  ;   [2^120,2^122) */
 
    /* Adjust the exponent according to where the decimal point ended
       up in the high 64 bit word.  In the source the decimal point
       was at NR_FRAC_GUARD. */
    f->normal_exp += NR_FRAC_GUARD + 64 - (NR_FRAC_GUARD * 2);

    /* The high word is bounded according to the above.  Consequently
       it has never overflowed into IMPLICIT_2. */
    ASSERT (high < LSBIT64 (((NR_FRAC_GUARD + 1) * 2) - 64));
    ASSERT (high >= LSBIT64 ((NR_FRAC_GUARD * 2) - 64));
    ASSERT (LSBIT64 (((NR_FRAC_GUARD + 1) * 2) - 64) < IMPLICIT_1);

    /* normalize */
    do
      {
	f->normal_exp--;
	high <<= 1;
	if (low & LSBIT64 (63))
	  high |= 1;
	low <<= 1;
      }
    while (high < IMPLICIT_1);

    ASSERT (high >= IMPLICIT_1 && high < IMPLICIT_2);
    if (low != 0)
      {
	f->fraction = (high | 1); /* sticky */
	return sim_fpu_status_inexact;
      }
    else
      {
	f->fraction = high;
	return 0;
      }
    return 0;
  }
}

INLINE_SIM_FPU (int)
sim_fpu_div (sim_fpu *f,
	     const sim_fpu *l,
	     const sim_fpu *r)
{
  if (sim_fpu_is_snan (l))
    {
      *f = *l;
      f->class = sim_fpu_class_qnan;
      return sim_fpu_status_invalid_snan;
    }
  if (sim_fpu_is_snan (r))
    {
      *f = *r;
      f->class = sim_fpu_class_qnan;
      return sim_fpu_status_invalid_snan;
    }
  if (sim_fpu_is_qnan (l))
    {
      *f = *l;
      f->class = sim_fpu_class_qnan;
      return 0;
    }
  if (sim_fpu_is_qnan (r))
    {
      *f = *r;
      f->class = sim_fpu_class_qnan;
      return 0;
    }
  if (sim_fpu_is_infinity (l))
    {
      if (sim_fpu_is_infinity (r))
	{
	  *f = sim_fpu_qnan;
	  return sim_fpu_status_invalid_idi;
	}
      else
	{
	  *f = *l;
	  f->sign = l->sign ^ r->sign;
	  return 0;
	}
    }
  if (sim_fpu_is_zero (l))
    {
      if (sim_fpu_is_zero (r))
	{
	  *f = sim_fpu_qnan;
	  return sim_fpu_status_invalid_zdz;
	}
      else
	{
	  *f = *l;
	  f->sign = l->sign ^ r->sign;
	  return 0;
	}
    }
  if (sim_fpu_is_infinity (r))
    {
      *f = sim_fpu_zero;
      f->sign = l->sign ^ r->sign;
      return 0;
    }
  if (sim_fpu_is_zero (r))
    {
      f->class = sim_fpu_class_infinity;
      f->sign = l->sign ^ r->sign;
      return sim_fpu_status_invalid_div0;
    }

  /* Calculate the mantissa by multiplying both 64bit numbers to get a
     128 bit number */
  {
    /* quotient =  ( ( numerator / denominator)
                      x 2^(numerator exponent -  denominator exponent)
     */
    unsigned64 numerator;
    unsigned64 denominator;
    unsigned64 quotient;
    unsigned64 bit;

    f->class = sim_fpu_class_number;
    f->sign = l->sign ^ r->sign;
    f->normal_exp = l->normal_exp - r->normal_exp;

    numerator = l->fraction;
    denominator = r->fraction;

    /* Fraction will be less than 1.0 */
    if (numerator < denominator)
      {
	numerator <<= 1;
	f->normal_exp--;
      }
    ASSERT (numerator >= denominator);
    
    /* Gain extra precision, already used one spare bit */
    numerator <<=    NR_SPARE;
    denominator <<=  NR_SPARE;

    /* Does divide one bit at a time.  Optimize???  */
    quotient = 0;
    bit = (IMPLICIT_1 << NR_SPARE);
    while (bit)
      {
	if (numerator >= denominator)
	  {
	    quotient |= bit;
	    numerator -= denominator;
	  }
	bit >>= 1;
	numerator <<= 1;
      }

    /* discard (but save) the extra bits */
    if ((quotient & LSMASK64 (NR_SPARE -1, 0)))
      quotient = (quotient >> NR_SPARE) | 1;
    else
      quotient = (quotient >> NR_SPARE);

    f->fraction = quotient;
    ASSERT (f->fraction >= IMPLICIT_1 && f->fraction < IMPLICIT_2);
    if (numerator != 0)
      {
	f->fraction |= 1; /* stick remaining bits */
	return sim_fpu_status_inexact;
      }
    else
      return 0;
  }
}


INLINE_SIM_FPU (int)
sim_fpu_max (sim_fpu *f,
	     const sim_fpu *l,
	     const sim_fpu *r)
{
  if (sim_fpu_is_snan (l))
    {
      *f = *l;
      f->class = sim_fpu_class_qnan;
      return sim_fpu_status_invalid_snan;
    }
  if (sim_fpu_is_snan (r))
    {
      *f = *r;
      f->class = sim_fpu_class_qnan;
      return sim_fpu_status_invalid_snan;
    }
  if (sim_fpu_is_qnan (l))
    {
      *f = *l;
      return 0;
    }
  if (sim_fpu_is_qnan (r))
    {
      *f = *r;
      return 0;
    }
  if (sim_fpu_is_infinity (l))
    {
      if (sim_fpu_is_infinity (r)
	  && l->sign == r->sign)
	{
	  *f = sim_fpu_qnan;
	  return sim_fpu_status_invalid_isi;
	}
      if (l->sign)
	*f = *r; /* -inf < anything */
      else
	*f = *l; /* +inf > anthing */
      return 0;
    }
  if (sim_fpu_is_infinity (r))
    {
      if (r->sign)
	*f = *l; /* anything > -inf */
      else
	*f = *r; /* anthing < +inf */
      return 0;
    }
  if (l->sign > r->sign)
    {
      *f = *r; /* -ve < +ve */
      return 0;
    }
  if (l->sign < r->sign)
    {
      *f = *l; /* +ve > -ve */
      return 0;
    }
  ASSERT (l->sign == r->sign);
  if (l->normal_exp > r->normal_exp
      || (l->normal_exp == r->normal_exp && 
	  l->fraction > r->fraction))
    {
      /* |l| > |r| */
      if (l->sign)
	*f = *r; /* -ve < -ve */
      else
	*f = *l; /* +ve > +ve */
      return 0;
    }
  else
    {
      /* |l| <= |r| */
      if (l->sign)
	*f = *l; /* -ve > -ve */
      else
	*f = *r; /* +ve < +ve */
      return 0;
    }
}


INLINE_SIM_FPU (int)
sim_fpu_min (sim_fpu *f,
	     const sim_fpu *l,
	     const sim_fpu *r)
{
  if (sim_fpu_is_snan (l))
    {
      *f = *l;
      f->class = sim_fpu_class_qnan;
      return sim_fpu_status_invalid_snan;
    }
  if (sim_fpu_is_snan (r))
    {
      *f = *r;
      f->class = sim_fpu_class_qnan;
      return sim_fpu_status_invalid_snan;
    }
  if (sim_fpu_is_qnan (l))
    {
      *f = *l;
      return 0;
    }
  if (sim_fpu_is_qnan (r))
    {
      *f = *r;
      return 0;
    }
  if (sim_fpu_is_infinity (l))
    {
      if (sim_fpu_is_infinity (r)
	  && l->sign == r->sign)
	{
	  *f = sim_fpu_qnan;
	  return sim_fpu_status_invalid_isi;
	}
      if (l->sign)
	*f = *l; /* -inf < anything */
      else
	*f = *r; /* +inf > anthing */
      return 0;
    }
  if (sim_fpu_is_infinity (r))
    {
      if (r->sign)
	*f = *r; /* anything > -inf */
      else
	*f = *l; /* anything < +inf */
      return 0;
    }
  if (l->sign > r->sign)
    {
      *f = *l; /* -ve < +ve */
      return 0;
    }
  if (l->sign < r->sign)
    {
      *f = *r; /* +ve > -ve */
      return 0;
    }
  ASSERT (l->sign == r->sign);
  if (l->normal_exp > r->normal_exp
      || (l->normal_exp == r->normal_exp && 
	  l->fraction > r->fraction))
    {
      /* |l| > |r| */
      if (l->sign)
	*f = *l; /* -ve < -ve */
      else
	*f = *r; /* +ve > +ve */
      return 0;
    }
  else
    {
      /* |l| <= |r| */
      if (l->sign)
	*f = *r; /* -ve > -ve */
      else
	*f = *l; /* +ve < +ve */
      return 0;
    }
}


INLINE_SIM_FPU (int)
sim_fpu_neg (sim_fpu *f,
	     const sim_fpu *r)
{
  if (sim_fpu_is_snan (r))
    {
      *f = *r;
      f->class = sim_fpu_class_qnan;
      return sim_fpu_status_invalid_snan;
    }
  if (sim_fpu_is_qnan (r))
    {
      *f = *r;
      return 0;
    }
  *f = *r;
  f->sign = !r->sign;
  return 0;
}


INLINE_SIM_FPU (int)
sim_fpu_abs (sim_fpu *f,
	     const sim_fpu *r)
{
  *f = *r;
  f->sign = 0;
  if (sim_fpu_is_snan (r))
    {
      f->class = sim_fpu_class_qnan;
      return sim_fpu_status_invalid_snan;
    }
  return 0;
}


INLINE_SIM_FPU (int)
sim_fpu_inv (sim_fpu *f,
	     const sim_fpu *r)
{
  return sim_fpu_div (f, &sim_fpu_one, r);
}


INLINE_SIM_FPU (int)
sim_fpu_sqrt (sim_fpu *f,
	      const sim_fpu *r)
{
  if (sim_fpu_is_snan (r))
    {
      *f = sim_fpu_qnan;
      return sim_fpu_status_invalid_snan;
    }
  if (sim_fpu_is_qnan (r))
    {
      *f = sim_fpu_qnan;
      return 0;
    }
  if (sim_fpu_is_zero (r))
    {
      f->class = sim_fpu_class_zero;
      f->sign = r->sign;
      f->normal_exp = 0;
      return 0;
    }
  if (sim_fpu_is_infinity (r))
    {
      if (r->sign)
	{
	  *f = sim_fpu_qnan;
	  return sim_fpu_status_invalid_sqrt;
	}
      else
	{
	  f->class = sim_fpu_class_infinity;
	  f->sign = 0;
	  f->sign = 0;
	  return 0;
	}
    }
  if (r->sign)
    {
      *f = sim_fpu_qnan;
      return sim_fpu_status_invalid_sqrt;
    }

  /* @(#)e_sqrt.c 5.1 93/09/24 */
  /*
   * ====================================================
   * Copyright (C) 1993 by Sun Microsystems, Inc. All rights reserved.
   *
   * Developed at SunPro, a Sun Microsystems, Inc. business.
   * Permission to use, copy, modify, and distribute this
   * software is freely granted, provided that this notice 
   * is preserved.
   * ====================================================
   */
  
  /* __ieee754_sqrt(x)
   * Return correctly rounded sqrt.
   *           ------------------------------------------
   *           |  Use the hardware sqrt if you have one |
   *           ------------------------------------------
   * Method: 
   *   Bit by bit method using integer arithmetic. (Slow, but portable) 
   *   1. Normalization
   *	Scale x to y in [1,4) with even powers of 2: 
   *	find an integer k such that  1 <= (y=x*2^(2k)) < 4, then
   *		sqrt(x) = 2^k * sqrt(y)
   -
   - Since:
   -   sqrt ( x*2^(2m) )     = sqrt(x).2^m    ; m even
   -   sqrt ( x*2^(2m + 1) ) = sqrt(2.x).2^m  ; m odd
   - Define:
   -   y = ((m even) ? x : 2.x)
   - Then:
   -   y in [1, 4)                            ; [IMPLICIT_1,IMPLICIT_4)
   - And:
   -   sqrt (y) in [1, 2)                     ; [IMPLICIT_1,IMPLICIT_2)
   -
   *   2. Bit by bit computation
   *	Let q  = sqrt(y) truncated to i bit after binary point (q = 1),
   *	     i							 0
   *                                     i+1         2
   *	    s  = 2*q , and	y  =  2   * ( y - q  ).		(1)
   *	     i      i            i                 i
   *                                                        
   *	To compute q    from q , one checks whether 
   *		    i+1       i                       
   *
   *			      -(i+1) 2
   *			(q + 2      ) <= y.			(2)
   *     			  i
   *							      -(i+1)
   *	If (2) is false, then q   = q ; otherwise q   = q  + 2      .
   *		 	       i+1   i             i+1   i
   *
   *	With some algebric manipulation, it is not difficult to see
   *	that (2) is equivalent to 
   *                             -(i+1)
   *			s  +  2       <= y			(3)
   *			 i                i
   *
   *	The advantage of (3) is that s  and y  can be computed by 
   *				      i      i
   *	the following recurrence formula:
   *	    if (3) is false
   *
   *	    s     =  s  ,	y    = y   ;			(4)
   *	     i+1      i		 i+1    i
   *
   -
   -                      NOTE: y    = 2*y
   -                             i+1      i
   -
   *	    otherwise,
   *                       -i                      -(i+1)
   *	    s	  =  s  + 2  ,  y    = y  -  s  - 2  		(5)
   *         i+1      i          i+1    i     i
   *				
   -
   -                                                   -(i+1)
   -                      NOTE: y    = 2 (y  -  s  -  2      ) 		
   -                             i+1       i     i
   -
   *	One may easily use induction to prove (4) and (5). 
   *	Note. Since the left hand side of (3) contain only i+2 bits,
   *	      it does not necessary to do a full (53-bit) comparison 
   *	      in (3).
   *   3. Final rounding
   *	After generating the 53 bits result, we compute one more bit.
   *	Together with the remainder, we can decide whether the
   *	result is exact, bigger than 1/2ulp, or less than 1/2ulp
   *	(it will never equal to 1/2ulp).
   *	The rounding mode can be detected by checking whether
   *	huge + tiny is equal to huge, and whether huge - tiny is
   *	equal to huge for some floating point number "huge" and "tiny".
   *		
   * Special cases:
   *	sqrt(+-0) = +-0 	... exact
   *	sqrt(inf) = inf
   *	sqrt(-ve) = NaN		... with invalid signal
   *	sqrt(NaN) = NaN		... with invalid signal for signaling NaN
   *
   * Other methods : see the appended file at the end of the program below.
   *---------------
   */

  {
    /* generate sqrt(x) bit by bit */
    unsigned64 y;
    unsigned64 q;
    unsigned64 s;
    unsigned64 b;

    f->class = sim_fpu_class_number;
    f->sign = 0;
    y = r->fraction;
    f->normal_exp = (r->normal_exp >> 1);	/* exp = [exp/2] */

    /* odd exp, double x to make it even */
    ASSERT (y >= IMPLICIT_1 && y < IMPLICIT_4);
    if ((r->normal_exp & 1))
      {
	y += y;
      }
    ASSERT (y >= IMPLICIT_1 && y < (IMPLICIT_2 << 1));

    /* Let loop determine first value of s (either 1 or 2) */
    b = IMPLICIT_1;
    q = 0;
    s = 0;
    
    while (b)
      {
	unsigned64 t = s + b;
	if (t <= y)
	  {
	    s |= (b << 1);
	    y -= t;
	    q |= b;
	  }
	y <<= 1;
	b >>= 1;
      }

    ASSERT (q >= IMPLICIT_1 && q < IMPLICIT_2);
    f->fraction = q;
    if (y != 0)
      {
	f->fraction |= 1; /* stick remaining bits */
	return sim_fpu_status_inexact;
      }
    else
      return 0;
  }
}


/* int/long <-> sim_fpu */

INLINE_SIM_FPU (int)
sim_fpu_i32to (sim_fpu *f,
	       signed32 i,
	       sim_fpu_round round)
{
  i2fpu (f, i, 0);
  return 0;
}

INLINE_SIM_FPU (int)
sim_fpu_u32to (sim_fpu *f,
	       unsigned32 u,
	       sim_fpu_round round)
{
  u2fpu (f, u, 0);
  return 0;
}

INLINE_SIM_FPU (int)
sim_fpu_i64to (sim_fpu *f,
	       signed64 i,
	       sim_fpu_round round)
{
  i2fpu (f, i, 1);
  return 0;
}

INLINE_SIM_FPU (int)
sim_fpu_u64to (sim_fpu *f,
	       unsigned64 u,
	       sim_fpu_round round)
{
  u2fpu (f, u, 1);
  return 0;
}


INLINE_SIM_FPU (int)
sim_fpu_to32i (signed32 *i,
	       const sim_fpu *f,
	       sim_fpu_round round)
{
  signed64 i64;
  int status = fpu2i (&i64, f, 0, round);
  *i = i64;
  return status;
}

INLINE_SIM_FPU (int)
sim_fpu_to32u (unsigned32 *u,
	       const sim_fpu *f,
	       sim_fpu_round round)
{
  unsigned64 u64;
  int status = fpu2u (&u64, f, 0);
  *u = u64;
  return status;
}

INLINE_SIM_FPU (int)
sim_fpu_to64i (signed64 *i,
	       const sim_fpu *f,
	       sim_fpu_round round)
{
  return fpu2i (i, f, 1, round);
}


INLINE_SIM_FPU (int)
sim_fpu_to64u (unsigned64 *u,
	       const sim_fpu *f,
	       sim_fpu_round round)
{
  return fpu2u (u, f, 1);
}



/* sim_fpu -> host format */

#if 0
INLINE_SIM_FPU (float)
sim_fpu_2f (const sim_fpu *f)
{
  return fval.d;
}
#endif


INLINE_SIM_FPU (double)
sim_fpu_2d (const sim_fpu *s)
{
  sim_fpu_map val;
  if (sim_fpu_is_snan (s))
    {
      /* gag SNaN's */
      sim_fpu n = *s;
      n.class = sim_fpu_class_qnan;
      val.i = pack_fpu (&n, 1);
    }
  else
    {
      val.i = pack_fpu (s, 1);
    }
  return val.d;
}


#if 0
INLINE_SIM_FPU (void)
sim_fpu_f2 (sim_fpu *f,
	    float s)
{
  sim_fpu_map val;
  val.d = s;
  unpack_fpu (f, val.i, 1);
}
#endif


INLINE_SIM_FPU (void)
sim_fpu_d2 (sim_fpu *f,
	    double d)
{
  sim_fpu_map val;
  val.d = d;
  unpack_fpu (f, val.i, 1);
}


/* General */

INLINE_SIM_FPU (int)
sim_fpu_is_nan (const sim_fpu *d)
{
  switch (d->class)
    {
    case sim_fpu_class_qnan:
    case sim_fpu_class_snan:
      return 1;
    default:
      return 0;
    }
}

INLINE_SIM_FPU (int)
sim_fpu_is_qnan (const sim_fpu *d)
{
  switch (d->class)
    {
    case sim_fpu_class_qnan:
      return 1;
    default:
      return 0;
    }
}

INLINE_SIM_FPU (int)
sim_fpu_is_snan (const sim_fpu *d)
{
  switch (d->class)
    {
    case sim_fpu_class_snan:
      return 1;
    default:
      return 0;
    }
}

INLINE_SIM_FPU (int)
sim_fpu_is_zero (const sim_fpu *d)
{
  switch (d->class)
    {
    case sim_fpu_class_zero:
      return 1;
    default:
      return 0;
    }
}

INLINE_SIM_FPU (int)
sim_fpu_is_infinity (const sim_fpu *d)
{
  switch (d->class)
    {
    case sim_fpu_class_infinity:
      return 1;
    default:
      return 0;
    }
}

INLINE_SIM_FPU (int)
sim_fpu_is_number (const sim_fpu *d)
{
  switch (d->class)
    {
    case sim_fpu_class_denorm:
    case sim_fpu_class_number:
      return 1;
    default:
      return 0;
    }
}

INLINE_SIM_FPU (int)
sim_fpu_is_denorm (const sim_fpu *d)
{
  switch (d->class)
    {
    case sim_fpu_class_denorm:
      return 1;
    default:
      return 0;
    }
}


INLINE_SIM_FPU (int)
sim_fpu_sign (const sim_fpu *d)
{
  return d->sign;
}


INLINE_SIM_FPU (int)
sim_fpu_exp (const sim_fpu *d)
{
  return d->normal_exp;
}


INLINE_SIM_FPU (unsigned64)
sim_fpu_fraction (const sim_fpu *d)
{
  return d->fraction;
}


INLINE_SIM_FPU (unsigned64)
sim_fpu_guard (const sim_fpu *d, int is_double)
{
  unsigned64 rv;
  unsigned64 guardmask = LSMASK64 (NR_GUARDS - 1, 0);
  rv = (d->fraction & guardmask) >> NR_PAD;
  return rv;
}


INLINE_SIM_FPU (int)
sim_fpu_is (const sim_fpu *d)
{
  switch (d->class)
    {
    case sim_fpu_class_qnan:
      return SIM_FPU_IS_QNAN;
    case sim_fpu_class_snan:
      return SIM_FPU_IS_SNAN;
    case sim_fpu_class_infinity:
      if (d->sign)
	return SIM_FPU_IS_NINF;
      else
	return SIM_FPU_IS_PINF;
    case sim_fpu_class_number:
      if (d->sign)
	return SIM_FPU_IS_NNUMBER;
      else
	return SIM_FPU_IS_PNUMBER;
    case sim_fpu_class_denorm:
      if (d->sign)
	return SIM_FPU_IS_NDENORM;
      else
	return SIM_FPU_IS_PDENORM;
    case sim_fpu_class_zero:
      if (d->sign)
	return SIM_FPU_IS_NZERO;
      else
	return SIM_FPU_IS_PZERO;
    default:
      return -1;
      abort ();
    }
}

INLINE_SIM_FPU (int)
sim_fpu_cmp (const sim_fpu *l, const sim_fpu *r)
{
  sim_fpu res;
  sim_fpu_sub (&res, l, r);
  return sim_fpu_is (&res);
}

INLINE_SIM_FPU (int)
sim_fpu_is_lt (const sim_fpu *l, const sim_fpu *r)
{
  int status;
  sim_fpu_lt (&status, l, r);
  return status;
}

INLINE_SIM_FPU (int)
sim_fpu_is_le (const sim_fpu *l, const sim_fpu *r)
{
  int is;
  sim_fpu_le (&is, l, r);
  return is;
}

INLINE_SIM_FPU (int)
sim_fpu_is_eq (const sim_fpu *l, const sim_fpu *r)
{
  int is;
  sim_fpu_eq (&is, l, r);
  return is;
}

INLINE_SIM_FPU (int)
sim_fpu_is_ne (const sim_fpu *l, const sim_fpu *r)
{
  int is;
  sim_fpu_ne (&is, l, r);
  return is;
}

INLINE_SIM_FPU (int)
sim_fpu_is_ge (const sim_fpu *l, const sim_fpu *r)
{
  int is;
  sim_fpu_ge (&is, l, r);
  return is;
}

INLINE_SIM_FPU (int)
sim_fpu_is_gt (const sim_fpu *l, const sim_fpu *r)
{
  int is;
  sim_fpu_gt (&is, l, r);
  return is;
}


/* Compare operators */

INLINE_SIM_FPU (int)
sim_fpu_lt (int *is,
	    const sim_fpu *l,
	    const sim_fpu *r)
{
  if (!sim_fpu_is_nan (l) && !sim_fpu_is_nan (r))
    {
      sim_fpu_map lval;
      sim_fpu_map rval;
      lval.i = pack_fpu (l, 1);
      rval.i = pack_fpu (r, 1);
      (*is) = (lval.d < rval.d);
      return 0;
    }
  else if (sim_fpu_is_snan (l) || sim_fpu_is_snan (r))
    {
      *is = 0;
      return sim_fpu_status_invalid_snan;
    }
  else
    {
      *is = 0;
      return sim_fpu_status_invalid_qnan;
    }
}

INLINE_SIM_FPU (int)
sim_fpu_le (int *is,
	    const sim_fpu *l,
	    const sim_fpu *r)
{
  if (!sim_fpu_is_nan (l) && !sim_fpu_is_nan (r))
    {
      sim_fpu_map lval;
      sim_fpu_map rval;
      lval.i = pack_fpu (l, 1);
      rval.i = pack_fpu (r, 1);
      *is = (lval.d <= rval.d);
      return 0;
    }
  else if (sim_fpu_is_snan (l) || sim_fpu_is_snan (r))
    {
      *is = 0;
      return sim_fpu_status_invalid_snan;
    }
  else
    {
      *is = 0;
      return sim_fpu_status_invalid_qnan;
    }
}

INLINE_SIM_FPU (int)
sim_fpu_eq (int *is,
	    const sim_fpu *l,
	    const sim_fpu *r)
{
  if (!sim_fpu_is_nan (l) && !sim_fpu_is_nan (r))
    {
      sim_fpu_map lval;
      sim_fpu_map rval;
      lval.i = pack_fpu (l, 1);
      rval.i = pack_fpu (r, 1);
      (*is) = (lval.d == rval.d);
      return 0;
    }
  else if (sim_fpu_is_snan (l) || sim_fpu_is_snan (r))
    {
      *is = 0;
      return sim_fpu_status_invalid_snan;
    }
  else
    {
      *is = 0;
      return sim_fpu_status_invalid_qnan;
    }
}

INLINE_SIM_FPU (int)
sim_fpu_ne (int *is,
	    const sim_fpu *l,
	    const sim_fpu *r)
{
  if (!sim_fpu_is_nan (l) && !sim_fpu_is_nan (r))
    {
      sim_fpu_map lval;
      sim_fpu_map rval;
      lval.i = pack_fpu (l, 1);
      rval.i = pack_fpu (r, 1);
      (*is) = (lval.d != rval.d);
      return 0;
    }
  else if (sim_fpu_is_snan (l) || sim_fpu_is_snan (r))
    {
      *is = 0;
      return sim_fpu_status_invalid_snan;
    }
  else
    {
      *is = 0;
      return sim_fpu_status_invalid_qnan;
    }
}

INLINE_SIM_FPU (int)
sim_fpu_ge (int *is,
	    const sim_fpu *l,
	    const sim_fpu *r)
{
  return sim_fpu_le (is, r, l);
}

INLINE_SIM_FPU (int)
sim_fpu_gt (int *is,
	    const sim_fpu *l,
	    const sim_fpu *r)
{
  return sim_fpu_lt (is, r, l);
}


/* A number of useful constants */

#if EXTERN_SIM_FPU_P
const sim_fpu sim_fpu_zero = {
  sim_fpu_class_zero,
};
const sim_fpu sim_fpu_qnan = {
  sim_fpu_class_qnan,
};
const sim_fpu sim_fpu_one = {
  sim_fpu_class_number, 0, IMPLICIT_1, 0
};
const sim_fpu sim_fpu_two = {
  sim_fpu_class_number, 0, IMPLICIT_1, 1
};
const sim_fpu sim_fpu_max32 = {
  sim_fpu_class_number, 0, LSMASK64 (NR_FRAC_GUARD, NR_GUARDS32), NORMAL_EXPMAX32
};
const sim_fpu sim_fpu_max64 = {
  sim_fpu_class_number, 0, LSMASK64 (NR_FRAC_GUARD, NR_GUARDS64), NORMAL_EXPMAX64
};
#endif


/* For debugging */

INLINE_SIM_FPU (void)
sim_fpu_print_fpu (const sim_fpu *f,
		   sim_fpu_print_func *print,
		   void *arg)
{
  sim_fpu_printn_fpu (f, print, -1, arg);
}

INLINE_SIM_FPU (void)
sim_fpu_printn_fpu (const sim_fpu *f,
		   sim_fpu_print_func *print,
		   int digits,
		   void *arg)
{
  print (arg, "%s", f->sign ? "-" : "+");
  switch (f->class)
    {
    case sim_fpu_class_qnan:
      print (arg, "0.");
      print_bits (f->fraction, NR_FRAC_GUARD - 1, digits, print, arg);
      print (arg, "*QuietNaN");
      break;
    case sim_fpu_class_snan:
      print (arg, "0.");
      print_bits (f->fraction, NR_FRAC_GUARD - 1, digits, print, arg);
      print (arg, "*SignalNaN");
      break;
    case sim_fpu_class_zero:
      print (arg, "0.0");
      break;
    case sim_fpu_class_infinity:
      print (arg, "INF");
      break;
    case sim_fpu_class_number:
    case sim_fpu_class_denorm:
      print (arg, "1.");
      print_bits (f->fraction, NR_FRAC_GUARD - 1, digits, print, arg);
      print (arg, "*2^%+d", f->normal_exp);
      ASSERT (f->fraction >= IMPLICIT_1);
      ASSERT (f->fraction < IMPLICIT_2);
    }
}


INLINE_SIM_FPU (void)
sim_fpu_print_status (int status,
		      sim_fpu_print_func *print,
		      void *arg)
{
  int i = 1;
  char *prefix = "";
  while (status >= i)
    {
      switch ((sim_fpu_status) (status & i))
	{
	case sim_fpu_status_denorm:
	  print (arg, "%sD", prefix);
	  break;
	case sim_fpu_status_invalid_snan:
	  print (arg, "%sSNaN", prefix);
	  break;
	case sim_fpu_status_invalid_qnan:
	  print (arg, "%sQNaN", prefix);
	  break;
	case sim_fpu_status_invalid_isi:
	  print (arg, "%sISI", prefix);
	  break;
	case sim_fpu_status_invalid_idi:
	  print (arg, "%sIDI", prefix);
	  break;
	case sim_fpu_status_invalid_zdz:
	  print (arg, "%sZDZ", prefix);
	  break;
	case sim_fpu_status_invalid_imz:
	  print (arg, "%sIMZ", prefix);
	  break;
	case sim_fpu_status_invalid_cvi:
	  print (arg, "%sCVI", prefix);
	  break;
	case sim_fpu_status_invalid_cmp:
	  print (arg, "%sCMP", prefix);
	  break;
	case sim_fpu_status_invalid_sqrt:
	  print (arg, "%sSQRT", prefix);
	  break;
	  break;
	case sim_fpu_status_inexact:
	  print (arg, "%sX", prefix);
	  break;
	  break;
	case sim_fpu_status_overflow:
	  print (arg, "%sO", prefix);
	  break;
	  break;
	case sim_fpu_status_underflow:
	  print (arg, "%sU", prefix);
	  break;
	  break;
	case sim_fpu_status_invalid_div0:
	  print (arg, "%s/", prefix);
	  break;
	  break;
	case sim_fpu_status_rounded:
	  print (arg, "%sR", prefix);
	  break;
	  break;
	}
      i <<= 1;
      prefix = ",";
    }
}

#endif
