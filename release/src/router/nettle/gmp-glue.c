/* gmp-glue.c

   Copyright (C) 2013 Niels Möller
   Copyright (C) 2013 Red Hat

   This file is part of GNU Nettle.

   GNU Nettle is free software: you can redistribute it and/or
   modify it under the terms of either:

     * the GNU Lesser General Public License as published by the Free
       Software Foundation; either version 3 of the License, or (at your
       option) any later version.

   or

     * the GNU General Public License as published by the Free
       Software Foundation; either version 2 of the License, or (at your
       option) any later version.

   or both in parallel, as here.

   GNU Nettle is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   General Public License for more details.

   You should have received copies of the GNU General Public License and
   the GNU Lesser General Public License along with this program.  If
   not, see http://www.gnu.org/licenses/.
*/

#if HAVE_CONFIG_H
# include "config.h"
#endif

#include <assert.h>
#include <stdlib.h>

#include "gmp-glue.h"

#if !GMP_HAVE_mpz_limbs_read

/* This implementation tries to make a minimal use of GMP internals.
   We access and _mp_size and _mp_d, but not _mp_alloc. */

/* Use macros compatible with gmp-impl.h. */
#define ABS(x) ((x) >= 0 ? (x) : -(x))
#define PTR(x) ((x)->_mp_d)
#define SIZ(x) ((x)->_mp_size)
#define ABSIZ(x) ABS (SIZ (x))

#define MPN_NORMALIZE(xp, xn) do {		\
    while ( (xn) > 0 && (xp)[xn-1] == 0)	\
      (xn)--;					\
  }  while (0)

/* NOTE: Makes an unnecessary realloc if allocation is already large
   enough, but looking at _mp_alloc may break in future GMP
   versions. */
#define MPZ_REALLOC(x, n) \
  (ABSIZ(x) >= (n) ? PTR(x) : (_mpz_realloc ((x),(n)), PTR (x)))

#define MPZ_NEWALLOC MPZ_REALLOC

/* Read access to mpz numbers. */

/* Return limb pointer, for read-only operations. Use mpz_size to get
   the number of limbs. */
const mp_limb_t *
mpz_limbs_read (mpz_srcptr x)
{
  return PTR (x);
}

/* Write access to mpz numbers. */

/* Get a limb pointer for writing, previous contents may be
   destroyed. */
mp_limb_t *
mpz_limbs_write (mpz_ptr x, mp_size_t n)
{
  assert (n > 0);
  return MPZ_NEWALLOC (x, n);
}

/* Get a limb pointer for writing, previous contents is intact. */
mp_limb_t *
mpz_limbs_modify (mpz_ptr x, mp_size_t n)
{
  assert (n > 0);
  return MPZ_REALLOC (x, n);
}

void
mpz_limbs_finish (mpz_ptr x, mp_size_t n)
{
  assert (n >= 0);
  MPN_NORMALIZE (PTR(x), n);

  SIZ (x) = n;
}

/* Needs some ugly casts. */
mpz_srcptr
mpz_roinit_n (mpz_ptr x, const mp_limb_t *xp, mp_size_t xs)
{
  mp_size_t xn = ABS (xs);
  
  MPN_NORMALIZE (xp, xn);

  x->_mp_size = xs < 0 ? -xn : xn;
  x->_mp_alloc = 0;
  x->_mp_d = (mp_limb_t *) xp;
  return x;
}
#endif /* !GMP_HAVE_mpz_limbs_read */

#if !GMP_HAVE_mpn_copyd
void
mpn_copyd (mp_ptr dst, mp_srcptr src, mp_size_t n)
{
  mp_size_t i;
  for (i = n - 1; i >= 0; i--)
    dst[i] = src[i];
}

void
mpn_copyi (mp_ptr dst, mp_srcptr src, mp_size_t n)
{
  mp_size_t i;
  for (i = 0; i < n; i++)
    dst[i] = src[i];
}

void
mpn_zero (mp_ptr ptr, mp_size_t n)
{
  mp_size_t i;
  for (i = 0; i < n; i++)
    ptr[i] = 0;
}
#endif /* !GMP_HAVE_mpn_copyd */

void
cnd_swap (mp_limb_t cnd, mp_limb_t *ap, mp_limb_t *bp, mp_size_t n)
{
  mp_limb_t mask = - (mp_limb_t) (cnd != 0);
  mp_size_t i;
  for (i = 0; i < n; i++)
    {
      mp_limb_t a, b, t;
      a = ap[i];
      b = bp[i];
      t = (a ^ b) & mask;
      ap[i] = a ^ t;
      bp[i] = b ^ t;
    }
}

/* Additional convenience functions. */

int
mpz_limbs_cmp (mpz_srcptr a, const mp_limb_t *bp, mp_size_t bn)
{
  mp_size_t an = mpz_size (a);
  assert (mpz_sgn (a) >= 0);
  assert (bn >= 0);

  if (an < bn)
    return -1;
  if (an > bn)
    return 1;
  if (an == 0)
    return 0;

  return mpn_cmp (mpz_limbs_read(a), bp, an);
}

/* Get a pointer to an n limb area, for read-only operation. n must be
   greater or equal to the current size, and the mpz is zero-padded if
   needed. */
const mp_limb_t *
mpz_limbs_read_n (mpz_ptr x, mp_size_t n)
{
  mp_size_t xn = mpz_size (x);
  mp_ptr xp;
  
  assert (xn <= n);

  xp = mpz_limbs_modify (x, n);

  if (xn < n)
    mpn_zero (xp + xn, n - xn);

  return xp;
}

void
mpz_limbs_copy (mp_limb_t *xp, mpz_srcptr x, mp_size_t n)
{
  mp_size_t xn = mpz_size (x);

  assert (xn <= n);
  mpn_copyi (xp, mpz_limbs_read (x), xn);
  if (xn < n)
    mpn_zero (xp + xn, n - xn);
}

void
mpz_set_n (mpz_t r, const mp_limb_t *xp, mp_size_t xn)
{
  mpn_copyi (mpz_limbs_write (r, xn), xp, xn);
  mpz_limbs_finish (r, xn);
}

void
mpn_set_base256 (mp_limb_t *rp, mp_size_t rn,
		 const uint8_t *xp, size_t xn)
{
  size_t xi;
  mp_limb_t out;
  unsigned bits;
  for (xi = xn, out = bits = 0; xi > 0 && rn > 0; )
    {
      mp_limb_t in = xp[--xi];
      out |= (in << bits) & GMP_NUMB_MASK;
      bits += 8;
      if (bits >= GMP_NUMB_BITS)
	{
	  *rp++ = out;
	  rn--;

	  bits -= GMP_NUMB_BITS;
	  out = in >> (8 - bits);
	}
    }
  if (rn > 0)
    {
      *rp++ = out;
      if (--rn > 0)
	mpn_zero (rp, rn);
    }
}

void
mpn_set_base256_le (mp_limb_t *rp, mp_size_t rn,
		    const uint8_t *xp, size_t xn)
{
  size_t xi;
  mp_limb_t out;
  unsigned bits;
  for (xi = 0, out = bits = 0; xi < xn && rn > 0; )
    {
      mp_limb_t in = xp[xi++];
      out |= (in << bits) & GMP_NUMB_MASK;
      bits += 8;
      if (bits >= GMP_NUMB_BITS)
	{
	  *rp++ = out;
	  rn--;

	  bits -= GMP_NUMB_BITS;
	  out = in >> (8 - bits);
	}
    }
  if (rn > 0)
    {
      *rp++ = out;
      if (--rn > 0)
	mpn_zero (rp, rn);
    }
}

void
mpn_get_base256_le (uint8_t *rp, size_t rn,
		    const mp_limb_t *xp, mp_size_t xn)
{
  unsigned bits;
  mp_limb_t in;
  for (bits = in = 0; xn > 0 && rn > 0; )
    {
      if (bits >= 8)
	{
	  *rp++ = in;
	  rn--;
	  in >>= 8;
	  bits -= 8;
	}
      else
	{
	  uint8_t old = in;
	  in = *xp++;
	  xn--;
	  *rp++ = old | (in << bits);
	  rn--;
	  in >>= (8 - bits);
	  bits += GMP_NUMB_BITS - 8;
	}
    }
  while (rn > 0)
    {
      *rp++ = in;
      rn--;
      in >>= 8;
    }
}

mp_limb_t *
gmp_alloc_limbs (mp_size_t n)
{

  void *(*alloc_func)(size_t);

  assert (n > 0);

  mp_get_memory_functions (&alloc_func, NULL, NULL);
  return (mp_limb_t *) alloc_func ( (size_t) n * sizeof(mp_limb_t));
}

void
gmp_free_limbs (mp_limb_t *p, mp_size_t n)
{
  void (*free_func)(void *, size_t);
  assert (n > 0);
  assert (p != 0);
  mp_get_memory_functions (NULL, NULL, &free_func);

  free_func (p, (size_t) n * sizeof(mp_limb_t));
}

void *
gmp_alloc(size_t n)
{
  void *(*alloc_func)(size_t);
  assert (n > 0);

  mp_get_memory_functions(&alloc_func, NULL, NULL);

  return alloc_func (n);
}

void
gmp_free(void *p, size_t n)
{
  void (*free_func)(void *, size_t);
  assert (n > 0);
  assert (p != 0);
  mp_get_memory_functions (NULL, NULL, &free_func);

  free_func (p, (size_t) n);
}
